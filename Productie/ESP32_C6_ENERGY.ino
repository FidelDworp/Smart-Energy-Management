// ESP32_C6_ENERGY_v1_26.ino — Zarlar Smart Energy Controller
// Developed by Filip Delannoy, april 2026.
// Bereikbaar op http://192.168.0.73
//
// PARTITIETABEL: Compileer met "partitions_16mb.csv" in de schetsmap
//   nvs,     data, nvs,   0x9000,   0x5000,
//   otadata, data, ota,   0xe000,   0x2000,
//   app0,    app,  ota_0, 0x10000,  0x600000,
//   app1,    app,  ota_1, 0x610000, 0x600000,
//   spiffs,  data, spiffs,0xC10000, 0x3F0000,
//
// ── VERSIEHISTORIE ──────────────────────────────────────────
// 25apr26 v1.26 Twee onafhankelijke simulatievlaggen:
//               SIM_S0: simuleert 3× S0-kanalen (solar, SCH afname, SCH injectie)
//               SIM_P1: simuleert P1-dongle data (WON afname + injectie)
//               Beide apart instelbaar via /settings en serieel commando's
//               NOOIT automatisch omschakelen — bewuste keuze per kanaal
//               HomeWizard P1 Meter HWE-P1-RJ12 integratie via lokale REST API:
//                 GET http://<P1_IP>/api/v1/data → JSON met active_power_w,
//                 total_power_import_t1/t2_kwh, total_power_export_t1/t2_kwh
//               P1_IP instelbaar via /settings, NVS-persistent
//               /json: sim_s0 en sim_p1 als aparte indicatoren toegevoegd
//               WON vermogen (b), WON dag import (i), WON dag export (vw) actief
//
// 25apr26 v1.25 Enkelvoudige SIMULATION_MODE (archief)
// 24apr26 v1.24 Eerste productieversie — live S0 ISR
//
// ── HARDWARE ────────────────────────────────────────────────
//   ESP32-C6 32-pin · Zarlar shield · IP 192.168.0.73
//   Roomsense RJ45 → S0 interface printje → 3× Inepro PRO380-S
//   Pixel-line connector → WS2812B matrix 12×4 (48 px, aparte 5V)
//   Voeding: 5V/2A extern (matrix NIET via shield PTC)
//
// S0 interface per kanaal (universeel printje):
//   3,3V ─[4,7kΩ]─┬─ GPIO (INPUT, geen interne pull-up)
//                 [1kΩ] serie
//                  │
//                 S0+ klem 18/20 (Inepro PRO380-S)
//                 S0– klem 19/21 ──── GND
//
// RJ45 Roomsense pinout:
//   Pin 1: GND | Pin 2: 3,3V | Pin 3: IO5 | Pin 4: IO6
//   Pin 5: IO7 | Pin 6: vrij | Pin 7: GND | Pin 8: 5V
//
// S0 kanalen:
//   IO5  Zonnepanelen A14 — forward productie  (klem 18/19)
//   IO6  Schuur A5 — forward afname            (klem 18/19)
//   IO7  Schuur A5 — reverse injectie          (klem 20/21)
//   IO4  WS2812B matrix DIN (via 330Ω, Pixel-line connector)
//
// HomeWizard P1 Meter HWE-P1-RJ12:
//   Model: HWE-P1-RJ12 · 5V 500mA · 2.4GHz WiFi
//   Aansluting: RJ12 op P1-poort digitale meter (WON, ~2028)
//   Activatie: HomeWizard Energy app → Settings → Meters → Local API AAN
//   Endpoint: GET http://<P1_IP>/api/v1/data (plain HTTP, geen auth)
//   Documentatie: https://api-documentation.homewizard.com/docs/introduction/
//   GitHub library: https://github.com/jvandenaardweg/homewizard-energy-api
//   Update: elke seconde (DSMR 5.0), elke 10s (oudere meters)
//   Wij pollen elke 5s (zelfde als S0 tick) — ruim voldoende
//
// WON-verbruik: niet gemeten in fase 1 (analoge meter)
//   SIM_P1=true → gesimuleerde data  (tot ~2028)
//   SIM_P1=false + P1_IP ingesteld → live HomeWizard data (~2028+)

// VERPLICHT voor ESP32-C6 (RISC-V) in Arduino IDE
#define Serial Serial0

#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <math.h>

// ── VERSIE ──────────────────────────────────────────────────
#define FW_VERSION   "1.26"
#define CTRL_ID      "S-ENERGY"
#define NVS_NS       "senrg"

// ── PINS ────────────────────────────────────────────────────
#define S0_SOL_PIN    5
#define S0_SCHF_PIN   6
#define S0_SCHR_PIN   7
#define LED_PIN       4   // IO4 via 330Ω, Pixel-line connector — Zarlar shield

// ── MATRIX 12×4 = 48 pixels ─────────────────────────────────
// Serpentine: rij 0 top L→R, rij 1 R→L, rij 2 L→R, rij 3 bodem R→L
// Labels voor op behuizing (zie ook projectdocument §11.2):
//  Col 0=SOL W   Col 1=SOL kWh  Col 2=SCH AF   Col 3=SCH INJ
//  Col 4=NETTO   Col 5=EPEX     Col 6=EPEX+1   Col 7=PIEK%
//  Col 8=KOKEN?  Col 9=WASSEN?  Col10=HEAP     Col11=WiFi
#define MATRIX_COLS  12
#define MATRIX_ROWS   4
#define NUM_PIXELS   48
#define DEF_BRIGHT   55

// ── ENERGIE ─────────────────────────────────────────────────
#define WH_PER_PULS    0.1f
#define WATT_FACTOR    360000.0f
#define POWER_TO_MS    180000UL
#define MAX_SOL_W      6000.0f
#define MAX_SCH_W     10000.0f
#define MAX_DAG_WH    30000.0f
#define VAST_CT_KWH   14.32f      // vaste opslag Fluvius Imewo + Ecopower

// ── SIMULATIE PROFIELPARAMETERS (april, België) ─────────────
#define SIM_SOLAR_PEAK_W  4000.0f  // W zonnepanelen piek
#define SIM_BASE_LOAD_W    500.0f  // W basis SCH verbruik
#define SIM_WON_BASE_W     400.0f  // W basis WON verbruik
#define SIM_TICK_S           5.0f  // seconden per tik

// ── NETWERK ─────────────────────────────────────────────────
const char* DEF_SSID  = "";
const char* DEF_PASS  = "";
const char* DEF_IP    = "192.168.0.73";
const char* AP_SSID   = "ZarlarSetup";
const char* RPI_BASE  = "http://192.168.0.50:3000";
const char* NTP_SRV   = "pool.ntp.org";
#define     TZ_SEC     3600

// ── NVS KEYS ────────────────────────────────────────────────
const char* NVS_SSID     = "wifi_ssid";
const char* NVS_PASS     = "wifi_pass";
const char* NVS_IP       = "static_ip";
const char* NVS_SOL      = "wh_sol";
const char* NVS_SCHF     = "wh_schf";
const char* NVS_SCHR     = "wh_schr";
const char* NVS_PIEK     = "piek_w";
const char* NVS_BRI      = "bright";
const char* NVS_MPIEK    = "max_piek_w";
const char* NVS_SIM_S0   = "sim_s0";   // bool — S0 kanalen simuleren
const char* NVS_SIM_P1   = "sim_p1";   // bool — P1 dongle simuleren
const char* NVS_P1_IP    = "p1_ip";    // string — HomeWizard P1 IP adres

// ── OBJECTEN ────────────────────────────────────────────────
AsyncWebServer    server(80);
DNSServer         dnsServer;
Preferences       prefs;
Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ── GLOBALS ─────────────────────────────────────────────────
bool          ap_mode      = false;
bool          SIM_S0       = true;   // ⚠️ START IN SIMULATIE — zet UIT na S0 bekabeling
bool          SIM_P1       = true;   // ⚠️ START IN SIMULATIE — zet UIT na P1-dongle (~2028)
wl_status_t   last_wifi_st = WL_IDLE_STATUS;
char          wifi_ssid[64]= "";
char          wifi_pass[64]= "";
char          static_ip[24]= "";
char          p1_ip[24]    = "";     // HomeWizard P1 Meter IP (instelbaar)
uint32_t      max_piek_w   = 10000;

// ── ISR VARIABELEN (S0 live modus) ──────────────────────────
volatile uint32_t      isr_sol_cnt  = 0, isr_schf_cnt  = 0, isr_schr_cnt  = 0;
volatile unsigned long isr_sol_last = 0, isr_schf_last = 0, isr_schr_last = 0;
volatile unsigned long isr_sol_iv   = 0, isr_schf_iv   = 0, isr_schr_iv   = 0;

// ── WERKDATA — SCH (S0) ──────────────────────────────────────
float    w_sol   = 0, w_schf  = 0, w_schr  = 0, w_netto = 0;
float    wh_sol  = 0, wh_schf = 0, wh_schr = 0;
float    piek_w  = 0;

// ── WERKDATA — WON (P1) ──────────────────────────────────────
float    w_won        = 0;   // W momentaan (+ = afname, − = injectie)
float    wh_won_imp   = 0;   // Wh dag afname WON
float    wh_won_exp   = 0;   // Wh dag injectie WON
// NB: dagcumulatieven WON worden berekend als delta tov NVS-snapshot bij midnight

// ── EPEX ─────────────────────────────────────────────────────
float    epex_nu = 0, epex_p1h = 0;  // all-in ct/kWh

// ── TIMING ──────────────────────────────────────────────────
unsigned long t_5s   = 0;
unsigned long t_15m  = 0;
unsigned long t_epex = 0;
unsigned long t_p1   = 0;
int           last_mday = -1;
uint32_t      prev_sol = 0, prev_schf = 0, prev_schr = 0;

// ── ISR ─────────────────────────────────────────────────────
void IRAM_ATTR isrSol() {
  unsigned long n = millis();
  if (isr_sol_last)  isr_sol_iv  = n - isr_sol_last;
  isr_sol_last = n; isr_sol_cnt++;
}
void IRAM_ATTR isrSchF() {
  unsigned long n = millis();
  if (isr_schf_last) isr_schf_iv = n - isr_schf_last;
  isr_schf_last = n; isr_schf_cnt++;
}
void IRAM_ATTR isrSchR() {
  unsigned long n = millis();
  if (isr_schr_last) isr_schr_iv = n - isr_schr_last;
  isr_schr_last = n; isr_schr_cnt++;
}

// ── MATRIX HELPERS ──────────────────────────────────────────
inline int pxIdx(int col, int row) {
  return (row % 2 == 0)
    ? row * MATRIX_COLS + col
    : row * MATRIX_COLS + (MATRIX_COLS - 1 - col);
}

void lightbar(int col, float v, uint32_t cf, uint32_t cd = 0x070707) {
  int n = (int)round(constrain(v, 0.0f, 1.0f) * MATRIX_ROWS);
  for (int r = 0; r < MATRIX_ROWS; r++) {
    int pos = MATRIX_ROWS - 1 - r;
    strip.setPixelColor(pxIdx(col, r), (pos < n) ? cf : cd);
  }
}

uint32_t epexKleur(float ct) {
  if (ct < 0)   return strip.Color(0,  80,  80);
  if (ct < 15)  return strip.Color(0, 110,   0);
  if (ct < 25)  return strip.Color(100, 95,   0);
  if (ct < 35)  return strip.Color(120,  45,  0);
  return               strip.Color(140,   0,  0);
}

// Sim-indicator: knippert col 0 rij 0 (S0 sim) en/of col 1 rij 0 (P1 sim)
void simIndicatorPulse() {
  static bool tog = false; tog = !tog;
  uint32_t kleur = tog ? strip.Color(80, 0, 0) : 0x070707;
  if (SIM_S0) strip.setPixelColor(pxIdx(0, 0), kleur);
  if (SIM_P1) strip.setPixelColor(pxIdx(1, 0), kleur);
}

// ── MATRIX UPDATE ───────────────────────────────────────────
void updateMatrix() {
  lightbar(0, w_sol   / MAX_SOL_W, strip.Color(0, 130, 0));
  lightbar(1, wh_sol  / MAX_DAG_WH, strip.Color(60, 130, 0));
  lightbar(2, w_schf  / MAX_SCH_W, strip.Color(150, 0, 0));
  lightbar(3, w_schr  / MAX_SOL_W, strip.Color(0, 80, 140));
  if (w_netto >= 0) lightbar(4,  w_netto / MAX_SOL_W, strip.Color(0, 130, 0));
  else              lightbar(4, -w_netto / MAX_SCH_W,  strip.Color(150, 0, 0));
  lightbar(5, constrain(epex_nu  / 40.0f, 0, 1), epexKleur(epex_nu));
  lightbar(6, constrain(epex_p1h / 40.0f, 0, 1), epexKleur(epex_p1h));
  {
    float f = constrain(piek_w / (float)max_piek_w, 0, 1);
    uint32_t c = (f < 0.6f) ? strip.Color(0, 100, 0)
               : (f < 0.85f)? strip.Color(120, 90, 0)
                             : strip.Color(140, 0, 0);
    lightbar(7, f, c);
  }
  {
    bool goed = (epex_nu < 15.0f) || (w_sol > 1500.0f);
    uint32_t ca = goed ? strip.Color(0, 120, 0) : strip.Color(120, 35, 0);
    lightbar(8, goed ? 1.0f : 0.3f, ca);
    lightbar(9, goed ? 1.0f : 0.3f, ca);
  }
  {
    float h = constrain((float)ESP.getMaxAllocHeap() / 55000.0f, 0, 1);
    uint32_t c = (h > 0.55f) ? strip.Color(0, 80, 0)
               : (h > 0.30f) ? strip.Color(110, 80, 0)
                              : strip.Color(140, 0, 0);
    lightbar(10, h, c);
  }
  {
    int rssi = ap_mode ? -99 : WiFi.RSSI();
    float f  = constrain((rssi + 90.0f) / 30.0f, 0, 1);
    uint32_t c = (rssi >= -60) ? strip.Color(0, 80, 0)
               : (rssi >= -75) ? strip.Color(110, 80, 0)
                               : strip.Color(140, 0, 0);
    lightbar(11, f, c);
  }
  if (SIM_S0 || SIM_P1) simIndicatorPulse();
  strip.show();
}

// ── VERMOGEN (live S0) ───────────────────────────────────────
float calcW(unsigned long iv, unsigned long last_ms) {
  if (!last_ms || (millis() - last_ms) > POWER_TO_MS || !iv) return 0.0f;
  return WATT_FACTOR / (float)iv;
}

// ── S0 SIMULATIE TICK ────────────────────────────────────────
// Realistisch april-profiel België
// ⚠️ Schakel SIM_S0 UIT zodra echte S0-bekabeling aanwezig is!
void simTickS0() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) {
    w_sol = 2000.0f; w_schf = 700.0f;
  } else {
    float hour = ti.tm_hour + ti.tm_min / 60.0f + ti.tm_sec / 3600.0f;
    float solar = 0.0f;
    if (hour >= 7.0f && hour <= 19.0f) {
      float fase = (hour - 7.0f) / 12.0f;
      solar = SIM_SOLAR_PEAK_W * sinf(fase * M_PI);
      solar *= 0.90f + (float)(random(0, 200)) / 1000.0f;
      solar = max(0.0f, solar);
    }
    float schf = SIM_BASE_LOAD_W + (float)(random(0, 200));
    if (hour >= 6.5f  && hour <  9.0f)  schf += 1800.0f;
    if (hour >= 11.5f && hour < 13.5f)  schf +=  600.0f;
    if (hour >= 17.0f && hour < 21.0f)  schf += 2200.0f;
    if (hour <  6.0f  || hour >= 23.0f) schf +=  800.0f;
    w_sol  = solar;
    w_schf = schf;
  }
  w_schr  = max(0.0f, w_sol - w_schf);
  w_netto = w_sol - w_schf + w_schr;

  const float DT_H = SIM_TICK_S / 3600.0f;
  wh_sol  += w_sol  * DT_H;
  wh_schf += w_schf * DT_H;
  wh_schr += w_schr * DT_H;

  float afname = w_schf - w_schr;
  if (afname > piek_w) piek_w = afname;
}

// ── P1 SIMULATIE TICK ────────────────────────────────────────
// Realistisch WON-verbruiksprofiel (april, België)
// ⚠️ Schakel SIM_P1 UIT zodra HomeWizard P1-dongle beschikbaar is (~2028)!
void simTickP1() {
  struct tm ti;
  float won = SIM_WON_BASE_W + (float)(random(0, 150));
  if (getLocalTime(&ti, 100)) {
    float hour = ti.tm_hour + ti.tm_min / 60.0f;
    if (hour >= 7.0f  && hour <  9.0f)  won += 1200.0f;
    if (hour >= 12.0f && hour < 13.5f)  won +=  500.0f;
    if (hour >= 17.5f && hour < 21.5f)  won += 1800.0f;
    if (hour <  6.5f  || hour >= 23.0f) won +=  600.0f;
  }
  w_won = won;  // Altijd afname in simulatie (geen eigen productie WON in fase 1)

  const float DT_H = SIM_TICK_S / 3600.0f;
  wh_won_imp += w_won * DT_H;
  // Geen export in WON simulatie (geen solar)
}

// ── S0 LIVE TICK ────────────────────────────────────────────
void liveTickS0() {
  noInterrupts();
  uint32_t      cs  = isr_sol_cnt,  cf  = isr_schf_cnt, cr  = isr_schr_cnt;
  unsigned long ivs = isr_sol_iv,   ivf = isr_schf_iv,  ivr = isr_schr_iv;
  unsigned long ls  = isr_sol_last, lf  = isr_schf_last,lr  = isr_schr_last;
  interrupts();

  wh_sol  += (cs - prev_sol)  * WH_PER_PULS;
  wh_schf += (cf - prev_schf) * WH_PER_PULS;
  wh_schr += (cr - prev_schr) * WH_PER_PULS;
  prev_sol = cs; prev_schf = cf; prev_schr = cr;

  w_sol  = calcW(ivs, ls);
  w_schf = calcW(ivf, lf);
  w_schr = calcW(ivr, lr);
  w_netto = w_sol - w_schf + w_schr;

  float afname = w_schf - w_schr;
  if (afname > piek_w) piek_w = afname;
}

// ── P1 LIVE FETCH ────────────────────────────────────────────
// Pollt HomeWizard P1 Meter HWE-P1-RJ12 via lokale REST API
// API v1: GET http://<P1_IP>/api/v1/data (plain HTTP, geen auth nodig)
// Documentatie: https://api-documentation.homewizard.com/docs/introduction/
// Activatie: HomeWizard Energy app → Settings → Meters → Local API AAN
//
// JSON response (relevante velden):
//   active_power_w          → momentaan nettovermogen (+ = afname, − = injectie)
//   total_power_import_t1_kwh + t2_kwh → totale import (dag via delta berekening)
//   total_power_export_t1_kwh + t2_kwh → totale export (dag via delta berekening)
//
// NB: total_power_*_kwh zijn CUMULATIEVE tellers (niet dag-reset).
//     Dagcumulatief = huidige waarde − snapshot bij midnight (zie checkMidnight)
void fetchP1() {
  if (SIM_P1) return;               // simulatie actief — niet ophalen
  if (strlen(p1_ip) == 0) return;   // geen IP ingesteld
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(4000);
  String url = "http://";
  url += p1_ip;
  url += "/api/v1/data";
  http.begin(url);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[P1] HTTP fout: %d\n", code);
    http.end(); return;
  }

  // Minimale JsonDocument — alleen de keys die wij nodig hebben
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) { Serial.println(F("[P1] JSON fout")); return; }

  // active_power_w: positief = afname, negatief = injectie (DSMR convention)
  float pwr = doc["active_power_w"] | 0.0f;
  w_won = pwr;  // positief = afname WON, negatief = injectie WON

  // Dag-cumulatieven via cumulatieve tellers (delta tov midnight snapshot — zie checkMidnight)
  float imp = (doc["total_power_import_t1_kwh"] | 0.0f)
            + (doc["total_power_import_t2_kwh"] | 0.0f);
  float exp = (doc["total_power_export_t1_kwh"] | 0.0f)
            + (doc["total_power_export_t2_kwh"] | 0.0f);

  // wh_won_imp/exp zijn dag-cumulatieven bijgehouden als delta tov midnight
  // (worden gereset in checkMidnight via NVS snapshot)
  static float imp_midnight = -1, exp_midnight = -1;
  if (imp_midnight < 0) { imp_midnight = imp; exp_midnight = exp; }  // eerste meting na boot
  wh_won_imp = (imp - imp_midnight) * 1000.0f;  // kWh → Wh
  wh_won_exp = (exp - exp_midnight) * 1000.0f;

  Serial.printf("[P1] %.0fW  imp:%.3f exp:%.3f kWh\n", pwr,
    wh_won_imp / 1000.0f, wh_won_exp / 1000.0f);
}

// ── EPEX OPHALEN VIA RPI ─────────────────────────────────────
void fetchEpex() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(6000);
  http.begin(String(RPI_BASE) + "/api/epex");
  int code = http.GET();
  if (code != 200) { Serial.printf("[EPEX] HTTP fout: %d\n", code); http.end(); return; }
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) { Serial.println(F("[EPEX] JSON fout")); return; }

  JsonArray unix_arr  = doc["unix_seconds"].as<JsonArray>();
  JsonArray price_arr = doc["price"].as<JsonArray>();
  if (!unix_arr || !price_arr || unix_arr.size() == 0) return;

  time_t now_ts = time(nullptr);
  int idx_nu = 0;
  for (int i = (int)unix_arr.size() - 1; i >= 0; i--) {
    if ((time_t)unix_arr[i].as<long>() <= now_ts) { idx_nu = i; break; }
  }
  int idx_p1 = min(idx_nu + 1, (int)price_arr.size() - 1);
  epex_nu  = price_arr[idx_nu].as<float>() * 0.1f + VAST_CT_KWH;
  epex_p1h = price_arr[idx_p1].as<float>() * 0.1f + VAST_CT_KWH;
  Serial.printf("[EPEX] nu=%.1f ct  +1u=%.1f ct\n", epex_nu, epex_p1h);
}

// ── NVS OPSLAAN ─────────────────────────────────────────────
void saveEnergy() {
  prefs.putFloat(NVS_SOL,  wh_sol);
  prefs.putFloat(NVS_SCHF, wh_schf);
  prefs.putFloat(NVS_SCHR, wh_schr);
  prefs.putFloat(NVS_PIEK, piek_w);
  Serial.printf("[NVS] Sol:%.0f SchF:%.0f SchR:%.0f Piek:%.0fW\n",
    wh_sol, wh_schf, wh_schr, piek_w);
}

// ── MIDNIGHT RESET ───────────────────────────────────────────
void checkMidnight() {
  struct tm ti;
  if (!getLocalTime(&ti, 500)) return;
  if (ti.tm_mday == last_mday || ti.tm_hour != 0 || ti.tm_min > 2) return;
  last_mday = ti.tm_mday;
  wh_sol = wh_schf = wh_schr = 0;
  wh_won_imp = wh_won_exp = 0;
  if (ti.tm_mday == 1) piek_w = 0;
  saveEnergy();
  Serial.printf("[RESET] Dag %d/%d\n", ti.tm_mday, ti.tm_mon + 1);
}

// ── SERIAL COMMANDO'S ────────────────────────────────────────
void handleSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n'); cmd.trim();

  if (cmd.equalsIgnoreCase("reset_nvs")) {
    prefs.clear(); delay(200); ESP.restart();
  } else if (cmd.equalsIgnoreCase("status")) {
    Serial.printf("Sol:%.0fW SchF:%.0fW SchR:%.0fW Netto:%.0fW WON:%.0fW EPEX:%.1fct\n",
      w_sol, w_schf, w_schr, w_netto, w_won, epex_nu);
    Serial.printf("Dag: Sol:%.0f SchF:%.0f SchR:%.0f WON-imp:%.0f WON-exp:%.0f Piek:%.0fW\n",
      wh_sol, wh_schf, wh_schr, wh_won_imp, wh_won_exp, piek_w);
    Serial.printf("SIM_S0: %s  SIM_P1: %s  P1_IP: %s\n",
      SIM_S0 ? "AAN" : "UIT", SIM_P1 ? "AAN" : "UIT",
      strlen(p1_ip) > 0 ? p1_ip : "(niet ingesteld)");

  // S0 simulatie — BEWUST handmatig omschakelen
  } else if (cmd.equalsIgnoreCase("sim s0 on")) {
    SIM_S0 = true;  prefs.putBool(NVS_SIM_S0, true);
    Serial.println(F("[SIM_S0] AAN — S0 pulsen worden gesimuleerd"));
  } else if (cmd.equalsIgnoreCase("sim s0 off")) {
    SIM_S0 = false; prefs.putBool(NVS_SIM_S0, false);
    Serial.println(F("[SIM_S0] UIT — live S0 ISR actief ⚠️  Controleer bekabeling!"));

  // P1 simulatie — BEWUST handmatig omschakelen
  } else if (cmd.equalsIgnoreCase("sim p1 on")) {
    SIM_P1 = true;  prefs.putBool(NVS_SIM_P1, true);
    Serial.println(F("[SIM_P1] AAN — P1 dongle wordt gesimuleerd"));
  } else if (cmd.equalsIgnoreCase("sim p1 off")) {
    SIM_P1 = false; prefs.putBool(NVS_SIM_P1, false);
    Serial.printf("[SIM_P1] UIT — live HomeWizard P1 actief (IP: %s)\n",
      strlen(p1_ip) > 0 ? p1_ip : "⚠️  NIET INGESTELD!");

  } else if (cmd.equalsIgnoreCase("help")) {
    Serial.println(F("Commando's: status | reset_nvs | "
      "sim s0 on/off | sim p1 on/off"));
  }
}

// ── /json ENDPOINT ──────────────────────────────────────────
// Conform §6.4 Master Overnamedocument + v1.26 uitbreidingen:
//   sim_s0: 1 = S0 gesimuleerd, 0 = live hardware
//   sim_p1: 1 = P1 gesimuleerd, 0 = live HomeWizard dongle
//   b:      WON momentaan vermogen W (+ = afname)
//   i:      WON dag afname Wh
//   vw:     WON dag injectie Wh (nieuw v1.26)
void serveJson(AsyncWebServerRequest *req) {
  char buf[560];
  snprintf(buf, sizeof(buf),
    "{"
    "\"a\":%d,"     // Solar W
    "\"b\":%d,"     // WON W (P1)
    "\"c\":%d,"     // SCH afname W
    "\"d\":%d,"     // SCH injectie W
    "\"e\":%d,"     // Netto W SCH (+ = injectie)
    "\"h\":%d,"     // Solar dag Wh
    "\"i\":%d,"     // WON dag afname Wh
    "\"j\":%d,"     // SCH afname dag Wh
    "\"k\":%d,"     // SCH injectie dag Wh
    "\"vw\":%d,"    // WON dag injectie Wh (nieuw v1.26)
    "\"n\":%d,"     // EPEX nu ct/kWh × 100
    "\"n2\":%d,"    // EPEX +1u ct/kWh × 100
    "\"pt\":%d,"    // Piek maand W
    "\"ac\":%d,"    // RSSI dBm
    "\"ae\":%d,"    // Heap largest block bytes
    "\"sim_s0\":%d,"// 1 = S0 gesimuleerd
    "\"sim_p1\":%d,"// 1 = P1 gesimuleerd
    "\"ver\":\"%s\""
    "}",
    (int)w_sol,
    (int)w_won,
    (int)w_schf, (int)w_schr, (int)w_netto,
    (int)wh_sol,
    (int)wh_won_imp,
    (int)wh_schf, (int)wh_schr,
    (int)wh_won_exp,
    (int)(epex_nu  * 100), (int)(epex_p1h * 100),
    (int)piek_w,
    WiFi.RSSI(),
    (int)ESP.getMaxAllocHeap(),
    SIM_S0 ? 1 : 0,
    SIM_P1 ? 1 : 0,
    FW_VERSION
  );
  req->send(200, "application/json", buf);
}

// ── STATUS PAGINA ────────────────────────────────────────────
void serveStatus(AsyncWebServerRequest *req) {
  AsyncResponseStream *p = req->beginResponseStream("text/html;charset=utf-8");
  p->print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>S-ENERGY</title><style>"
    "body{font-family:Arial,sans-serif;margin:0;background:#f4f4f4;}"
    ".hdr{background:#ffcc00;padding:10px 15px;font-weight:bold;font-size:17px;"
    "display:flex;justify-content:space-between;align-items:center;}"
    ".nav{display:flex;flex-wrap:wrap;gap:5px;padding:7px 12px;background:#fff;"
    "border-bottom:2px solid #ddd;}"
    ".nav a{background:#369;color:#fff;padding:5px 11px;border-radius:4px;"
    "text-decoration:none;font-size:13px;}"
    ".nav a:hover{background:#036;}.nav a.act{background:#c00;}"
    ".banner{padding:9px 14px;margin:8px 14px;border-radius:6px;"
    "font-weight:bold;font-size:13px;text-align:center;}"
    ".banner-red{background:#c00;color:#fff;}"
    ".banner-ok{background:#2a8a3e;color:#fff;}"
    "table{margin:8px 14px;border-collapse:collapse;width:calc(100% - 28px);max-width:500px;}"
    "td{padding:6px 8px;border-bottom:1px solid #ddd;font-size:14px;}"
    "td:first-child{font-weight:bold;color:#369;width:44%;}"
    ".lbl-sim{background:#c00;color:#fff;font-size:10px;padding:1px 5px;"
    "border-radius:3px;margin-left:5px;}"
    ".lbl-live{background:#2a8a3e;color:#fff;font-size:10px;padding:1px 5px;"
    "border-radius:3px;margin-left:5px;}"
    "</style></head><body>"
    "<div class='hdr'><span>" CTRL_ID " v" FW_VERSION "</span>"
    "<span id='ts' style='font-size:12px;font-weight:normal'></span></div>"
    "<div class='nav'>"
    "<a href='/' class='act'>Status</a><a href='/json'>JSON</a>"
    "<a href='/update'>OTA</a><a href='/settings'>Settings</a>"
    "</div>"));

  // Statusbanners per kanaal
  if (SIM_S0) p->print(F("<div class='banner banner-red'>"
    "⚠️ S0 SIMULATIE ACTIEF — S0 kanalen genereren neppe data!</div>"));
  else        p->print(F("<div class='banner banner-ok'>"
    "✅ S0 LIVE — hardware S0-pulsen (Inepro PRO380-S)</div>"));
  if (SIM_P1) p->print(F("<div class='banner banner-red'>"
    "⚠️ P1 SIMULATIE ACTIEF — WON data is nep!</div>"));
  else        p->print(F("<div class='banner banner-ok'>"
    "✅ P1 LIVE — HomeWizard P1 Meter (HWE-P1-RJ12)</div>"));

  p->print(F("<table>"
    "<tr><td>☀️ Solar</td><td id='sol'>—</td></tr>"
    "<tr><td>⚡ SCH afname</td><td id='schf'>—</td></tr>"
    "<tr><td>🔄 SCH injectie</td><td id='schr'>—</td></tr>"
    "<tr><td>⚖️ Netto SCH</td><td id='net'>—</td></tr>"
    "<tr><td>🏠 WON vermogen</td><td id='won'>—</td></tr>"
    "<tr><td>☀️ Solar dag</td><td id='sold'>—</td></tr>"
    "<tr><td>⚡ SCH afname dag</td><td id='schfd'>—</td></tr>"
    "<tr><td>🔄 SCH injectie dag</td><td id='schrd'>—</td></tr>"
    "<tr><td>🏠 WON afname dag</td><td id='wond'>—</td></tr>"
    "<tr><td>🔄 WON injectie dag</td><td id='wonx'>—</td></tr>"
    "<tr><td>📊 Maandpiek</td><td id='piek'>—</td></tr>"
    "<tr><td>💰 EPEX nu</td><td id='epn'>—</td></tr>"
    "<tr><td>💰 EPEX +1u</td><td id='ep1'>—</td></tr>"
    "<tr><td>📶 WiFi RSSI</td><td id='rssi'>—</td></tr>"
    "<tr><td>💾 Heap</td><td id='heap'>—</td></tr>"
    "</table>"
    "<script>function upd(){fetch('/json').then(r=>r.json()).then(d=>{"
    "var f=v=>typeof v==='number'?v.toFixed(2):v;"
    "document.getElementById('sol').textContent=d.a+' W';"
    "document.getElementById('schf').textContent=d.c+' W';"
    "document.getElementById('schr').textContent=d.d+' W';"
    "document.getElementById('net').textContent=(d.e>=0?'+':'')+d.e+' W';"
    "document.getElementById('won').textContent=(d.b>=0?'+':'')+d.b+' W';"
    "document.getElementById('sold').textContent=f(d.h/1000)+' kWh';"
    "document.getElementById('schfd').textContent=f(d.j/1000)+' kWh';"
    "document.getElementById('schrd').textContent=f(d.k/1000)+' kWh';"
    "document.getElementById('wond').textContent=f(d.i/1000)+' kWh';"
    "document.getElementById('wonx').textContent=f((d.vw||0)/1000)+' kWh';"
    "document.getElementById('piek').textContent=d.pt+' W';"
    "document.getElementById('epn').textContent=f(d.n/100)+' ct/kWh';"
    "document.getElementById('ep1').textContent=f(d.n2/100)+' ct/kWh';"
    "document.getElementById('rssi').textContent=d.ac+' dBm';"
    "document.getElementById('heap').textContent=Math.round(d.ae/1024)+' KB';"
    "var n=new Date();"
    "document.getElementById('ts').textContent="
    "n.toLocaleDateString('nl-BE')+' '+n.toLocaleTimeString('nl-BE');"
    "}).catch(()=>{});}"
    "upd();setInterval(upd,5000);</script></body></html>"));
  req->send(p);
}

// ── SETTINGS PAGINA ──────────────────────────────────────────
void serveSettings(AsyncWebServerRequest *req) {
  AsyncResponseStream *p = req->beginResponseStream("text/html;charset=utf-8");
  p->print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>S-ENERGY Settings</title><style>"
    "body{font-family:Arial,sans-serif;margin:0;background:#f4f4f4;}"
    ".hdr{background:#ffcc00;padding:10px 15px;font-weight:bold;font-size:17px;}"
    ".nav{display:flex;flex-wrap:wrap;gap:5px;padding:7px 12px;background:#fff;"
    "border-bottom:2px solid #ddd;}"
    ".nav a{background:#369;color:#fff;padding:5px 11px;border-radius:4px;"
    "text-decoration:none;font-size:13px;}"
    ".nav a:hover{background:#036;}.nav a.act{background:#c00;}"
    ".form{margin:14px;max-width:520px;}"
    "table{width:100%;border-collapse:collapse;}"
    "td{padding:7px 6px;border-bottom:1px solid #ddd;font-size:14px;}"
    "td:first-child{font-weight:bold;color:#369;width:42%;}"
    "input[type=text],input[type=number],input[type=password]"
    "{width:100%;padding:6px;border:1px solid #ccc;border-radius:4px;"
    "box-sizing:border-box;font-size:14px;}"
    ".btn{background:#369;color:#fff;padding:9px 20px;border:none;border-radius:5px;"
    "font-size:14px;cursor:pointer;margin:8px 6px 0 0;}"
    ".btn:hover{background:#036;}.btn-red{background:#c00;}.btn-red:hover{background:#900;}"
    ".sim-block{border:2px solid #c00;border-radius:8px;padding:12px 14px;margin:14px 0;"
    "background:#fff8f8;}"
    ".sim-ok{border-color:#2a8a3e;background:#f0fff4;}"
    ".sim-title{font-weight:bold;font-size:14px;margin-bottom:8px;}"
    ".sim-warn{color:#c00;font-size:12px;margin-top:6px;}"
    ".sim-ok-txt{color:#2a8a3e;font-size:12px;margin-top:4px;}"
    "label{cursor:pointer;font-size:14px;}"
    "</style></head><body>"
    "<div class='hdr'>S-ENERGY v" FW_VERSION " Settings</div>"
    "<div class='nav'>"
    "<a href='/'>Status</a><a href='/json'>JSON</a>"
    "<a href='/update'>OTA</a><a href='/settings' class='act'>Settings</a>"
    "</div><div class='form'>"
    "<form action='/save_settings' method='get' id='sf'><table>"));

  p->printf("<tr><td>MAC adres</td><td><code>%s</code></td></tr>",
    WiFi.macAddress().c_str());
  p->printf("<tr><td>WiFi SSID</td><td>"
    "<input name='ssid' value='%s' required></td></tr>", wifi_ssid);
  p->print(F("<tr><td>WiFi wachtwoord</td><td>"
    "<input name='pass' type='password' placeholder='leeg = ongewijzigd'></td></tr>"));
  p->printf("<tr><td>Statisch IP</td><td>"
    "<input name='ip' value='%s' placeholder='leeg = DHCP'></td></tr>", static_ip);
  p->printf("<tr><td>LED helderheid (0–255)</td><td>"
    "<input name='bri' type='number' min='5' max='255' value='%d'></td></tr>",
    (int)prefs.getUChar(NVS_BRI, DEF_BRIGHT));
  p->printf("<tr><td>Max piek (W)</td><td>"
    "<input name='mpiek' type='number' min='1000' max='20000' value='%d'></td></tr>",
    (int)max_piek_w);
  p->print(F("</table>"));

  // ── S0 Simulatie sectie ──────────────────────────────────
  p->printf("<div class='sim-block%s'>"
    "<div class='sim-title'>📡 S0 kanalen — Inepro PRO380-S</div>"
    "<label><input type='checkbox' name='sim_s0' value='1' style='width:auto;margin-right:8px'%s>"
    " Simuleer S0-pulsen (solar, SCH afname, SCH injectie)</label>"
    "<div class='%s'>%s</div>"
    "</div>",
    SIM_S0 ? "" : " sim-ok",
    SIM_S0 ? " checked" : "",
    SIM_S0 ? "sim-warn" : "sim-ok-txt",
    SIM_S0
      ? "⚠️ SIMULATIE AAN — zet UIT na S0-bekabeling (IO5/IO6/IO7 via Roomsense RJ45)"
      : "✅ LIVE — hardware S0-pulsen actief (IO5=solar, IO6=SCH afname, IO7=SCH injectie)");

  // ── P1 Simulatie sectie ──────────────────────────────────
  p->printf("<div class='sim-block%s'>"
    "<div class='sim-title'>🔌 P1-dongle — HomeWizard HWE-P1-RJ12 (WON, ~2028)</div>"
    "<label><input type='checkbox' name='sim_p1' value='1' style='width:auto;margin-right:8px'%s>"
    " Simuleer P1-dongle data (WON afname + injectie)</label><br><br>"
    "<b style='font-size:13px'>IP-adres HomeWizard P1 Meter:</b><br>"
    "<input name='p1_ip' value='%s' placeholder='bv. 192.168.0.80 (leeg = niet actief)'"
    " style='margin-top:4px'><br>"
    "<div style='font-size:11px;color:#666;margin-top:4px'>"
    "Activeer Local API via HomeWizard Energy app: Settings → Meters → Local API<br>"
    "API endpoint: GET http://&lt;IP&gt;/api/v1/data (geen auth, plain HTTP)<br>"
    "Docs: <a href='https://api-documentation.homewizard.com/docs/introduction/' target='_blank'>"
    "api-documentation.homewizard.com</a></div>"
    "<div class='%s'>%s</div>"
    "</div>",
    SIM_P1 ? "" : " sim-ok",
    SIM_P1 ? " checked" : "",
    p1_ip,
    SIM_P1 ? "sim-warn" : "sim-ok-txt",
    SIM_P1
      ? "⚠️ SIMULATIE AAN — zet UIT na plaatsing HomeWizard P1 Meter (~2028)"
      : (strlen(p1_ip) > 0
          ? "✅ LIVE — pollt HomeWizard P1 Meter elke 5s"
          : "⚠️ LIVE modus maar geen IP ingesteld — stel IP in!"));

  p->print(F("<button class='btn' type='submit'>Opslaan &amp; Reboot</button>"
    "<button class='btn btn-red' type='button' "
    "onclick=\"if(confirm('Factory reset?')) location.href='/factory_reset';\">"
    "Factory Reset</button></form>"
    "<hr style='margin:14px 0;border:none;border-top:1px solid #ddd;'>"
    "<form action='/reset_dag' method='get'>"
    "<button class='btn' onclick=\"return confirm('Dagcumulatieven resetten?');\""
    " type='submit'>Reset dag Wh</button></form>"
    "<form action='/reset_piek' method='get' style='margin-top:6px;'>"
    "<button class='btn' onclick=\"return confirm('Maandpiek resetten?');\""
    " type='submit'>Reset maandpiek</button></form>"
    "<script>document.getElementById('sf').onsubmit=function(e){"
    "const ip=this.ip.value.trim();"
    "if(ip&&!/^(\\d{1,3}\\.){3}\\d{1,3}$/.test(ip)){alert('Ongeldig statisch IP!');e.preventDefault();return false;}"
    "const p1=this.p1_ip.value.trim();"
    "if(p1&&!/^(\\d{1,3}\\.){3}\\d{1,3}$/.test(p1)){alert('Ongeldig P1 IP-adres!');e.preventDefault();return false;}"
    "if(!this.ssid.value.trim()){alert('SSID verplicht!');e.preventDefault();return false;}"
    "return true;};</script>"
    "</div></body></html>"));
  req->send(p);
}

// ── OTA PAGINA ───────────────────────────────────────────────
void serveOTA(AsyncWebServerRequest *req) {
  AsyncResponseStream *p = req->beginResponseStream("text/html;charset=utf-8");
  p->print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA</title><style>"
    "body{font-family:Arial,sans-serif;margin:0;background:#f4f4f4;}"
    ".hdr{background:#ffcc00;padding:10px 15px;font-weight:bold;font-size:17px;}"
    ".nav{display:flex;flex-wrap:wrap;gap:5px;padding:7px 12px;background:#fff;"
    "border-bottom:2px solid #ddd;}"
    ".nav a{background:#369;color:#fff;padding:5px 11px;border-radius:4px;"
    "text-decoration:none;font-size:13px;}"
    ".nav a:hover{background:#036;}.nav a.act{background:#c00;}"
    ".main{padding:18px;max-width:400px;}"
    ".btn{background:#369;color:#fff;padding:9px 20px;border:none;border-radius:5px;"
    "font-size:14px;cursor:pointer;margin-top:10px;}"
    ".btn:hover{background:#036;}.btn-red{background:#c00;}"
    "</style></head><body>"
    "<div class='hdr'>OTA Firmware Update</div>"
    "<div class='nav'><a href='/'>Status</a><a href='/json'>JSON</a>"
    "<a href='/update' class='act'>OTA</a><a href='/settings'>Settings</a>"
    "</div><div class='main'>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<p>Selecteer .bin bestand:</p>"
    "<input type='file' name='update' accept='.bin'><br><br>"
    "<button class='btn' type='submit'>Upload firmware</button></form><br>"
    "<button class='btn btn-red' onclick=\"location.href='/reboot'\">Reboot</button>"
    "</div></body></html>"));
  req->send(p);
}

// ── BOOT ANIMATIE ───────────────────────────────────────────
void bootAnim() {
  for (int c = 0; c < MATRIX_COLS; c++) {
    for (int r = 0; r < MATRIX_ROWS; r++)
      strip.setPixelColor(pxIdx(c, r), strip.Color(0, 40, 0));
    strip.show(); delay(40);
  }
  // Rode knippering als één of beide kanalen in simulatie staan
  if (SIM_S0 || SIM_P1) {
    for (int i = 0; i < 4; i++) {
      strip.fill(strip.Color(30, 0, 0)); strip.show(); delay(150);
      strip.clear(); strip.show(); delay(100);
    }
  } else {
    delay(200);
    strip.fill(strip.Color(25, 25, 25)); strip.show(); delay(100);
  }
  strip.clear(); strip.show();
}

// ── WIFI STARTEN ────────────────────────────────────────────
void startWiFi() {
  if (strlen(wifi_ssid) == 0) goto start_ap;
  {
    IPAddress sip, gw, sn;
    if (strlen(static_ip) > 0 && sip.fromString(static_ip)) {
      gw.fromString("192.168.0.1"); sn.fromString("255.255.255.0");
      WiFi.config(sip, gw, sn);
    }
    WiFi.begin(wifi_ssid, wifi_pass);
    Serial.print(F("[WiFi] Verbinden"));
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 12000) {
      delay(250); Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nVerbonden: " + WiFi.localIP().toString());
      ap_mode = false; configTime(TZ_SEC, 3600, NTP_SRV); return;
    }
  }
start_ap:
  Serial.println(F("\n[WiFi] Geen verbinding — AP modus"));
  WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID);
  dnsServer.start(53, "*", WiFi.softAPIP());
  ap_mode = true;
  Serial.printf("[AP] %s  %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); delay(300);
  Serial.println(F("\n=== S-ENERGY v" FW_VERSION " boot ==="));
  Serial.println(F("Commando's: status | sim s0 on/off | sim p1 on/off | reset_nvs | help"));

  prefs.begin(NVS_NS, false);
  wh_sol     = prefs.getFloat(NVS_SOL,  0.0f);
  wh_schf    = prefs.getFloat(NVS_SCHF, 0.0f);
  wh_schr    = prefs.getFloat(NVS_SCHR, 0.0f);
  piek_w     = prefs.getFloat(NVS_PIEK, 0.0f);
  max_piek_w = prefs.getUInt(NVS_MPIEK, 10000);
  SIM_S0     = prefs.getBool(NVS_SIM_S0, true);   // default: simulatie AAN
  SIM_P1     = prefs.getBool(NVS_SIM_P1, true);   // default: simulatie AAN
  strlcpy(wifi_ssid,  prefs.getString(NVS_SSID, DEF_SSID).c_str(), sizeof(wifi_ssid));
  strlcpy(wifi_pass,  prefs.getString(NVS_PASS, DEF_PASS).c_str(), sizeof(wifi_pass));
  strlcpy(static_ip,  prefs.getString(NVS_IP,   DEF_IP).c_str(),   sizeof(static_ip));
  strlcpy(p1_ip,      prefs.getString(NVS_P1_IP, "").c_str(),       sizeof(p1_ip));
  uint8_t bri = prefs.getUChar(NVS_BRI, DEF_BRIGHT);

  Serial.printf("[NVS] Sol:%.0f SchF:%.0f SchR:%.0f Piek:%.0fW\n",
    wh_sol, wh_schf, wh_schr, piek_w);
  Serial.printf("[SIM] S0=%s  P1=%s  P1-IP=%s\n",
    SIM_S0 ? "SIM" : "LIVE", SIM_P1 ? "SIM" : "LIVE",
    strlen(p1_ip) > 0 ? p1_ip : "(geen)");

  strip.begin(); strip.setBrightness(bri); strip.clear(); strip.show();
  bootAnim();

  // S0 interrupts — altijd registreren (in sim-modus pulseren de pinnen niet)
  pinMode(S0_SOL_PIN,  INPUT);
  pinMode(S0_SCHF_PIN, INPUT);
  pinMode(S0_SCHR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(S0_SOL_PIN),  isrSol,  FALLING);
  attachInterrupt(digitalPinToInterrupt(S0_SCHF_PIN), isrSchF, FALLING);
  attachInterrupt(digitalPinToInterrupt(S0_SCHR_PIN), isrSchR, FALLING);
  Serial.println(F("[S0] Interrupts geregistreerd IO5/IO6/IO7"));

  startWiFi();

  server.on("/",         HTTP_GET, serveStatus);
  server.on("/json",     HTTP_GET, serveJson);
  server.on("/update",   HTTP_GET, serveOTA);
  server.on("/settings", HTTP_GET, serveSettings);

  server.on("/save_settings", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasArg("ssid")) { strlcpy(wifi_ssid, req->arg("ssid").c_str(), sizeof(wifi_ssid)); prefs.putString(NVS_SSID, wifi_ssid); }
    if (req->hasArg("pass") && req->arg("pass").length() > 0) { strlcpy(wifi_pass, req->arg("pass").c_str(), sizeof(wifi_pass)); prefs.putString(NVS_PASS, wifi_pass); }
    if (req->hasArg("ip"))    { strlcpy(static_ip, req->arg("ip").c_str(), sizeof(static_ip)); prefs.putString(NVS_IP, static_ip); }
    if (req->hasArg("p1_ip")) { strlcpy(p1_ip, req->arg("p1_ip").c_str(), sizeof(p1_ip)); prefs.putString(NVS_P1_IP, p1_ip); }
    if (req->hasArg("bri"))   prefs.putUChar(NVS_BRI, (uint8_t)req->arg("bri").toInt());
    if (req->hasArg("mpiek")) { max_piek_w = req->arg("mpiek").toInt(); prefs.putUInt(NVS_MPIEK, max_piek_w); }

    // ⚠️ BEWUST handmatig — nooit automatisch omschakelen!
    SIM_S0 = req->hasArg("sim_s0"); prefs.putBool(NVS_SIM_S0, SIM_S0);
    SIM_P1 = req->hasArg("sim_p1"); prefs.putBool(NVS_SIM_P1, SIM_P1);
    Serial.printf("[SIM] Opgeslagen — S0=%s P1=%s P1-IP=%s\n",
      SIM_S0 ? "SIM" : "LIVE", SIM_P1 ? "SIM" : "LIVE", p1_ip);

    req->send(200, "text/html",
      "<h2 style='text-align:center;padding:30px;color:#369;'>"
      "Opgeslagen &mdash; Rebooting...</h2>"
      "<script>setTimeout(()=>location.href='/',2500);</script>");
    delay(500); ESP.restart();
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *req) {
      bool ok = !Update.hasError();
      req->send(200, "text/html", ok
        ? "<h2 style='color:green;text-align:center;padding:30px'>Update OK — Rebooting...</h2>"
        : "<h2 style='color:red;text-align:center;padding:30px'>MISLUKT</h2><a href='/update'>Opnieuw</a>");
      if (ok) { delay(1000); ESP.restart(); }
    },
    [](AsyncWebServerRequest *req, String fn, size_t idx, uint8_t *data, size_t len, bool fin) {
      if (!idx) { Update.begin(UPDATE_SIZE_UNKNOWN); }
      Update.write(data, len);
      if (fin && Update.end(true)) Serial.println(F("[OTA] OK"));
    });

  server.on("/factory_reset", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", "<h2 style='text-align:center;padding:30px;color:#c00'>Factory reset — Rebooting...</h2>");
    delay(300); prefs.clear(); delay(200); ESP.restart();
  });
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", "<h2 style='text-align:center;padding:30px'>Rebooting...</h2>");
    delay(500); ESP.restart();
  });
  server.on("/reset_dag", HTTP_GET, [](AsyncWebServerRequest *req) {
    wh_sol = wh_schf = wh_schr = wh_won_imp = wh_won_exp = 0; saveEnergy();
    req->send(200, "text/plain", "Dagcumulatieven gereset");
  });
  server.on("/reset_piek", HTTP_GET, [](AsyncWebServerRequest *req) {
    piek_w = 0; saveEnergy();
    req->send(200, "text/plain", "Maandpiek gereset");
  });

  if (ap_mode) {
    server.onNotFound([](AsyncWebServerRequest *r) { r->redirect("/settings"); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("/settings"); });
    server.on("/generate_204",        HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("/settings"); });
    server.on("/ncsi.txt",            HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("/settings"); });
  }

  server.begin();
  fetchEpex();
  t_epex = t_15m = t_p1 = millis();

  Serial.printf("[HEAP] %d bytes  %dKB largest\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap()/1024);
  Serial.printf("HTTP: http://%s\n",
    ap_mode ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());
  Serial.println(F("=== Setup klaar ===\n"));
}

// ── LOOP ────────────────────────────────────────────────────
void loop() {
  if (ap_mode) dnsServer.processNextRequest();
  handleSerialCommands();

  if (!ap_mode) {
    wl_status_t ws = WiFi.status();
    if (ws != last_wifi_st) {
      if (ws != WL_CONNECTED) WiFi.reconnect();
      last_wifi_st = ws;
    }
  }

  unsigned long now = millis();

  // ── 5s tick ──────────────────────────────────────────────
  if (now - t_5s >= 5000) {
    t_5s = now;

    // S0 kanalen: simulatie OF live — nooit automatisch
    if (SIM_S0) simTickS0();
    else        liveTickS0();

    // P1 dongle: simulatie OF live — nooit automatisch
    if (SIM_P1) simTickP1();
    else        fetchP1();   // HomeWizard API v1 — elke 5s pollen

    updateMatrix();

    Serial.printf("[%s|%s][%5lus] Sol:%4.0fW SCHf:%4.0fW SCHr:%4.0fW"
                  " WON:%+5.0fW EPEX:%.1fct Heap:%dKB\n",
      SIM_S0 ? "S0-SIM" : "S0-LIVE",
      SIM_P1 ? "P1-SIM" : "P1-LIVE",
      now / 1000, w_sol, w_schf, w_schr, w_won,
      epex_nu, ESP.getMaxAllocHeap() / 1024);
  }

  // ── 15 min: NVS + midnight check ─────────────────────────
  if (now - t_15m >= 900000UL) {
    t_15m = now;
    saveEnergy();
    checkMidnight();
  }

  // ── 15 min: EPEX herladen ────────────────────────────────
  if (now - t_epex >= 900000UL) {
    t_epex = now;
    fetchEpex();
  }
}

// ── TODO v1.27 ───────────────────────────────────────────────
// - ntfy.sh push bij piekdrempel overschreden
// - Automatische zomertijd via NTP DST
// - Vaste opslag ophalen via /api/settings (nu hardcoded VAST_CT_KWH)
// - WON individuele piek bijhouden (key pw) zodra P1-dongle actief
