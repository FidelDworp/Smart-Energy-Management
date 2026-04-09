# Energy Management System — Zarlardinge
## Technisch referentiedocument voor continuïteit van gesprekken

*Locatie: Zarlardinge, Vlaanderen, België*
*Auteur: Filip Dworp (Papa / FiDel)*
*Laatste update: April 2026*

---

## 1. Context & doelstelling

Dit document beschrijft de volledige hardware en software van het Smart Energy Management systeem voor de gedeelde energieinstallatie op Zarlardinge. Het dient als **overnamedocument** bij het starten van een nieuwe gespreksessie.

Het systeem beheert de zonne-energieopbrengst van twee huishoudens:
- **WON** — Woning: Maarten + Céline + kinderen (hoofdgebouw)
- **SCH** — Schuur: Filip (Papa) + Mireille (verbouwde schuur)

De tellerkast bevindt zich bij Maarten in de keuken. WiFi is beschikbaar aan de kast.

---

## 2. Bestaande installatie

### 2.1 Zonne-installatie

| Component | Details |
| --- | --- |
| Gemiddelde jaaropbrengst | ~11.950 kWh/jaar |
| Omvormer 1 & 2 (zuiddak) | SMA Sunny Boy SB3600TL-21 |
| Serienummers | SN 2130419465 & SN 2130419851 |
| Instellingscode | BES204 \| Norm C10/11:2012 |
| Communicatie omvormers | Speedwire/Webconnect ingebouwd — geen extra hardware nodig |
| RID-codes (lokaal netwerk) | RHCD4F & QX6NUA |
| Installateur / jaar | Alfisun, 2017 |
| Omvormer 3 (westdak) | SMA Sunny Boy — type & SN nog te noteren (actiepunt #1) |
| Solar S0-teller | Digitale teller met S0-pulsuitgang, in tellerkast bij Maarten |

### 2.2 Netmeters & tellers

| Component | Details |
| --- | --- |
| Fluvius digitale meter | **NEE** — wettelijke uitzondering tot eind 2028 |
| Huidig type | Terugdraaiende teller — injectie vergoed aan aankoopprijs |
| Overgang | Digitale meter verplicht in 2028. Injektietarief daalt naar ~€0,05/kWh |
| S0-tellers aanwezig | Solar opbrengst, Verbruik WON, Verbruik SCH (+ evt. meer) |
| Pulsen per kWh | Nog te lezen van label — typisch 1000 imp/kWh (actiepunt #2) |

### 2.3 Stuurbare verbruikers

| Verbruiker | Hardware | Stuurbaarheid |
| --- | --- | --- |
| ECO-boiler (SCH) | OEG dompelweerstand + thermostaat, ~2 kW | Remote via bestaand relais vanuit tellerkast SCH |
| EV-lader 1 (WON) | Tesla Wallcharger Gen 3 | Lokale REST API — HTTP PUT (onofficieel, zie §4.3) |
| EV-lader 2 | Merk/type nog te bepalen (actiepunt #6) | Stuurbaarheid nog te bepalen |
| Thuisbatterij | Nog aan te kopen (~2028), Huawei/BYD voorkeur | Modbus TCP |
| **Regenwaterpomp** | Op WON-teller, ~500 kWh/jaar | **NOOIT STUREN** — essentieel voor al het sanitair |

### 2.4 Warmtepompen (Panasonic)

| | WP SCH (Schuur) | WP WON (Woning) |
| --- | --- | --- |
| In werking sinds | Januari 2019 | November 2019 |
| Internetmodule | CZ-TAW1 — werkend | CZ-TAW1 — geïnstalleerd nov. 2023 |
| Comfort Cloud toegang | ✅ Filip als eigenaar | ❌ Geblokkeerd (zie hieronder) |
| Compressoruren | ~15.231 uur (t.e.m. maart 2026) | Niet geregistreerd |

**Probleem WP WON:** De module werd geregistreerd onder het iTroniX bedrijfsadres (Filip, bedrijf gestopt 2022). Dat e-mailadres bestaat niet meer. Maarten kan niet activeren zonder eigenaargoedkeuring.
**Oplossing:** CZ-TAW1 resetten via paperclip → opnieuw registreren met privé-mailadres Filip → Maarten toevoegen als gebruiker. **(actiepunt #10)**

---

## 3. Hardware van het EMS

### 3.1 ESP32-C6 "Smart Energy" controller *(nog te bouwen)*

| Parameter | Waarde |
| --- | --- |
| Module | ESP32-C6 DevModule |
| Flash | 16 MB |
| Locatie | Tellerkast keuken SCH |
| Vaste IP | 192.168.0.XX (nog in te stellen) |
| Partitietabel | 16MB custom: `app0/app1` OTA + SPIFFS |
| IDE | Arduino IDE (ESP32 Arduino core) |

**Taak:** verzamelt alle S0-pulsdata, berekent solar-overschot in real time, toont dashboard, stuurt aanvragen naar verbruikers.

### 3.2 ESP32-C6 "ECO-boiler controller" *(bestaand, v1.23)*

| Parameter | Waarde |
| --- | --- |
| Module | ESP32-C6 DevModule, 16 MB flash |
| Locatie | Bij de ECO-boiler (schuur) |
| Vaste IP | 192.168.0.71 |
| Firmware versie | v1.23 (22 maart 2026) |
| Auteur | Filip Dworp |

**Taak:** ontvangt schakelcommando's van Smart Energy via HTTP REST. Beheert OEG dompelweerstand (~2 kW). Zelfde UI-stijl als Smart Energy.

### 3.3 Zarlar Dashboard

| Parameter | Waarde |
| --- | --- |
| IP-adres | 192.168.0.60 |
| Taak | Ontvangt /json data van alle controllers → doorstuurt naar Google Sheets |
| Integratie | Alle ESP32-controllers in het systeem loggen hiernaartoe |

---

## 4. Software & communicatie

### 4.1 Arduino IDE — bibliotheken en configuratie

| Bibliotheek / instelling | Gebruik |
| --- | --- |
| `ESPAsyncWebServer` | Webserver, chunked streaming, alle HTTP endpoints |
| `Preferences` | NVS opslag voor instellingen en crash-log |
| `SPIFFS` | debug.log + debug.log.old (rotatie bij >800 KB) |
| `HTTPClient` | Uitgaande HTTP calls naar andere controllers |
| `Adafruit_MAX31865` | PT100/PT1000 temperatuursensor (ECO-boiler controller) |
| `OneWireNg` | DS18B20 temperatuursensoren |
| `arduino-esp32-Matter` | Matter endpoints (temperatuur, schakelaar) |
| `esp_wifi` / `esp_pm` | WiFi power management |
| Board instelling | ESP32C6 Dev Module — Flash 16MB — Custom partitietabel |
| Seriële fix (verplicht!) | `#define Serial Serial0` bovenaan sketch |

### 4.2 HTTP REST API — communicatieprotocol

Alle controllers communiceren via **HTTP REST op het lokale WiFi-netwerk**. Geen MQTT, geen cloud, geen Home Assistant.

```
Smart Energy (192.168.0.XX)
    → HTTP GET/POST → ECO-boiler controller (192.168.0.71)
    → HTTP PUT      → Tesla Wallcharger (lokaal IP, onofficieel)
    → Modbus TCP    → Thuisbatterij (toekomst)
    → /json output  → Zarlar Dashboard (192.168.0.60) → Google Sheets
```

### 4.3 Tesla Wallcharger Gen 3 — lokale API

| Endpoint | Methode | Gebruik |
| --- | --- | --- |
| `http://[IP]/api/1/vitals` | GET | Real-time laadstroom, spanning, energie |
| `http://[IP]/api/1/status` | PUT | Laden in-/uitschakelen of beperken |

- **Solar overdag:** inschakelen bij solar-overschot > drempel (instelbaar)
- **Nachtladen:** laden bij lage/negatieve EPEX day-ahead prijs
- **Override:** knop in UI om solar/day-ahead logica te overrulen
- ⚠️ Onofficiële API — kan veranderen bij Tesla firmware-update

### 4.4 SMA Omvormer — communicatie (fase 2)

| Methode | Status |
| --- | --- |
| S0-pulsen | ✅ Fase 1 — voldoende voor alle sturing |
| Speedwire/Webconnect | Ingebouwd in beide SB3600TL-21 — geen extra hardware nodig |
| Modbus TCP | Beschikbaar via Speedwire-interface |
| RID-codes | RHCD4F (SN 2130419465) & QX6NUA (SN 2130419851) |

SMA-koppeling enkel zinvol in fase 2 voor extra detail (rendement per string, per omvormer). Niet nodig voor de basissturing.

### 4.5 EPEX SPOT day-ahead prijssturing

- ENTSO-E publiceert dagelijks om ~13h de kwartuurprijzen voor de volgende dag via gratis publieke API
- Ophalen via HTTP GET op de ESP32-C6 of via lokale proxy server
- **Sturingslogica:**
  - Negatieve prijs (≤ €0,00): batterij maximaal laden + ECO-boiler inschakelen
  - Goedkope kwartieren (01h–05h): batterij laden aan netprijs
  - Piekprijs (> €0,20/kWh, 07h–09h & 17h–20h): batterij ontladen, grote verbruikers uit
  - Solar-overschot heeft altijd prioriteit boven day-ahead sturing
- Vereist dynamisch energiecontract (marktprijs per kwartier)

---

## 5. UI-architectuur (ECO-boiler stijlgids — ook voor Smart Energy)

Gebaseerd op ECO-boiler controller v1.23. Beide controllers volgen dezelfde stijl.

| Element | Waarde |
| --- | --- |
| Technologie | ESPAsyncWebServer, chunked streaming |
| Header | Geel `#ffcc00`, tekst zwart, bold |
| Sidebar | Marineblauw `#336699`, witte links, 60px breed |
| Groepstitels | `#336699` achtergrond, cursief-vet, witte tekst |
| Tabel labels | Kleur `#369` |
| Statusbadges | Groen `#0a0` (aan) / grijs `#999` (uit) |
| Voortgangsbalkjes | Voor vermogens en energiewaarden |

### Pagina-structuur

| URL | Inhoud |
| --- | --- |
| `/` | Hoofddashboard: real-time solar, verbruik WON/SCH, overschot, relaisstatus |
| `/charts` | Historische grafieken (Chart.js) — aparte pagina (geheugen) |
| `/settings` | Drempelwaarden, day-ahead aan/uit, laadbegrenzingen |
| `/json` | Compact JSON (keys a, b, c...) voor Zarlar Dashboard & andere controllers |
| `/update` | OTA firmware-update |

### JSON endpoint `/json` — sleutelstructuur (voorbeeld ECO-boiler)

```json
{ "a": 72.3, "b": 45.1, "c": 1, "d": 0, "e": 38920, ... }
```
Compacte keys (a, b, c...) voor minimale payload. Zarlar Dashboard (192.168.0.60) verzorgt de logging naar Google Sheets.

### SIMULATION_MODE

Vlag in de sketch: genereert nepsensordata voor ontwikkeling zonder hardware. Rode banner in UI. Vereist `HUGE APP 3MB No OTA` partitie.

---

## 6. Sturingslogica — prioriteitsvolgorde

```
1. ECO-boiler (~2 kW)        → altijd beschikbaar, hoogste prioriteit
2. EV-lader 1 Tesla           → solar overdag + day-ahead 's nachts + override
3. EV-lader 2 (t.b.d.)        → zelfde logica als lader 1 eens type bekend
4. Thuisbatterij Huawei/BYD   → laden bij overschot of negatieve day-ahead prijs
                                  ontladen bij piektarief
⛔ Regenwaterpomp              → NOOIT STUREN (essentieel sanitair)
```

**Solar-overschot berekening:**
```
Overschot = Solar productie − (Verbruik WON + Verbruik SCH)
```
Bij overschot > instelbare drempel gedurende instelbare tijd → verbruikers inschakelen.

---

## 7. S0-pulssturing — technische details

| Parameter | Waarde |
| --- | --- |
| Principe | 1 puls = vaste hoeveelheid energie (bv. 1 Wh bij 1000 imp/kWh) |
| Aansluiting | Pull-up weerstand + optocoupler (galvanische scheiding aanbevolen) |
| Tellers aanwezig | Solar, Verbruik WON, Verbruik SCH, ECO-boiler (+ evt. meer) |
| Pulsen/kWh | Staat op label van elke teller — nog te lezen (actiepunt #2) |

---

## 8. Systeemoverzicht

```
[Solar panelen]
      |
[SMA omvormers SB3600TL-21 x2 + westdak x1]
  Speedwire/Webconnect ingebouwd (fase 2)
      |
[S0-pulsen] ──────────────────────────────────────────┐
                                                       ↓
                                        [ESP32-C6 "Smart Energy"]
                                          IP: 192.168.0.XX
                                          Tellerkast keuken SCH
                                               |
                    ┌──────────────────────────┼──────────────────────┐
                    ↓                          ↓                      ↓
        [ECO-boiler controller]    [Tesla Wallcharger]    [Thuisbatterij]
          IP: 192.168.0.71           Lokale REST API       Modbus TCP
          HTTP REST                  onofficieel           Huawei/BYD
                    |
                    └──────────→ [Zarlar Dashboard 192.168.0.60]
                                          ↓
                                   [Google Sheets]
```

---

## 9. Openstaande actiepunten

| # | Actie | Door wie | Status |
| --- | --- | --- | --- |
| 1 | Typeplaatje westdak SMA omvormer 3: type + SN noteren | Maarten | Open |
| 2 | S0-tellers: pulsen/kWh lezen van label | Filip | Open |
| 6 | EV-lader 2: merk/type noteren, stuurbaarheid nagaan | Maarten | Open |
| 8 | SMA Speedwire testen op lokaal netwerk (fase 2) | Filip | Open |
| 9 | Jaarbedrag Engie FLOW invullen zodra factuur ontvangen | Maarten | Open |
| 10 | CZ-TAW1 WP WON resetten (paperclip) + herregistreren op privé-mail Filip | Filip | Open |

*Afgevinkt: #3 ESP32-C6 16MB ✅ | #4 WiFi tellerkast ✅ | #5 ECO-boiler OEG ~2kW ✅ | #7 geen HA ✅*

---

## 10. Nog te ontwikkelen — fasering

| Fase | Inhoud | Status |
| --- | --- | --- |
| **Fase 1** | ESP32-C6 Smart Energy bouwen: S0-pulsen lezen, solar-overschot berekenen, ECO-boiler sturen via HTTP REST | 🔴 Nog te starten |
| **Fase 1** | Tesla Wallcharger integreren: solar-sturing overdag + override UI | 🔴 Nog te starten |
| **Fase 1** | Dashboard + /json logging naar Zarlar Dashboard | 🔴 Nog te starten |
| **Fase 2** | SMA Speedwire/Modbus TCP integreren voor detaildata | ⏳ Na fase 1 |
| **Fase 2** | EPEX day-ahead integratie voor nachtladen EV + batterijsturing | ⏳ Na batterij |
| **~2028** | Thuisbatterij aankopen (10 kWh, Huawei/BYD voorkeur) | ⏳ Bij digitale meter |

---

## 11. Energiedata — referentiewaarden 2025

| Parameter | Waarde |
| --- | --- |
| Solar productie | 10.763 kWh/jaar |
| Totaal verbruik | 18.762 kWh/jaar |
| Netto netaankoop | 7.999 kWh/jaar (terugdraaiende teller) |
| Bruto injectie | ~6.996 kWh/jaar |
| Bruto aankoop | ~14.995 kWh/jaar |
| Zelfverbruiksgraad | ~35% |
| Jaarfactuur | €2.643 (incl. prosumententarief €556) |
| Prosumententarief | €556/jaar — verdwijnt bij digitale meter (2028) |

---

## 12. Versiegeschiedenis document

| Versie | Datum | Wijziging |
| --- | --- | --- |
| v0.1 | April 2026 | Initieel document aangemaakt op basis van projectdocument v0.7 |
