# Smart Energy Management

**Gedeelde Energie Zarlardinge — Projectdocument**

*Versie 0.7 — April 2026*


---


## 1. Betrokkenen

| Locatie | Zarlardinge (Vlaanderen, België) |
| --- | --- |
| WONing (WON) | Maarten + Céline + dochters Hanne, Paulien & Lena |
| SCHuur (SCH) | Papa + Mireille (verbouwde schuur naast de woning) |
| Meetkast locatie | Bij Maarten in de keuken |


## 2. Zonne-installatie

| Gemiddelde jaaropbrengst | ~11.950 kWh/jaar |
| --- | --- |
| Omvormer 1 & 2 (Zuid) | 2× SMA Sunny Boy SB3600TL-21 \| SN: 2130419465 & 2130419851 \| BES204 \| C10/11:2012 \| Speedwire/Webconnect ingebouwd \| RID: RHCD4F & QX6NUA \| Geïnstalleerd 2017 (Alfisun) |
| Omvormer 3 (West) | SMA Sunny Boy — type NOG OP TE ZOEKEN (westdak) |
| SMA communicatie | Speedwire/Webconnect ingebouwd in beide zuidomvormers. Modbus TCP of Speedwire beschikbaar op het lokale netwerk. Geen extra hardware nodig. |
| Solar kWh-teller | Digitale teller met S0-pulsuitgang — in kast bij Maarten |


## 3. Meters & Tellers

| Digitale Fluvius meter | NEE — uitzondering tot 2028 (oude nachttariefteller aanwezig) |
| --- | --- |
| Type teller | Terugdraaiende teller (wettelijk toegestaan tot 2028) |
| S0-tellers aanwezig | Solar opbrengst, Verbruik WON, Verbruik SCH (+ evt. meer) |
| Pulsen per kWh | NOG OP TE ZOEKEN — staat op het label (typisch 1000 imp/kWh) |


## 4. Stuurbare Verbruikers

| ECO-boiler (op SCH-teller) | OEG dompelweerstand met thermostaat \| Vermogen: ~2 kW \| Verbruik ~1.250 kWh/jaar \| Remote in/uitschakelbaar vanuit tellerkast SCH via bestaand relais |
| --- | --- |
| EV-lader 1 | Tesla Wallcharger Gen 3 \| Lokale REST API op thuisnetwerk \| Lading sturen via HTTP PUT (onofficiële API) |
| EV-lader 2 | Merk/type NOG TE BEPALEN (zie actiepunt #6) \| Stuurbaarheid nog te bepalen |


### 4B. Warmtepompen & Panasonic Comfort Cloud

Er zijn twee Panasonic warmtepompen aanwezig, beide geïnstalleerd door Filip (iTroniX). Beide zijn uitgerust met een internetmodule (CZ-TAW1) voor bediening via de Panasonic Comfort Cloud app.

| Meldingen | Optioneel: pushmelding bij anomalieen (groot overschot/tekort) |
| --- | --- |


## 5. Hardware & Netwerk

| Microcontroller | ESP32-C6 DevModule, 16 Mb flash — bevestigd |
| --- | --- |
| WiFi bij tellerkast | JA — bevestigd |
| Home automation | NEE — standalone ESP32-C6, geen Home Assistant gewenst |
| Vaste IP-adressen | Smart Energy: 192.168.0.XX (in te stellen). ECO-boiler: 192.168.0.71. Zarlar Dashboard: 192.168.0.60 |


## 6. Doelstellingen van het Systeem

Prioriteitsvolgorde (instelbaar):

- 1. ECO-boiler (hoogste prioriteit — direct nut, altijd beschikbaar)
- 2. EV-lader 1 — Tesla Wallcharger (lokale REST API, solar-sturing overdag; 's nachts optionele goedkope stroom via day-ahead; override-knop in UI)
- 3. EV-lader 2 (merk/type nog te bepalen)
- 4. Thuisbatterij (Huawei/BYD, Modbus TCP) — laden bij solar-overschot of negatieve day-ahead prijs; ontladen bij piektarief

## 7. Technische Aanpak


### 7A. Meting via S0-pulsen (kern van het systeem)

| Principe | Elke puls van de S0-teller = vaste hoeveelheid energie (bv. 1 Wh bij 1000 imp/kWh) |
| --- | --- |
| ESP32 telt pulsen | Real-time berekening van vermogen en energie per teller |
| Solar overschot | = Solar productie — (Verbruik WON + Verbruik SCH) |
| Drempellogica | Bij overschot: grote verbruikers inschakelen \| Bij tekort: uitschakelen |
| S0 aansluiting | Pull-up weerstand + optocoupler aanbevolen (galvanische scheiding) |


### 7B. SMA Omvormer (optioneel / aanvullend)

| Speedwire (Ethernet) | Beide zuidomvormers (SB3600TL-21) hebben Speedwire/Webconnect ingebouwd. RID: RHCD4F & QX6NUA. Geen extra hardware nodig. |
| --- | --- |
| Modbus TCP | Modbus TCP via Speedwire-interface, leesbaar met ESP32-C6. |
| Lokale webinterface | HTML scraping mogelijk als fallback |
| S0-teller (backup) | Fase 1: S0-pulsen volstaan. SMA-koppeling toevoegen in fase 2 indien nuttig. Hardware is er al. |


### 7C. Software, Dashboard & Logging

| Dashboard | Webpagina op ESP32-C6 (ESPAsyncWebServer), zelfde stijl als ECO-boiler controller. /json endpoint voor externe logging. |
| --- | --- |
| Google Sheets logging | Via /json naar Zarlar Dashboard (192.168.0.60) → Google Sheets. Zelfde werkwijze als alle controllers. |
| Meldingen | Optioneel: pushmelding bij grote overschotten of tekorten. |
| Meldingen | Optioneel: pushmelding bij anomalieen (groot overschot/tekort) |


### 7D. Sturing van verbruikers

De ESP32 berekent continu het solar-overschot. Wanneer dit overschot gedurende een instelbare tijd een drempelwaarde overschrijdt, worden verbruikers ingeschakeld via relaismodules:

- ECO-boiler (OEG ~2 kW): opwarmen bij solar-overschot. Remote schakelbaar via relais vanuit tellerkast.
- EV-lader 1 (Tesla Wallcharger): solar overdag via REST API. Day-ahead 's nachts. Override-knop in UI.
- Regenwater pomp: NIET opgenomen in de automatische sturing. De pomp is essentiël voor sanitair gebruik (toilet, wasmachine, afwas) — het huis is voor 95% afhankelijk van regenwater. Onderbreken bij solar-sturing is onverantwoord.
Prioriteitsvolgorde (instelbaar):

- 1. ECO-boiler (hoogste prioriteit — direct nut, altijd beschikbaar)
- 2. EV-lader 1 — Tesla Wallcharger (solar overdag, day-ahead 's nachts, override-knop)
- 3. EV-lader 2 (merk/type nog te bepalen)
- 4. Thuisbatterij (Huawei/BYD, Modbus TCP) — laden bij overschot of negatieve day-ahead prijs

## 8. Historisch Verbruik (ter referentie)

| Jaar | Solar (kWh) | Verbruik (kWh) | Netto (kWh) | Jaarfactuur | € SCH | € WON | Opmerking |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2022 | 11.046 | 12.760 | +1.714 | €850 | €425 | €425 |  |
| 2023 | 12.439 | 10.704 | -1.735 | €604 | €302 | €302 |  |
| 2024 | 11.191 | 13.527 | +2.336 | €1.171 | €798 | €373 | Eerste jaar met 2 EV's |
| 2025 | 10.763 | 18.762 | +7.999 | €2.643 | €1.543 | €1.100 | Sterk gestegen verbruik EV's + WP (+20%) |
| 2026 | 10.906 | 16.411 | +5.505 | INVULLEN | ? | ? | Derde jaar met 2 EV's |

*Opmerking: het sterk gestegen verbruik in 2025 is te wijten aan de toevoeging van de tweede EV en een warmtepomp. De netto teruglevering neemt toe, maar door de terugdraaiende teller is dit voorlopig geen financieel nadeel.*


## 9. Principes voor Kostenverdeling WON / SCH

- 1. Er wordt maximaal gebruik gemaakt van de SOLAR energie. Deze wordt gelijk verdeeld tussen WON en SCH.
- 2. Extra verbruik komt uit het net. Hoe meer er verbruikt wordt, hoe lager de prijs per kWh.
- 3. Vaste kosten voor gebruik van het net worden gelijk verdeeld (= Prosumententarief: Jaarbedrag €/kW x 9,86 kW).
- 4. De betaalde facturen voor netverbruik (min prosumententarief) worden verdeeld in verhouding met % WON en % SCH zoals berekend in de tabel.
- 5. De ECO-boiler zit op de SCH-teller: het verbruik wordt verdeeld tussen WON & SCH. (Huidig verbruik ~1.250 kWh/jaar is te hoog!)
- 6. De regenwaterpomp (~500 kWh/jaar) zit op de WON-teller en wordt meegenomen in de kostenverdeling WON/SCH. De pomp is essentieel voor het volledige sanitaire gebruik en wordt NIET gestuurd.
*Het prosumententarief is jaarlijks te controleren. Bij iedere jaarafrekening in maart kan reeds het prosumententarief voor het volgende jaar worden genoteerd.*

Referentie: https://www.vlaamsenutsregulator.be (zoek: vereenvoudigde tarieflijsten elektriciteit 2025)


## 10. Energieleverancier & Contract

| Huidig contract | Engie FLOW — gestart 07/05/2025 \| Werkelijke kost/kWh (variabel, excl. prosumententarief): 2022: €0,07 \| 2023: €0,06 \| 2024: €0,34 (eerste jaar 2 EV's) \| 2025: €0,26 |
| --- | --- |
| Contractduur | 2 jaar, maar opzegbaar na jaar 1 |
| Korting jaar 1 | ~6 cent/kWh korting in jaar 1 (enkel marge op de variabele prijs, geen effect op nettarieven of prosumententarief) |
| Herziening gepland | Maart/April 2026 — overstap naar Total Energies (My Confort) overwegen |
| Tip | Steeds overstappen zo dicht mogelijk bij de Fluvius meteropname (maart) |


## 11. Systeemarchitectuur & Toekomstige Uitbreidingen


### 11A. Communicatiearchitectuur ESP32-controllers

Het Smart Energy systeem bestaat uit twee ESP32-C6 controllers die draadloos samenwerken via het lokale IP-netwerk:

| ESP32-C6 "Smart Energy" | In de tellerkast (keuken SCH). Verzamelt alle S0-pulsdata, berekent solar-overschot in real time, toont dashboard-UI, stuurt aanvragen naar stuurbare verbruikers. |
| --- | --- |
| ESP32-C6 "ECO-boiler controller" | Bij de ECO-boiler (OEG dompelweerstand ~2 kW). Ontvangt schakelcommandos via lokaal IP van de Smart Energy controller. Zelfde UI-stijl als Smart Energy. |
| Communicatieprotocol | Lokaal WiFi-netwerk. HTTP REST (ESPAsyncWebServer) — identiek aan de ECO-boiler controller. Geen MQTT, geen cloud. Controllers communiceren via directe HTTP GET/POST calls op vaste IP-adressen. |
| Prioriteitssturing | Smart Energy bepaalt wanneer overschot groot genoeg is en stuurt schakelverzoek naar ECO-boiler controller (en later naar andere verbruikers). |
| Google Sheets logging | Via /json endpoint naar Zarlar Dashboard (192.168.0.60). Zarlar Dashboard verzorgt de logging naar Google Sheets. Identiek aan alle andere controllers in het systeem. |
| Vaste IP-adressen | Smart Energy: 192.168.0.XX (nog in te stellen). ECO-boiler: 192.168.0.71. Zarlar Dashboard: 192.168.0.60. |


### 11B. Tesla Wall Connector — lokale API

De Tesla Wall Connector Gen 3 biedt een lokale (onofficiële) REST API op het thuisnetwerk, zonder Tesla-account of cloud:

| Endpoint data | http://[IP-lader]/api/1/vitals — real-time laadstroom, spanning, energie |
| --- | --- |
| Solar sturing (overdag) | Bij solar-overschot: laden inschakelen via PUT-request. Bij tekort: laden beperken of stoppen. De lader laadt enkel bij voldoende overschot (instelbare drempel). |
| Voordeel | Volledig lokaal, geen internetverbinding nodig, direct stuurbaar vanuit ESP32 of lokale server |
| Risico | Onofficiële API — kan veranderen bij Tesla firmware-updates. Regelmatig testen aanbevolen. |
| Nachtladen (day-ahead) | 's Nachts laden bij lage/negatieve day-ahead prijs (EPEX). De Smart Energy controller plant de laadtijden op basis van de goedkopste kwartieren van de volgende dag. |
| Override (UI) | Override-knop in het dashboard: forceer laden of blokkeer laden ongeacht solar/day-ahead logica. Status zichtbaar in UI en /json output. |
| Lader 2 | Merk/type nog te bepalen (zie actiepunt #6). Zelfde sturing als lader 1 eens type bekend. |


### 11C. Thuisbatterij — Gedetailleerde Berekening & Advies

*Deze analyse is gebaseerd op de werkelijke meetdata van Zarlardinge (2016–2026) en berekent of een thuisbatterij financieel zinvol is, welke capaciteit optimaal is, en wanneer de beste aankooptijd is.*


### 11C-1. Sleutelassumpties voor de berekening

| Gebruikte data | Werkelijke meetdata 534 wekelijkse opnames (okt 2016 – dec 2026). Jaarwaarden 2025 als referentiejaar. |
| --- | --- |
| Solar productie 2025 | 10.763 kWh/jaar (gemiddeld over 10 jaar: 11.693 kWh/jaar) |
| Totaal verbruik 2025 | 18.762 kWh/jaar (sterk gestegen door 2 EV's + warmtepompen) |
| Netto netverbruik 2025 | 7.999 kWh/jaar (terugdraaiende teller: consumptie minus injectie) |
| Zelfverbruiksgraad solar | ~35% direct verbruikt (EV's laden 's nachts, WP dag+nacht). Rest = injectie. |
| Bruto injectie (schatting) | ~6.996 kWh/jaar naar het net gestuurd. Bruto aankoop: ~14.995 kWh/jaar. |
| Aankoopprijs na 2028 | €0,28/kWh (excl. vaste nettarieven — marktschatting) |
| Injektietarief na 2028 | €0,05/kWh vergoed bij teruglevering (typisch prosumententarief na 2028) |
| Batterijrendement | 92% round-trip (laden + ontladen verlies = 8%) |
| Prosumententarief | €556/jaar verschuldigd bij terugdraaiende teller. Verdwijnt volledig bij digitale meter (2028). Wordt als besparing meegeteld in alle scenario's na 2028 (zie simulatietabel). |


### 11C-2. Maandelijkse energiebalans (referentiejaar 2025)

*Groen = maandelijks netto overschot solar vs. verbruik. Oranje = tekort. De bruto injectie (laatste kolom) is de effectief naar het net gestuurde energie, ook in maanden zonder netto overschot (zonnige dagen temidden van donkere maand).*

| Maand | Solar (kWh) | Verbruik (kWh) | Overschot (kWh) | Tekort (kWh) | Zelfverbruik ~35% | Bruto injectie |
| --- | --- | --- | --- | --- | --- | --- |
| Jan | 285 | 2,486 | — | 2,201 | 100 | 185 |
| Feb | 489 | 2,369 | — | 1,879 | 171 | 318 |
| Mar | 865 | 2,447 | — | 1,582 | 303 | 562 |
| Apr | 1,206 | 1,391 | — | 185 | 422 | 784 |
| Mei | 1,487 | 1,313 | 175 | — | 520 | 967 |
| Jun | 1,423 | 1,064 | 358 | — | 498 | 925 |
| Jul | 1,445 | 825 | 620 | — | 506 | 939 |
| Aug | 1,202 | 835 | 367 | — | 421 | 781 |
| Sep | 1,003 | 990 | 12 | — | 351 | 652 |
| Okt | 702 | 1,163 | — | 461 | 246 | 456 |
| Nov | 409 | 2,004 | — | 1,595 | 143 | 266 |
| Dec | 247 | 1,876 | — | 1,628 | 86 | 161 |
| TOTAAL | 10,763 | 18,762 | 1,532 | 9,531 | 3,767 | 6,996 |

*Opmerking: de bruto injectie van 6.996 kWh/jaar is de realistische basis voor de batterijberekening — niet de netto 7.999 kWh van de terugdraaiende teller. Een batterij verschuift de bruto injectie, niet het netto saldo.*


### 11C-3. De terugdraaiende teller: waarom nu geen batterij

Dit is de meest kritieke factor voor de beslissing:

| Hoe werkt het nu? | De teller draait TERUG bij injectie aan het net. Elke geïnjecteerde kWh wordt gecompenseerd aan de VOLLEDIGE aankoopprijs (€0,28/kWh). Het net fungeert dus als een gratis batterij. |
| --- | --- |
| Financiëel equivalent | 6.996 kWh × (€0,28 − €0,05) = €1.609/jaar "gratis" voordeel dankzij de terugdraaiende teller. |
| Batterij vóór 2028? | Een batterij van €7.500 zou dezelfde 6.996 kWh kunnen verschuiven — maar dat is al gratis via de teller. Netto winst vóór 2028: €0/jaar. Terugverdientijd: oneindig. |
| Na 2028: digitale meter | Injectie vergoed aan €0,05/kWh, aankoop €0,28/kWh. Verlies terugdraaiende teller: €1.609/jaar. Maar: prosumententarief verdwijnt (€556/jaar besparing). Netto verslechtering zonder batterij: €1.053/jaar. Met 10 kWh batterij: slechts €365/jaar slechter dan huidige situatie. |
| Uitloopperiode | De terugdraaiende teller loopt wettelijk door tot eind 2028. De overgang is stapsgewijs (Fluvius plant meterronde). Exacte datum nog te bevestigen. |


### 11C-4. Slimme batterijsturing: Day-Ahead prijzen (EPEX SPOT)

*Een batterij is pas echt renabel als ze slim gestuurd wordt — niet alleen op basis van solar, maar ook op de dynamische dagelijkse elektriciteitsprijs. Dit opent een tweede bron van besparing (of zelfs inkomsten).*

EPEX SPOT publiceert dagelijks om ~13h de elektriciteitsprijzen voor elk kwartier van de volgende dag. De prijs varieert sterk: negatief (overaanbod wind/solar) tot boven €0,50/kWh (piekverbruik). Met een dynamisch contract betaal je deze marktprijs per kwartier.

EPEX SPOT publiceert elke dag om ±13h de elektriciteitsprijzen voor elk kwartier van de volgende dag. De prijs fluctueert sterk: negatief (veel wind/solar, overaanbod) tot €0,50+/kWh (piekverbruik, weinig productie). Leveranciers zoals Engie FLOW bieden contracten aan waarbij je prijs per kwartier varieert met de beursprijs.

| Wanneer laden? | Kwartieren met lage of negatieve prijs ('s nachts 01h–05h, of overdag bij veel wind/solar). Batterij laden aan €0,00–€0,05/kWh. |
| --- | --- |
| Wanneer ontladen? | Kwartieren met hoge prijs (07h–09h en 17h–20h piek). Batterij ontladen ipv net afnemen aan €0,25–€0,50/kWh. |
| Solar prioriteit | Overdag bij solar-overschot: batterij laden met eigen productie (gratis). Day-ahead sturing is aanvullend voor restcapaciteit. |
| API voor prijsdata | ENTSO-E Transparency Platform (gratis, publiek). Data ophalen via HTTP GET op ESP32-C6 of via een lokale proxy server. |
| Combinatiestrategie | ESP32 Smart Energy haalt dag-voor-dag de prijzen op, bepaalt de goedkope kwartieren en plant lading/ontlading. Solar-overschot heeft altijd prioriteit; day-ahead vult aan. |
| Mogelijke extra besparing | Op basis van historische EPEX-data: €100–€300/jaar extra besparing bovenop de solar-gebaseerde sturing, afhankelijk van contract en prijs-spreiding. |
| Vereist dynamisch contract | Engie FLOW heeft een dynamisch tarief beschikbaar (MY Optiflex of gelijkaardig). Bij contractverlenging in 2026 hierop letten. Total Energies MY Confort: navragen of dynamisch tarief optioneel is. |

Vereenvoudigde sturingslogica voor ESP32 Smart Energy:

- Elke avond: ophalen EPEX SPOT prijzen volgende dag via API (JSON)
- Rangschikken: goedkope kwartieren = laadmomenten, dure kwartieren = ontlaadmomenten
- Overdag: zodra solar-overschot > drempel → batterij laden (prioriteit boven netlading)
- Negatieve prijs (≤ €0,00): batterij maximaal laden én ECO-boiler inschakelen
- Piekprijs (> €0,20/kWh): batterij ontladen, grote verbruikers (EV, boiler) uitschakelen
- Batterij nooit volledig leeg laten voor noodreserve (instelbare drempel, bv. 20%)

### 11C-5. Day-ahead arbitrage — maandelijkse simulatie

Berekening per maand: hoeveel batterijcapaciteit resteert na solar-sturing, de gemiddelde EPEX prijsspread zomer/winter, en de resulterende besparing. Aannames: 70% uptime (niet elke dag is gunstig), round-trip efficiëntie 92%.

| Maand | Solar/dag (kWh) | Batcap. vrij voor DA | Prijsspread (€/kWh) | DA arbitrage (kWh/maand) | Besparing (€/maand) |
| --- | --- | --- | --- | --- | --- |
| Jan | 9 | 9.0 kWh | €0.13 | 174 | €22.6 |
| Feb | 16 | 8.0 kWh | €0.12 | 155 | €18.5 |
| Mar | 29 | 6.5 kWh | €0.11 | 126 | €13.8 |
| Apr | 40 | 4.0 kWh | €0.10 | 77 | €7.7 |
| Mei | 48 | 2.5 kWh | €0.09 | 48 | €4.3 |
| Jun | 47 | 2.0 kWh | €0.08 | 39 | €3.1 |
| Jul | 48 | 2.0 kWh | €0.08 | 39 | €3.1 |
| Aug | 40 | 3.0 kWh | €0.09 | 58 | €5.2 |
| Sep | 33 | 5.0 kWh | €0.10 | 97 | €9.7 |
| Okt | 23 | 7.0 kWh | €0.12 | 135 | €16.2 |
| Nov | 14 | 8.5 kWh | €0.13 | 164 | €21.3 |
| Dec | 8 | 9.0 kWh | €0.14 | 174 | €24.3 |
| TOTAAL |  |  |  | 1.285 kWh | €150 |

*Conclusie day-ahead: zomer beperkt voordeel (batterij al vol van solar). Winter significant voordeel (grote beschikbare capaciteit + hogere prijsspread). Jaaropbrengst: ~€150/jaar extra bovenop de solar-strategie. Met dynamisch contract: gecombineerde terugverdientijd ~8 jaar (vs. ~10 jaar solar only).*


### 11C-6. Simulatieresultaten na 2028

Onderstaande tabel toont de volledige kostenstructuur per scenario. Let op: het prosumententarief (€556/jaar) verdwijnt voor álle scenario's bij de overgang naar de digitale meter — het is geen batterijvoordeel. De batterij levert bovenop die verandering nog eens €688/jaar extra.


**A — Energiestromen (kWh/jaar)**


**B — Jaarlijkse kosten (€/jaar)**


**C — Batterij-investering: 10 kWh vs. geen batterij na 2028**

| Parameter | Huidig (2025) | Na 2028 geen batterij | Na 2028 10 kWh (solar) | Na 2028 10 kWh (solar+day-ahead) |
| --- | --- | --- | --- | --- |
| Aankoop van net | 14.995 | 14.995 | 11.948 | 11.948 |
| Injectie naar net | 6.996 | 6.996 | 3.684 | 3.684 |
| Via day-ahead geladen (kWh) | — | — | — | 1.285 |
| Energiekost variabel | €2.087 | €3.849 | €3.161 | €3.011 |
| Prosumententarief | €556 | €0 ✓ | €0 ✓ | €0 ✓ |
| TOTALE jaarlijkse kost | €2.643 | €3.849 | €3.161 | €3.011 |
| Verschil t.o.v. huidig | — | +€1.206 | +€518 | +€368 |
| Besparing vs. geen batterij | — | — | €688/jaar | €838/jaar |
| waarvan solar-strategie | — | — | €688/jaar | €688/jaar |
| waarvan day-ahead arbitrage | — | — | — | €150/jaar |
| Marktprijs 10 kWh (indicatief, incl BTW 21%) | — | — | ~€7.500–8.000 | ~€7.500–8.000 |
| Effectieve kost na BTW 6% (woningen ≥10 jaar) | — | — | ~€6.570–7.050 | ~€6.570–7.050 |
| BTW-voordeel (21% → 6%) | — | — | ~€930 | ~€930 |
| Terugverdientijd vs. geen batterij | — | — | ~10 jaar | ~8 jaar |

*€3.849 - €150 (day-ahead) = €3.011 bij solar+day-ahead strategie. Day-ahead vereist een dynamisch energiecontract (marktprijs per kwartier).*


### 11C-7. Vergelijking systemen — sweet spot & gevoeligheid

Alle terugverdientijden: investering (na BTW 6%) ÷ besparing t.o.v. geen batterij na 2028. Consistent met 11C-5 en 11C-6. Day-ahead extra vereist dynamisch contract.

| Systeem | Invest. (BTW 6%) | Solar /jaar | Day-ahead extra | Totaal /jaar | Terugverd. solar | Terugverd. +DA |
| --- | --- | --- | --- | --- | --- | --- |
| 5 kWh (Huawei LUNA2000) | ~€4.800 | €374 | ~€80 | ~€454 | ~13 jaar | ~11 jaar |
| 10 kWh (Huawei / BYD) | ~€6.600 | €688 | ~€150 | ~€838 | ~10 jaar | ~8 jaar |
| 15 kWh (BYD HVM) | ~€8.800 | €914 | ~€175 | ~€1.089 | ~10 jaar | ~8 jaar |
| 20 kWh (Tesla PW3/BYD) | ~€12.300 | €1.048 | ~€185 | ~€1.233 | ~12 jaar | ~10 jaar |

*Zonder dynamisch contract: gebruik enkel de kolom terugverdientijd solar.*


### 11C-7B. Waarom 10 kWh de sweet spot is voor Zarlardinge

De keuze voor 10 kWh volgt direct uit onze specifieke situatie:

- Zomers solar-overschot: ~7.000 kWh/jaar bruto. Een 10 kWh batterij vangt ~60% op. Een 15 kWh vangt ~85% op, maar de meerprijs (€2.200) levert slechts €226/jaar extra op. Terugverdientijd van die meerkost alleen: ~10 jaar.
- Verbruiksprofiel: EV's laden 's nachts, warmtepompen dag en nacht. De batterij blijft 's winters vrijwel leeg na de nacht en is volledig beschikbaar voor day-ahead arbitrage — het ideale profiel voor 10 kWh.
- Boven 15 kWh neemt de extra besparing nauwelijks meer toe. De terugverdientijd van 20 kWh (~10–12 jaar) is slechter dan 10 kWh (~8 jaar).

### 11C-7C. Factoren die de terugverdientijd kunnen beïnvloeden

✅ Factoren die de ROI verbeteren:

| Dalende batterijtarieven | Prijzen dalen ~10%/jaar. In 2028 kost een 10 kWh systeem naar schatting €500–€800 minder. Wachten loont. |
| --- | --- |
| Stijgend verbruik | Verbruik steeg van ~12.000 kWh (2022) naar ~18.000 kWh (2025). Elk extra kWh netaankoop verbetert direct de ROI. |
| Smart Energy systeem werkt | ESP32-sturing verhoogt eigenverbruik van solar en vermindert nettaankoop. Elke kWh die direct verbruikt wordt i.p.v. terug naar net is winst. |
| Dynamisch contract | Met EPEX-gebaseerd contract stijgt besparing van €688 naar ~€838/jaar. Terugverdientijd daalt van ~10 naar ~8 jaar. |
| Capaciteitstarief (digitale meter) | De digitale meter brengt een capaciteitstarief op kwartier-piekverbruik. Een batterij die pieken afvlakt bespaart ook hierop — niet meegerekend in onze simulatie. |

⚠ Factoren die de ROI kunnen verslechteren:

| Geen dynamisch contract | Zonder dynamisch tarief vervalt day-ahead besparing (€150/jaar). Terugverdientijd stijgt van ~8 naar ~10 jaar. |
| --- | --- |
| Verbruik daalt | Als verbruik daalt (zuiniger EV, betere isolatie), minder netaankoop en minder batterijrendement. |
| Injektietarief hóóger dan €0,05 | Als Fluvius meer vergoedt, is de prikkel voor opslag kleiner. |
| Onderhoud / BMS-vervanging | LFP-batterij: 15–20 jaar. Omvormer of BMS na 10–12 jaar: ±€500–€1.000 extra. |

Samenvatting: de berekening is voorzichtig. Dalende batterijtarieven, stijgend verbruik en het capaciteitstarief zullen de ROI in de praktijk waarschijnlijk verbeteren. Een dynamisch contract is de sleutel tot terugverdientijd ~8 jaar.


### 11C-8. Wanneer kopen? Drie redenen om te wachten

- Batterijprijzen dalen ~10–15%/jaar. Een 10 kWh systeem dat nu €7.500 kost, kost in 2027 vermoedelijk €6.000–6.500. Besparing door 1 jaar wachten: ~€1.000.
- Vlaamse premie: STOPGEZET sinds 1 april 2023 — geen nieuwe premies gepland. Wel: BTW 6% (i.p.v. 21%) voor woningen ouder dan 10 jaar bij installatie door erkend elektrotechnisch installateur. Op een investering van €7.500 betekent dit een besparing van €1.125. Gemeentelijke premies: sporadisch en beperkt (bv. gemeente Essen €100). Groene lening: via Vlaamse overheid of banken aan gunstige rente. Geen federale subsidie beschikbaar in 2026.
- Meer zekerheid over het injektietarief na 2028: dat tarief bepaalt de terugverdientijd sterk. Een tarief van €0,08 i.p.v. €0,05 verkort de terugverdientijd met 2 jaar.
*Aanbevolen moment: vroeg 2028, gelijktijdig met of net na de overgang naar digitale meter. De BTW-besparing van 6% (€1.125 op €7.500) geldt onmiddellijk. Geen andere Vlaamse premie te verwachten. Gemeentelijke premie in Zarlardinge-regio navragen bij gemeente.*


### 11C-9. Prioriteit: het Smart Energy systeem

Het Smart Energy systeem (ESP32-sturing) heeft een terugverdientijd van quasi nul — enkel hardwarekosten van ~€50–100. Door de ECO-boiler (2 kW) en EV-laders automatisch te sturen op solar-overschot, wordt al 1.000–2.500 kWh/jaar nuttig verbruikt i.p.v. teruggestuurd naar het net. Dit voordeel geldt NU al, ook met de terugdraaiende teller — want hoe meer directe eigenverbruik, hoe minder men afhankelijk is van de nettarieven na 2028.

*Opmerking: deze logica kan gefäseerd geïmplementeerd worden. Fase 1 = solar-sturing alleen (eenvoudig). Fase 2 = day-ahead integratie toevoegen zodra batterij aanwezig is.*


### 11D. UI-stijl en architectuur Smart Energy controller

De Smart Energy controller (ESP32-C6 in de tellerkast) volgt dezelfde UI-architectuur als de bestaande ECO-boiler controller (v1.23, Filip Dworp). Dit zorgt voor een consistente look-and-feel op beide apparaten.

| Technologie | ESPAsyncWebServer met chunked streaming. Geen Chart.js op hoofdpagina (geheugen). Aparte /charts pagina met historische grafieken. |
| --- | --- |
| Kleurpalet | Header: geel (#ffcc00), tekst zwart. Sidebar: marineblauw (#336699), witte links. Groepstitels: #336699 achtergrond, cursief-vet, witte tekst. Tabel labels: #369 kleur. |
| Layout | Flexbox: sidebar links (60px breed) + main content. Tabelgebaseerde dataweergave. Statusbadges groen (#0a0) / grijs (#999). Voortgangsbalkjes voor vermogens en energiewaarden. |
| Paginastructuur | /  (hoofddashboard), /charts  (historische grafieken), /settings  (instelpagina), /json  (REST endpoint voor externe logging/Zarlar Dashboard) |
| JSON endpoint | /json geeft compacte data terug (sleutel a, b, c...) voor integratie met Zarlar Dashboard op 192.168.0.60 of andere controllers. |
| OTA updates | Over-The-Air firmware update via /update. Partitietabel: 16Mb met app0/app1 OTA partities + SPIFFS voor logging. |
| Logging | SPIFFS debug.log + debug.log.old (rotatie bij >800KB). Crash-log in NVS. Seriële output via Serial0 (ESP32-C6 fix). |
| Matter integratie | arduino-esp32-Matter library. Temperatuursensoren en schakelaar als Matter endpoints (compatibel met Apple Home, Google Home). |
| Simulatiemodus | SIMULATION_MODE vlag: valse sensordata voor ontwikkeling zonder hardware. Rode banner in UI. |

Relevante pagina's voor Smart Energy (analoog aan ECO-boiler):

- Hoofdpagina: real-time solar productie, verbruik WON/SCH, huidig overschot/tekort, relaisstatus ECO-boiler en EV-lader, batterijstatus (later)
- /charts: weekgrafieken solar productie, verbruik, netbalans
- /settings: drempelwaarden solar-sturing, day-ahead sturing aan/uit, laadbegrenzingen per verbruiker
- /json: alle waarden als compact JSON voor Zarlar Dashboard en eventuele andere controllers

### 11E. Volledig toekomstig systeemoverzicht

[Solar panelen] → [SMA omvormers] → [S0-pulsen] → [ESP32-C6 Smart Energy, tellerkast] → sturing van: ECO-boiler (HTTP REST), EV-laders (REST API), Thuisbatterij (Modbus TCP)

De Smart Energy controller stuurt aan: ECO-boiler controller (HTTP REST/IP), Tesla Wallcharger Gen 3 (lokale REST API), EV-lader 2 (te bepalen), Thuisbatterij (Modbus TCP).


## 12. Openstaande Actiepunten

| # | Actie | Door wie | Status |
| --- | --- | --- | --- |
| 1 | Typeplaatje SMA omvormers: Zuid omvormers 1&2 = SB3600TL-21, SN 2130419465 & 2130419851. Westdak omvormer 3: type + SN nog te noteren. | Maarten | Open |
| 2 | Specificaties S0-tellers: pulsen/kWh — staat op het label van de teller | Filip | Open |
| 3 | ESP32 modules inventariseren: welke types zijn beschikbaar? (antw: ESP32-C6, 16Mb versie) | Filip | ✅ OK |
| 4 | WiFi beschikbaar bij de tellerkast? (antw: JA) | Filip | ✅ OK |
| 5 | Merk/type en vermogen van de ECO-boiler noteren (antw: OEG dompelweerstand met thermostaat. Waarschijnlijk 2 kW. Kan vanuit tellerkast in SCH remote in- en uitgeschakeld worden) | Filip | ✅ OK |
| 6 | Merk/type van beide EV-laders noteren (stuurbaarheid nagaan) (antw: Lader 1 = Tesla Wallcharger, Lader 2: ???) | Maarten | Open |
| 7 | Bepalen of Home Assistant aanwezig of gewenst is (antw: NEE, indien niet nodig) | Filip | ✅ OK |
| 8 | SMA communicatiemogelijkheden verifiëren (lokaal netwerk aanwezig?) | Filip | Open |
| 9 | Jaarbedrag Engie FLOW invullen in de opvolgingstabel (2026) zodra factuur ontvangen | Maarten | Open |
| 10 | CZ-TAW1 module WP WON resetten (paperclip op resetknop) en opnieuw registreren met privé-mailadres Filip als eigenaar. Daarna Maarten toevoegen als gebruiker. | Filip | Open |


## 13. Versiegeschiedenis

| v0.1 — April 2026 | Initieel document op basis van gesprek Papa & Claude. Situatie, doelen, actiepunten vastgelegd. |
| --- | --- |
| v0.2 — April 2026 | Sectie 4B toegevoegd: warmtepompen WP SCH en WP WON, Panasonic Comfort Cloud blokkering en gekozen oplossing (CZ-TAW1 reset met paperclip, herregistratie op privé-mail Filip). Actiepunt #10 toegevoegd. |
| v0.3 — April 2026 | Nieuwe sectie 11 toegevoegd: systeemarchitectuur, Tesla Wall Connector lokale API, batterijvergelijking 2028. Secties hernummerd. |
| v0.4 — April 2026 | Sectie 11C uitgebreid met gedetailleerde batterijanalyse (7 subsecties): assumpties, maandelijkse energiebalans, terugdraaiende teller uitleg, simulatieresultaten per capaciteit, systeemvergelijking, aankoopadvies en prioriteiten. |
| v0.5 — April 2026 | Correcties v0.6: regenwaterpomp volledig verwijderd uit sturing en sec. 4/7D/11D. EV-lader 1 = Tesla Wallcharger. Doel 6 toegevoegd. Sectie 7C: MQTT/HA verwijderd, Google Sheets logging toegevoegd. 11A: HTTP REST gestandaardiseerd. 11B: Tesla nacht+override. 11C: prosumententarief correct in berekening. 11C-6 ingekort. Versienummer titel verwijderd. |
| v0.7 — April 2026 | Sec. 2 & 7B bijgewerkt met omvormergegevens uit typeplaatjes: 2× SMA SB3600TL-21 (zuiddak), Speedwire/Webconnect ingebouwd, SN en RID-codes gedocumenteerd. Westdak omvormer 3 nog te identificeren. Actiepunt 1 deels afgevinkt. |
