# Energy Management System — Zarlardinge
## Technisch werkdocument v0.2 — April 2026

**ESP32-C6 · Arduino IDE · ESPAsyncWebServer · Matter · Google Sheets**
*Filip Dworp (FiDel) — Zarlardinge (BE)*

---

## Instructies bij gebruik van dit document

1. Upload dit document + eventuele sketch aan het begin van elke nieuwe sessie
2. Vraag Claude het document samen te vatten vóór er iets aangepast wordt
3. Eerst een plan — Claude mag pas beginnen coderen na expliciete goedkeuring
4. **Kritische regels voor alle Zarlar-controllers (ook Smart Energy):**
   - `#define Serial Serial0` bovenaan — verplicht voor ESP32-C6 RISC-V serieel
   - Nooit `huge_app` partitie — breekt OTA. Gebruik altijd `partitions_16mb.csv`
   - Versieheader aanpassen bij elke wijziging
   - Bij JSON-structuurwijziging: ook GAS-script en Zarlar Dashboard nalopen
   - IO-pins altijd onmiddellijk aansturen — nooit wachten op pollcyclus
   - Pairing code altijd in webUI tonen, niet alleen in Serial
   - Commentaar: nooit `*/` in de tekst zelf (compileerfout)
   - Nooit IO8, IO9 als input gebruiken (strapping pins)
   - ECO gebruikt RSSI key `p`, alle andere controllers gebruiken `ac`
   - Dashboard gebruikt `WebServer` (blocking), niet `AsyncWebServer`

---

## 1. Systeemcontext — Zarlar thuisautomatisering

Het Smart Energy systeem is onderdeel van het **Zarlar** thuisautomatiseringsplatform.
Alle controllers publiceren een `/json` endpoint. Het **Zarlar Dashboard (192.168.0.60)** pollt alle controllers en POST de data naar Google Sheets via Google Apps Script. Controllers doen nooit zelf HTTPS-calls naar Google — dit mislukte structureel door heap-druk.

```
[HVAC  192.168.0.70] --+
[ECO   192.168.0.71] --+
[SENRG 192.168.0.73] --+--> Zarlar Dashboard 192.168.0.60 --> Google Sheets
[ROOM  192.168.0.80] --+
         |
         +--> Apple Home (via Matter/WiFi)
```

### Bestaande controllers

| Controller | Naam | IP | Versie | Status |
| --- | --- | --- | --- | --- |
| HVAC | ESP32_HVAC | 192.168.0.70 | v1.19 | Productie stabiel |
| ECO Boiler | ESP32_ECO Boiler | 192.168.0.71 | v1.23 | Productie stabiel |
| **Smart Energy** | **ESP32_SMART_ENERGY** | **192.168.0.73** | **v0.1** | Te bouwen |
| ROOM Eetplaats | ESP32_EETPLAATS | 192.168.0.80 | v2.10 | Productie stabiel |
| Zarlar Dashboard | ESP32_ZARLAR | 192.168.0.60 | v5.0 | Productie stabiel |

**IP-keuze 192.168.0.73:** past in het blok systeem-controllers (70-74), na ECO (71) en de geplande buitensensor S-OUTSIDE (72). Room-controllers starten vanaf .75.

---

## 2. Installatie — hardware overzicht

### 2.1 Zonne-installatie

| Component | Details |
| --- | --- |
| Jaaropbrengst (gemiddeld) | ~11.950 kWh/jaar |
| Omvormer 1 & 2 — zuiddak | SMA Sunny Boy SB3600TL-21 |
| Serienummers | 2130419465 & 2130419851 |
| Instellingscode / norm | BES204 / C10/11:2012 |
| Communicatie | Speedwire/Webconnect ingebouwd — geen extra hardware nodig |
| RID-codes | RHCD4F (SN ...465) & QX6NUA (SN ...851) |
| Installateur / jaar | Alfisun, 2017 |
| Omvormer 3 — westdak | SMA Sunny Boy — type & SN nog te noteren (AP1) |
| Solar S0-teller | Digitale S0-pulsuitgang — in tellerkast keuken |

### 2.2 Meters & tellers

| Component | Details |
| --- | --- |
| Fluvius digitale meter | NEE — uitzondering tot eind 2028 |
| Huidig metertype | Terugdraaiende teller — injectie vergoed aan aankoopprijs |
| Overgang 2028 | Digitale meter verplicht. Injektietarief daalt naar ~0,05 EUR/kWh |
| S0-tellers aanwezig | Solar, Verbruik WON, Verbruik SCH (evt. meer) |
| Pulsen/kWh | Nog te lezen van label — typisch 1000 imp/kWh (AP2) |

### 2.3 Stuurbare verbruikers

| Verbruiker | Hardware | Stuurbaarheid | Prioriteit |
| --- | --- | --- | --- |
| ECO-boiler (SCH) | OEG dompelweerstand ~2 kW | HTTP REST naar 192.168.0.71 | 1 hoogste |
| EV-lader 1 (WON) | Tesla Wallcharger Gen 3 | Lokale REST API HTTP PUT | 2 |
| EV-lader 2 (WON) | Merk/type onbekend (AP6) | Nog te bepalen | 3 |
| Thuisbatterij | Huawei/BYD voorkeur ~2028 | Modbus TCP | 4 |
| **Regenwaterpomp** | Op WON-teller ~500 kWh/j | **NOOIT STUREN** | --- |

De regenwaterpomp is essentieel voor al het sanitair. Dit is een harde constraint die nooit mag wijzigen.

### 2.4 Warmtepompen (Panasonic + CZ-TAW1)

| | WP SCH | WP WON |
| --- | --- | --- |
| In werking | Januari 2019 | November 2019 |
| Comfort Cloud | OK — Filip als eigenaar | GEBLOKKEERD |
| Probleem WP WON | --- | Registratie-email iTroniX adres (bedrijf gestopt 2022) bestaat niet meer |
| Oplossing | --- | CZ-TAW1 resetten via paperclip, herregistreren op prive-mail Filip, Maarten toevoegen (AP10) |

---

## 3. Hardware Smart Energy controller

### 3.1 Module & board

Identiek aan alle andere Zarlar-controllers:

| Parameter | Waarde |
| --- | --- |
| Module | ESP32-C6-WROOM-1N16 (Espressif) |
| Flash | 16 MB |
| Board | 32-pin clone (AliExpress batch dec 2025, EUR 2.52/stuk) |
| WiFi | WiFi 6 (2.4 GHz), 3.3V IO niet 5V-tolerant |
| Locatie | Tellerkast keuken SCH |
| Vaste IP | 192.168.0.73 |
| Naam netwerk | ESP32_SMART_ENERGY |

Bij zwakke WiFi: gebruik ESP32-C6-WROOM-1U (zelfde module, U.FL/IPEX voor externe antenne).

### 3.2 Partitietabel (identiek voor alle Zarlar-controllers)

Bestand `partitions_16mb.csv` naast het `.ino` bestand plaatsen:

| Naam | Type | Offset | Grootte |
| --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 | 20 KB |
| otadata | data/ota | 0xe000 | 8 KB |
| app0 | app/ota_0 | 0x10000 | 6 MB |
| app1 | app/ota_1 | 0x610000 | 6 MB |
| spiffs | data/spiffs | 0xC10000 | ~4 MB |

Nooit `huge_app` gebruiken — brak OTA. Nooit 4MB controller gebruiken.

### 3.3 Strapping pins — nooit als input

| Pin | Reden |
| --- | --- |
| IO8 | Strapping pin — LEEG LATEN |
| IO9 | Strapping pin — LEEG LATEN |
| IO0 | Boot pin — alleen als output of met sterke pull-up |
| IO15 | Alleen als output |

IO14 bestaat niet op het 32-pin devboard (wel in SoC datasheet maar niet uitgebroken).

### 3.4 Voeding

| Situatie | Voeding |
| --- | --- |
| Test | 5V via USB-C connector van het devboard |
| Productie | 5V via VIN-pin, beveiligd met PTC-zekering 500 mA |

### 3.5 Voorgestelde pinout Smart Energy

| Pin | Functie | Opmerking |
| --- | --- | --- |
| IO3 | S0-puls Solar | Interrupt FALLING, pull-up + optocoupler |
| IO4 | S0-puls Verbruik WON | Interrupt FALLING, pull-up + optocoupler |
| IO5 | S0-puls Verbruik SCH | Interrupt FALLING, pull-up + optocoupler |
| IO6 | S0-puls ECO-boiler (optioneel) | Interrupt FALLING |
| IO1 | Relaisuitgang reserve | Optioneel |
| IO8 | LEEG LATEN | Strapping pin |
| IO9 | LEEG LATEN | Strapping pin |

S0-aansluiting per kanaal:
```
S0+ --+-- 3.3V (via 10k pull-up)
      +-- ESP32 GPIO (interrupt INPUT_PULLUP)
S0- -- GND
Optocoupler tussen S0-teller en ESP32 aanbevolen (galvanische scheiding).
```

---

## 4. Software & architectuur

### 4.1 Arduino IDE instellingen

| Parameter | Waarde |
| --- | --- |
| Board | ESP32C6 Dev Module |
| Flash Size | 16MB |
| Partition Scheme | Custom (partitions_16mb.csv) |
| Upload Speed | 921600 |

### 4.2 Bibliotheken

| Bibliotheek | Gebruik |
| --- | --- |
| ESPAsyncWebServer (Me-No-Dev) | Webserver, alle endpoints, chunked streaming |
| AsyncTCP (Me-No-Dev) | Vereist door ESPAsyncWebServer |
| Preferences (built-in) | NVS opslag voor instellingen |
| SPIFFS (built-in) | debug.log rotatie bij >800KB |
| HTTPClient (built-in) | HTTP calls naar ECO-controller en Tesla API |
| WiFi / esp_wifi (built-in) | WiFi + power save UIT via esp_wifi_set_ps |
| arduino-esp32-Matter | Matter endpoints |

### 4.3 Sketch-structuur (verplichte elementen)

```cpp
// VERPLICHT voor ESP32-C6 RISC-V serieel
#define Serial Serial0

// Versie
#define VERSION "Smart Energy v0.1"

// Pinnen S0-tellers
#define S0_SOLAR_PIN  3
#define S0_WON_PIN    4
#define S0_SCH_PIN    5
#define S0_ECO_PIN    6   // optioneel

// S0 configuratie (te verifiëren van label, AP2)
#define PULSEN_PER_KWH  1000
#define WH_PER_PULS     (1000.0 / PULSEN_PER_KWH)

// Netwerk
#define MY_IP           "192.168.0.73"
#define ECO_BOILER_IP   "192.168.0.71"
#define ZARLAR_DASH_IP  "192.168.0.60"

// Drempelwaarden (ook instelbaar via /settings + NVS)
#define DREMPEL_OVERSCHOT_W   200  // Watt
#define DREMPEL_DUUR_SEC       60  // seconden

// Simulatiemodus — ALLEEN voor ontwikkeling zonder hardware
bool SIMULATION_MODE = false;
```

### 4.4 S0-pulsmeting — implementatieprincipe

```cpp
volatile uint32_t cnt_solar = 0;
volatile uint32_t cnt_won   = 0;
volatile uint32_t cnt_sch   = 0;

void IRAM_ATTR isr_solar() { cnt_solar++; }
void IRAM_ATTR isr_won()   { cnt_won++;   }
void IRAM_ATTR isr_sch()   { cnt_sch++;   }

void setup() {
  pinMode(S0_SOLAR_PIN, INPUT_PULLUP);
  attachInterrupt(S0_SOLAR_PIN, isr_solar, FALLING);
  // idem WON en SCH
}

// Elke seconde in loop():
// Sla cnt_* op, reset tellers
// vermogen_W = pulsen_per_seconde * WH_PER_PULS * 3600.0
// overschot_W = solar_W - won_W - sch_W
```

### 4.5 HTTP REST communicatie

Geen MQTT, geen cloud, geen Home Assistant.

```
Smart Energy (192.168.0.73)
  --> HTTP GET/POST --> ECO-boiler (192.168.0.71)     [aan/uit]
  --> HTTP PUT      --> Tesla Wallcharger [IP t.b.d.] [lader sturen]
  --> Modbus TCP    --> Thuisbatterij [toekomst 2028]
  --> /json output  --> Zarlar Dashboard (192.168.0.60) --> Google Sheets
```

ECO-boiler aansturen:
```cpp
HTTPClient http;
http.begin("http://192.168.0.71/relay?state=1");  // inschakelen
int code = http.GET();
http.end();
```

Tesla Wallcharger Gen 3 API:
```
GET  http://[IP]/api/1/vitals  -> laadstroom, spanning, energie, status
PUT  http://[IP]/api/1/status  -> laden in-/uitschakelen
```
Onofficieel — kan veranderen bij Tesla firmware-update.

### 4.6 Sturingslogica

```
Elke seconde:
  vermogen solar/won/sch berekenen uit pulsen
  overschot_W = solar_W - won_W - sch_W

Elke 60 seconden:
  gemiddeld_overschot = mean(laatste 60 waarden)

  ALS gemiddeld_overschot > DREMPEL_OVERSCHOT_W:
    -> ECO-boiler AAN via HTTP 192.168.0.71 (prioriteit 1)
    -> [fase 2] Tesla-lader AAN via REST API (prioriteit 2)
    -> [fase 3] Batterij laden via Modbus (prioriteit 4)
  ANDERS:
    -> ECO-boiler UIT (enkel als wij hem inschakelden)
    -> [fase 2] Tesla-lader beperken/stoppen

EPEX day-ahead (fase 2):
  Elke avond: prijzen volgende dag ophalen via ENTSO-E API
  Prijs <= 0,00 EUR/kWh : batterij + ECO-boiler + EV maximaal laden
  Prijs 0,00-0,10       : EV laden 's nachts
  Prijs > 0,20          : batterij ontladen, grote verbruikers uit
  Solar-overschot heeft altijd prioriteit boven day-ahead
```

---

## 5. UI-architectuur (Zarlar stijlgids — identiek aan ECO v1.23)

### 5.1 CSS-kleurpalet

```
Header:       background #ffcc00, kleur zwart, vetgedrukt
Sidebar:      background #336699, links wit, breedte 60px
Groepstitels: background #336699, cursief-vet, wit
Tabel labels: kleur #369, fontsize 13px
Status AAN:   background #0a0, tekst wit
Status UIT:   background #999, tekst wit
Sim-banner:   background #c00, tekst wit (SIMULATION MODE)
```

### 5.2 Pagina-structuur

| URL | Inhoud |
| --- | --- |
| `/` | Hoofddashboard: solar W, WON W, SCH W, overschot W, relaisstatus ECO + EV |
| `/charts` | Historische grafieken Chart.js (aparte pagina, geheugen!) |
| `/settings` | Drempelwaarden, day-ahead aan/uit, override, PT-sensortype |
| `/json` | Compact JSON voor Zarlar Dashboard |
| `/update` | OTA firmware-update |
| `/log` | SPIFFS debug.log |
| `/clear_log` | Log wissen |
| `/restart` | Controller herstarten |

### 5.3 /json endpoint — sleutelstructuur

```json
{
  "a": 3450,   "b": 1200,   "c": 800,    "d": 1450,
  "e": 1,      "f": 0,      "g": 0,
  "h": 10450,  "i": 4200,   "j": 2800,
  "ac": -65,   "ae": 48320
}
```

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| a | Solar vermogen huidig | W |
| b | Verbruik WON huidig | W |
| c | Verbruik SCH huidig | W |
| d | Overschot (a - b - c) | W |
| e | ECO-boiler aan/uit | 0/1 |
| f | Tesla lader aan/uit | 0/1 |
| g | Override actief | 0/1 |
| h | Solar dag cumulatief | Wh |
| i | WON dag cumulatief | Wh |
| j | SCH dag cumulatief | Wh |
| ac | WiFi RSSI | dBm |
| ae | Heap largest block | bytes |

Na definitieve keuze: GAS-script aanmaken + Zarlar Dashboard polling configureren.

### 5.4 Logging SPIFFS

```cpp
// Rotatie bij >800KB (identiek ECO-boiler)
logEvent(LOG_INFO,  "Solar: 3450W, Overschot: 1450W, ECO: AAN");
logEvent(LOG_WARN,  "Tesla API onbereikbaar");
logEvent(LOG_ERROR, "Heap < 25KB, crash-log geschreven");
// Crash-log naar NVS bij largest_block < 25KB
```

---

## 6. Matter-integratie (fase 2)

```cpp
#include <Matter.h>
#include <MatterEndPoints/MatterOnOffPlugin.h>

MatterOnOffPlugin matterOverride;  // Override solar-sturing via Apple Home
// Pairing code ALTIJD tonen in webUI, niet alleen in Serial
// MDNS.begin() weglaten (conflicteert met Matter mDNS)
// Auto-recovery corrupt Matter NVS na Matter.begin()
```

---

## 7. Zarlar Dashboard integratie

### 7.1 Matrix — nieuwe rij S-ENERGY

| Matrix rij | Label | Controller | IP |
| --- | --- | --- | --- |
| 0 | S-HVAC | HVAC | 192.168.0.70 |
| 1 | S-ECO | ECO Boiler | 192.168.0.71 |
| **2** | **S-ENERGY** | **Smart Energy** | **192.168.0.73** |
| 3 | S-OUTSIDE | Buiten (gepland) | 192.168.0.72 |

### 7.2 Kolomindeling rij 2 (S-ENERGY)

| Col | Key | Label | Kleur |
| --- | --- | --- | --- |
| 0 | --- | Status | Groen=online, rood=offline |
| 1 | a | Solar W | Geel: helder=veel solar |
| 2 | d | Overschot W | Groen=overschot, rood=tekort |
| 3 | e | ECO-boiler | Oranje=aan, dim=uit |
| 4 | f | Tesla laden | Groen=laden, dim=uit |
| 5 | g | Override | Rood=override, dim=auto |
| 6 | h | Solar dag Wh | Geel schaal |
| 7-13 | --- | reserve | --- |
| 14 | ae | Heap KB | Groen>35 / geel>25 / rood |
| 15 | ac | RSSI | Groen>=-60 / oranje / rood |

### 7.3 Google Apps Script

Nieuw script aanmaken analoog aan `ECO_GoogleScript.gs`.
Kolommen A-M (13 velden: timestamp + 12 JSON-keys a t/m ae).

---

## 8. Energiedata referentiewaarden 2025

| Parameter | Waarde |
| --- | --- |
| Solar productie | 10.763 kWh/jaar |
| Totaal verbruik | 18.762 kWh/jaar |
| Netto netaankoop | 7.999 kWh/jaar |
| Bruto injectie | ~6.996 kWh/jaar |
| Bruto aankoop | ~14.995 kWh/jaar |
| Jaarfactuur | EUR 2.643 (incl. prosumententarief EUR 556) |
| Prosumententarief | EUR 556/jaar — verdwijnt bij digitale meter 2028 |

Seizoensprofiel:
- Mrt-Okt: structureel solar-overschot — ECO-boiler + EV sturing actief
- Nov-Feb: tekort — sturing weinig zinvol

---

## 9. Openstaande actiepunten

| # | Actie | Door wie | Status |
| --- | --- | --- | --- |
| AP1 | Westdak SMA omvormer 3: type + SN noteren | Maarten | Open |
| AP2 | S0-tellers: pulsen/kWh lezen van label | Filip | Open |
| AP6 | EV-lader 2: merk/type en stuurbaarheid | Maarten | Open |
| AP8 | SMA Speedwire testen op lokaal netwerk | Filip | Open |
| AP9 | Jaarbedrag Engie FLOW invullen | Maarten | Open |
| AP10 | CZ-TAW1 WP WON resetten + herregistreren | Filip | Open |

Afgevinkt: ESP32-C6 16MB OK, WiFi tellerkast OK, ECO-boiler OEG ~2kW OK, geen HA OK.

---

## 10. Fasering

### Fase 1 — Basissturing (nu starten)

1. S0-pulsmeting: interrupt-driven tellers solar + WON + SCH
2. Vermogen- en overschotberekening real time
3. Dashboard UI: solar/WON/SCH/overschot + statusbadges
4. /json endpoint met compacte keys
5. ECO-boiler sturen via HTTP REST 192.168.0.71
6. Drempellogica instelbaar via /settings + NVS
7. OTA via /update
8. SPIFFS logging + crash-log NVS
9. Zarlar Dashboard: polling 192.168.0.73 toevoegen
10. Google Apps Script aanmaken voor Smart Energy data

### Fase 2 — EV & geavanceerde sturing

- Tesla Wallcharger integreren (solar overdag + override UI)
- EPEX day-ahead API (nachtladen EV)
- SMA Speedwire/Modbus TCP (optioneel, detail)
- Matter-integratie (override via Apple Home)
- Matrix-rij S-ENERGY activeren in Dashboard

### Fase 3 — Thuisbatterij (~2028)

- Aankoop 10 kWh Huawei/BYD bij overgang digitale meter
- Modbus TCP batterijsturing
- Day-ahead arbitrage: laden goedkoop, ontladen duur
- Capaciteitstarief: piekafvlakking

---

## 11. Systeemoverzicht

```
[Zonnepanelen zuiddak + westdak]
        |
[SMA omvormers SB3600TL-21 x2 + westdak x1]
  Speedwire ingebouwd (fase 2)
        |
[S0-pulstellers]
  Solar / WON / SCH / ECO-boiler
        |
        v
[ESP32-C6 "Smart Energy" -- 192.168.0.73 -- Tellerkast keuken SCH]
        |
   +----+------------------+------------------+
   v                       v                  v
[ECO-boiler ctrl]   [Tesla Wallcharger]  [Batterij ~2028]
 192.168.0.71        REST API lokaal      Modbus TCP
 HTTP REST           onofficieel          Huawei/BYD
   |
   +--> [Zarlar Dashboard 192.168.0.60]
              |
              v
        [Google Sheets]
```

---

## 12. Versiegeschiedenis

| Versie | Datum | Inhoud |
| --- | --- | --- |
| v0.1 | April 2026 | Initieel document op basis van projectdocument v0.7 |
| v0.2 | April 2026 | Geintegreerd met Zarlar Master Overnamedocument: IP 192.168.0.73, matrix rij S-ENERGY, GAS-script, pinout, S0-code, JSON-keys, fasering, alle Zarlar-regels opgenomen |
