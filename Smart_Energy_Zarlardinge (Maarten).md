# SMART ENERGY MANAGEMENT
## Zarlardinge — Gedeelde Energie WON & SCH
*Projectdocument voor Maarten & Filip  |  Mei 2026  |  v1.3*

---

## 1.  Waarom dit project?

Onze zonne-installatie produceert jaarlijks bijna 12.000 kWh — genoeg om een groot deel van het verbruik van WON en SCH samen te dekken. Maar vandaag is het niet duidelijk hoeveel van die gratis zonne-energie direct nuttig gebruikt wordt.

Er is geen enkel scherm dat vertelt: 'de zon schijnt — zet nu de wasmachine aan.' Of: 'stroom is negatief vannacht — laad de auto.' Dat verandert met dit project.

> **Overgang naar digitale meter — gepland 2028**
>
> Nu: teller draait terug. Elke kWh die we produceren en niet gebruiken
> wordt vergoed aan de volledige aankoopprijs. Het net = gratis batterij.
>
> Na 2028: injectie vergoed aan slechts ~5 ct/kWh. Aankoop kost ~28 ct/kWh.
> Verschil: wie eigenverbruik niet verhoogt, betaalt ~€1.200 meer per jaar.
>
> Dit systeem bereidt ons exact voor — en levert vandaag al inzicht.

> **Wat al werkt — vandaag, 1 mei 2026:**
>
> De EPEX-stroomprijzen vandaag zijn negatief: -50 ct/kWh.
> Dat betekent dat je BETAALD wordt om stroom af te nemen van het net.
> Ons systeem toont dit live — en adviseert wanneer verbruik slim is.
> Dit is exact de reden waarom dit project nuttig is.

---

## 2.  Wat werkt er al?

We zijn niet meer in de planningsfase — het systeem draait. Hier is wat vandaag al operationeel is:

| Wat | Status | Waar |
| --- | --- | --- |
| Live EPEX-prijsgrafiek 48u | ✅ Draait | portal.zarlardinge.be |
| Solar productie grafiek | ✅ Draait | Gesimuleerd, wacht op S0 |
| Batterij laad/ontlaad simulatie | ✅ Draait | SOC, kW, arbitrage |
| Net import/export grafiek | ✅ Draait | Live financieel resultaat |
| Dagkost indicator | ✅ Draait | € per dag, dynamisch vs vast |
| Ecopower tariefcomponenten | ✅ Draait | Fluvius, GSC, WKK, heffingen, BTW |
| Planningstabel 48u | ✅ Draait | Checkboxes, piekbeheer |
| LED-strip simulatie 12 pixels | ✅ Draait | Advies koken/wassen |
| RPi server op lokaal netwerk | ✅ Draait | 192.168.0.50:3000 |
| Smart Energy controller (ESP32) | ✅ Eerste data | SIM modus, logging OK |
| Google Sheets logging | ✅ Draait | Via Dashboard controller |
| Fysieke LED-strip kastje | ⬜ Te bouwen | PCB bestellen |
| S0 pulsmeting (echte data) | ⬜ Te bouwen | UTP kabel trekken |

**10 van de 13 onderdelen draaien al. De hardware komt eraan.**

---

## 3.  Waarom slim sturen zoveel verschil maakt

Onze installatie produceert tot 60 kWh per dag op zonnige dagen. Maar niet elke kWh is evenveel waard. Het verschil zit in wanneer je verbruikt en wanneer je injecteert.

### 3.1  De basis — eigenverbruik maximaliseren

Elke kWh die je zelf verbruikt in plaats van te injecteren bespaart ~23 ct/kWh (verschil tussen 28 ct aankoop en 5 ct injectievergoeding). Het systeem adviseert wanneer het slim is om de wasmachine, droogkast of afwasmachine aan te zetten.

### 3.2  Negatieve stroomprijzen — je wordt betaald om te verbruiken

Op 1 mei 2026 was de EPEX-prijs -50 ct/kWh. Dat betekent:

| Situatie | Wat je verdient/betaalt |
| --- | --- |
| Solar injecteren | €0,00 per kWh (vloer — je krijgt niks) |
| Stroom afnemen van net | -40 ct/kWh → je wordt BETAALD |
| EV laden van net (10 kW × 4u) | 40 kWh × 40 ct = €16 winst |
| Solar aan + injecteren | €0 — gemiste kans! |

Het systeem detecteert deze situaties automatisch en adviseert: stop met injecteren, verbruik maximaal van het net. Bij een dynamisch contract na 2028 wordt dit geld in de portemonnee.

### 3.3  Wat ons systeem kan sturen

Het systeem kan verschillende toestellen automatisch in- en uitschakelen, elk met zijn eigen interface:

| Toestel | Vermogen | Hoe gestuurd | Status |
| --- | --- | --- | --- |
| ECO-boiler SCH | 2 kW | WiFi — lokaal, direct | ✅ Klaar |
| EV WON (Tesla) | 9 kW | WiFi — via Tesla cloud | ⬜ Fase 2 |
| SMA omvormer westdak | 3,7 kW | Modbus — lokaal, direct | ⬜ Fase 2 |
| WP WON (Panasonic) | 2,5 kW | WiFi — via Panasonic cloud | ⬜ Later |
| WP SCH (Panasonic) | 2,5 kW | WiFi — via Panasonic cloud | ⬜ Later |
| Thuisbatterij | 5 kW | Modbus — lokaal, direct | ⬜ 2028 |
| Verlichting (alle kamers) | — | WiFi — lokaal, direct | ✅ Klaar |

*De regenwaterpomp wordt nooit automatisch gestuurd — essentieel voor al het sanitair.*

---

## 4.  Waarom dit zelf bouwen? — vs kant-en-klare systemen

Er bestaan commerciële energy management systemen. Waarom bouwen we het zelf?

| | Ons systeem | Commercieel (bv. Smappee, Solar Manager) |
| --- | --- | --- |
| Aankoopprijs | ~€50–100 hardware | €500–2.000 + installatie |
| Maandelijks abonnement | €0 — gratis, voor altijd | €5–15/maand (€60–180/jaar) |
| Na 10 jaar betaald | €50–100 totaal | €1.100–3.800 totaal |
| Aanpasbaar | Volledig — wij schrijven de code | Beperkt tot wat de fabrikant voorziet |
| Kostenverdeling WON/SCH | ✅ Uniek — nergens anders | ❌ Niet beschikbaar |
| Ecopower tarieven exact | ✅ Tot op de cent | ❌ Generieke tarieven |
| Solar curtailment | ✅ Via Modbus (fase 2) | ❌ Zelden beschikbaar |
| Werkt zonder internet | ✅ Volledig lokaal | ⚠️ Vaak cloud-afhankelijk |
| Privacy | ✅ Data blijft thuis | ⚠️ Data naar fabrikant |
| Onderhoud | Filip — updates via GitHub | Fabrikant — afhankelijkheid |

> **Ons systeem is niet goedkoper omdat het minder kan.**
> Het is goedkoper omdat we geen winstmarge betalen op hardware
> die we zelf kunnen bouwen, en geen abonnement op software
> die we zelf kunnen schrijven.
>
> De echte meerwaarde: de kostenverdeling WON/SCH bestaat nergens
> anders — geen enkel commercieel systeem ondersteunt twee
> huishoudens op één aansluiting.

---

## 5.  Financieel potentieel — wat valt er te verdienen?

| Strategie | Besparing/jaar | Wanneer |
| --- | --- | --- |
| Eigenverbruik verhogen (+20%) | ~€400–600 | Nu — met advies van het systeem |
| Dynamisch contract (na data-analyse) | ~€150–300 | Na 1 jaar meten |
| Thuisbatterij 10 kWh | ~€688 | Na 2028 (digitale meter) |
| Batterij + dynamisch | ~€838 | Na 2028 |
| Solar curtailment bij neg. prijs | ~€50–150 | Na 2028 (dynamisch contract) |
| Capaciteitstarief verlagen | ~€100–200 | Na 2028 (digitale meter) |
| **TOTAAL potentieel** | **~€1.500–2.000/jaar** | Volledig systeem operationeel |

Dit alles voor een investering van ~€50–100 in hardware en de tijd van Filip. Geen maandelijks abonnement, geen cloud-afhankelijkheid, geen fabrikant die zijn dienst stopzet.

---

## 6.  Onze installatie in cijfers

| Dak | Panelen | Omvormer | Vermogen |
| --- | --- | --- | --- |
| Zuiddak | 32× Viessmann 275 Wp | 2× SMA SB3600TL-21 | 8.800 Wp |
| Westdak | 12× Viessmann 275 Wp | SMA SB3.6-1AV-41 | 3.300 Wp |
| **Totaal** | **44 panelen** | **3 SMA-omvormers** | **12.100 Wp │ ~11.950 kWh/j** |

---

## 7.  Tariefstructuur — Ecopower Dynamische burgerstroom

Op basis van een echte Ecopower-factuur (februari 2026) hebben we alle tariefcomponenten exact gemodelleerd in ons systeem:

| Component | Bedrag | Type |
| --- | --- | --- |
| EPEX spotprijs | variabel per kwartier | De beursprijs — kan negatief zijn |
| Afnametarief Fluvius | 5,23 ct/kWh | Transport via het net |
| GSC (groenestroomcert.) | 1,10 ct/kWh | Vlaamse groene stroom verplichting |
| WKK (warmtekracht) | 0,39 ct/kWh | Vlaamse WKK verplichting |
| Heffingen & accijnzen | 4,94 ct/kWh | Federale belasting |
| BTW 6% | op de volledige som | Op EPEX + alle componenten |
| | | |
| Abonnement Ecopower | €5,00/maand | Vast per aansluiting |
| Databeheer Fluvius | €1,49/maand | Vast per aansluiting |
| Capaciteitstarief | €4,52/kW/maand | Op de maandpiek (min. 2,5 kW) |

Ons systeem berekent de reële prijs per kwartier: (EPEX + vaste componenten) × (1 + BTW%). Het verschil met een vast contract is elke dag zichtbaar.

---

## 8.  Bouwfases

### Fase 1 — Meten, tonen, adviseren (nu)
Software 90% klaar. Hardware (kastje + bekabeling) volgt.

### Fase 2 — Automatisch sturen (~6 maanden)
ECO-boiler en EV laden automatisch op basis van EPEX-prijs en solar. SMA westdak uitlezen en curtailment bij negatieve prijzen.

### Fase 3 — Thuisbatterij (~2028)
10 kWh Huawei/BYD. Terugverdientijd ~8 jaar met dynamisch contract.

---

## 9.  Wat moet er gebeuren?

| Maarten | Filip |
| --- | --- |
| Typeplaatje westdak omvormer noteren | PCB ontwerpen + bestellen JLCPCB |
| Pulsen/kWh S0-tellers lezen | ESP32-C6 kastje samenstellen |
| EV-lader SCH merk/type opzoeken | S0 bekabeling aansluiten |
| Engie FLOW jaarbedrag doorgeven | SMA Modbus westdak testen |
| ntfy.sh app installeren | LED-strip 12px + pushberichten |
| UTP kabel trekken inkomhal (~2m) | Google Sheets scripts afwerken |
| Kastje ophangen naast router | Cloudflare tunnel activeren |
| CZ-TAW1 WP WON resetten | Voor Maarten: document bijhouden |

---

> **Dit is een haalbare kaart.**
>
> 10 van de 13 software-onderdelen draaien al.
> De hardware kost minder dan €100.
> Het potentieel is €1.500–2.000 besparing per jaar.
> En het belangrijkste: het is ons eigen systeem,
> gebouwd op maat van Zarlardinge, voor beide families.
>
> Geen fabrikant, geen abonnement, geen afhankelijkheid.

---
*Versie 1.3  |  Mei 2026  |  Smart Energy Management — Zarlardinge*
