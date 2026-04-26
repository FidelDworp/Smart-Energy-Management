# Energy Management System — Zarlardinge
## Technisch werkdocument v1.7 — April 2026

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
         +--> Cloudflare Tunnel --> publieke URL (remote toegang)
```

### Bestaande controllers

| Controller | Naam | IP | Versie | Status |
| --- | --- | --- | --- | --- |
| HVAC | ESP32_HVAC | 192.168.0.70 | v1.19 | Productie stabiel |
| ECO Boiler | ESP32_ECO Boiler | 192.168.0.71 | v1.23 | Productie stabiel |
| **Smart Energy** | **ESP32_C6_ENERGY** | **192.168.0.73** | **v1.26** | ✅ Actief (SIM_S0+SIM_P1) |
| ROOM Eetplaats | ESP32_EETPLAATS | 192.168.0.80 | v2.21 | Productie stabiel |
| Zarlar Dashboard | ESP32_ZARLAR | 192.168.0.60 | v5.8 | Productie stabiel |
| RPi Portal | Node.js server | 192.168.0.50 | v2.0 | ✅ Actief (Tailscale) |

**IP-keuze 192.168.0.73:** past in het blok systeem-controllers (70-74), na ECO (71) en de geplande buitensensor S-OUTSIDE (72). Room-controllers starten vanaf .75.

### Eerste ingebruikname S-ENERGY — 26 april 2026

De S-ENERGY controller werd voor het eerst in gebruik genomen op **26 april 2026 om 21:05**.
Eerste Google Sheets logging bevestigd met volgende meetwaarden:

| Parameter | Waarde | Opmerking |
| --- | --- | --- |
| Tijdstempel | 2026-04-26 21:05:53 | Eerste logging ✅ |
| Solar W | 2000 W | SIM_S0 actief |
| SCH afname W | 700 W | SIM_S0 actief |
| SCH injectie W | 1300 W | SIM_S0 actief |
| EPEX nu | 22,9 ct/kWh | ✅ Live van RPi |
| EPEX +1u | 22,7 ct/kWh | ✅ Live van RPi |
| WiFi RSSI | −49 dBm | ✅ Uitstekend |
| Heap largest | 256 KB | ✅ Gezond |
| SIM_S0 | 1 | ⚠️ Simulatie actief — wacht op S0 bekabeling |
| SIM_P1 | 1 | ⚠️ Simulatie actief — wacht op HomeWizard (~2028) |
| FW versie | 1.26 | ✅ |

**Validatiestatus bij eerste boot:**
- ✅ Matrix rij 2 pixels verschenen op fysieke 16×16 matrix
- ✅ S-ENERGY tegel actief in RPi portal (index.html)
- ✅ Virtuele matrix (matrix.html) toont rij 2 correct
- ✅ Google Sheets logging actief (eerste dataregel ontvangen)
- ✅ EPEX data live ontvangen van RPi
- ⚠️ SIM_S0 en SIM_P1 beide actief — omschakelen naar LIVE na bekabeling

---

## 2. Installatie — hardware overzicht

### 2.1 Fysieke locatie controller

De Smart Energy controller staat in de **inkomhal van Maarten**, naast de Telenet router:
- WiFi optimaal (rechtstreeks naast router)
- 2 meter UTP kabel naar de verdeelkast (diffs, automaten, S0-tellers)
- Zichtbaar voor Maarten en Celine: LED-strip op het kastje
- Voeding: 5V via eigen adaptor of USB-C van router

### 2.2 Zonne-installatie

**Systeemoverzicht:**

| Parameter | Waarde |
| --- | --- |
| Installateur / jaar | ALFASUN, 2017 |
| Totaal geinstalleerd vermogen | **12.100 Wp** (44 panelen x 275 Wp) |
| Totaal omvormersvermogen | **10.880 W** (7.200 + 3.680 W) |
| Jaaropbrengst (gemiddeld 10 jaar) | ~11.950 kWh/jaar |
| Solar S0-teller | Digitale teller met S0-pulsuitgang — in verdeelkast |

**Zuiddak — string 1 & 2:**

| Component | Details |
| --- | --- |
| Panelen | **32x Viessmann Vitovolt 300, 275 Wp** (polycrystallijn, 60 cellen) |
| String vermogen | 32 x 275 Wp = **8.800 Wp** |
| Omvormer 1 | SMA Sunny Boy **SB3600TL-21** |
| Omvormer 2 | SMA Sunny Boy **SB3600TL-21** |
| Serienummers | SN 2130419465 & SN 2130419851 |
| Instellingscode / norm | BES204 / C10/11:2012 |
| Communicatie | Speedwire/Webconnect ingebouwd — geen extra hardware |
| RID-codes | RHCD4F (SN ...465) & QX6NUA (SN ...851) |
| Omvormersvermogen | 2 x 3.600 W = **7.200 W** |
| Bezettingsgraad | 8.800 / 7.200 = 1,22 (normaal voor Belgisch klimaat) |

**Westdak — string 3:**

| Component | Details |
| --- | --- |
| Panelen | **12x Viessmann Vitovolt 300, 275 Wp** (polycrystallijn, 60 cellen) |
| String vermogen | 12 x 275 Wp = **3.300 Wp** |
| Omvormer 3 | SMA Sunny Boy **SB3.6-1AV-41** |
| Serienummer | Nog te noteren van typeplaatje (AP1) |
| Communicatie | WLAN + Ethernet + RS485 — **Modbus (SMA + SunSpec) + Webconnect** |
| Omvormersvermogen | **3.680 W** |
| MPP-ingangen | **2 onafhankelijke MPP-ingangen** (ingang A + ingang B) |
| Bezettingsgraad | 3.300 / 3.680 = 0,90 (onderbezet — conservatief voor westorientatie) |

**Panelen — Viessmann Vitovolt 300, 275 Wp (2017):**

| Parameter | Waarde |
| --- | --- |
| Type | Vitovolt 300, polycrystallijn, 60 cellen |
| Typeplaatje code | Nog te lezen van paneel of installatiedossier (AP2b) |
| Piek vermogen STC | 275 Wp |
| Module-efficientie | ~17% (typisch 60-cel poly 2017) |
| Temperatuurcoefficient Pmax | -0,41 %/K (typisch polycrystallijn) |
| Garantie product | 10 jaar (Viessmann standaard) |
| Garantie opbrengst | 25 jaar lineair, min. 80% |

**SMA SB3600TL-21 — specs:**

| Parameter | Waarde |
| --- | --- |
| Max. AC-vermogen | 3.680 W |
| Max. efficientie | 97,6% |
| Europese efficientie | 97,2% |
| Ingangsspanning bereik | 125-600 V DC |
| MPP-bereik | 188-500 V |
| Aantal MPPT | 1 |
| AC-aansluiting | 1-fase, 230 V / 50 Hz |
| Communicatie | Speedwire + Webconnect ingebouwd |
| Beschermingsklasse | IP65 |

**SMA SB3.6-1AV-41 — specs (westdak):**

| Parameter | Waarde |
| --- | --- |
| Max. AC-vermogen | 3.680 W |
| Max. efficiëntie / Europese eff. | 97,0% / 96,5% |
| Max. DC-ingangsvermogen | 5.500 Wp |
| Ingangsspanning bereik | 100-600 V DC |
| MPP-bereik | 130-500 V |
| Aantal MPPT / strings | **2 onafhankelijk** / A:2 + B:2 |
| Max. ingangsstroom per MPPT | 15 A |
| AC-aansluiting | 1-fase, 230 V / 50 Hz |
| Communicatie | **WLAN + Ethernet + RS485** |
| Protocollen | **Modbus (SMA + SunSpec), Webconnect, SMA Data** |
| Beschermingsklasse | IP65 |
| Gewicht | 17,5 kg |

> De SB3.6-1AV-41 biedt ruimere ESP32-integratiemogelijkheden dan de SB3600TL-21:
> WLAN, Ethernet en RS485 zijn beschikbaar naast Speedwire.
> SunSpec Modbus-protocol is breed compatibel met ESP32 Modbus-bibliotheken.
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

Bestand **`partitions_16mb.csv`** naast het `.ino` bestand plaatsen (exacte naam vereist door Arduino IDE — anders werkt Custom partition scheme niet):

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

### 4.3 S0-interface circuit — Aanpak A (directe verbinding)

**Waarom geen optocoupler nodig:**

De S0-uitgang van een energieteller is **intern al volledig geïsoleerd** van de
230V netspanning, verplicht conform IEC 62053-31. Het contact is een spanningsloos
reed-relais of open-collector transistor — geen enkel onderdeel van de 230V
installatie is bereikbaar via de S0-klemmen. Over een afgeschermde kabel van 2m
is directe verbinding met de ESP32 volkomen veilig.

Optocouplers zijn zinvol bij lange kabels (>10m), industriële omgevingen met sterke
EMC-storingen, of wanneer de S0-bron een actieve spanning heeft. Geen van deze
condities is van toepassing in onze installatie.

**Schema per S0-kanaal (4× identiek):**

```
  ESP32-SHIELD ZIJDE                    TELLER ZIJDE

  3.3V
    |
  [R1 10kΩ 0805]  ←── pull-up
    |
    +────────────────────────── S0+ klem (teller)
    |                           S0- klem (teller) ──── GND
    |
  [C1 10nF 0805]  ←── HF-filter (vermijdt valse pulsen door storingen)
    |
   GND

  Middenknoop (tussen R1 en S0+) → GPIO (INPUT, geen interne pull-up nodig)
```

**Werking:**
```
Rust (S0 open):    3.3V → R1 → GPIO → HIGH  (pull-up houdt GPIO hoog)
Puls (S0 sluit):   3.3V → R1 → S0+ → contact → S0- → GND
                   GPIO zakt naar LOW → ESP32 detecteert FALLING interrupt
```

**PCB-specificaties:**
- Afmeting per bordje: **40 × 40 mm**
- Panelisatie: **2×2 tiles op 100×100 mm** met V-score snijlijnen
- Bestelling JLCPCB: 5 panels = **20 bordjes** voor ±€5
- Componenten: volledig **SMD** (0805)
- Connector naar ESP32 shield: **RJ45 female** (zie §4.5)
- S0-ingangen: **4× 2-polige schroefbornier** (3,5mm raster, SMD)

> ⚠️ Actiepunt: controleer of de S0-uitgang van onze specifieke meetmodules
> passief (droog contact) of actief (met spanning) is — zie AP2c.

### 4.4 Componentenlijst PCB (SMD, 4 kanalen, 40×40mm)

| Ref | Component | Waarde / Type | Footprint | Qty | LCSC # | Opmerking |
| --- | --- | --- | --- | --- | --- | --- |
| R1,R3,R5,R7 | Weerstand | 10 kΩ | 0805 | 4 | C98220 | Pull-up 3.3V per kanaal |
| C1,C2,C3,C4 | Condensator | 10 nF | 0805 | 4 | C57112 | HF-filter per kanaal |
| R2,R4,R6,R8 | Weerstand | 1 kΩ | 0805 | 4 | C21190 | Serie status-LED (optioneel) |
| LED1–LED4 | LED | 3mm groen | 0805 | 4 | C2286 | Optioneel — visuele pulsbevestiging |
| J1–J4 | Schroefbornier | 2-polig, 3,5mm | THT/SMD | 4 | C474033 | S0+ en S0- per kanaal |
| J5 | RJ45 female | 8P8C | THT | 1 | C2884862 | Verbinding naar ESP32 shield |
| C5 | Condensator | 100 nF | 0805 | 1 | C49678 | Ontkoppeling 3.3V voeding |

> LCSC-nummers zijn indicatief — verifiëren bij aanmaken BOM in EasyEDA/Eagle.

**Panelisatie:**
- 4× 40×40mm tiles in 2×2 raster op 100×100mm panel
- V-score snijlijnen tussen tiles (vlakke breukrand, makkelijker te separeren dan mousebites)
- JLCPCB: 5 panels bestellen = 20 individuele bordjes voor ±€5 excl. verzending
- Optie: JLCPCB SMT Assembly voor R en C (volledig geassembleerd behalve connectors)

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

## 5. LED-strip indicatie — 12 pixels WS2812B

### 5.1 Plaatsing

Horizontale strip van **12 WS2812B LEDs** op de buitenkant van het kastje in de inkomhal van Maarten.
Symbool + korte titel naast elke LED (labelstrip of gravure).
Zichtbaar voor Maarten, Céline, Filip en Mireille zonder app te openen.

### 5.2 Bedrading

| Aansluiting | Waarde |
| --- | --- |
| ESP32 GPIO | IO4 |
| Voeding pixels | 5V (rechtstreeks van voeding, niet van ESP32 3.3V!) |
| GND | Gemeenschappelijk met ESP32 |
| Data | Via 330 Ohm serie-weerstand naar DIN WS2812B |
| Ontkoppelcondensator | 100 uF 10V over 5V-GND aan begin strip |

Bibliotheek: Adafruit NeoPixel of FastLED.

### 5.3 LED-indeling — 12 pixels in 5 groepen

Alle 12 LEDs altijd actief. Kleur = status, helderheid = intensiteit.
Doelgroep: Céline en Mireille voor snelle beslissingen, Filip en Maarten voor energiebeheer.

**Live testpagina (LED-simulatie):** https://fideldworp.github.io/ZarlarApp/epex-grafiek.html

| # | Sym | Titel | Groep | Kleurlogica |
| --- | --- | --- | --- | --- |
| 1 | ☀️ | Solar | Energie | Uit=geen / dim groen=weinig / fel groen=veel (sinusoïde 07-20u) |
| 2 | 💰 | Prijs | Energie | Lime=negatief / groen=goedkoop / geel=normaal / rood=duur |
| 3 | ⚖️ | Netto | Energie | Groen=overschot naar net / rood=afname van net |
| 4 | 🔋 | Batterij | Batterij | Groen=laden / oranje=ontladen / dim=idle (kleur = SOC) |
| 5 | ♨️ | ECO | Groot | Groen=aan / zwart=uit |
| 6 | 🚙 | EV WON | Groot | Groen gradient op laadvermogen (0–9 kW) |
| 7 | 🚗 | EV SCH | Groot | Idem |
| 8 | 🏠 | WP WON | Groot | Groen=aan / zwart=uit |
| 9 | 🏚️ | WP SCH | Groot | Groen=aan / zwart=uit |
| 10 | 🍳 | Koken? | Advies | Lime=negatief prijs / groen=goed moment / geel=kan / rood=duur of piek vol |
| 11 | 👕 | Wassen? | Advies | Zelfde logica, drempel 2 kW piekcapaciteit |
| 12 | 📊 | Piek | Piek | Groen=OK / geel=75-90% / oranje=90-100% / rood=over limiet |

**Pixels 10 en 11 (🍳 en 👕) zijn speciaal voor Céline en Mireille:**
groen = goed moment om in te schakelen (goedkope stroom + voldoende piegruimte),
rood = wacht (duur of piek bijna vol).

**Advieslogica pixels 10-11:**
```
EPEX < 0 ct:                          → LIME  (negatief = echt gratis)
EPEX < 10 ct EN piekcapaciteit >= kW: → GROEN (goedkoop + ruimte)
EPEX < 20 ct EN piekcapaciteit >= kW: → GEEL  (normaal, ruimte)
piekcapaciteit < kW:                  → DIM ROOD (piek vol)
EPEX >= 20 ct:                        → ROOD  (duur)
```

### 5.4 Helderheid

- Overdag actief: 40% helderheid
- Advies-pixels groen/rood: 100% + zachte puls
- Nacht (23h-06h): 10%
- Dimbaar via /settings (key `o` in /json)

### 5.5 JSON keys voor LED-strip

De ESP32 berekent LED-kleuren intern. Geen extra /json keys nodig.
Key `o` = helderheidspercentage (0-100).

---

## 6. Remote toegang — Raspberry Pi Gateway

De remote toegang voor **alle** Zarlar-controllers (HVAC, ECO, Smart Energy, ROOM,
Dashboard) verloopt via een **Raspberry Pi** die als centrale gateway dient.
De Pi draait nginx + cloudflared en fungeert als veilige brug tussen het internet
en het lokale netwerk — zonder open poorten op de Telenet-router.

**Volledige uitwerking: zie §17.**

Bereikbare URLs na installatie:
```
https://controllers.zarlardinge.be/senrg/    → Smart Energy (deze controller)
https://controllers.zarlardinge.be/eco/      → ECO Boiler
https://controllers.zarlardinge.be/hvac/     → HVAC
https://controllers.zarlardinge.be/status    → Health alle controllers
```

De ESP32-controllers zelf worden niet gewijzigd voor remote toegang.
OTA-updates ook mogelijk van overal: `/senrg/update`.

---

## 7. EPEX day-ahead prijsdata — plannings-UI en controller

**Live testpagina:** https://fideldworp.github.io/ZarlarApp/epex-grafiek.html
**Broncode:** `epex-grafiek.html` in GitHub repo `fideldworp/ZarlarApp`

### 7.1 Databron

**energy-charts.info** (Fraunhofer ISE) — gratis, geen API-key, 15-minuut resolutie voor België.

```
URL: https://api.energy-charts.info/price?bzn=BE&start=YYYY-MM-DD&end=YYYY-MM-DD
Respons: { unix_seconds: [...], price: [...] }  // prijs in €/MWh
```

EPEX publiceert dag-ahead prijzen dagelijks om ~13:00 CET.
Browserbeperking: CORS-proxy `corsproxy.io` voor GitHub Pages. Op ESP32: direct ophalen.

### 7.2 Reële prijsstructuur

```
Reële prijs (ct/kWh) = (EPEX + Fluvius + Elia + Heffingen + Hernieuwbaar) × (1 + BTW%)
```

BTW wordt berekend op de **volledige som** — niet enkel op EPEX. Alle componenten instelbaar:

| Component | Default | Opgeslagen |
| --- | --- | --- |
| Fluvius distributienettarief | 9.0 ct/kWh | `t_fluvius` |
| Elia transmissie | 1.0 ct/kWh | `t_elia` |
| Energieheffingen & accijnzen | 2.0 ct/kWh | `t_heffingen` |
| Hernieuwbare bijdragen | 1.5 ct/kWh | `t_hernieuwbaar` |
| BTW | 6% | `t_btw_pct` |

Ook bij negatieve EPEX-prijs worden de vaste componenten aangerekend.
De reële prijs wordt negatief enkel als EPEX < −(som vaste componenten) / (1+BTW%).

### 7.3 Prijsklassen (op EPEX-basis)

| Klasse | EPEX drempel | Kleur |
| --- | --- | --- |
| Negatief | < 0 ct | Superhel lime `#a8ff3e` |
| Goedkoop | 0–10 ct | Donker groen `#2a8a3e` |
| Normaal | 10–20 ct | Geel `#d29922` |
| Duur | > 20 ct | Rood `#f85149` |

### 7.4 48u rolling window met cache

```
Uurlijkse refresh:
  1. Haal vandaag op (altijd, start 00:00)
  2. Haal morgen op (beschikbaar na ~13:00)
  3. Samenvoegen → 96–192 kwartierslots
  4. Opslaan in localStorage / NVS
Cache-geldigheid: 26 uur
```

### 7.5 Vier grafieken op dezelfde tijdas (00:00 → +48u)

Alle vier grafieken gebruiken `allTimes` als x-as. Tijdlabels: elk even uur (0,2,4...22).
Verticale lijnen identiek in alle grafieken:
- Witte stippellijn = huidig tijdstip (exact op de minuut)
- Blauwe stippellijn = middernacht (dag-scheiding)

#### Grafiek 1 — EPEX prijzen

Gestapelde balken:
- Gekleurde balk = kale EPEX (prijsklasse-kleur)
- Grijs balkje = vaste opslag incl. BTW (varieert per slot want BTW hangt af van EPEX)
- Witte stippellijn = vast contract
- Lichtblauwe y-as links

#### Grafiek 2 — Solar productie

- Geel-oranje balken op sinusoïde-profiel (07:00–20:00, instelbaar max kW)
- Alleen slots ≤ nu gevuld — toekomst leeg
- Lichtblauwe y-as links
- Later: vervangen door echte S0-meting via `/json` key `a`

#### Grafiek 3 — Batterij laden/ontladen + SOC

- Groene balken omhoog = laden (kW), linker as
- Oranje balken omlaag = ontladen (kW), linker as
- Blauwe lijn = SOC in % (0–100), rechter as
- Dummy dataset forceert volledige breedte ook als alle data null is
- Later: echte SOC via Modbus batterijsturing

#### Grafiek 4 — Net import/export

- Groene balken omhoog = injectie naar net (opbrengst)
- Rode balken omlaag = afname van net (kost)
- Gele lijn = reële prijs ct/kWh (alleen verleden)
- Toont het financieel eindresultaat per kwartier

### 7.6 Planningstabel

Chronologische tabel van alle toekomstige periodes. Huidige periode bovenaan met `▶`.

**Vier klassen:**
- ★ Negatief: batterij laden geforceerd, alle groot aan
- Goedkoop: ECO + EVs + batterij aan, WPs uit, vul tot MAX_PIEK
- Normaal: alles uit (huishoud blijft klikbaar)
- Duur: alles uit, batterij ontladen geforceerd

**Piekbeheer afschakelvolgorde:**
```
1. EV SCH (Maarten — laagste prio)
2. Batterij laden
3. ECO-boiler
4. WP SCH
5. EV WON (Filip)
6. WP WON (allerlaatst)
```

**Kolommen:**
- ⏰ Periode + 💰 Reële prijs (EPEX+opslag, gekleurd)
- 🍳🌡️🍽️👕🌀 Huishoudtoestellen — **manueel**, altijd klikbaar, default uit
- ♨️🚙🚗🏠🏚️🔋 Grootverbruikers — **automatisch** algoritme
- 📊 Piekkolom — kleurverloop grijs→groen→geel→oranje→rood vs MAX_PIEK

Blauwe stippellijn (3px) scheidt huishoud van groot.

**Override-knoppen** per apparaat: AUTO / AAN / UIT. Hertekent tabel onmiddellijk.

### 7.7 LED-strip simulatie (12 pixels)

Tijdschuif (nu → einde data). Per slider-positie: 12 LEDs + SOC-balk + piekmeter + apparatenstatus.

| Groep | Pixels | Inhoud |
| --- | --- | --- |
| Energie | ☀️💰⚖️ | Solar · Prijs · Netto |
| Batterij | 🔋 | Laden/ontladen/SOC |
| Groot | ♨️🚙🚗🏠🏚️ | Verbruikers |
| Advies | 🍳👕 | Goed/slecht moment voor koken/wassen |
| Piek | 📊 | Piekmeter |

### 7.8 Infokaarten

Twee kaarten tussen grafieken en planningstabel:

**💶 Dagkost vandaag:**
```
kost = Σ_slots_vandaag (afname_kWh × reelePrijs/100) − (injectie_kWh × 0.05)
```

**📊 Dynamisch vs vast:**
```
besparing = kostVast − kostDyn
```
Groen = dynamisch goedkoper, rood = dynamisch duurder.

### 7.9 Instellingen (localStorage → NVS op ESP32)

| Parameter | Sleutel | Default |
| --- | --- | --- |
| Vaste contractprijs | `vast_prijs` | 28.0 ct/kWh |
| Batterijcapaciteit | `bat_kwh` | 10 kWh |
| Start SOC simulatie | `soc_start` | 50% |
| Solar max vermogen | `solar_max_kw` | 8 kW |
| Max piekgrens | `max_piek` | 15 kW |
| Capaciteitstarief | `cap_tar` | 4.80 €/kW/mnd |
| Fluvius nettarief | `t_fluvius` | 9.0 ct/kWh |
| Elia transmissie | `t_elia` | 1.0 ct/kWh |
| Heffingen | `t_heffingen` | 2.0 ct/kWh |
| Hernieuwbaar | `t_hernieuwbaar` | 1.5 ct/kWh |
| BTW | `t_btw_pct` | 6% |
| Periode-keuzes | `periode_keuzes` | — |
| Overrides | `overrides` | — |

### 7.10 Van GitHub Pages naar ESP32

| GitHub Pages (nu) | ESP32 (fase 1+) |
| --- | --- |
| `corsproxy.io` → energy-charts | ESP32 haalt zelf op → `/epex` |
| `localStorage` | NVS (Preferences) |
| Sinusoïde solar | Echte S0 via `/json` key `a` |
| Gesimuleerde batterij | Modbus SOC via `/json` key `bat_soc` |
| Demo-data fallback | NVS-cache fallback |

**Endpoint `/epex` (ESP32):**
```json
{
  "unix_seconds": [...], "price": [...],
  "interval_s": 900, "opslag_ct": 21, "cached_ts": 1744315800
}
```

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

### 11.1 Basis — sketch structuur v1.26

De Smart Energy sketch (`ESP32_C6_ENERGY_v1_26.ino`) is volledig zelfstandig gebouwd,
gebaseerd op de Zarlar-conventies maar met eigen structuur. Niet van ECO-boiler afgeleid.

**Twee onafhankelijke simulatievlaggen (kernprincipe):**

| Vlag | NVS key | Default | Omschakelen naar LIVE |
| --- | --- | --- | --- |
| `SIM_S0` | `sim_s0` | `true` | Na S0-bekabeling: uitvinken in /settings → reboot |
| `SIM_P1` | `sim_p1` | `true` | Na HomeWizard dongle (~2028): uitvinken + P1-IP invullen → reboot |

⚠️ **Nooit automatisch omschakelen** — altijd bewuste handeling om valse logs te voorkomen.
Beide vlaggen zijn NVS-persistent en onafhankelijk van elkaar.

Serieel commando's: `sim s0 on/off` · `sim p1 on/off` · `status` · `help`

**Simulatieprofielen (april, België):**
- S0 solar: sinus-curve 07:00–19:00, piek 4000 W, ±10% ruis
- S0 SCH afname: 500 W basis + ochtendspits +1800 W, avondspits +2200 W, nacht +800 W
- S0 SCH injectie: max(0, solar − afname)
- P1 WON afname: 400 W basis + spitsprofielen, geen injectie in simulatie

### 11.2 Kritische defines (bovenaan sketch v1.26)

```cpp
#define Serial Serial0   // VERPLICHT ESP32-C6 RISC-V

#define FW_VERSION   "1.26"
#define CTRL_ID      "S-ENERGY"
#define NVS_NS       "senrg"

// S0 pinnen — Roomsense RJ45 connector op Zarlar shield
#define S0_SOL_PIN    5   // IO5 — Zonnepanelen A14 forward (klem 18/19)
#define S0_SCHF_PIN   6   // IO6 — Schuur A5 afname forward (klem 18/19)
#define S0_SCHR_PIN   7   // IO7 — Schuur A5 injectie reverse (klem 20/21)

// LED matrix — WS2812B 12×4 = 48 pixels, serpentine
#define LED_PIN       4   // IO4 via 330Ω, Pixel-line connector — Zarlar shield
#define MATRIX_COLS  12
#define MATRIX_ROWS   4
#define NUM_PIXELS   48
#define DEF_BRIGHT   55

// S0 configuratie — Inepro PRO380-S
#define WH_PER_PULS    0.1f        // 0,1 Wh/imp = 10.000 imp/kWh
#define WATT_FACTOR    360000.0f   // P(W) = 360.000 / interval_ms
#define POWER_TO_MS    180000UL    // 3 min geen puls → P = 0 W

// Netwerk
#define DEF_IP       "192.168.0.73"
#define RPI_BASE     "http://192.168.0.50:3000"  // RPi Node.js portal
#define NTP_SRV      "pool.ntp.org"

// EPEX — vaste opslag (Fluvius Imewo + Ecopower)
#define VAST_CT_KWH   14.32f   // ct/kWh opslag bovenop EPEX spotprijs

// Simulatie — april-profiel België
#define SIM_SOLAR_PEAK_W  4000.0f
#define SIM_BASE_LOAD_W    500.0f
#define SIM_WON_BASE_W     400.0f
#define SIM_TICK_S           5.0f
```

⚠️ **Niet meer van toepassing:** ENTSO-E API, LED-strip 8 pixels, ntfy.sh, Cloudflare Worker.
EPEX-data komt via de RPi (`/api/epex`), niet rechtstreeks van ENTSO-E.

### 11.3 Pagina-structuur (v1.26)

| URL | Inhoud |
| --- | --- |
| `/` | Live status: solar/SCH/WON/netto/EPEX + sim-banners |
| `/json` | Compact JSON voor Dashboard + RPi polling |
| `/settings` | SIM_S0 checkbox, SIM_P1 checkbox + P1-IP, LED helderheid, max piek |
| `/update` | OTA firmware |
| `/reset_dag` | Dagcumulatieven resetten |
| `/reset_piek` | Maandpiek resetten |
| `/reboot` | Herstart |
| `/factory_reset` | NVS wissen + herstart |

### 11.4 JSON /json endpoint — actuele key-lijst v1.26

Dit is de **enige referentie** voor alle /json keys. GAS-script §9.1 en matrix §11.6 zijn hierop gebaseerd.

**Real-time vermogen (W):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| `a` | Solar vermogen huidig | W |
| `b` | WON vermogen huidig (P1, + = afname, − = injectie) | W |
| `c` | SCH afname huidig | W |
| `d` | SCH injectie huidig | W |
| `e` | Netto SCH (+ = injectie naar net) | W |

**Dagcumulatieven (Wh):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| `h` | Solar productie dag | Wh |
| `i` | WON afname dag (P1) | Wh |
| `j` | SCH afname dag | Wh |
| `k` | SCH injectie dag | Wh |
| `vw` | WON injectie dag (P1) | Wh |

**EPEX-prijzen (all-in):**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| `n` | EPEX all-in nu (EPEX + vaste opslag) | ct/kWh × 100 |
| `n2` | EPEX all-in +1u | ct/kWh × 100 |

**Vermogenpiek maand:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| `pt` | Maandpiek gecombineerde netto afname | W |

**Simulatievlaggen:**

| Key | Beschrijving | Waarden |
| --- | --- | --- |
| `sim_s0` | S0 kanalen gesimuleerd | 1 = SIM / 0 = LIVE hardware |
| `sim_p1` | P1 dongle gesimuleerd | 1 = SIM / 0 = LIVE HomeWizard |

**Systeem:**

| Key | Beschrijving | Eenheid |
| --- | --- | --- |
| `ac` | WiFi RSSI | dBm |
| `ae` | Heap largest block | bytes |
| `ver` | Firmware versie | string |

> ⚠️ **Vervallen keys** (waren gepland in v0.x, niet geïmplementeerd in v1.26):
> `q`, `qv`, `r`, `rv`, `s`, `sv`, `v`, `nv`, `pw`, `ps`, `eod`, `e` (ECO), `f` (Tesla), `g`, `o`, `p`
> Deze worden eventueel herintroduceerd in v1.27+ als de betreffende functionaliteit gebouwd wordt.

### 11.5 S0-pulsmeting principe (v1.26 — ISR-gedreven)

```cpp
// ISR — IRAM_ATTR verplicht voor ESP32-C6
void IRAM_ATTR isrSol() {
  unsigned long n = millis();
  if (isr_sol_last) isr_sol_iv = n - isr_sol_last;  // interval voor vermogen
  isr_sol_last = n; isr_sol_cnt++;
}

// 5s tick — vermogen berekenen
float calcW(unsigned long iv, unsigned long last_ms) {
  if (!last_ms || (millis() - last_ms) > POWER_TO_MS || !iv) return 0.0f;
  return WATT_FACTOR / (float)iv;  // W = 360.000 / interval_ms
}

// Delta pulsen → Wh dagcumulatief
wh_sol += (cs - prev_sol) * WH_PER_PULS;  // 0,1 Wh/puls
```

Pinnen worden altijd als `INPUT` geregistreerd (geen interne pull-up — externe 4,7kΩ op PCB).
Interrupts worden altijd geregistreerd, ook in SIM-modus (de pinnen pulseren dan gewoon niet).

### 11.6 LED Matrix 12×4 WS2812B (v1.26)

**Hardware:**
- 48 pixels WS2812B, serpentine layout (rij 0: L→R, rij 1: R→L, enz.)
- Connector: Pixel-line JST SM 3-pin (wit=DI, rood=+5V, blauw=GND)
- Voeding: aparte 5V/2A — NIET via shield PTC (te zwak)
- Data: IO4 via 330Ω serieweerstand

**Kolomlabels (voor op behuizing):**

| Col | Label | Inhoud |
| --- | --- | --- |
| 0 | SOL W | Solar vermogen (groen, 0–6000 W) |
| 1 | SOL kWh | Solar dag totaal (geel-groen, 0–30 kWh) |
| 2 | SCH AF | SCH afname (rood, 0–10000 W) |
| 3 | SCH INJ | SCH injectie (cyaan, 0–6000 W) |
| 4 | NETTO | Groen=injectie / Rood=afname |
| 5 | WON W | WON vermogen (amber, 0–5000 W) |
| 6 | EPEX | EPEX prijs nu (groen/geel/rood) |
| 7 | EPEX+1 | EPEX prijs +1u |
| 8 | PIEK% | Maandpiek % van max |
| 9 | KOKEN? | Groen = goed moment (EPEX < 15 ct) |
| 10 | WASSEN? | Zelfde logica als KOKEN? |
| 11 | HEAP | Geheugen ESP32 (groen/amber/rood) |

> ⚠️ **Verschil met EMS v1.5:** het document beschreef een LED-strip van 8 pixels in 5 groepen.
> De werkelijke implementatie is een **12×4 matrix van 48 pixels** — wezenlijk anders.

**Sim-indicator:** col 0 rij 0 knippert rood als SIM_S0 actief · col 1 rij 0 als SIM_P1 actief.

---

## 12. Fasering — gefaseerd ontwikkelplan

### Fase 1 — Meten, leren en adviseren (nu te realiseren)

Doel: een werkende "energiecoach" die meet en adviseert, maar nog niets automatisch stuurt.

| Stap | Taak | Status |
| --- | --- | --- |
| 1.1 | Interface PCB ontwerpen in Eagle + bestellen JLCPCB | ⬜ Open |
| 1.2 | UTP kabel trekken van verdeelkast naar inkomhal | ⬜ Open |
| 1.3 | Sketch v1.26: S0-pulsmeting + simulatiemodus SIM_S0 | ✅ Klaar |
| 1.4 | Sketch v1.26: UI dashboard solar/SCH/WON/EPEX | ✅ Klaar |
| 1.5 | Sketch v1.26: /json endpoint + OTA + NVS | ✅ Klaar |
| 1.6 | Dashboard v5.8: S-ENERGY rij 2 matrix geactiveerd | ✅ Klaar |
| 1.7 | GAS script v1: 19 kolommen A–S, sim-vlaggen gelogd | ✅ Klaar |
| 1.8 | EPEX data via RPi /api/epex (niet ENTSO-E direct) | ✅ Klaar |
| 1.9 | LED matrix 12×4 WS2812B, 48 pixels | ✅ Klaar |
| 1.10 | RPi portal: /api/poll/senrg + matrix.html + S-ENERGY tegel | ✅ Klaar |
| 1.11 | SIM_P1: HomeWizard P1 integratie klaar in sketch (wacht op hardware) | ✅ Klaar (LIVE in 2028) |
| 1.11b | **Eerste ingebruikname 26/04/2026** — alle systemen gevalideerd ✅ | ✅ **Mijlpaal** |
| 1.12 | S0 bekabeling aansluiten → SIM_S0 uitschakelen | ⬜ Open (na PCB) |
| 1.13 | ntfy.sh push-notificaties | ⬜ Open (v1.27) |

### Fase 2 — Adviseren + sturen (na ~6 maanden observatie)

Doel: automatisch sturen op basis van bewezen patronen.

- ECO-boiler automatisch bij solar overschot (HTTP REST naar 192.168.0.71)
- Tesla Wallcharger solar overdag + EPEX nachtladen
- Override-knoppen per verbruiker in UI
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
| AP1 | Westdak SMA omvormer 3: type + SN noteren | Maarten | ⬜ Open |
| AP2 | S0-tellers: pulsen/kWh lezen van label (Inepro PRO380-S = 10.000 imp/kWh ✅) | Filip | ✅ Gedaan |
| AP2b | Viessmann Vitovolt 275Wp typeplaatje lezen | Filip/Maarten | ⬜ Open |
| AP2c | S0-uitgang passief of actief bevestigen | Filip | ⬜ Open |
| AP3 | ENTSO-E API token — **niet meer nodig** (EPEX via RPi /api/epex) | — | ✅ Vervallen |
| AP4 | ntfy.sh topic instellen op telefoons Filip + Maarten | Filip + Maarten | ⬜ Open (v1.27) |
| AP5 | Eagle PCB ontwerp S0 interface board | Filip | ⬜ Open |
| AP6 | EV-lader 2: merk/type en stuurbaarheid | Maarten | ⬜ Open |
| AP7 | UTP kabel trekken verdeelkast → inkomhal | Filip + Maarten | ⬜ Open |
| AP8 | SMA Speedwire testen op lokaal netwerk (fase 2) | Filip | ⬜ Open |
| AP9 | Jaarbedrag Engie FLOW invullen | Maarten | ⬜ Open |
| AP10 | CZ-TAW1 WP WON resetten + herregistreren | Filip | ⬜ Open |
| AP11 | Cloudflare Worker uitbreiden voor remote UI — **vervallen** (RPi + Tailscale) | — | ✅ Vervallen |
| AP12 | S-ENERGY v1.26 flashen en valideren | Filip | ✅ Gedaan (26/04/2026) |
| AP13 | GAS script ENERGY installeren in Google Sheets | Filip | ✅ Gedaan (26/04/2026) |
| AP14 | S0 bekabeling aansluiten → SIM_S0 uitschakelen | Filip + Maarten | ⬜ Open (na PCB) |
| AP15 | HomeWizard P1 dongle plaatsen → SIM_P1 uitschakelen | Maarten | ⬜ Open (~2028) |
| AP16 | Matter verwijderen uit HVAC, ECO en Dashboard sketches (heap besparing) | Filip | ⬜ Open |
| AP17 | Maarten + Céline uitnodigen op Tailscale | Filip | ⬜ Open |

**Afgevinkt eerder:** ESP32-C6 16MB OK · WiFi tellerkast OK · ECO-boiler OEG ~2kW OK · geen HA OK.

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
| v0.2 | April 2026 | Geïntegreerd met Zarlar Master Overnamedocument: IP 192.168.0.73, matrix, GAS-script, pinout, S0-code, JSON-keys, fasering |
| v0.3 | April 2026 | Gedetailleerd uitgewerkt plan: locatie inkomhal, PCB Eagle/JLCPCB, LED-strip, ntfy.sh, Cloudflare, EPEX ENTSO-E API, fasering 1-2-3 |
| v0.4 | April 2026 | Sectie 16: NVS-strategie, twee GAS-tabbladen Data+Verbruik, capaciteitstarief, SPIFFS dagarchief, /history pagina |
| v0.5 | April 2026 | Piekvermogens gesplitst in piek_won / piek_sch, proportionele solar verdeling per huishouden |
| v0.6 | April 2026 | Één aansluiting verduidelijkt, gecombineerde piek pt, dubbel tariefvergelijk dynamisch/vast |
| v0.7 | April 2026 | JSON-keys en GAS-kolommen geconsolideerd: §11.4 en §9.1 als enige referentie. Duplicaten verwijderd. |
| v0.9 | April 2026 | S0-schema gecorrigeerd naar Aanpak A (IEC 62053-31, directe verbinding zonder optocoupler) |
| v1.0 | April 2026 | SMA SB3.6-1AV-41 westdak specs, Viessmann 275Wp installatie volledig uitgewerkt |
| v1.1 | April 2026 | ALFASUN, WP types (WH-UX09HE5 + WH-SXC09H3E5) uit Frigro factuur, partitions exacte naam, sketch v0.0 |
| v1.2 | April 2026 | Sectie 7: energy-charts.info API, 48u rolling window, prijsklassen, localStorage cache, ESP32 /epex endpoint |
| v1.3 | April 2026 | Sectie 5: 12 LED pixels in 5 groepen, advies-pixels voor Céline/Mireille. Sectie 7: reële prijsstructuur, planningstabel, piekbeheer, SOC-simulatie, live testpagina |
| v1.4 | April 2026 | Sectie 7: 4 grafieken op zelfde tijdas, BTW op volledige som, 5 instelbare tariefcomponenten, infokaarten, cache-fix 00:00 |
| **v1.6** | April 2026 | §1 controllers bijgewerkt (v1.26 actief, Dashboard v5.8, RPi v2.0). §11.1–11.6 volledig herschreven: twee sim-vlaggen, actuele pins, LED matrix 12×4, JSON keys v1.26. §12 fasering bijgewerkt. §17 RPi gateway vervangen door §18 RPi Portal (Node.js, Tailscale). §19 HomeWizard P1 dongle toegevoegd. §20 EPEX injectieberekening toegevoegd. |
| **v1.5** | April 2026 | Sectie 17 toegevoegd: Raspberry Pi Gateway (nginx + cloudflared). |

---

## 17. Remote toegang — Tailscale (actueel, april 2026)

> ⚠️ **§17 is vervangen door de werkelijke implementatie.** De geplande nginx+cloudflared aanpak
> (EMS v1.5) is **niet gerealiseerd**. In plaats daarvan draait de RPi Node.js + Tailscale.
> De originele §17 installatie-instructies zijn bewaard onderaan dit document als historische referentie.

**Actuele situatie:**

| Component | Waarde |
| --- | --- |
| Hardware | Raspberry Pi, vaste IP `192.168.0.50` |
| Software | Node.js v18 + Express, poort 3000 |
| Remote toegang | **Tailscale** VPN (`100.123.74.113`) — werkt overal |
| Deploy | `bash ~/deploy.sh "omschrijving"` op Mac |

**Tailscale gebruikers:**

| Gebruiker | Status |
| --- | --- |
| Filip + Mireille | ✅ Verbonden (gedeeld Apple account) |
| Maarten | ⬜ Nog uit te nodigen |
| Céline | ⬜ Nog uit te nodigen |

Portal bereikbaar via: `http://100.123.74.113:3000`

---

## 18. RPi Portal — Node.js server v2.0

### 18.1 Architectuur

De RPi is een **display + controle laag** — hij meet niets zelf.
De **Dashboard controller (192.168.0.60) blijft de bron van waarheid** voor Google Sheets logging.

```
Browser (overal via Tailscale)
        ↓
RPi Node.js :3000  (/api/poll/senrg, /api/epex, /api/matrix, ...)
        ↓
ESP32 controllers (192.168.0.70–.80) + Photons (via Cloudflare Worker)
```

**Gouden regel:** de browser gebruikt NOOIT lokale IPs — alles via `/api/` op de RPi.

### 18.2 API endpoints

| Endpoint | Functie |
| --- | --- |
| `GET /api/poll/senrg` | Poll S-ENERGY /json |
| `GET /api/poll/eco` | Poll ECO /json |
| `GET /api/poll/hvac` | Poll HVAC /json |
| `GET /api/epex` | EPEX België spotprijzen (gecached) |
| `GET /api/matrix` | Alle controllers parallel (ESP32 + Photons) |
| `GET /api/photon/:id` | Photon proxy via Cloudflare Worker |
| `GET /api/settings` | Persistente instellingen laden |
| `POST /api/settings` | Instelling opslaan |
| `GET /api/status` | Status alle controllers |

### 18.3 Portal pagina's

| Pagina | URL | Status |
| --- | --- | --- |
| Portal overzicht | `/` (index.html) | ✅ Actief |
| ECO Boiler detail | `/eco.html` | ✅ Actief |
| EPEX grafiek | `/epex-grafiek.html` | ✅ Actief |
| Live matrix | `/matrix.html` | ✅ Actief |
| HVAC detail | `/hvac.html` | ⬜ Gepland |
| Afrekening WON/SCH | `/afrekening` | ⬜ Gepland |

### 18.4 S-ENERGY integratie in de portal

**index.html — S-ENERGY tegel:**
- Toont: solar W (boog), netto W (kleur), EPEX ct, sim-badge (oranje/groen)
- Linkt naar: `/matrix.html`
- Demodata bij server offline: `senrg: {a:2800, e:350, n:1820, sim_s0:1, sim_p1:1}`

**epex-grafiek.html — Injectie kaart:**
- Derde info-kaart "☀️ Injectie vandaag" met kWh + € opbrengst
- Badge toont: `S0:SIM · P1:SIM` / `S0:LIVE · P1:SIM` / `S0:LIVE · P1:LIVE`
- Data via `pollSenrg()` elke 30s — valt terug op simulatie als offline

**matrix.html — Rij 2 S-ENERGY:**
- 16 pixels conform `renderEnergyRow()` in Dashboard v5.8
- Col 12 oranje = SIM_S0 actief · col 13 oranje = SIM_P1 actief
- Auto-refresh 15s, Photon fallback voor kamers automatisch

### 18.5 Deploy workflow

```bash
# Alle bestanden naar ~/Downloads kopiëren, dan:
bash ~/deploy.sh "omschrijving van wijziging"
```

`deploy.sh` doet automatisch: git commit → push → SSH naar RPi → rsync → herstart indien server.js gewijzigd.

---

## 19. HomeWizard P1 Meter — HWE-P1-RJ12

> WON-verbruik fase 1: **niet gemeten** (analoge teller).
> De P1-dongle integratie is klaar in de sketch maar wacht op hardware (~2028).

### 19.1 Hardware

| Parameter | Waarde |
| --- | --- |
| Model | HomeWizard P1 Meter **HWE-P1-RJ12** |
| Aansluiting | RJ12 op P1-poort digitale slimme meter (WON) |
| Voeding | 5V 500mA via P1-poort (geen externe voeding) |
| WiFi | 2.4 GHz, via HomeWizard Energy app |

### 19.2 Activatie lokale API

1. Installeer de HomeWizard Energy app
2. Koppel de P1 Meter aan het WiFi-netwerk
3. Ga naar: Settings → Meters → jouw meter → **Local API: AAN**
4. Vul het IP-adres in bij S-ENERGY `/settings` → "P1 IP-adres"

### 19.3 API endpoint

```
GET http://<P1_IP>/api/v1/data
```
Plain HTTP, geen authenticatie, geen cloud vereist.
Documentatie: https://api-documentation.homewizard.com/docs/introduction/
GitHub library: https://github.com/jvandenaardweg/homewizard-energy-api

### 19.4 JSON response → S-ENERGY keys

```json
{
  "active_power_w": 997,
  "total_power_import_t1_kwh": 19055.287,
  "total_power_import_t2_kwh": 19505.815,
  "total_power_export_t1_kwh": 0.002,
  "total_power_export_t2_kwh": 0.007
}
```

| P1 JSON key | Betekenis | → S-ENERGY /json key |
| --- | --- | --- |
| `active_power_w` | Momentaan vermogen WON | `b` (W) |
| `total_power_import_t1+t2_kwh` | Dag afname (via delta midnight) | `i` (Wh) |
| `total_power_export_t1+t2_kwh` | Dag injectie (via delta midnight) | `vw` (Wh) |

> `total_power_*_kwh` zijn **cumulatieve tellers**. Dagwaarden = actuele waarde − snapshot bij midnight.

---

## 20. EPEX Injectieberekening — Ecopower

### 20.1 Principe (conform factuur Geert Van Leuven, feb. 2026)

Bij het **dynamisch Ecopower-tarief** geldt voor teruglevering aan het net:

```
Injectieprijs (ct/kWh) = EPEX spotprijs − onbalansafslag
                       = max(0, EPEX − 0,67 ct/kWh)
BTW op injectie        = 0%   (tegenover 6% op afname)
```

**Gemeten in feb. 2026:** gem. injectieprijs = 5,25 ct/kWh (EPEX gem. ~5,92 − afslag 0,67).

**Bij negatieve EPEX-prijs:** injectieprijs = 0 ct/kWh (vloer — je krijgt niets).

**Wat er NIET vergoed wordt bij injectie:**
- Afnametarief (5,23 ct/kWh) — enkel bij afname
- GSC (1,10 ct/kWh) — enkel bij afname
- WKK (0,39 ct/kWh) — enkel bij afname
- Heffingen + accijnzen (~4,94 ct/kWh) — enkel bij afname

### 20.2 Toepassing in epex-grafiek.html

De onbalansafslag is instelbaar via de Instellingen tab (standaard 0,67 ct/kWh):

```javascript
// Injectieprijs per slot (EPEX-gebaseerd, 0% BTW)
const injCt = Math.max(0, epexCt - ONBALANS_CT);

// Kost dynamisch contract per slot:
kostDyn += afname   * reelePrijs(epexCt) / 100;  // afname: all-in tarief
kostDyn -= injectie * injCt              / 100;  // injectie: EPEX − afslag
```

### 20.3 Vergelijking contracten

| | Dynamisch | Vast | Injectie |
| --- | --- | --- | --- |
| Energieprijs | EPEX spotprijs | ~12,80 ct/kWh | EPEX spotprijs |
| Afnametarief | 5,23 ct/kWh | 5,23 ct/kWh | **niet van toepassing** |
| GSC | 1,10 ct/kWh | 1,10 ct/kWh | **niet van toepassing** |
| WKK | 0,39 ct/kWh | 0,39 ct/kWh | **niet van toepassing** |
| Heffingen | 4,94 ct/kWh | 4,94 ct/kWh | **niet van toepassing** |
| BTW | 6% | 6% | **0%** |
| Onbalans | toeslag (afname) | — | **aftrek 0,67 ct/kWh** |

> 💡 Injecteren levert slechts ~5,25 ct/kWh op tegenover ~28 ct/kWh afnameprijs.
> **Zelf verbruiken of opslaan in batterij is altijd beter dan injecteren.**

---

## 21. GAS Script — ENERGY_GoogleScript_v1

### 21.1 Structuur

19 kolommen (A t/m S), logging via Dashboard controller (192.168.0.60).

| Kolom | Inhoud | Bron key |
| --- | --- | --- |
| A | Tijdstempel | — |
| B | Solar W | `a` |
| C | SCH afname W | `c` |
| D | SCH injectie W | `d` |
| E | Netto SCH W | `e` |
| F | WON W | `b` |
| G | Solar dag kWh | `h` / 1000 |
| H | SCH afname dag kWh | `j` / 1000 |
| I | SCH injectie dag kWh | `k` / 1000 |
| J | WON afname dag kWh | `i` / 1000 |
| K | WON injectie dag kWh | `vw` / 1000 |
| L | EPEX nu ct/kWh | `n` / 100 |
| M | EPEX +1u ct/kWh | `n2` / 100 |
| N | Maandpiek W | `pt` |
| O | RSSI dBm | `ac` |
| P | Heap KB | `ae` / 1024 |
| Q | SIM S0 (0/1) | `sim_s0` — **oranje header** |
| R | SIM P1 (0/1) | `sim_p1` — **oranje header** |
| S | FW versie | `ver` |

### 21.2 Functies

- `doPost(e)` — logt één meting, handelt ontbrekende keys af
- `setupHeaders()` — 2 bevroren rijen, kolomtitels, oranje sim-kolommen
- `test()` — realistische testdata v1.26
- `sendDailySummary()` — dagelijkse e-mail met **waarschuwing als sim-data aanwezig**

MAX_ROWS = 2000 (≈ 6 maanden bij 5-min interval).

---

## 22. Dashboard v5.8 — S-ENERGY integratie

### 22.1 Wijzigingen t.o.v. v5.7

| Wat | Wijziging |
| --- | --- |
| Controller tabel idx 4 | FUT1 → S-ENERGY, active=true |
| MROW rij 2 | `{-1,-1,-1}` → `{-1,-1,4}` (S-ENERGY) |
| `renderEnergyRow()` | Nieuw — 16 kolommen |
| `updateMatrix()` | `else if S-ENERGY` toegevoegd |

### 22.2 renderEnergyRow — kolomlogica

| Col | Inhoud | Kleurschaal |
| --- | --- | --- |
| 0 | Status | groen/amber/rood |
| 1–4 | Solar W / SCH af / SCH inj / Netto | gradient groen/rood/cyaan |
| 5 | WON W | amber gradient |
| 6–8 | Solar kWh / SCH af kWh / SCH inj kWh | dag cumulatieven |
| 9–10 | EPEX nu / +1u | groen(<15)/geel(<25)/rood(>35 ct) |
| 11 | Maandpiek % | groen(<60%)/amber(<85%)/rood |
| 12 | sim_s0 | **oranje=SIM / groen=LIVE** |
| 13 | sim_p1 | **oranje=SIM / groen=LIVE** |
| 14–15 | Heap / RSSI | groen/amber/rood |

> Dit document vervangt de historische §17 installatie-instructies voor nginx+cloudflared.
> Die zijn niet meer relevant voor de huidige Tailscale+Node.js aanpak.

*Energy Management System — Filip Delannoy — bijgewerkt april 2026*

De Raspberry Pi fungeert als **permanente brug** tussen het internet en alle lokale
Zarlar-controllers. Hij meet niets, stuurt niets aan — hij is uitsluitend een veilige
en betrouwbare doorgang. Dit vervangt de Cloudflare Worker aanpak uit §6 (die Worker
kon het lokale netwerk niet rechtstreeks bereiken).

```
[Buiten — overal ter wereld]        [Lokaal netwerk Zarlardinge]

Telefoon Filip                 →    Pi Gateway (192.168.0.50)
Telefoon Maarten               →      |
Browser (overal)               →    nginx reverse proxy
                                      |         |         |
                               HVAC(.70)  ECO(.71)  SENRG(.73)
                               ROOM(.80)  DASH(.60)  ...
```

De tunnel loopt altijd **outbound** van de Pi naar Cloudflare.
Geen open poorten op de Telenet-router. Geen DynDNS nodig.
Alle bestaande controllers blijven ongewijzigd — enkel de Pi heeft extra software.

### 17.2 Hardware

| Parameter | Waarde |
| --- | --- |
| Model | Raspberry Pi Zero 2W (voorkeur) of Pi 3/4 |
| OS | Raspberry Pi OS Lite 64-bit (Bookworm) — geen desktop |
| Vaste IP | 192.168.0.50 (DHCP-reservering op basis van MAC) |
| Voeding | 5V USB-C, ~1,5W (Pi Zero 2W) |
| Hostname | zarlar-gateway |
| SD-kaartje | 16GB+, te schrijven via RPi Imager op Mac (USB-C adapter) |

**Geen Homebridge, geen Node.js, geen npm.** Die complexiteit is precies waarom
een vorige Pi onverwacht kapotging. Deze Pi draait enkel nginx + cloudflared +
fail2ban + unattended-upgrades. Totaal RAM-gebruik: ~80 MB.

### 17.3 Software stack

| Component | Type | Gebruik | Onderhoud |
| --- | --- | --- | --- |
| nginx | Reverse proxy | Routeert URLs naar juiste controller | Quasi nul |
| cloudflared | Cloudflare Tunnel | Veilige outbound tunnel naar internet | ~1x/jaar update |
| systemd | Procesmanager | Auto-start + auto-restart bij crash | Automatisch |
| unattended-upgrades | OS-updates | Automatisch patchen | Nul |
| fail2ban | Bruteforce-bescherming | Blokkeert herhaalde mislukte logins | Nul |
| Python (~50 lijnen) | Health dashboard | Status alle controllers op /status | Eenmalig schrijven |

### 17.4 nginx configuratie

```nginx
# /etc/nginx/sites-available/zarlar
server {
    listen 80;
    location /hvac/      { proxy_pass http://192.168.0.70/; }
    location /eco/       { proxy_pass http://192.168.0.71/; }
    location /senrg/     { proxy_pass http://192.168.0.73/; }
    location /room/      { proxy_pass http://192.168.0.80/; }
    location /dashboard/ { proxy_pass http://192.168.0.60/; }
}
```

Één bestand. Eén keer instellen. Nieuwe controller = één extra regel + `sudo nginx -s reload`.

### 17.5 Cloudflare Tunnel configuratie

```yaml
# /etc/cloudflared/config.yml
tunnel: zarlar-home
credentials-file: /home/pi/.cloudflared/<tunnel-id>.json
ingress:
  - hostname: controllers.zarlardinge.be
    service: http://localhost:80
  - service: http_status:404
```

Cloudflare genereert eenmalig een credentials-bestand bij `cloudflared tunnel create`.
Daarna opent de Pi zelf de verbinding naar buiten — geen router-configuratie nodig.

### 17.6 URL-structuur na activatie

```
https://controllers.zarlardinge.be/hvac/        → HVAC controller
https://controllers.zarlardinge.be/eco/         → ECO Boiler
https://controllers.zarlardinge.be/senrg/       → Smart Energy
https://controllers.zarlardinge.be/room/        → Room Eetplaats
https://controllers.zarlardinge.be/dashboard/   → Zarlar Dashboard
https://controllers.zarlardinge.be/status       → Health overzicht alle controllers
https://controllers.zarlardinge.be/eco/update   → OTA firmware van overal
```

### 17.7 Beveiliging via Cloudflare Access (gratis)

| Endpoint | Toegang |
| --- | --- |
| `/*/` — dashboards | Vrij voor Filip + Maarten |
| `/*/settings` | Vereist login (Google/GitHub) |
| `/*/update` (OTA) | Enkel Filip |
| `/status` | Vrij voor Filip + Maarten |

### 17.8 Installatie stap voor stap

**Stap 1 — SD-kaartje (Mac + RPi Imager):**
```
OS: Raspberry Pi OS Lite 64-bit (Bookworm)
Geavanceerde instellingen:
  Hostname:  zarlar-gateway
  SSH:       inschakelen + publieke sleutel van Mac
  WiFi:      SSID + wachtwoord Zarlardinge
  Locale:    Belgium / nl_BE / Europe/Brussels
```

**Stap 2 — Basisbeveiliging:**
```bash
ssh pi@zarlar-gateway.local
sudo apt update && sudo apt upgrade -y
sudo apt install -y unattended-upgrades fail2ban ufw
sudo ufw allow ssh && sudo ufw allow 80 && sudo ufw enable
```

**Stap 3 — nginx:**
```bash
sudo apt install -y nginx
# Config schrijven (zie 17.4)
sudo nginx -t && sudo systemctl enable --now nginx
```

**Stap 4 — cloudflared:**
```bash
curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-arm64.deb -o cloudflared.deb
sudo dpkg -i cloudflared.deb
cloudflared tunnel login
cloudflared tunnel create zarlar-home
# Config schrijven (zie 17.5)
sudo cloudflared service install
sudo systemctl enable --now cloudflared
```

### 17.9 Onderhoud

| Taak | Frequentie | Actie |
| --- | --- | --- |
| Beveiligingsupdates OS | Automatisch | Nul |
| cloudflared update | ~1x/jaar | `sudo apt upgrade cloudflared` |
| nginx uitbreiden | Bij nieuwe controller | 1 regel + `sudo nginx -s reload` |
| Logs bekijken | Bij probleem | `journalctl -u cloudflared -u nginx` |
| Reboot na kernel update | ~maandelijks | `sudo reboot` (30 sec downtime) |

### 17.10 Openstaande actiepunten

> ✅ **Alle AP12–AP15 zijn afgewerkt en vervangen door een betere aanpak.**
> De nginx+cloudflared aanpak is nooit gerealiseerd — vervangen door **Node.js + Tailscale** (zie §18).
> AP12–AP15 zijn verwijderd uit de actiepuntenlijst in §14.

> Dit hoofdstuk (§17) blijft bewaard als historische referentie voor de oorspronkelijke aanpak.
