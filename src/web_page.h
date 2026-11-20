#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>UAV Harvester — OLED Mirror HUD</title>
  <style>
    :root {
      --oled-blue: #38bdf8;
      --oled-cyan: #00f5ff;
      --oled-white: #ffffff;
      --oled-amber: #fbbf24;
      --oled-green: #34d399;
      --oled-bg: #030712;
      --bezel: #111827;
      --card-bg: #0f172a;
      --card-border: #1e293b;
      --text-sub: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
    body {
      background: #020617;
      color: #f8fafc;
      font-family: 'Courier New', Courier, monospace;
      padding: 12px;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    .header-bar {
      width: 100%;
      max-width: 480px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      padding: 6px 10px;
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 10px;
    }
    .header-title { font-size: 0.85rem; font-weight: bold; color: var(--oled-cyan); letter-spacing: 1px; }
    .badge { font-size: 0.7rem; font-weight: bold; padding: 3px 8px; border-radius: 6px; background: #1e293b; color: var(--text-sub); }
    .badge.active { background: #065f46; color: #34d399; animation: pulse 1.5s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 0.7; } 50% { opacity: 1; } }

    .time-card {
      width: 100%;
      max-width: 480px;
      background: linear-gradient(135deg, #1e1b4b 0%, #0f172a 100%);
      border: 1px solid #4338ca;
      border-radius: 12px;
      padding: 12px 16px;
      margin-bottom: 12px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 4px 16px rgba(67, 56, 202, 0.25);
    }
    .time-meta { display: flex; flex-direction: column; }
    .time-label { font-size: 0.68rem; color: #a5b4fc; text-transform: uppercase; letter-spacing: 1px; font-weight: bold; }
    .time-val { font-size: 1.5rem; font-weight: 900; color: #38bdf8; font-family: monospace; }
    .time-status { font-size: 0.72rem; color: var(--oled-green); font-weight: bold; }

    .oled-chassis {
      width: 100%;
      max-width: 480px;
      background: var(--bezel);
      padding: 14px;
      border-radius: 16px;
      border: 2px solid #334155;
      box-shadow: 0 10px 30px rgba(0,0,0,0.8), inset 0 2px 4px rgba(255,255,255,0.1);
      margin-bottom: 14px;
    }
    .chassis-label {
      font-size: 0.65rem;
      color: #64748b;
      letter-spacing: 1px;
      text-transform: uppercase;
      text-align: center;
      margin-bottom: 8px;
    }

    .oled-screen {
      background: var(--oled-bg);
      border: 3px solid #000;
      border-radius: 8px;
      aspect-ratio: 128 / 64;
      width: 100%;
      position: relative;
      padding: 10px 14px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      box-shadow: inset 0 0 20px rgba(0,0,0,0.9), 0 0 15px rgba(56, 189, 248, 0.15);
      overflow: hidden;
    }
    .oled-screen::after {
      content: " ";
      display: block;
      position: absolute;
      top: 0; left: 0; bottom: 0; right: 0;
      background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
      background-size: 100% 3px;
      z-index: 2;
      pointer-events: none;
      opacity: 0.6;
    }

    .view-container { width: 100%; height: 100%; display: flex; flex-direction: column; justify-content: space-between; z-index: 1; }
    
    .oled-head {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      font-weight: bold;
      color: var(--oled-white);
      border-bottom: 1px solid rgba(255,255,255,0.4);
      padding-bottom: 4px;
    }

    .oled-cell-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 0.85rem;
      color: var(--oled-white);
      margin: 2px 0;
    }
    .oled-cell-label { width: 75px; }
    .oled-bar-box {
      flex: 1;
      height: 10px;
      border: 1px solid var(--oled-white);
      margin: 0 8px;
      padding: 1px;
    }
    .oled-bar-fill {
      height: 100%;
      width: 0%;
      background: var(--oled-white);
      transition: width 0.2s;
    }

    .oled-foot {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      font-weight: bold;
      color: var(--oled-white);
      border-top: 1px solid rgba(255,255,255,0.4);
      padding-top: 4px;
    }

    .chg-active-banner {
      background: var(--oled-white);
      color: #000;
      text-align: center;
      font-size: 0.8rem;
      font-weight: 900;
      padding: 2px 0;
      letter-spacing: 1px;
    }
    .chg-hero-mid {
      display: flex;
      align-items: center;
      justify-content: space-around;
      margin: 4px 0;
    }
    .chg-big-mult {
      font-size: 3.6rem;
      font-weight: 900;
      line-height: 1;
      color: var(--oled-white);
      text-shadow: 0 0 10px rgba(255,255,255,0.8);
    }
    .chg-meta-text {
      text-align: left;
      font-size: 0.8rem;
      font-weight: bold;
      color: var(--oled-white);
    }
    .oled-power-bar {
      height: 12px;
      border: 1px solid var(--oled-white);
      padding: 1px;
      width: 100%;
      margin: 2px 0;
    }
    .oled-power-fill {
      height: 100%;
      width: 0%;
      background: var(--oled-white);
      box-shadow: 0 0 8px #fff;
      transition: width 0.2s;
    }
    .chg-foot-stats {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      font-weight: bold;
      color: var(--oled-white);
    }

    .no-batt-box {
      text-align: center;
      padding: 12px 0;
      color: var(--oled-white);
    }
    .no-batt-title { font-size: 1rem; font-weight: bold; margin-bottom: 6px; }
    .no-batt-sub { font-size: 0.75rem; color: #cbd5e1; }

    .info-card {
      width: 100%;
      max-width: 480px;
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 14px;
      margin-bottom: 10px;
    }
    .info-title { font-size: 0.72rem; color: var(--text-sub); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; font-weight: bold; }
    .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .stat-box { background: #090d16; border: 1px solid var(--card-border); border-radius: 8px; padding: 10px; }
    .stat-label { font-size: 0.65rem; color: var(--text-sub); }
    .stat-val { font-size: 1.25rem; font-weight: bold; color: var(--oled-cyan); margin-top: 2px; }

    footer { font-size: 0.7rem; color: #64748b; margin-top: 8px; text-align: center; }
  </style>
</head>
<body>

  <div class="header-bar">
    <span class="header-title">UAV TELEMETRY HUD</span>
    <span id="badge" class="badge">CONNECTING...</span>
  </div>

  <div class="time-card">
    <div class="time-meta">
      <span class="time-label">Estimated Time To Full Charge</span>
      <span id="time-val" class="time-val">--</span>
    </div>
    <div id="time-status" class="time-status">STANDBY</div>
  </div>

  <div class="oled-chassis">
    <div class="chassis-label">SSD1306 OLED (128 x 64) LIVE MIRROR</div>
    
    <div class="oled-screen">
      
      <!-- VIEW 1: NO BATTERY -->
      <div id="view-nobatt" class="view-container" style="display: none;">
        <div class="oled-head">
          <span>3S MONITOR</span>
          <span>[NO BATT]</span>
        </div>
        <div class="no-batt-box">
          <div class="no-batt-title">NO BATTERY DETECTED</div>
          <div class="no-batt-sub">Connect 3S Balance Lead</div>
        </div>
        <div class="oled-foot">
          <span id="nobatt-chg-text">CHG: 0.00V [IDLE]</span>
          <span>192.168.4.1</span>
        </div>
      </div>

      <!-- VIEW 2: FULL SCREEN CHARGING (x1 for 0.2V-1.0V) -->
      <div id="view-charging" class="view-container" style="display: none;">
        <div class="chg-active-banner">>>> CHARGING ACTIVE <<<</div>
        <div class="chg-hero-mid">
          <div id="oled-big-mult" class="chg-big-mult">x1</div>
          <div class="chg-meta-text">
            <div id="oled-chg-label">HARVEST x1</div>
            <div style="font-size: 0.7rem; margin-top: 4px; color: #94a3b8;">ENERGY IN</div>
          </div>
        </div>
        <div class="oled-power-bar">
          <div id="oled-power-fill" class="oled-power-fill"></div>
        </div>
        <div class="chg-foot-stats">
          <span>IN:<span id="oled-chg-vin">0.45</span>V</span>
          <span>BAT:<span id="oled-chg-vbat">12.20</span>V</span>
        </div>
      </div>

      <!-- VIEW 3: HOMESCREEN (All Cells) -->
      <div id="view-home" class="view-container">
        <div class="oled-head">
          <span>3S MONITOR</span>
          <span>192.168.4.1</span>
        </div>
        <div style="margin: 2px 0;">
          <div class="oled-cell-row">
            <span class="oled-cell-label" id="oled-c1-txt">C1: 0.00V</span>
            <div class="oled-bar-box"><div id="oled-c1-bar" class="oled-bar-fill"></div></div>
          </div>
          <div class="oled-cell-row">
            <span class="oled-cell-label" id="oled-c2-txt">C2: 0.00V</span>
            <div class="oled-bar-box"><div id="oled-c2-bar" class="oled-bar-fill"></div></div>
          </div>
          <div class="oled-cell-row">
            <span class="oled-cell-label" id="oled-c3-txt">C3: 0.00V</span>
            <div class="oled-bar-box"><div id="oled-c3-bar" class="oled-bar-fill"></div></div>
          </div>
        </div>
        <div class="oled-foot">
          <span>TOT:<span id="oled-tot-txt">0.00V</span></span>
          <span id="oled-pct-txt">0%</span>
        </div>
      </div>

    </div>
  </div>

  <div class="info-card">
    <div class="info-title">System Telemetry</div>
    <div class="stats-grid">
      <div class="stat-box">
        <div class="stat-label">HARVESTING VOLTAGE</div>
        <div id="stat-vin" class="stat-val">0.00 V</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">HARVESTING MULTIPLIER</div>
        <div id="stat-mult" class="stat-val" style="color: var(--oled-amber);">x0</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">3S PACK TOTAL</div>
        <div id="stat-total" class="stat-val" style="color: var(--oled-green);">0.00 V</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">BATTERY HEALTH</div>
        <div id="stat-health" class="stat-val">--%</div>
      </div>
    </div>
  </div>

  <footer>ESP32 UAV Harvester &bull; Wi-Fi: UAV-Energy-Monitor</footer>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/data');
        const d = await res.json();
        
        const badge = document.getElementById('badge');
        if (d.isCharging) {
          badge.textContent = "CHARGING";
          badge.className = "badge active";
        } else {
          badge.textContent = "ONLINE (IDLE)";
          badge.className = "badge";
        }

        const timeVal = document.getElementById('time-val');
        const timeStatus = document.getElementById('time-status');
        if (d.isCharging && d.battConnected) {
          timeVal.textContent = d.chargeTime;
          timeStatus.textContent = "CHARGING ACTIVE";
          timeStatus.style.color = "var(--oled-green)";
        } else if (!d.battConnected) {
          timeVal.textContent = "--";
          timeStatus.textContent = "NO BATTERY";
          timeStatus.style.color = "#ef4444";
        } else {
          timeVal.textContent = "--";
          timeStatus.textContent = "NOT CHARGING (IDLE)";
          timeStatus.style.color = "#94a3b8";
        }

        const vNoBatt = document.getElementById('view-nobatt');
        const vChg = document.getElementById('view-charging');
        const vHome = document.getElementById('view-home');

        if (!d.battConnected) {
          vNoBatt.style.display = "flex";
          vChg.style.display = "none";
          vHome.style.display = "none";
          document.getElementById('nobatt-chg-text').textContent = d.isCharging ? `CHG: ${d.vChg.toFixed(2)}V [${d.multStr}]` : "CHG: 0.00V [IDLE]";
        } else if (d.isCharging) {
          vNoBatt.style.display = "none";
          vChg.style.display = "flex";
          vHome.style.display = "none";

          document.getElementById('oled-big-mult').textContent = d.multStr;
          document.getElementById('oled-chg-label').textContent = d.labelStr;
          document.getElementById('oled-power-fill').style.width = d.strengthPct + "%";
          document.getElementById('oled-chg-vin').textContent = d.vChg.toFixed(2);
          document.getElementById('oled-chg-vbat').textContent = d.total.toFixed(2);
        } else {
          vNoBatt.style.display = "none";
          vChg.style.display = "none";
          vHome.style.display = "flex";

          document.getElementById('oled-c1-txt').textContent = `C1:${d.c1.toFixed(2)}V`;
          document.getElementById('oled-c2-txt').textContent = `C2:${d.c2.toFixed(2)}V`;
          document.getElementById('oled-c3-txt').textContent = d.tap3Ok ? `C3:${d.c3.toFixed(2)}V` : "C3:NO WIRE";

          document.getElementById('oled-c1-bar').style.width = Math.min(100, Math.max(0, (d.c1 - 3.2)/1.0 * 100)) + "%";
          document.getElementById('oled-c2-bar').style.width = Math.min(100, Math.max(0, (d.c2 - 3.2)/1.0 * 100)) + "%";
          document.getElementById('oled-c3-bar').style.width = d.tap3Ok ? Math.min(100, Math.max(0, (d.c3 - 3.2)/1.0 * 100)) + "%" : "0%";

          document.getElementById('oled-tot-txt').textContent = `${d.total.toFixed(2)}V`;
          document.getElementById('oled-pct-txt').textContent = `${d.pct}%`;
        }

        document.getElementById('stat-vin').textContent = d.vChg.toFixed(2) + " V";
        document.getElementById('stat-mult').textContent = d.multStr;
        document.getElementById('stat-total').textContent = d.total.toFixed(2) + " V";
        document.getElementById('stat-health').textContent = d.pct + "%";

      } catch (err) {
        document.getElementById('badge').textContent = "OFFLINE";
        document.getElementById('badge').className = "badge";
      }
    }
    setInterval(updateData, 400);
    updateData();
  </script>
</body>
</html>
)rawliteral";
