# Energy Management System — Zarlardinge
## Technisch werkdocument v0.7 — April 2026

**ESP32-C6 · Arduino IDE · ESPAsyncWebServer · Matter · Google Sheets**
*Filip Dworp (FiDel) — Zarlardinge (BE)*

---

## Instructies bij gebruik van dit document

1. Upload dit document + eventuele sketch aan het begin van elke nieuwe sessie
2. Vraag Claude het document samen te vatten voor er iets aangepast wordt
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
         +--> Cloudflare Tunnel --> publieke URL (remote toegang)
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

### 2.1 Fysieke locatie controller

De Smart Energy controller staat in de **inkomhal van Maarten**, naast de Telenet router:
- WiFi optimaal (rechtstreeks naast router)
- 2 meter UTP kabel naar de verdeelkast (diffs, automaten, S0-tellers)
- Zichtbaar voor Maarten en Celine: LED-strip op het kastje
- Voeding: 5V via eigen adaptor of USB-C van router

### 2.2 Zonne-installatie

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
| Solar S0-teller | Digitale S0-pulsuitgang — in verdeelkast |

### 2.3 Meters & tellers

| Component | Details |
| --- | --- |
| Fluvius digitale meter | NEE — uitzondering tot eind 2028 |
| Huidig metertype | Terugdraaiende teller — injectie vergoed aan aankoopprijs |
| Overgang 2028 | Digitale meter verplicht. Injektietarief daalt naar ~0,05 EUR/kWh |
| S0-tellers aanwezig | Solar, Verbruik WON, Verbruik SCH |
| Pulsen/kWh | Nog te lezen van label — typisch 1000 imp/kWh (AP2) |

### 2.4 Stuurbare verbruikers (toekomst fase 2+)

| Verbruiker | Hardware | Stuurbaarheid | Prioriteit |
| --- | --- | --- | --- |
| ECO-boiler (SCH) | OEG dompelweerstand ~2 kW | HTTP REST naar 192.168.0.71 | 1 hoogste |
| EV-lader 1 (WON) | Tesla Wallcharger Gen 3 | Lokale REST API HTTP PUT | 2 |
| EV-lader 2 (WON) | Merk/type onbekend (AP6) | Nog te bepalen | 3 |
| Thuisbatterij | Huawei/BYD voorkeur ~2028 | Modbus TCP | 4 |
| WP WON (Panasonic) | Warmtepomp woning, CZ-TAW1 module | Comfort Cloud API of smart relay (fase 2+) | 5 |
| WP SCH (Panasonic) | Warmtepomp schuur, CZ-TAW1 module | Comfort Cloud API of smart relay (fase 2+) | 5 |
| **Regenwaterpomp** | Op WON-teller ~500 kWh/j | **NOOIT STUREN** | --- |

De regenwaterpomp is essentieel voor al het sanitair. Dit is een harde constraint die nooit mag wijzigen.

> Warmtepompen zijn grote verbruikers (~1,5–3 kW) die zinvol zijn om te sturen bij solar-overschot
> of goedkope EPEX-prijzen. Via de CZ-TAW1 module en Panasonic Comfort Cloud API of via een
> smart relay op de stuurlijn. Eerst AP10 oplossen (WP WON herregistratie) voor sturing mogelijk is.

### 2.5 Warmtepompen (Panasonic + CZ-TAW1)

| | WP SCH | WP WON |
| --- | --- | --- |
| In werking | Januari 2019 | November 2019 |
| Comfort Cloud | OK — Filip als eigenaar | GEBLOKKEERD |
| Oplossing WP WON | --- | CZ-TAW1 resetten via paperclip, herregistreren op prive-mail Filip, Maarten toevoegen (AP10) |

---

## 3. Hardware Smart Energy controller

### 3.1 Module & board

| Parameter | Waarde |
| --- | --- |
| Module | ESP32-C6-WROOM-1N16 (Espressif) |
| Flash | 16 MB |
| Board | 32-pin clone (AliExpress batch dec 2025, EUR 2.52/stuk) |
| WiFi | WiFi 6 (2.4 GHz), 3.3V IO niet 5V-tolerant |
| Shield | ESP32-C6 Zarlar shield (5V voeding via VIN + PTC 500mA) |
| Locatie | Kastje inkomhal Maarten, naast Telenet router |
| Vaste IP | 192.168.0.73 |
| Naam netwerk | ESP32_SMART_ENERGY |

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

IO14 bestaat niet op het 32-pin devboard (niet uitgebroken).

---

## 4. Interface PCB — S0 optocoupler board

### 4.1 Ontwerp & productie

- Ontwerpen in **Eagle (Autodesk)**
- Bestellen via **JLCPCB** (5 stuks, ~EUR 5 inclusief verzending)
- Formaat: ~4 x 5 cm, past in kastje voor de ESP32-C6 shield
- Connector naar ESP32 shield via 3-polige header (GND + 3 GPIO-lijnen)

### 4.2 Aansluitkabel

Afgeschermde UTP kabel (~2 m) van verdeelkast naar kastje inkomhal:
- Pair 1: Solar S0+ / S0-
- Pair 2: WON   S0+ / S0-
- Pair 3: SCH   S0+ / S0-
- Pair 4: reserve / scherm naar GND

### 4.3 Schema per S0-kanaal — SMD ontwerp, 4 kanalen

**PCB-specificaties:**
- Afmeting per bordje: **40 × 40 mm**
- Panelisatie: **2×2 tiles op 100×100 mm** met V-score snijlijnen
- Bestelling JLCPCB: 5 panels = **20 bordjes** voor ±€5
- Componenten: **SMD** (0805 weerstanden, SOP-4 optocouplers)
- Connector naar ESP32 shield: **RJ45 female** (zie §4.5)
- S0-ingangen: **4× 2-polige schroefbornier** (5mm raster)

**Schema per S0-kanaal (4× identiek op het bordje):**

```
  TELLER ZIJDE (galvanisch vrij)    ESP32 SHIELD ZIJDE (3.3V)

  S0+ --[R 330Ω 0805]--+            3.3V (van RJ45 pin 2)
                        |               |
                     EL817S         [R 10kΩ 0805]
                     (SOP-4)            |
                     anode              +---> RJ45 data pin --> GPIO INPUT_PULLUP
                     kathode            |
                        |            EL817S collector
  S0- ─────────────────+            EL817S emitter
                                        |
                                       GND (van RJ45 pin 1)
```

Puls van teller: S0+ – S0− kort gesloten
→ LED EL817S licht op → transistor geleidt → GPIO naar LOW → ESP32 FALLING interrupt

**SMD component keuze:**
- Optocoupler: **EL817S** (SOP-4) — drop-in SMD equivalent van PC817
- Weerstanden: **0805** formaat — makkelijk handmatig te solderen
- Alle componenten beschikbaar bij LCSC (JLCPCB's component library voor PCBA)

### 4.4 Componentenlijst PCB (SMD, 4 kanalen, 40×40mm)

| Ref | Component | Waarde / Type | Footprint | Qty | Opmerking |
| --- | --- | --- | --- | --- | --- |
| U1–U4 | Optocoupler | EL817S | SOP-4 | 4 | 1 per S0-kanaal, LCSC: C6578 |
| R1,R3,R5,R7 | Weerstand | 330 Ω | 0805 | 4 | Serie LED meter-zijde |
| R2,R4,R6,R8 | Weerstand | 10 kΩ | 0805 | 4 | Pull-up 3.3V ESP32-zijde |
| R9–R12 | Weerstand | 1 kΩ | 0805 | 4 | Serie status-LED (optioneel) |
| LED1–LED4 | LED | groen | 0805 | 4 | Optioneel — visuele pulsbevestiging |
| J1–J4 | Schroefbornier | 2-polig, 5mm | THT | 4 | S0+ en S0- per kanaal |
| J5 | RJ45 female | 8P8C | THT | 1 | Verbinding naar ESP32 shield |
| C1 | Condensator | 100 nF | 0805 | 1 | Ontkoppeling 3.3V voeding |

**Panelisatie:**
- 4× 40×40mm tiles in 2×2 raster op 100×100mm panel
- V-score snijlijnen tussen tiles (geen mousebites — vlakke breukrand)
- JLCPCB: 5 panels bestellen = 20 individuele bordjes voor ±€5 excl. verzending

### 4.5 Verbinding naar ESP32-C6 shield — RJ45

De interface PCB verbindt met de ESP32-C6 shield via de **Roomsense** of **Option**
aansluiting: een 8-pins RJ45 female bus op het shield, bereikbaar via een korte
patch-kabel (of rechte kabelverbinding) binnen het kastje.

**RJ45 pinout (T568B kleurcode):**

| RJ45 pin | Kleur (T568B) | Signaal | ESP32 GPIO |
| --- | --- | --- | --- |
| 1 | Oranje-wit | GND | GND |
| 2 | Oranje | 3.3V | 3.3V |
| 3 | Groen-wit | S0 Solar | IO3 (interrupt FALLING) |
| 4 | Blauw | S0 WON | IO4 (interrupt FALLING) |
| 5 | Blauw-wit | S0 SCH | IO5 (interrupt FALLING) |
| 6 | Groen | S0 ECO-boiler / reserve | IO6 (interrupt FALLING) |
| 7 | Bruin-wit | Reserve | IO7 |
| 8 | Bruin | Reserve | IO1 |

> Gebruik een **standaard UTP patchkabel** (max. 30 cm) binnen het kastje.
> Dezelfde RJ45 pinout geldt voor de UTP kabel naar de verdeelkast (§4.2)
> maar daar worden enkel de S0-paren gebruikt (pins 3-8), niet 3.3V/GND.

### 4.6 Waarom geen I2C uitbreiding

I2C voegt latentie toe en de ESP32 zou pulsen missen bij hoge pulsfrequentie
(bijv. bij 1000 imp/kWh en 3 kW vermogen = 833 ms/puls, maar bij piekverbruik
kan dit sneller). Directe GPIO-interrupts zijn de enige correcte aanpak voor S0.

---

## 5. LED-strip indicatie — 8 pixels WS2812B

### 5.1 Plaatsing

Verticale strip van **8 WS2812B LEDs** op de buitenkant van het kastje in de
inkomhal van Maarten. Legende naast elke LED (labelstrip of gravure).
Zichtbaar voor Maarten, Celine, Filip en Mireille.

### 5.2 Bedrading

| Aansluiting | Waarde |
| --- | --- |
| ESP32 GPIO | IO10 (of vrije pin, aan te passen) |
| Voeding pixels | 5V (rechtstreeks van voeding, niet van ESP32 3.3V!) |
| GND | Gemeenschappelijk met ESP32 |
| Data | Via 330 Ohm serie-weerstand naar DIN WS2812B |
| Ontkoppelcondensator | 100 uF 10V over 5V-GND aan begin strip |

Bibliotheek: Adafruit NeoPixel of FastLED (al beschikbaar in Zarlar ecosystem).

### 5.3 LED-strip indeling — 8 LEDs altijd aan, elk één aspect

Alle 8 LEDs zijn **altijd actief** en tonen elk een eigen aspect van de energiesituatie.
Kleur = status, helderheid = intensiteit van het aspect.
Legende op het kastje naast elke LED (labelstrip of gravure).

**Doelgroep:** Céline, Mireille, Maarten — niet-technisch. In één oogopslag zien
wat ze kunnen of moeten doen, zonder app te openen.

| LED | Aspect | Kleurschaal | Praktische betekenis |
| --- | --- | --- | --- |
| 1 | **Solar productie** | Uit=geen solar / Geel dim=beetje / Geel helder=matig / Groen helder=veel | Hoeveel gratis stroom is er nu? |
| 2 | **Netto balans** | Rood=we kopen van net / Blauw=evenwicht / Groen=we sturen in | Zijn we netto verbruiker of producent? |
| 3 | **EPEX prijs nu** | Groen<€0,10 / Geel €0,10-0,20 / Oranje €0,20-0,35 / Rood>€0,35 | Is stroom nu goedkoop of duur? |
| 4 | **EPEX prijs +1 uur** | Zelfde schaal als LED 3 | Wordt stroom straks goedkoper of duurder? |
| 5 | **ECO-boiler** | Grijs=uit / Oranje=aan via solar / Wit=aan via goedkoop net / Rood=aan maar duur | Warmt de boiler nu op? Mag dat? |
| 6 | **EV-lader** | Grijs=niet actief / Groen=laden via solar / Blauw=laden goedkoop tarief / Rood=laden duur tarief | Laadt de auto op een slim moment? |
| 7 | **Actie-advies** *(samenvatting)* | Groen helder=goed moment grote verbruiker aan / Geel=neutraal / Oranje=wacht even / Rood=vermijd verbruik nu | De belangrijkste LED: wat moet ik doen? |
| 8 | **Systeem** | Groen=alles OK / Geel=geen EPEX data / Rood=controller offline | Werkt het systeem correct? |

**LED 7 — advieslogica (combinatie van solar + EPEX):**
```
IF solar_overschot > 500W OF epex_prijs < 0.08:   GROEN  (goed moment)
ELIF solar_overschot > 0 EN epex_prijs < 0.15:    GEEL   (neutraal-goed)
ELIF epex_prijs < 0.20:                            GEEL   (normaal)
ELIF epex_prijs < 0.35:                            ORANJE (wacht)
ELSE:                                              ROOD   (vermijd verbruik)
```

**Helderheid:**
- Overdag actief: 40% helderheid als rustige achtergrond
- Bij urgente situatie (LED 7 = GROEN of ROOD): 100% helder + zachte puls
- Nacht (23h–06h): 10% — zichtbaar maar niet storend
- Dimbaar per tijdzone via /settings

### 5.4 JSON voor LED-strip

De ESP32 berekent de LED-kleuren intern uit de bestaande data (a, d, n, n2, e, f).
Geen extra JSON-keys nodig voor de LED-kleuren zelf.
Key `o` in /json = helderheidspercentage (0–100, instelbaar via /settings).

---

## 6. Remote toegang — Cloudflare Tunnel

### 6.1 Concept

De ESP32 opent zelf een veilige HTTPS-tunnel naar Cloudflare.
Geen port-forwarding nodig op de Telenet router.
Bereikbaar via een vaste URL zoals `smart-energy.zarlardinge.be` van overal ter wereld.

Sluit aan bij de bestaande Cloudflare-infrastructuur van Filip
(workers: `controllers-diagnose.filip-delannoy.workers.dev`).

### 6.2 Implementatie op ESP32

Cloudflare Tunnel (cloudflared) draait normaal als daemon op een server.
Voor ESP32 is de aanpak anders: een **Cloudflare Worker als proxy**:

```
iPhone / Android (overal ter wereld)
    |
    v
https://smart-energy.filip-delannoy.workers.dev  (Cloudflare Worker)
    |
    v
http://192.168.0.73  (ESP32 thuis, lokaal netwerk)
    |
    v
ESPAsyncWebServer response
```

De Worker stuurt de request door naar het lokale IP via een vaste tunnel.
Authenticatie via een geheim token in de Worker (niet in de browser).

Alternatief eenvoudiger: **Cloudflare Tunnel daemon op de Telenet router**
(als die OpenWrt draait) of op een Raspberry Pi als die beschikbaar is.

### 6.3 Wat bereikbaar is remote

- `/` — hoofddashboard (read-only view, mobiel geoptimaliseerd)
- `/json` — live data voor integratie
- `/settings` — alleen via authenticatie (token in URL of header)

---

## 7. EPEX day-ahead prijsdata in de UI

### 7.1 Databron

ENTSO-E Transparency Platform: gratis, publiek, geen API-key nodig.

```
URL: https://web-api.tp.entsoe.eu/api
Parameters:
  documentType=A44 (day-ahead prices)
  in_Domain=10YBE----------2 (Belgie)
  out_Domain=10YBE----------2
  periodStart=YYYYMMDD0000
  periodEnd=YYYYMMDD2300
  securityToken=<gratis aan te vragen>
```

De ESP32 haalt dagelijks om 14h30 de prijzen op voor de volgende dag (EPEX
publiceert om ~13h). Data wordt opgeslagen in SPIFFS voor gebruik zonder WiFi.

### 7.2 Weergave in de UI

- Huidige prijs groot bovenaan in EUR/kWh, kleurgecodeerd
- Balk- of lijndiagram van de komende 24 uur (Chart.js op /charts pagina)
- Markering goedkoopste 4 uur (voor EV nachtladen)
- Markering piekprijzen in rood
- Advies: "Beste laadvenster EV: 02h-06h (gem. EUR 0,03/kWh)"

### 7.3 Lokale opslag SPIFFS

Prijsdata opslaan als JSON in SPIFFS (`/epex.json`):
- Beschikbaar zonder WiFi
- Gebruikt als basis voor LED-strip kleur en push-notificaties
- Dagelijks overschreven bij succesvolle download

---

## 8. Push-notificaties — ntfy.sh

### 8.1 Keuze: ntfy.sh

Gratis, open-source, geen account vereist, eenvoudige HTTP POST vanuit ESP32.
App beschikbaar voor iOS en Android.

### 8.2 Werking

```
ESP32 --> HTTP POST --> https://ntfy.sh/zarlardinge-energy
                            |
                            v
                    iPhone / Android app
                    (geabonneerd op topic)
```

Het topic `zarlardinge-energy` is privaat (niemand anders weet de naam).
Optioneel beveiligen met een zelfgekozen wachtwoord.

### 8.3 Implementatie ESP32

```cpp
void sendNotification(const char* title, const char* message, const char* priority) {
  HTTPClient http;
  http.begin("https://ntfy.sh/zarlardinge-energy");
  http.addHeader("Title", title);
  http.addHeader("Priority", priority);  // "low", "default", "high", "urgent"
  http.addHeader("Tags", "solar");       // emoji tag
  http.POST(message);
  http.end();
}

// Voorbeelden:
sendNotification("Gratis stroom!", "Solar overschot 2.3 kW - ideaal voor wasmachine", "default");
sendNotification("EPEX piekprijs", "EUR 0,48/kWh nu - stop grote verbruikers", "high");
sendNotification("Goedkope stroom vannacht", "Laad EV tussen 02h-05h (gem. EUR 0,02/kWh)", "low");
```

### 8.4 Drie niveaus — instelbaar per gebruiker

Push-notificaties zijn **optioneel** en configureerbaar via /settings.
Standaard: enkel URGENT aan. Maarten wil geen storende berichten — de LED-strip
en UI zijn zijn primaire informatiebron.

| Niveau | Standaard | Voorbeeldberichten |
| --- | --- | --- |
| **URGENT** (altijd aan, niet uitschakelbaar) | AAN | Controller offline >10 min / Heap kritisch laag / WiFi verloren |
| **ACTIONABLE** (optioneel, per gebruiker) | UIT | "Goedkope stroom nu — ideaal voor wasmachine" / "Grote solar productie — EV laden?" / "Goedkoopste nachtvenster EV: 02h–05h (€0,02/kWh)" |
| **INFO** (dagelijks rapport, optioneel) | UIT | "Dagrapport: WON €1,84 / SCH €0,92 / Injectie €0,34" |

**Configuratie per gebruiker in /settings:**
```
Notificaties:
  [x] URGENT     — altijd actief (Filip + Maarten)
  [ ] ACTIONABLE — Filip: AAN  /  Maarten: UIT (eigen voorkeur)
  [ ] INFO       — Filip: UIT  /  Maarten: UIT

Drempelwaarden (instelbaar):
  Solar actie-drempel:   500 W overschot gedurende 5 min
  EPEX goedkoop-drempel: 0,08 EUR/kWh
  EPEX duur-drempel:     0,35 EUR/kWh
  Offline-timeout:       10 min
```

### 8.5 Prioriteit: visueel boven push

De **LED-strip** (§5.3) en het **UI dashboard** zijn de primaire informatiebronnen.
Push-notificaties zijn enkel aanvullend voor situaties die actie vragen terwijl
niemand naar het kastje kijkt (bv. 's nachts goedkope stroom voor EV-laden).

**UI dashboard — visuele prioriteiten:**
- Groot en duidelijk leesbaar op iPhone/Android (mobiel geoptimaliseerd)
- Huidig advies altijd bovenaan: groene/oranje/rode banner met één duidelijke zin
  ("☀️ Solar overschot — zet wasmachine aan" / "💰 Goedkope stroom — laad EV" / "🔴 Piekprijs — vermijd verbruik")
- EPEX-prijsgrafiek 24h direct zichtbaar zonder scrollen
- Tariefvergelijk dynamisch/vast als compact blok

---

## 9. Google Sheets logging

Identiek aan alle andere Zarlar-controllers:
- Zarlar Dashboard (192.168.0.60) pollt /json elke 5 minuten
- POST naar Google Apps Script (GAS) nieuw script aan te maken
- Nieuw tabblad `S-ENERGY` in de Zarlar Google Sheet

### 9.1 Kolommen GAS-script tabblad "Data" — definitieve versie

Dit tabblad bevat de continue logging (elke 5-15 minuten één rij).
Het is de **enige referentie** voor de GAS "Data" kolommen.
Keys verwijzen naar §11.4.

| Kol | Key | Beschrijving | Eenheid |
| --- | --- | --- | --- |
| A | — | Tijdstip | datetime |
| B | a | Solar vermogen | W |
| C | b | Verbruik WON | W |
| D | c | Verbruik SCH | W |
| E | d | Overschot (a-b-c) | W |
| F | h | Solar productie dag | Wh |
| G | i | WON verbruik dag bruto | Wh |
| H | j | SCH verbruik dag bruto | Wh |
| I | v | Injectie dag | Wh |
| J | q | Kost WON dag — dynamisch | EUR/100 |
| K | qv | Kost WON dag — vast | EUR/100 |
| L | r | Kost SCH dag — dynamisch | EUR/100 |
| M | rv | Kost SCH dag — vast | EUR/100 |
| N | s | Solar opbrengst dag — dynamisch | EUR/100 |
| O | sv | Solar opbrengst dag — vast | EUR/100 |
| P | n | EPEX prijs huidig kwartier | EUR/kWh x1000 |
| Q | n2 | EPEX prijs volgend kwartier | EUR/kWh x1000 |
| R | nv | Vast tarief geconfigureerd | EUR/kWh x1000 |
| S | pt | Piek gecombineerde afname (maand) | W |
| T | pw | Piek WON individueel (maand) | W |
| U | ps | Piek SCH individueel (maand) | W |
| V | e | ECO-boiler aan/uit | 0/1 |
| W | f | Tesla laden aan/uit | 0/1 |
| X | g | Override actief | 0/1 |
| Y | o | LED-strip status | 0-7 |
| Z | eod | End-of-day vlag | 0/1 |
| AA | ac | WiFi RSSI | dBm |
| AB | ae | Heap largest block | bytes |

---

## 10. Zarlar Dashboard integratie

### 10.1 Matrix rij S-ENERGY (rij 2)

| Matrix rij | Label | Controller | IP |
| --- | --- | --- | --- |
| 0 | S-HVAC | HVAC | 192.168.0.70 |
| 1 | S-ECO | ECO Boiler | 192.168.0.71 |
| **2** | **S-ENERGY** | **Smart Energy** | **192.168.0.73** |
| 3 | S-OUTSIDE | Buiten (gepland) | 192.168.0.72 |

### 10.2 Kolomindeling rij 2 (S-ENERGY)

| Col | Key | Label | Kleur |
| --- | --- | --- | --- |
| 0 | --- | Status | Groen=online, rood=offline |
| 1 | a | Solar W | Geel: helder=veel solar |
| 2 | d | Overschot W | Groen=overschot, rood=tekort |
| 3 | n | EPEX prijs | Groen=goedkoop, rood=duur |
| 4 | e | ECO-boiler | Oranje=aan, dim=uit |
| 5 | f | Tesla laden | Groen=laden, dim=uit |
| 6 | g | Override | Rood=override, dim=auto |
| 7 | o | LED helderheid | Wit dim tot helder |
| 8-13 | --- | reserve | --- |
| 14 | ae | Heap KB | Groen>35KB / geel / rood |
| 15 | ac | RSSI | Groen>=-60 / oranje / rood |

---

## 11. Software architectuur — sketch opbouw

### 11.1 Basis: ECO-boiler v1.23 als template

De Smart Energy sketch vertrekt van `ESP32_C6_MATTER_ECO-boiler_22mar_2200.ino`
en behoudt de volledige structuur:
- ESPAsyncWebServer met chunked streaming
- Matter integratie
- SPIFFS logging + NVS crash-log
- OTA via /update
- UI-stijl (geel/marineblauw)

Wat nieuw is ten opzichte van ECO-boiler:
- 3x S0 interrupt-driven pulsmeting (ipv temperatuursensoren)
- EPEX data ophalen en tonen
- WS2812B LED-strip aansturen (8 pixels)
- ntfy.sh push-notificaties
- Cloudflare Worker integratie (remote toegang)
- Uitgebreidere /charts pagina (vermogen + EPEX grafiek)

### 11.2 Kritische defines (bovenaan sketch)

```cpp
#define Serial Serial0  // VERPLICHT ESP32-C6

#define VERSION        "Smart Energy v0.1"
#define HOSTNAME       "ESP32_SMART_ENERGY"
#define MY_IP          "192.168.0.73"

// S0 pinnen
#define S0_SOLAR_PIN   3
#define S0_WON_PIN     4
#define S0_SCH_PIN     5

// S0 configuratie (te lezen van label teller, AP2)
#define PULSEN_PER_KWH 1000
#define WH_PER_PULS    (1000.0 / PULSEN_PER_KWH)

// LED strip
#define LED_PIN        10
#define LED_COUNT      8

// Netwerk
#define ECO_BOILER_IP  "192.168.0.71"
#define ZARLAR_IP      "192.168.0.60"
#define NTFY_TOPIC     "zarlardinge-energy"
#define ENTSO_TOKEN    ""  // aan te vragen op transparency.entsoe.eu

// Drempelwaarden (ook via /settings + NVS)
#define DREMPEL_OVERSCHOT_W   200
#define DREMPEL_GOEDKOOP      0.05  // EUR/kWh
#define DREMPEL_DUUR          0.25  // EUR/kWh
#define DREMPEL_PIEK          0.40  // EUR/kWh
```

### 11.3 Pagina-structuur

| URL | Inhoud | Toegang |
| --- | --- | --- |
| `/` | Dashboard: solar/WON/SCH/overschot + EPEX prijs + LED status | Lokaal + remote |
| `/charts` | Grafieken: vermogen 24h + EPEX prijscurve 24h | Lokaal + remote |
| `/settings` | Drempelwaarden, EPEX, notificaties, LED-dimmer | Lokaal + token |
| `/json` | Compact JSON voor Zarlar Dashboard | Lokaal + remote |
| `/update` | OTA firmware-update | Lokaal enkel |
| `/log` | SPIFFS debug.log | Lokaal enkel |
| `/restart` | Controller herstarten | Lokaal enkel |

### 11.4 JSON /json endpoint — volledige en definitieve key-lijst

Dit is de **enige referentie** voor alle /json keys. Sectie 9.1 (GAS-kolommen)
en sectie 16.4 (Sheets-architectuur) zijn hierop gebaseerd.

**Real-time vermogen:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| a | Solar vermogen huidig | W |
| b | Verbruik WON huidig | W |
| c | Verbruik SCH huidig | W |
| d | Overschot (a-b-c, negatief = tekort) | W |

**Dagcumulatieven — energie (kWh):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| h | Solar productie dag | Wh |
| i | WON verbruik dag bruto | Wh |
| j | SCH verbruik dag bruto | Wh |
| v | Injectie naar net dag | Wh |

**Dagcumulatieven — kost dynamisch tarief (EPEX):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| q | Kost WON dag — dynamisch | EUR x100 (int) |
| r | Kost SCH dag — dynamisch | EUR x100 (int) |
| s | Solar opbrengst dag — dynamisch | EUR x100 (int) |

**Dagcumulatieven — kost vast tarief (vergelijking):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| qv | Kost WON dag — vast tarief | EUR x100 (int) |
| rv | Kost SCH dag — vast tarief | EUR x100 (int) |
| sv | Solar opbrengst dag — vast tarief | EUR x100 (int) |

**EPEX-prijzen:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| n | EPEX prijs huidig kwartier | EUR/kWh x1000 (int) |
| n2 | EPEX prijs volgend kwartier | EUR/kWh x1000 (int) |
| nv | Vast tarief geconfigureerd | EUR/kWh x1000 (int) |

**Vermogenpiek huidige maand:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| pt | Piek gecombineerde netto afname (basis capaciteitstarief) | W (int) |
| pw | Piek netto afname WON individueel | W (int) |
| ps | Piek netto afname SCH individueel | W (int) |

**Sturings- en systeemstatus:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| e | ECO-boiler aan/uit (fase 2) | 0/1 |
| f | Tesla laden aan/uit (fase 2) | 0/1 |
| g | Override actief | 0/1 |
| o | LED-strip helderheid (0–100%, instelbaar) | int |
| p | Laatste push verstuurd (unix timestamp) | int |
| eod | End-of-day vlag (00:00–00:05) | 0/1 |
| ac | WiFi RSSI | dBm |
| ae | Heap largest block | bytes |

### 11.5 S0-pulsmeting principe

```cpp
volatile uint32_t cnt_solar = 0;
volatile uint32_t cnt_won   = 0;
volatile uint32_t cnt_sch   = 0;
volatile uint32_t ts_solar  = 0;  // timestamp laatste puls (ms)
volatile uint32_t ts_won    = 0;
volatile uint32_t ts_sch    = 0;

void IRAM_ATTR isr_solar() { cnt_solar++; ts_solar = millis(); }
void IRAM_ATTR isr_won()   { cnt_won++;   ts_won   = millis(); }
void IRAM_ATTR isr_sch()   { cnt_sch++;   ts_sch   = millis(); }

// Elke seconde: vermogen berekenen uit pulsen
// vermogen_W = cnt_per_sec * WH_PER_PULS * 3600.0
// Als laatste puls > 30s geleden: vermogen = 0 (stilstand)
```

---

## 12. Fasering — gefaseerd ontwikkelplan

### Fase 1 — Meten, leren en adviseren (nu te realiseren)

Doel: een werkende "energiecoach" die meet en adviseert, maar nog niets automatisch stuurt.
Dit laat Maarten, Celine, Filip en Mireille toe hun leefpatroon te begrijpen en aan te passen
vóór de digitale meter verplicht wordt in 2028.

| Stap | Taak | Afhankelijkheid |
| --- | --- | --- |
| 1.1 | Interface PCB ontwerpen in Eagle + bestellen JLCPCB | --- |
| 1.2 | UTP kabel trekken van verdeelkast naar inkomhal | --- |
| 1.3 | Sketch v0.1: S0-pulsmeting + vermogensberekening | ECO v1.23 als basis |
| 1.4 | Sketch v0.1: UI dashboard solar/WON/SCH/overschot | 1.3 |
| 1.5 | Sketch v0.1: /json endpoint + OTA + logging | 1.4 |
| 1.6 | Zarlar Dashboard: polling 192.168.0.73 toevoegen | 1.5 |
| 1.7 | GAS script aanmaken voor Smart Energy logging | 1.6 |
| 1.8 | Sketch v0.2: EPEX data ophalen + UI grafiek | 1.4 |
| 1.9 | Sketch v0.2: EPEX in JSON key n | 1.8 |
| 1.10 | Sketch v0.3: LED-strip 8 pixels (WS2812B) | 1.8 |
| 1.11 | Sketch v0.3: ntfy.sh push-notificaties | 1.8 |
| 1.12 | Cloudflare Worker uitbreiden voor remote /json + UI | 1.5 |
| 1.13 | Matrix rij S-ENERGY activeren in Dashboard | 1.6 |

Volgorde van uitvoering: 1.1 en 1.2 kunnen parallel aan de sketch-ontwikkeling.
Start met 1.3-1.5 (meten en loggen), daarna 1.8-1.11 (EPEX + LED + push).

### Fase 2 — Adviseren + sturen (na ~6 maanden observatie)

Doel: automatisch sturen op basis van bewezen patronen.

- ECO-boiler automatisch bij solar overschot (HTTP REST naar 192.168.0.71)
- Tesla Wallcharger solar overdag + EPEX nachtladen
- Override-knoppen per verbruiker in UI en Apple Home (Matter)
- Dagschema EPEX: goedkoopste vensters automatisch benutten

### Fase 3 — Thuisbatterij + digitale meter (~2028)

- Aankoop 10 kWh Huawei/BYD bij overgang digitale meter
- Modbus TCP batterijsturing
- Capaciteitstarief optimalisatie (piekafvlakking)
- Volledige autonome energieoptimalisatie

---

## 13. Systeemoverzicht volledig

```
[Zonnepanelen zuiddak + westdak]
        |
[SMA omvormers x3] -- Speedwire (fase 2)
        |
[S0-tellers in verdeelkast]
  Solar / WON / SCH
        |
[UTP kabel ~2m afgeschermd]
        |
[Interface PCB -- optocouplers PC817]
  3x galvanische scheiding
        |
[ESP32-C6 Shield -- kastje inkomhal Maarten -- 192.168.0.73]
        |
   +----+-------+--------+----------+----------+
   |            |        |          |          |
[LED strip]  [ntfy.sh] [/json]  [EPEX API]  [fase 2+]
 8x WS2812B   push      |       ENTSO-E      ECO-boiler
 inkomhal    notif.     |       dag-voor-dag  Tesla
                        |                    Batterij
                        v
              [Zarlar Dashboard 192.168.0.60]
                        |
                        v
                 [Google Sheets]
                 [Matrix rij 2: S-ENERGY]

                        |
              [Cloudflare Worker]
                        |
              [iPhone / Android]
              [overal ter wereld]
```

---

## 14. Openstaande actiepunten

| # | Actie | Door wie | Status |
| --- | --- | --- | --- |
| AP1 | Westdak SMA omvormer 3: type + SN noteren | Maarten | Open |
| AP2 | S0-tellers: pulsen/kWh lezen van label | Filip | Open |
| AP3 | ENTSO-E API token aanvragen (gratis) | Filip | Open |
| AP4 | ntfy.sh topic instellen op telefoons Filip + Maarten | Filip + Maarten | Open |
| AP5 | Eagle PCB ontwerp interface board | Filip | Open |
| AP6 | EV-lader 2: merk/type en stuurbaarheid | Maarten | Open |
| AP7 | UTP kabel trekken verdeelkast -> inkomhal | Filip + Maarten | Open |
| AP8 | SMA Speedwire testen op lokaal netwerk (fase 2) | Filip | Open |
| AP9 | Jaarbedrag Engie FLOW invullen | Maarten | Open |
| AP10 | CZ-TAW1 WP WON resetten + herregistreren | Filip | Open |
| AP11 | Cloudflare Worker uitbreiden voor remote UI | Filip | Open |

Afgevinkt: ESP32-C6 16MB OK, WiFi tellerkast OK, ECO-boiler OEG ~2kW OK, geen HA OK.

---

## 16. Energiekostenregistratie — NVS, Google Sheets & Capaciteitstarief

### 16.1 Concept en doelstelling

**Één aansluiting — twee huishoudens**

WON en SCH vormen juridisch en technisch één gebouw met één Fluvius-netaansluiting
en één elektriciteitsmeter. Na 2028 komt er één digitale meter voor de volledige
aansluiting. Het capaciteitstarief geldt voor de gecombineerde piekafname.

De S0-subtellers (WON, SCH, Solar) zijn interne meetpunten die de ESP32 gebruikt
voor de kostenverdeling tussen Filip en Maarten. Dit is een afspraak tussen de
bewoners — Fluvius en de energieleverancier zien alleen het totaal.

De controller registreert elke 15 minuten de werkelijke energiekost per huishouden
onder twee tariefstelsels tegelijk. Samen met de vermogenpiek vormt dit de basis
voor drie toepassingen:

1. **Kostenverdeling WON/SCH** — transparante en eerlijke verdeling van de energiekost
2. **Tariefvergelijking** — vast contract vs. dynamisch EPEX, side-by-side, per maand
3. **Batterij-investeringsanalyse** — simulatie van het capaciteitstarief en ROI-berekening

Alle data overleeft stroomuitval (NVS), wordt lokaal gearchiveerd (SPIFFS) en
gesynchroniseerd naar Google Sheets in twee tabbladen: continue meting en dagoverzicht.

---

### 16.2 Berekeningsmodel per kwartier (15 minuten)

**Stap 1 — kWh per huishouden meten:**
```
solar_kWh = pulsen_solar_15min / PULSEN_PER_KWH
won_kWh   = pulsen_won_15min   / PULSEN_PER_KWH
sch_kWh   = pulsen_sch_15min   / PULSEN_PER_KWH
```

**Stap 2 — solar proportioneel verdelen (principe 1 projectdocument):**
```
totaal_verbr = won_kWh + sch_kWh

IF totaal_verbr > 0:
  solar_won = solar_kWh * (won_kWh / totaal_verbr)
  solar_sch = solar_kWh * (sch_kWh / totaal_verbr)
ELSE:
  solar_won = solar_kWh / 2    // gelijke verdeling als niemand verbruikt
  solar_sch = solar_kWh / 2
```

**Stap 3 — netto aankoop of injectie:**
```
netto_won = MAX(0, won_kWh - solar_won)     // kWh aangekocht voor WON
netto_sch = MAX(0, sch_kWh - solar_sch)     // kWh aangekocht voor SCH
netto_inj = MAX(0, solar_kWh - won_kWh - sch_kWh)  // kWh naar net gestuurd
```

**Stap 4 — kosten berekenen met EPEX-prijs:**
```
epex_prijs = kwartierprijs EUR/kWh uit ENTSO-E data

kost_won  = netto_won * epex_prijs
kost_sch  = netto_sch * epex_prijs
sol_opbr  = netto_inj * inj_tarief
  // inj_tarief nu (terugdr. teller): = epex_prijs (teller draait terug aan aankoopprijs)
  // inj_tarief na 2028: ~0,05 EUR/kWh (instelbaar in /settings)
```

**Stap 5 — vermogenpiek bijhouden (capaciteitstarief):**
```
// Gecombineerde netto afname = basis voor het werkelijke capaciteitstarief
afname_totaal_kw = MAX(0, won_kWh + sch_kWh - solar_kWh) / 0.25  // gem. kW

IF afname_totaal_kw > piek_kw_totaal_maand:
  piek_kw_totaal_maand = afname_totaal_kw
  piek_ts_totaal_maand = huidig tijdstip

// Individuele pieken: voor gedragsanalyse per huishouden (niet voor Fluvius-tarief)
afname_won_kw = MAX(0, won_kWh - solar_won) / 0.25
afname_sch_kw = MAX(0, sch_kWh - solar_sch) / 0.25

IF afname_won_kw > piek_kw_won_maand: piek_kw_won_maand = afname_won_kw; piek_ts_won_maand = nu
IF afname_sch_kw > piek_kw_sch_maand: piek_kw_sch_maand = afname_sch_kw; piek_ts_sch_maand = nu
// Alle vier opslaan in NVS (zie 16.3)
```

**Stap 6 — dubbel tariefvergelijk (dynamisch vs. vast):**
```
vast_prijs = NVS-instelling "vast_tarief" (EUR/kWh, instelbaar via /settings)
             Standaard: huidig Engie FLOW variabel tarief (bijv. 0,28 EUR/kWh)

// Kost onder vast tarief (parallel aan dynamische berekening)
kost_won_v  = netto_won * vast_prijs
kost_sch_v  = netto_sch * vast_prijs
sol_opbr_v  = netto_inj * vast_prijs   // terugdraaiende teller = zelfde prijs
                                        // na 2028: inj_tarief_v apart instellen

// Verschil per kwartier (positief = dynamisch duurder dan vast)
diff_won  = kost_won  - kost_won_v
diff_sch  = kost_sch  - kost_sch_v
```

**Dagcumulatieven bijhouden (dynamisch + vast parallel):**
```
// Dynamisch (EPEX)
dag_kWh_solar   += solar_kWh
dag_kWh_won     += won_kWh
dag_kWh_sch     += sch_kWh
dag_kWh_inj     += netto_inj
dag_kost_won_d  += kost_won       // d = dynamisch
dag_kost_sch_d  += kost_sch
dag_sol_opbr_d  += sol_opbr
dag_epex_sum    += epex_prijs     // voor daggemiddelde

// Vast tarief (parallel)
dag_kost_won_v  += kost_won_v     // v = vast
dag_kost_sch_v  += kost_sch_v
dag_sol_opbr_v  += sol_opbr_v

dag_kwartieren  += 1
```
Reset dagcumulatieven elke dag om 00:00.

---

### 16.3 NVS — stroombeveiligde opslag

NVS (Non-Volatile Storage) overleeft stroomuitval en herstart volledig transparant.

**Schrijfstrategie:**
- Elke 15 minuten: dagcumulatieven wegschrijven naar NVS
- Enkel bij nieuwe maandpiek: vermogenpiek bijwerken in NVS
- Bij midnight: volledig dagresultaat als "laatste dag" in NVS
- Bij boot: NVS inlezen en doortellen alsof er niets gebeurd is

**Maximaal dataverlies bij stroomuitval: 15 minuten** — aanvaardbaar.
De lopende dag wordt correct afgemaakt zodra de stroom terugkomt.

**NVS namespace: "senrg" (Smart Energy)**

| NVS key | Type | Inhoud |
| --- | --- | --- |
| dag_kwh_sol | float | Solar dag kWh |
| dag_kwh_won | float | WON bruto dag kWh |
| dag_kwh_sch | float | SCH bruto dag kWh |
| dag_kwh_inj | float | Injectie dag kWh |
| dag_eur_won | float | WON dag kost EUR |
| dag_eur_sch | float | SCH dag kost EUR |
| dag_eur_inj | float | Solar opbrengst dag EUR |
| dag_epex_sum | float | Som EPEX prijzen (voor gem.) |
| dag_kwartieren | int | Aantal kwartieren geteld |
| piek_kw_tot | float | Gecombineerde piek netto afname (basis capaciteitstarief) |
| piek_ts_tot | uint32 | Timestamp gecombineerde piek |
| piek_kw_won | float | Individuele piek WON (gedragsanalyse) |
| piek_ts_won | uint32 | Timestamp WON-piek |
| piek_kw_sch | float | Individuele piek SCH (gedragsanalyse) |
| piek_ts_sch | uint32 | Timestamp SCH-piek |
| piek_maand | int | Maandnummer van de pieken (1-12) |
| vast_tarief | int | Vast contract prijs EUR/kWh x1000 (instelbaar) |
| inj_tarief | int | Injectietarief dynamisch EUR/kWh x1000 |
| inj_tarief_v | int | Injectietarief vast EUR/kWh x1000 |
| last_dag_data | string | JSON van laatste volledige dag (backup) |
| pulsen_kwh | int | Pulsen/kWh (geconfigureerd, AP2) |

**Boot-herstel procedure:**
```cpp
void bootHerstel() {
  // Lees NVS dagcumulatieven terug
  dag_kwh_sol = prefs.getFloat("dag_kwh_sol", 0.0);
  // ... alle andere keys
  
  // Controleer of het nog dezelfde dag is
  if (huidigeDag() != prefs.getInt("huidige_dag", 0)) {
    // Nieuwe dag: sla vorige dag op als 'last_dag_data' en reset
    slaLaatsteDagOp();
    resetDagCumulatieven();
  }
  // Anders: gewoon doortellen
}
```

---

### 16.4 Google Sheets — architectuur twee tabbladen

#### Tabblad "Data" — continue logging

Zarlar Dashboard pollt /json elke 5 minuten. Bij kwartierwissel (elke 15 min)
wordt ook de volledige kostendata opgenomen. Eén rij per pollingcyclus.

**Kolomdefinitie: zie §9.1** — dit is de enige referentie voor de "Data" kolommen.
Kolommen A-AB (28 velden: tijdstip + alle JSON-keys uit §11.4).

#### Tabblad "Verbruik" — dagelijkse samenvatting

Elke nacht om 00:00 schrijft de GAS-trigger (of de ESP32 via een speciale /eod call)
een dagsamenvattingsrij. Dit tabblad is het werkinstrument voor Filip en Maarten.

**Kolommen tabblad "Verbruik" (A tot P):**

| Kol | Beschrijving | Eenheid | Gebruik |
| --- | --- | --- | --- |
| A | Datum | DD/MM/YYYY | --- |
| B | Solar productie | kWh | Seizoensanalyse |
| C | WON verbruik bruto | kWh | Verbruiksevolutie |
| D | SCH verbruik bruto | kWh | Verbruiksevolutie |
| E | Solar aandeel WON | kWh | Gratis eigenverbruik WON |
| F | Solar aandeel SCH | kWh | Gratis eigenverbruik SCH |
| G | Injectie naar net | kWh | Batterijpotentieel |
| H | Kost WON — dynamisch (EPEX) | EUR | Werkelijke kost bij dynamisch contract |
| I | Kost WON — vast tarief | EUR | Vergelijkingsbasis vast contract |
| J | Verschil WON (H-I) | EUR | Negatief = dynamisch goedkoper |
| K | Kost SCH — dynamisch (EPEX) | EUR | Werkelijke kost bij dynamisch contract |
| L | Kost SCH — vast tarief | EUR | Vergelijkingsbasis vast contract |
| M | Verschil SCH (K-L) | EUR | Negatief = dynamisch goedkoper |
| N | Solar opbrengst — dynamisch | EUR | Waarde injectie tegen EPEX-prijs |
| O | Solar opbrengst — vast | EUR | Waarde injectie tegen vast tarief |
| P | EPEX gem. dagprijs | EUR/kWh | Prijspatroon |
| Q | Vast tarief gebruikt | EUR/kWh | Referentie (uit /settings) |
| R | Piek gecombineerd (echt tarief) | kW | Basis capaciteitstarief Fluvius |
| S | Piek tijdstip gecombineerd | HH:MM | Wanneer treedt de piek op |
| T | Piek WON individueel | kW | Gedragsanalyse WON |
| U | Piek SCH individueel | kW | Gedragsanalyse SCH |
| V | Cap. tarief simulatie (R x EUR 47,48) | EUR/maand | Kostprijs als digitale meter actief was |

**GAS berekent automatisch:**
- Rij "Maandtotaal": SOM kolommen C-G, H-O; MAX kolommen R, T, U
- Rij "Jaarttotaal" in december
- Kolom J en M: highlight groen als dynamisch goedkoper, rood als duurder
- Kolom V: capaciteitstarief simulatie op basis van maandpiek

#### Hoe de midnight-write werkt

Bij Zarlar wordt de GAS-trigger aangestuurd door polling van het Dashboard.
Voor de dagelijks samenvatting zijn twee aanpakken mogelijk:

**Aanpak A (aanbevolen):** ESP32 zet om 00:00 een speciale vlag in /json
(bijv. key `eod=1` gedurende 5 minuten). GAS-script detecteert dit en schrijft
de "Verbruik" rij op basis van de final-waarden in die /json response.

**Aanpak B:** GAS time-based trigger om 00:05 (elke nacht) die de laatste
waarden ophaalt uit de "Data" sheet en een dagsamenvattingsrij schrijft.

Aanpak B is robuuster (geen timing-afhankelijkheid van ESP32) en eenvoudiger.

---

### 16.5 SPIFFS — lokale backup en off-grid werking

SPIFFS bewaart dezelfde dagdata lokaal als het netwerk tijdelijk uitvalt.
Bij herverbinding worden ontbrekende dagbestanden via /eod endpoint aangeboden.

**Bestandsstructuur:**
```
/epex.json          -- EPEX prijsdata volgende 24h (dagelijks overschreven)
/data/2026-04-09.json  -- dagresultaat per dag
/data/2026-04-10.json
...
/debug.log          -- rolling log (<800KB)
/debug.log.old
```

**Formaat dagbestand:**
```json
{
  "date": "2026-04-09",
  "solar_kwh": 28.14,
  "won_kwh": 12.41,
  "sch_kwh": 6.22,
  "inj_kwh": 6.83,
  "won_eur_d": 1.84,     // dynamisch (EPEX)
  "sch_eur_d": 0.92,
  "inj_eur_d": 0.34,
  "won_eur_v": 2.11,     // vast tarief
  "sch_eur_v": 1.06,
  "inj_eur_v": 0.39,
  "epex_gem": 0.148,
  "vast_prijs": 0.280,   // geconfigureerd vast tarief die dag
  "piek_kw_tot": 5.12,   // gecombineerde piek (echt capaciteitstarief)
  "piek_ts_tot": "17:45",
  "piek_kw_won": 3.24,   // individuele piek WON
  "piek_ts_won": "17:45",
  "piek_kw_sch": 2.18,   // individuele piek SCH
  "piek_ts_sch": "18:00"
}
```

SPIFFS ~4MB: bij ~500 bytes/dag past **meer dan 20 jaar** aan dagdata.
Maand- en jaaroverzichten worden on-the-fly berekend uit de dagbestanden.

**Nieuwe /history pagina in de UI:**
- Tabel laatste 30 dagen (uit SPIFFS dagbestanden)
- Maandtotalen (berekend uit dagbestanden)
- Jaarkolom naast kolom voor kostenvergelijking WON vs SCH
- Download als CSV knop

---

### 16.6 Capaciteitstarief — meting en simulatie

Het capaciteitstarief (actief na 2028 met digitale meter, Vlaanderen) is gebaseerd op
de hoogste gemiddelde 15-minuten afname van het net per maand, uitgedrukt in kW.

Tarief 2025 (referentie): ~EUR 47,48/kW/jaar = ~EUR 3,96/kW/maand.
Een maandpiek van 5 kW kost dus ~EUR 20/maand = EUR 240/jaar extra.

**Één aansluiting — één capaciteitstarief:**
WON en SCH hebben één gezamenlijke netaansluiting. Het Fluvius-capaciteitstarief
is gebaseerd op de **gecombineerde** piekafname van de totale aansluiting.

**Berekening gecombineerde netto afname (basis voor echt tarief):**
```
afname_totaal_kw = MAX(0, won_W + sch_W - solar_W) / 1000.0
// Enkel wat netto van het net komt telt mee
// Solar die direct verbruikt wordt telt niet mee
```

**Individuele pieken (gedragsanalyse, niet voor Fluvius-tarief):**
```
afname_won_kw = MAX(0, won_W - solar_won_W) / 1000.0
afname_sch_kw = MAX(0, sch_W - solar_sch_W) / 1000.0
// Helpt zien WIE de piek veroorzaakt (EV bij WON? ECO-boiler bij SCH?)
```

**Wat de /history pagina toont:**
- Gecombineerde maandpiek per maand + gesimuleerd capaciteitstarief (piek_kw * EUR 47,48)
- Individuele WON/SCH pieken voor gedragsanalyse: wie veroorzaakt de piek?
- Potentiele besparing met een batterij die de gecombineerde piek afvlakt

**Nut voor batterij-investeringsbeslissing:**
Na 1 jaar data kunnen Filip en Maarten zien:
- Op welke momenten de piek optreedt (typisch 17h-19h in winter)
- Of een batterij die pieken beperkt tot bijv. 2,5 kW zinvol is
- Bijkomende ROI van het capaciteitstarief bovenop de energiebesparing

---

### 16.7 Dubbel tariefvergelijk — dynamisch vs. vast

**Doel:** aantonen of een dynamisch EPEX-contract financieel beter of slechter is dan
een vast contract, op basis van jullie werkelijk verbruiksprofiel.

**Configuratie in /settings:**
```
Vast tarief:  [0,280] EUR/kWh  (huidig Engie FLOW variabel excl. nettarieven)
              Aanpasbaar per jaar om historische vergelijking te maken
Inj. tarief dynamisch: [EPEX] (automatisch)
Inj. tarief vast:      [0,280] EUR/kWh (terugdraaiende teller = zelfde prijs)
                       Na 2028: ~0,050 EUR/kWh voor beide
```

**Weergave in UI hoofddashboard:**
```
+------------------------------------------+
| VANDAAG — TARIEFVERGELIJK                |
+--------------------+---------------------+
| DYNAMISCH (EPEX)   | VAST (EUR 0,28/kWh) |
+--------------------+---------------------+
| WON:  EUR 1,84     | WON:  EUR 2,11      |
| SCH:  EUR 0,92     | SCH:  EUR 1,06      |
| Inj.: EUR 0,34     | Inj.: EUR 0,39      |
| TOTAAL: EUR 2,76   | TOTAAL: EUR 3,17    |
+--------------------+---------------------+
| Dynamisch: EUR 0,41 GOEDKOPER vandaag   |
+------------------------------------------+
```

**Weergave in Google Sheets "Verbruik":**
- Kolommen H/I naast elkaar (dynamisch/vast WON)
- Kolom J = verschil, kleurgecodeerd: groen = dynamisch wint, rood = vast wint
- Maandtotaalrij: hoeveel bespaard/verloren per maand met dynamisch contract
- Na 1 jaar: duidelijk zichtbaar of omschakeling naar dynamisch contract loont

**Waarde van deze meting:**
- Zomers (veel solar + lage EPEX): dynamisch altijd beter
- Winteravonden (hoge EPEX piek): dynamisch soms duurder
- Gecombineerd effect over een jaar: de echte onderbouwing voor contractkeuze
- Dit is ook de data voor de bespreking bij contractverlenging (Engie FLOW loopt
  af na jaar 1, evaluatie gepland maart/april 2026 → Total Energies MY Confort)

---

### 16.8 Overzicht: wat gaat waar

| Data | NVS | SPIFFS | Sheets "Data" | Sheets "Verbruik" |
| --- | --- | --- | --- | --- |
| Puls-tellers (lopende 15') | Elke 15 min | Nee | Nee | Nee |
| Dagcumulatieven | Elke 15 min | Bij midnight | Ja (continu) | Ja (midnight) |
| Maandpiek WON | Bij nieuwe piek | Via dagbestand | Ja (continu) | Ja (midnight) |
| Maandpiek SCH | Bij nieuwe piek | Via dagbestand | Ja (continu) | Ja (midnight) |
| EPEX prijsdata | Nee | /epex.json | Ja (per rij) | Gem. per dag |
| Jaaroverzicht | Nee | Berekend | Via filter | Via GAS formule |
| Firmware instellingen | Ja | Nee | Nee | Nee |

---

### 16.9 JSON-keys

Alle JSON-keys zijn gedefinieerd in **§11.4** — de enige referentie.
De GAS "Data" kolommen zijn gedefinieerd in **§9.1**.
De SPIFFS dagbestanden en GAS "Verbruik" kolommen staan in §16.5 en §16.4.

---

### 16.10 Implementatievolgorde in de sketch

```
Sketch v0.1 (basis):
  - S0 interrupts, vermogensberekening, UI, /json, OTA, logging

Sketch v0.2 (EPEX + kosten):
  - EPEX data ophalen, kwartierkostenberekening
  - Dagcumulatieven met NVS backup (elke 15 min)
  - Maandpiek vermogen (NVS)
  - Nieuwe JSON-keys q, qv, r, rv, s, sv, v, n, n2, nv, pt, pw, ps, eod
  - Vast tarief instelling in NVS + /settings
  - Dubbel tariefvergelijk in UI hoofddashboard
  - GAS Verbruik-tabblad: dynamisch/vast kolommen + verschilkolom
  - /history pagina (SPIFFS dagbestanden)
  - GAS-script "Data" tabblad uitbreiden
  - GAS-script "Verbruik" tabblad aanmaken (midnight trigger)

Sketch v0.3 (LED + push):
  - LED-strip 8 pixels met EPEX+solar combinatielogica
  - ntfy.sh notificaties met kostencontext
```


---

## 15. Versiegeschiedenis

| Versie | Datum | Inhoud |
| --- | --- | --- |
| v0.1 | April 2026 | Initieel document op basis van projectdocument v0.7 |
| v0.2 | April 2026 | Geintegreerd met Zarlar Master Overnamedocument: IP 192.168.0.73, matrix, GAS-script, pinout, S0-code, JSON-keys, fasering |
| v0.3 | April 2026 | Gedetailleerd uitgewerkt plan: locatie inkomhal, PCB Eagle/JLCPCB, LED-strip 8px legende, ntfy.sh, Cloudflare Tunnel, EPEX ENTSO-E API, fasering 1-2-3 |
| v0.4 | April 2026 | Sectie 16 herschreven: NVS-strategie stroombeveiligd (elke 15 min), twee GAS-tabbladen Data+Verbruik, capaciteitstarief meting en simulatie, SPIFFS dagarchief 20+ jaar, midnight GAS-trigger, /history pagina, volledige kolommen- en key-definitie |
| v0.5 | April 2026 | Piek vermogen gesplitst in piek_won en piek_sch: aparte NVS-keys, JSON-keys pw/ps, GAS-kolommen, /history pagina, berekening via proportionele solar verdeling per huishouden |
| v0.6 | April 2026 | Een aansluiting verduidelijkt, gecombineerde capaciteitspiek pt, dubbel tariefvergelijk dynamisch/vast |
| v0.7 | April 2026 | JSON-keys, GAS-kolommen en SPIFFS geconsolideerd: §11.4 = enige JSON-referentie, §9.1 = enige GAS-Data-referentie. Duplicaten verwijderd. |
| v0.8 | April 2026 | WP WON/SCH toegevoegd aan stuurbare verbruikers. PCB: 4 kanalen SMD EL817S SOP-4, 40x40mm 2x2 paneel JLCPCB. Verbinding via RJ45 (Roomsense/Option shield). LED-strip: alle 8 permanent, elk eigen aspect (solar/balans/EPEX nu+straks/ECO/EV/advies/systeem). Push 3 niveaus configureerbaar, standaard enkel URGENT. UI: visuele banner prioriteit boven push. Matrix rij 2 spiegelt LED-aspecten. |
