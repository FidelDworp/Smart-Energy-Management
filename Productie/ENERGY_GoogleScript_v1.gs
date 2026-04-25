// ============================================================
// S-ENERGY DATA LOGGER — Google Apps Script v1
// Ontvangt JSON van Zarlar Dashboard (192.168.0.60)
// die /json pollt van S-ENERGY controller (192.168.0.73)
//
// Logging-interval: elke 5 minuten (via Dashboard controller)
// Portaal RPi (192.168.0.50) is NIET betrokken bij logging —
// als de RPi uitvalt blijft de data-logging gewoon werken.
//
// v1 (25apr26):
//   - Conform ECO_GoogleScript_v2 structuur
//   - 2 bevroren rijen: rij 1 = titel+URL, rij 2 = kolomtitels
//   - MAX_ROWS limiet (default 2000 — meer kanalen dan ECO)
//   - HEADER_ROWS = 2
//   - sim_s0 + sim_p1 vlaggen gelogd — achteraf zichtbaar
//     welke metingen echt of gesimuleerd waren
//   - sendDailySummary() met waarschuwing als sim-data aanwezig
//
// JSON keys S-ENERGY v1.26 (/json endpoint 192.168.0.73):
//   a  = Solar W (momentaan)
//   b  = WON W (momentaan, + = afname, − = injectie)
//   c  = SCH afname W
//   d  = SCH injectie W
//   e  = Netto W SCH (+ = injectie naar net)
//   h  = Solar dag Wh
//   i  = WON dag afname Wh
//   j  = SCH afname dag Wh
//   k  = SCH injectie dag Wh
//   vw = WON dag injectie Wh
//   n  = EPEX nu all-in ct/kWh × 100
//   n2 = EPEX +1u all-in ct/kWh × 100
//   pt = Maandpiek W
//   ac = WiFi RSSI dBm
//   ae = Heap largest block bytes
//   sim_s0 = 1 als S0 gesimuleerd, 0 = live hardware
//   sim_p1 = 1 als P1 gesimuleerd, 0 = live HomeWizard dongle
//   ver = firmware versie string
// ============================================================

// ============================================================
// CONFIGURATIE
// ============================================================
const MAX_ROWS    = 2000;   // 5-min interval → ~6 maanden bij 2000 rijen
const HEADER_ROWS = 2;
// ============================================================


function doPost(e) {
  try {
    const data  = JSON.parse(e.postData.contents);
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

    const timestamp = Utilities.formatDate(
      new Date(), "Europe/Brussels", "yyyy-MM-dd HH:mm:ss"
    );

    // Veilige getter: retourneert 0 als key ontbreekt of null is
    const v = (key, decimals) => {
      const val = data[key];
      if (val === undefined || val === null) return 0;
      return decimals !== undefined ? parseFloat(val.toFixed(decimals)) : val;
    };

    const row = [
      // Kolom A: tijdstempel
      timestamp,

      // Kolommen B–F: momentane vermogens (W)
      v("a"),                             // Solar W
      v("c"),                             // SCH afname W
      v("d"),                             // SCH injectie W
      v("e"),                             // Netto W SCH
      v("b"),                             // WON W

      // Kolommen G–K: dagcumulatieven (kWh, afgerond op 3 decimalen)
      parseFloat((v("h") / 1000).toFixed(3)),   // Solar dag kWh
      parseFloat((v("j") / 1000).toFixed(3)),   // SCH afname dag kWh
      parseFloat((v("k") / 1000).toFixed(3)),   // SCH injectie dag kWh
      parseFloat((v("i") / 1000).toFixed(3)),   // WON afname dag kWh
      parseFloat(((v("vw") || 0) / 1000).toFixed(3)), // WON injectie dag kWh

      // Kolommen L–M: EPEX (ct/kWh, 2 decimalen)
      parseFloat((v("n") / 100).toFixed(2)),    // EPEX nu ct/kWh
      parseFloat((v("n2") / 100).toFixed(2)),   // EPEX +1u ct/kWh

      // Kolom N: maandpiek W
      v("pt"),

      // Kolommen O–P: systeem
      v("ac"),                            // RSSI dBm
      Math.round(v("ae") / 1024),         // Heap KB

      // Kolommen Q–R: simulatievlaggen (0/1)
      v("sim_s0"),                        // 1 = S0 gesimuleerd
      v("sim_p1"),                        // 1 = P1 gesimuleerd

      // Kolom S: firmware versie
      data.ver || "?"
    ];

    sheet.appendRow(row);

    // MAX_ROWS bewaking — verwijder oudste datarij (rij HEADER_ROWS+1)
    const dataRows = sheet.getLastRow() - HEADER_ROWS;
    if (dataRows > MAX_ROWS) {
      sheet.deleteRow(HEADER_ROWS + 1);
      Logger.log("MAX_ROWS (" + MAX_ROWS + ") bereikt — oudste rij verwijderd");
    }

    return ContentService
      .createTextOutput(JSON.stringify({
        status:    "success",
        message:   "Data gelogd",
        timestamp: timestamp,
        solar_w:   data.a,
        sim_s0:    data.sim_s0,
        sim_p1:    data.sim_p1
      }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (error) {
    Logger.log("doPost fout: " + error.toString());
    return ContentService
      .createTextOutput(JSON.stringify({
        status:  "error",
        message: error.toString()
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}


function setupHeaders() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const ss    = SpreadsheetApp.getActiveSpreadsheet();

  // Verwijder bestaande header-rijen als aanwezig (idempotent)
  if (sheet.getLastRow() >= 2) {
    if (sheet.getRange(2, 1).getValue() === "Tijdstempel") {
      sheet.deleteRow(2);
      Logger.log("Bestaande kolomtitelrij (rij 2) verwijderd.");
    }
  }
  if (sheet.getLastRow() >= 1) {
    const r1 = sheet.getRange(1, 1).getValue();
    if (typeof r1 === "string" && r1.startsWith("S-ENERGY")) {
      sheet.deleteRow(1);
      Logger.log("Bestaande titelrij (rij 1) verwijderd.");
    }
  }

  // ── Rij 1: titelrij ─────────────────────────────────────
  sheet.insertRowBefore(1);
  const titleCell = sheet.getRange(1, 1);
  titleCell.setValue("S-ENERGY DATA LOGGER v1  |  " + ss.getUrl());
  titleCell.setFontSize(9);
  titleCell.setFontWeight("normal");
  titleCell.setFontStyle("italic");
  titleCell.setFontColor("#cccccc");
  titleCell.setBackground("#222222");
  titleCell.setHorizontalAlignment("left");
  titleCell.setVerticalAlignment("middle");
  sheet.setRowHeight(1, 24);

  // ── Rij 2: kolomtitels ───────────────────────────────────
  const headers = [
    // A
    "Tijdstempel",
    // B–F momentane vermogens
    "Solar (W)",
    "SCH afname (W)",
    "SCH injectie (W)",
    "Netto SCH (W)",
    "WON (W)",
    // G–K dagcumulatieven
    "Solar dag (kWh)",
    "SCH afname dag (kWh)",
    "SCH injectie dag (kWh)",
    "WON afname dag (kWh)",
    "WON injectie dag (kWh)",
    // L–M EPEX
    "EPEX nu (ct/kWh)",
    "EPEX +1u (ct/kWh)",
    // N maandpiek
    "Maandpiek (W)",
    // O–P systeem
    "RSSI (dBm)",
    "Heap (KB)",
    // Q–R simulatievlaggen
    "SIM S0 (0/1)",
    "SIM P1 (0/1)",
    // S versie
    "FW versie"
  ];

  sheet.insertRowBefore(2);
  const headerRange = sheet.getRange(2, 1, 1, headers.length);
  headerRange.setValues([headers]);
  headerRange.setFontSize(10);
  headerRange.setFontWeight("normal");
  headerRange.setFontStyle("normal");
  headerRange.setFontColor("#ffffff");
  headerRange.setBackground("#000000");
  headerRange.setHorizontalAlignment("center");
  headerRange.setVerticalAlignment("middle");
  headerRange.setWrap(true);
  sheet.setRowHeight(2, 40);

  // Kolombreedtes
  sheet.setColumnWidth(1, 140);   // A: tijdstempel
  for (let i = 2; i <= 6;  i++) sheet.setColumnWidth(i, 90);  // B–F: W
  for (let i = 7; i <= 11; i++) sheet.setColumnWidth(i, 100); // G–K: kWh
  for (let i = 12; i <= 13; i++) sheet.setColumnWidth(i, 95); // L–M: EPEX
  sheet.setColumnWidth(14, 95);   // N: piek
  sheet.setColumnWidth(15, 80);   // O: RSSI
  sheet.setColumnWidth(16, 75);   // P: heap
  sheet.setColumnWidth(17, 75);   // Q: sim_s0
  sheet.setColumnWidth(18, 75);   // R: sim_p1
  sheet.setColumnWidth(19, 75);   // S: versie

  // Kleur sim-kolommen licht oranje als waarschuwing
  sheet.getRange(2, 17).setBackground("#cc6600");  // SIM S0
  sheet.getRange(2, 18).setBackground("#cc6600");  // SIM P1

  sheet.setFrozenRows(2);
  sheet.setFrozenColumns(1);

  Logger.log("Headers aangemaakt! " + headers.length + " kolommen (A t/m S)");
  Logger.log("Rij 1 = titelrij | Rij 2 = kolomtitels | Data vanaf rij 3");
  Logger.log("MAX_ROWS instelling: " + MAX_ROWS);
}


function test() {
  // Realistische testdata conform S-ENERGY v1.26 /json — gesimuleerde middag
  const testData = {
    postData: {
      contents: JSON.stringify({
        "a":  3200,    // Solar W — zonnige middag
        "b":   850,    // WON W — afname
        "c":  1200,    // SCH afname W
        "d":   400,    // SCH injectie W — overschot
        "e":   400,    // Netto SCH W
        "h":  14800,   // Solar dag Wh
        "i":   3200,   // WON dag afname Wh
        "j":   5600,   // SCH afname dag Wh
        "k":   1800,   // SCH injectie dag Wh
        "vw":     0,   // WON dag injectie Wh (geen solar WON)
        "n":   1820,   // EPEX nu: 18.20 ct/kWh (× 100)
        "n2":  2100,   // EPEX +1u: 21.00 ct/kWh (× 100)
        "pt":  4500,   // Maandpiek W
        "ac":   -58,   // RSSI dBm
        "ae":  38912,  // Heap bytes
        "sim_s0": 1,   // S0 nog gesimuleerd
        "sim_p1": 1,   // P1 nog gesimuleerd
        "ver": "1.26"
      })
    }
  };
  const result = doPost(testData);
  Logger.log(result.getContent());
}


function sendDailySummary() {
  const sheet   = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const lastRow = sheet.getLastRow();
  if (lastRow < HEADER_ROWS + 1) { Logger.log("Nog geen data."); return; }

  const today = new Date();

  let count        = 0;
  let maxSolarW    = 0;
  let maxSolarKwh  = 0;
  let schAfnameKwh = 0;
  let schInjectKwh = 0;
  let wonAfnameKwh = 0;
  let wonInjectKwh = 0;
  let maxPiekW     = 0;
  let simS0Rows    = 0;
  let simP1Rows    = 0;
  let avgRssi      = 0;
  let minHeapKb    = 9999;
  let epexSum      = 0;

  // Lees alle rijen van vandaag (van achter naar voor, stop bij andere dag)
  for (let i = lastRow; i > HEADER_ROWS; i--) {
    const ts = new Date(sheet.getRange(i, 1).getValue());
    if (ts.toDateString() !== today.toDateString()) break;
    count++;

    const solarW    = sheet.getRange(i, 2).getValue();   // B
    const solarKwh  = sheet.getRange(i, 7).getValue();   // G
    const schAfKwh  = sheet.getRange(i, 8).getValue();   // H
    const schInKwh  = sheet.getRange(i, 9).getValue();   // I
    const wonAfKwh  = sheet.getRange(i, 10).getValue();  // J
    const wonInKwh  = sheet.getRange(i, 11).getValue();  // K
    const epexCt    = sheet.getRange(i, 12).getValue();  // L
    const piekW     = sheet.getRange(i, 14).getValue();  // N
    const rssi      = sheet.getRange(i, 15).getValue();  // O
    const heapKb    = sheet.getRange(i, 16).getValue();  // P
    const simS0     = sheet.getRange(i, 17).getValue();  // Q
    const simP1     = sheet.getRange(i, 18).getValue();  // R

    if (solarW   > maxSolarW)   maxSolarW   = solarW;
    if (solarKwh > maxSolarKwh) maxSolarKwh = solarKwh;
    if (schAfKwh > schAfnameKwh) schAfnameKwh = schAfKwh;
    if (schInKwh > schInjectKwh) schInjectKwh = schInKwh;
    if (wonAfKwh > wonAfnameKwh) wonAfnameKwh = wonAfKwh;
    if (wonInKwh > wonInjectKwh) wonInjectKwh = wonInKwh;
    if (piekW    > maxPiekW)    maxPiekW    = piekW;
    if (heapKb   < minHeapKb)   minHeapKb   = heapKb;
    if (simS0 === 1) simS0Rows++;
    if (simP1 === 1) simP1Rows++;
    avgRssi  += rssi;
    epexSum  += epexCt;
  }

  if (count === 0) { Logger.log("Geen data vandaag."); return; }

  avgRssi  = avgRssi  / count;
  const avgEpex = epexSum / count;

  const expected   = 24 * 60 / 5;   // 288 metingen per dag bij 5-min interval
  const pct        = (count / expected * 100).toFixed(1);
  const collectieStatus = pct > 95 ? "✓ Uitstekend" : pct > 80 ? "⚠ Matig" : "❌ Slecht";
  const heapStatus = minHeapKb >= 35 ? "✓ OK" : minHeapKb >= 25 ? "⚠ Laag" : "❌ Kritiek";
  const rssiStatus = avgRssi >= -60 ? "Goed" : avgRssi >= -75 ? "Matig" : "Zwak";

  // Waarschuwing als simulatiedata in de log aanwezig was
  let simWaarschuwing = "";
  if (simS0Rows > 0 || simP1Rows > 0) {
    simWaarschuwing =
      "\n⚠️  SIMULATIEDATA AANWEZIG IN LOG:\n" +
      (simS0Rows > 0 ? "  S0 (solar/SCH): " + simS0Rows + " metingen gesimuleerd\n" : "") +
      (simP1Rows > 0 ? "  P1 (WON):       " + simP1Rows + " metingen gesimuleerd\n" : "") +
      "  Schakel simulatie uit via http://192.168.0.73/settings\n";
  }

  MailApp.sendEmail({
    to: "filip.delannoy@gmail.com",
    subject: "⚡ S-ENERGY Dagelijkse Samenvatting — " +
      Utilities.formatDate(today, "Europe/Brussels", "dd/MM/yyyy"),
    body:
      "=== S-ENERGY DAGELIJKSE SAMENVATTING ===\n\n" +

      "📊 Data collectie:\n" +
      "  Metingen: " + count + "/" + expected + " (" + pct + "%)\n" +
      "  Status:   " + collectieStatus + "\n\n" +

      "☀️ Zonne-energie (SCH):\n" +
      "  Opbrengst vandaag:  " + maxSolarKwh.toFixed(2) + " kWh\n" +
      "  Piek vermogen:      " + maxSolarW + " W\n\n" +

      "⚡ SCH (Schuur):\n" +
      "  Afname vandaag:     " + schAfnameKwh.toFixed(2) + " kWh\n" +
      "  Injectie vandaag:   " + schInjectKwh.toFixed(2) + " kWh\n\n" +

      "🏠 WON (Woning Maarten):\n" +
      "  Afname vandaag:     " + wonAfnameKwh.toFixed(2) + " kWh\n" +
      "  Injectie vandaag:   " + wonInjectKwh.toFixed(2) + " kWh\n\n" +

      "💰 EPEX:\n" +
      "  Gemiddeld vandaag:  " + avgEpex.toFixed(2) + " ct/kWh\n\n" +

      "📊 Maandpiek:\n" +
      "  Hoogste piek:       " + maxPiekW + " W\n\n" +

      "📡 Systeem:\n" +
      "  Gem. WiFi signaal:  " + avgRssi.toFixed(0) + " dBm (" + rssiStatus + ")\n" +
      "  Min heap block:     " + minHeapKb + " KB " + heapStatus + "\n" +

      simWaarschuwing +

      "\nBekijk volledige data: " +
      SpreadsheetApp.getActiveSpreadsheet().getUrl() + "\n"
  });

  Logger.log("Dagelijkse samenvatting verstuurd. count=" + count +
    " simS0=" + simS0Rows + " simP1=" + simP1Rows);
}
