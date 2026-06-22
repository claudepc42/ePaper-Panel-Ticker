#pragma once

// Single-page captive portal served from the ESP32's WebServer.
// Matches the design (dark theme, step indicator, mobile layout, 420px max-width).
// Self-contained: no external font or script dependencies (offline AP network).
// Fonts fall back to system monospace/sans because Google Fonts won't load
// on a phone disconnected from the internet and connected to the device AP.

static const char PORTAL_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stock Ticker Setup</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{min-height:100vh;background:#0f0f0f;font-family:system-ui,-apple-system,sans-serif;color:#e8e4dc;font-size:15px}
input,button,select{-webkit-appearance:none;appearance:none;outline:none;font-family:inherit;font-size:inherit}
.wrap{max-width:420px;margin:0 auto;min-height:100vh;display:flex;flex-direction:column}

/* ── Step header ── */
.step-hdr{padding:16px 24px 14px;border-bottom:1px solid #1a1a1a;background:#0a0a0a;flex:0 0 auto}
.step-label{font-family:monospace;font-size:9px;color:#383838;letter-spacing:.18em;text-transform:uppercase;margin-bottom:12px}
.steps{display:flex;align-items:center}
.step-item{display:flex;align-items:center;gap:7px;flex:0 0 auto}
.step-circle{width:24px;height:24px;border-radius:50%;display:flex;align-items:center;justify-content:center;flex:0 0 24px;font-family:monospace;font-size:10px;font-weight:700;line-height:1}
.step-name{font-family:monospace;font-size:9px;letter-spacing:.08em;white-space:nowrap}
.step-line{flex:1;height:1px;margin:0 8px}

/* ── Page content ── */
.content{flex:1;padding:28px 24px 56px;display:flex;flex-direction:column}

/* ── Form elements ── */
.field-label{font-family:monospace;font-size:10px;font-weight:700;letter-spacing:.14em;color:#8a8a8a;margin-bottom:8px;display:block;text-transform:uppercase}
.text-input{width:100%;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:4px;padding:14px 16px;font-size:15px;color:#e8e4dc}
.row-pair{display:flex;gap:8px}
.row-pair .text-input{flex:1}
.btn-icon{background:#141414;border:1px solid #2a2a2a;border-radius:4px;padding:0 14px;font-family:monospace;font-size:10px;letter-spacing:.08em;color:#8a8a8a;cursor:pointer;white-space:nowrap}

.btn-primary{width:100%;background:#f0ece3;color:#0f0f0f;border:none;border-radius:4px;padding:16px;font-size:15px;font-weight:600;cursor:pointer}
.btn-primary:disabled{background:#1a1a1a;color:#3a3a3a;cursor:default}
.btn-secondary{background:transparent;color:#8a8a8a;border:1px solid #2a2a2a;border-radius:4px;padding:14px 18px;font-size:14px;cursor:pointer}
.btn-toggle{flex:1;background:#1a1a1a;color:#8a8a8a;border:1px solid #2a2a2a;border-radius:4px;padding:12px;font-size:13px;font-weight:500;cursor:pointer}
.btn-toggle.active{background:#f0ece3;color:#0f0f0f;border-color:#f0ece3}

.scan-btn{background:transparent;border:none;color:#8a8a8a;font-size:12px;font-family:monospace;letter-spacing:.06em;cursor:pointer;padding:10px 0 4px;text-transform:uppercase}
.net-list{background:#141414;border:1px solid #242424;border-radius:4px;overflow:hidden;margin-bottom:4px}
.net-item{display:flex;justify-content:space-between;align-items:center;padding:13px 16px;border-bottom:1px solid #1e1e1e;cursor:pointer}
.net-item:last-child{border-bottom:none}
.net-item:hover{background:#1a1a1a}
.net-signal{font-family:monospace;font-size:12px;color:#8a8a8a}

.mb8{margin-bottom:8px}.mb12{margin-bottom:12px}.mb16{margin-bottom:16px}.mb20{margin-bottom:20px}.mb22{margin-bottom:22px}
.mt-auto{margin-top:auto}.pt12{padding-top:12px}.pt20{padding-top:20px}

/* ── Ticker list ── */
.ticker-card{background:#141414;border:1px solid #242424;border-radius:4px;margin-bottom:8px;overflow:hidden}
.ticker-top{display:flex;align-items:center;padding:12px 16px 8px;gap:12px}
.ticker-sym{font-family:monospace;font-size:16px;font-weight:700;color:#f0ece3;flex:1}
.ticker-btns{display:flex;gap:8px;padding:0 16px 12px}
.btn-mode{flex:1;background:#1a1a1a;color:#8a8a8a;border:1px solid #2a2a2a;border-radius:3px;padding:9px;font-family:monospace;font-size:10px;letter-spacing:.1em;cursor:pointer}
.btn-mode.active{background:#f0ece3;color:#0f0f0f;border-color:#f0ece3}
.btn-move{background:transparent;border:1px solid #242424;border-radius:3px;padding:5px 10px;color:#8a8a8a;font-size:12px;cursor:pointer;line-height:1}
.btn-move:disabled{color:#2a2a2a;cursor:default}
.btn-remove{background:transparent;border:none;color:#8a8a8a;font-size:18px;cursor:pointer;padding:0 0 0 6px;line-height:1}

.empty-tickers{border:1px dashed #2a2a2a;border-radius:4px;padding:24px;text-align:center;margin-bottom:12px;color:#383838;font-size:13px;line-height:1.6}
.ticker-err{font-family:monospace;font-size:10px;color:#b84040;letter-spacing:.06em;margin-bottom:10px}

/* ── Rotation interval ── */
.interval-box{padding:16px;background:#141414;border:1px solid #1e1e1e;border-radius:4px;margin-bottom:8px}
.interval-row{display:flex;align-items:center;gap:12px;margin-bottom:6px}
.interval-pair{display:flex;align-items:center;gap:8px}
.num-input{width:64px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:4px;padding:10px;font-size:16px;color:#e8e4dc;font-family:monospace;text-align:center}
.unit-label{font-size:13px;color:#8a8a8a}
.interval-err{font-family:monospace;font-size:10px;color:#b84040;letter-spacing:.06em;margin-top:4px}
.interval-hint{font-size:12px;color:#8a8a8a;margin-top:4px}

/* ── WiFi error ── */
.wifi-err{background:#1c0f0f;border:1px solid #3a1a1a;border-radius:4px;padding:12px 16px;margin-bottom:16px}
.wifi-err-title{font-family:monospace;font-size:10px;font-weight:700;letter-spacing:.1em;color:#b84040;margin-bottom:5px}
.wifi-err-body{font-size:13px;color:#7a5252;line-height:1.5}

/* ── Testing state ── */
.testing-box{background:#141414;border:1px solid #242424;border-radius:4px;padding:16px;display:flex;align-items:center;justify-content:center;gap:10px;font-family:monospace;font-size:11px;letter-spacing:.1em;color:#8a8a8a}

/* ── Review page ── */
.review-card{background:#141414;border:1px solid #1e1e1e;border-radius:6px;padding:16px;margin-bottom:10px}
.review-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}
.review-hdr{font-family:monospace;font-size:10px;font-weight:700;letter-spacing:.12em;color:#8a8a8a}
.edit-btn{background:transparent;border:none;font-family:monospace;font-size:9px;letter-spacing:.08em;color:#8a8a8a;cursor:pointer;text-decoration:underline}
.review-val{font-size:15px;color:#f0ece3;font-weight:500}
.review-sub{font-family:monospace;font-size:12px;color:#8a8a8a}
.review-ticker-row{display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-bottom:1px solid #1e1e1e}
.review-ticker-sym{font-family:monospace;font-size:14px;font-weight:700;color:#f0ece3}
.review-ticker-mode{font-family:monospace;font-size:10px;color:#8a8a8a;letter-spacing:.08em}

/* ── Advanced section ── */
.advanced-card{background:#141414;border:1px solid #1e1e1e;border-radius:6px;margin-bottom:22px;overflow:hidden}
.advanced-toggle{width:100%;display:flex;justify-content:space-between;align-items:center;padding:14px 16px;background:transparent;border:none;cursor:pointer}
.advanced-toggle-label{font-family:monospace;font-size:10px;font-weight:700;letter-spacing:.12em;color:#8a8a8a}
.advanced-icon{font-family:monospace;font-size:11px;color:#8a8a8a}
.advanced-inner{padding:4px 16px 20px;border-top:1px solid #1e1e1e}
.advanced-section{margin-top:16px;margin-bottom:18px}
.adv-label{font-family:monospace;font-size:10px;font-weight:700;letter-spacing:.12em;color:#8a8a8a;margin-bottom:8px;display:block}
.adv-hint{font-size:12px;color:#8a8a8a;line-height:1.5;margin-top:6px}

/* ── Power mode ── */
.power-card{padding:14px 16px;border-radius:4px;margin-bottom:8px;cursor:pointer;background:#0f0f0f;border:1px solid #242424}
.power-card.active{border:2px solid #f0ece3}
.power-title{font-size:14px;font-weight:600;color:#f0ece3;margin-bottom:4px}
.power-desc{font-size:12px;color:#8a8a8a;line-height:1.4}

/* ── Saving page ── */
.saving-wrap{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;padding:0 16px}
.saving-bullet{font-family:monospace;font-size:36px;color:#f0ece3;margin-bottom:20px;line-height:1}
.saving-title{font-size:24px;font-weight:700;color:#f0ece3;margin-bottom:12px}
.saving-detail{font-size:14px;color:#7a7672;max-width:300px;line-height:1.7;margin-bottom:32px}
.saving-done-box{border:1px solid #2a2a2a;border-radius:4px;padding:14px 24px;font-family:monospace;font-size:11px;color:#8a8a8a;letter-spacing:.08em}

/* ── Page visibility ── */
.page{display:none}.page.active{display:flex;flex-direction:column;flex:1}
h1{font-size:22px;font-weight:700;color:#f0ece3;margin-bottom:6px;line-height:1.2}
.subtitle{font-size:14px;color:#7a7672;line-height:1.5}
.divider{height:1px;background:#1e1e1e;margin-bottom:20px}
</style>
</head>
<body>
<div class="wrap">

  <!-- ── Step header ── -->
  <div class="step-hdr" id="stepHdr">
    <div class="step-label" id="pageLabel">ePaperTicker SETUP</div>
    <div class="steps">
      <div class="step-item">
        <div class="step-circle" id="s1circle">1</div>
        <span class="step-name" id="s1name">WIFI</span>
      </div>
      <div class="step-line" id="c1" style="background:#242424"></div>
      <div class="step-item">
        <div class="step-circle" id="s2circle">2</div>
        <span class="step-name" id="s2name">TICKERS</span>
      </div>
      <div class="step-line" id="c2" style="background:#242424"></div>
      <div class="step-item">
        <div class="step-circle" id="s3circle">3</div>
        <span class="step-name" id="s3name">REVIEW</span>
      </div>
    </div>
  </div>

  <div class="content">

    <!-- ══ PAGE: WIFI ══ -->
    <div class="page" id="pageWifi">
      <div class="mb22">
        <h1 id="wifiTitle">Connect to WiFi</h1>
        <p class="subtitle" id="wifiSub">Connect this device to your home WiFi network.</p>
      </div>

      <div class="mb16">
        <label class="field-label">NETWORK</label>
        <input type="text" class="text-input mb8" id="ssidInput" placeholder="WiFi network name" autocomplete="off">
        <button class="scan-btn" id="scanBtn" onclick="toggleScan()">▸ &nbsp;Scan nearby networks</button>
        <div id="netList" class="net-list" style="display:none"></div>
      </div>

      <div class="mb20">
        <label class="field-label">PASSWORD</label>
        <div class="row-pair">
          <input type="password" class="text-input" id="passInput" placeholder="Network password" autocomplete="off">
          <button class="btn-icon" onclick="togglePass()">SHOW</button>
        </div>
      </div>

      <div id="wifiErrBox" style="display:none" class="wifi-err">
        <div class="wifi-err-title">CONNECTION FAILED</div>
        <div class="wifi-err-body" id="wifiErrMsg"></div>
      </div>

      <div class="mt-auto pt12">
        <div id="testingBox" class="testing-box" style="display:none">
          TESTING CONNECTION<span id="dots"></span>
        </div>
        <button class="btn-primary" id="wifiContinueBtn" disabled onclick="testWifi()">Continue →</button>
      </div>
    </div>

    <!-- ══ PAGE: TICKERS ══ -->
    <div class="page" id="pageTickers">
      <div class="mb22">
        <h1>Configure Tickers</h1>
        <p class="subtitle">Add the markets and funds you want to track.</p>
      </div>

      <div class="mb8">
        <label class="field-label">DATA PROVIDER</label>
        <div class="row-pair mb12">
          <button class="btn-toggle active" id="btnTD" onclick="setProvider('twelve-data')">Twelve Data</button>
          <button class="btn-toggle" id="btnFH" onclick="setProvider('finnhub')">Finnhub</button>
        </div>
      </div>

      <div class="mb20">
        <label class="field-label" id="apiKeyLabel">TWELVE DATA API KEY</label>
        <div class="row-pair mb8">
          <input type="password" class="text-input" id="apiKeyInput" placeholder="Your API key" autocomplete="off">
          <button class="btn-icon" onclick="toggleApiKey()">SHOW</button>
        </div>
        <div class="row-pair mb8">
          <button class="btn-primary" id="apiTestBtn" style="width:auto;padding:0 18px;font-size:14px" onclick="testApi()">Test key</button>
          <span id="apiTestStatus" style="font-size:12px;color:#8a8a8a;align-self:center">Not tested</span>
        </div>
        <div class="ticker-err" id="apiErrBox" style="display:none"><span id="apiErrMsg"></span></div>
        <div style="font-size:12px;color:#8a8a8a" id="apiKeyHelp">Get a free key at twelvedata.com</div>
      </div>

      <div class="divider"></div>
      <label class="field-label">TICKERS</label>

      <div class="row-pair mb10">
        <input type="text" class="text-input" id="newTickerInput" placeholder="e.g. AAPL" autocomplete="off"
               style="font-family:monospace" onkeydown="if(event.key==='Enter')addTicker()">
        <button class="btn-primary" style="width:auto;padding:0 18px;font-size:14px" onclick="addTicker()">+ Add</button>
      </div>
      <div class="ticker-err" id="tickerErr" style="display:none"></div>

      <div id="emptyTickers" class="empty-tickers">
        No tickers added yet.<br>Add at least one to continue.
      </div>
      <div id="tickerList"></div>

      <div id="rotSection" style="display:none" class="interval-box">
        <label class="field-label">ROTATION INTERVAL</label>
        <div class="interval-row">
          <div class="interval-pair">
            <input type="number" class="num-input" id="rotH" value="0" min="0" max="99">
            <span class="unit-label">hrs</span>
          </div>
          <div class="interval-pair">
            <input type="number" class="num-input" id="rotM" value="30" min="0" max="59">
            <span class="unit-label">min</span>
          </div>
        </div>
        <div class="interval-err" id="rotErr" style="display:none"></div>
        <div class="interval-hint" id="rotHint">Minimum 15 minutes total.</div>
      </div>

      <div class="row-pair mt-auto pt20">
        <button class="btn-secondary" onclick="goBack()">← Back</button>
        <button class="btn-primary" id="tickersContinueBtn" disabled onclick="goToReview()" style="flex:1">Review →</button>
      </div>
    </div>

    <!-- ══ PAGE: REVIEW ══ -->
    <div class="page" id="pageReview">
      <div class="mb22">
        <h1>Review Settings</h1>
        <p class="subtitle">Confirm everything looks right before saving.</p>
      </div>

      <div class="review-card">
        <div class="review-row">
          <span class="review-hdr">WIFI NETWORK</span>
          <button class="edit-btn" onclick="showPage('wifi')">EDIT</button>
        </div>
        <div class="review-val" id="rvSSID">—</div>
      </div>

      <div class="review-card">
        <div class="review-hdr mb8">DATA PROVIDER</div>
        <div class="review-val" id="rvProvider">Twelve Data</div>
        <div class="review-sub" id="rvKey">API key: not entered</div>
      </div>

      <div class="review-card">
        <div class="review-row">
          <span class="review-hdr">TICKERS</span>
          <button class="edit-btn" onclick="showPage('tickers')">EDIT</button>
        </div>
        <div id="rvTickerList"></div>
        <div style="font-size:12px;color:#8a8a8a;margin-top:10px" id="rvRotSummary" style="display:none"></div>
      </div>

      <div class="advanced-card">
        <button class="advanced-toggle" onclick="toggleAdv()">
          <span class="advanced-toggle-label">ADVANCED</span>
          <span class="advanced-icon" id="advIcon">▸</span>
        </button>
        <div class="advanced-inner" id="advInner" style="display:none">

          <div class="advanced-section">
            <label class="adv-label">BOARD / DISPLAY TARGET</label>
            <div class="row-pair mb8">
              <button class="btn-toggle active" id="btnXiao" onclick="setBoard('xiao-panel')">XIAO Panel</button>
              <button class="btn-toggle" id="btnRT1" onclick="setBoard('reterminal-e1001')">reTerminal E1001</button>
            </div>
            <p class="adv-hint">Choose the hardware you're using. This only needs to be set once.</p>
          </div>

          <div class="advanced-section" id="panelVariantSection">
            <label class="adv-label">DISPLAY PANEL VARIANT</label>
            <div class="row-pair mb8">
              <button class="btn-toggle active" id="btnPA" onclick="setPanelVariant('A')">Panel A</button>
              <button class="btn-toggle" id="btnPB" onclick="setPanelVariant('B')">Panel B</button>
            </div>
            <p class="adv-hint">If your display shows inverted colors or garbled output, switch to the other option and save again.</p>
          </div>

          <div class="advanced-section">
            <label class="adv-label">DATA REFRESH INTERVAL</label>
            <div class="interval-row">
              <div class="interval-pair">
                <input type="number" class="num-input" id="refH" value="1" min="0" max="99">
                <span class="unit-label">hrs</span>
              </div>
              <div class="interval-pair">
                <input type="number" class="num-input" id="refM" value="0" min="0" max="59">
                <span class="unit-label">min</span>
              </div>
            </div>
            <div class="interval-err" id="refErr" style="display:none"></div>
            <div class="interval-hint" id="refHint">Minimum 15 min. Recommended: 1 hour.</div>
          </div>

          <div class="advanced-section" style="margin-bottom:0">
            <label class="adv-label">POWER MODE</label>
            <div class="power-card active" id="pcBatterySaver" onclick="setPowerMode('battery-saver')">
              <div class="power-title">Battery Saver</div>
              <div class="power-desc">Device sleeps deeply between refreshes for maximum battery life. Recommended for most setups.</div>
            </div>
            <div class="power-card" id="pcQuickWake" onclick="setPowerMode('quick-wake')">
              <div class="power-title">Quick Wake</div>
              <div class="power-desc">Wakes faster between refreshes, uses more battery. Choose if faster updates matter more than battery life.</div>
            </div>
          </div>

        </div>
      </div>

      <button class="btn-primary mb10" onclick="saveAndRestart()">Save &amp; Restart</button>
      <button class="btn-secondary" style="width:100%;text-align:center" onclick="goBack()">← Back</button>
    </div>

    <!-- ══ PAGE: SAVING ══ -->
    <div class="page" id="pageSaving">
      <div class="saving-wrap">
        <div class="saving-bullet" id="savingBullet">◌</div>
        <div class="saving-title" id="savingTitle">Saving…</div>
        <div class="saving-detail" id="savingDetail">Applying settings and restarting the device. This takes about 10–15 seconds.</div>
        <div class="saving-done-box" id="savingDone" style="display:none">You can safely close this page.</div>
      </div>
    </div>

  </div><!-- /content -->
</div><!-- /wrap -->

<script>
// ── State ──────────────────────────────────────────────────────────────────
var state = {
  page: 'wifi',
  ssid: '', pass: '', wifiOk: false,
  provider: 'twelve-data', apiKey: '', apiKeyOk: false,
  tickers: [
    {id:1, symbol:'NVDA',  mode:'static'},
    {id:2, symbol:'MSFT',  mode:'rotating'},
    {id:3, symbol:'VUG',   mode:'static'},
    {id:4, symbol:'AAPL',  mode:'rotating'},
    {id:5, symbol:'AMD',   mode:'rotating'},
    {id:6, symbol:'VOO',   mode:'rotating'},
    {id:7, symbol:'VTI',   mode:'rotating'},
    {id:8, symbol:'VGT',   mode:'rotating'},
    {id:9, symbol:'BND',   mode:'rotating'},
    {id:10, symbol:'GOOGL', mode:'rotating'}
  ], nextId: 11,
  rotH: 0, rotM: 30,
  board: 'xiao-panel', panelVariant: 'A',
  refH: 1, refM: 0,
  powerMode: 'battery-saver',
  showNetworks: false, showAdvanced: false,
  dots: '', dotsTimer: null
};

// ── Bootstrap (check if reconfiguring) ────────────────────────────────────
window.onload = function() {
  fetchConfig();
  startDots();
  showPage('wifi');
  syncSSIDInput();
  renderTickers();  // paint the pre-populated default ticker list
};

function startDots() {
  state.dotsTimer = setInterval(function() {
    state.dots = state.dots.length >= 3 ? '' : state.dots + '.';
    var d = document.getElementById('dots');
    if (d) d.textContent = state.dots;
  }, 400);
}

// Fetch current NVS config to pre-populate fields (reconfigure path, PRD §8.6)
function fetchConfig() {
  fetch('/config').then(function(r){ return r.json(); }).then(function(cfg) {
    if (cfg.ssid) {
      state.ssid = cfg.ssid; state.wifiOk = true;
      document.getElementById('ssidInput').value = cfg.ssid;
      document.getElementById('pageLabel').textContent = 'UPDATE SETTINGS';
      document.getElementById('wifiTitle').textContent = 'Update WiFi Network';
      document.getElementById('wifiSub').textContent = 'Change the network this device connects to.';
      syncContinueBtn();
    }
    if (cfg.provider) setProvider(cfg.provider);
    if (cfg.apiKey)   {
      state.apiKey = cfg.apiKey;
      document.getElementById('apiKeyInput').value = cfg.apiKey;
      // Trust previously-saved key; user can re-test if they want
      state.apiKeyOk = true;
      var st = document.getElementById('apiTestStatus');
      if (st) { st.textContent = 'Previously saved'; st.style.color = '#8a8a8a'; }
    }
    if (cfg.tickers && cfg.tickers.length > 0) {
      state.tickers = []; state.nextId = 1;  // clear defaults before loading saved config
      cfg.tickers.forEach(function(t){ addTickerFromConfig(t.symbol, t.mode); });
    }
    if (cfg.rotH !== undefined) { state.rotH = cfg.rotH; document.getElementById('rotH').value = cfg.rotH; }
    if (cfg.rotM !== undefined) { state.rotM = cfg.rotM; document.getElementById('rotM').value = cfg.rotM; }
    if (cfg.board)    setBoard(cfg.board);
    if (cfg.panelVariant) setPanelVariant(cfg.panelVariant);
    if (cfg.refH !== undefined) { document.getElementById('refH').value = cfg.refH; }
    if (cfg.refM !== undefined) { document.getElementById('refM').value = cfg.refM; }
    if (cfg.powerMode) setPowerMode(cfg.powerMode);
  }).catch(function(){});
}

// ── Page navigation ────────────────────────────────────────────────────────
function showPage(p) {
  state.page = p;
  ['wifi','tickers','review','saving'].forEach(function(id) {
    document.getElementById('page'+cap(id)).classList.toggle('active', id === p);
  });
  document.getElementById('stepHdr').style.display = (p === 'saving') ? 'none' : '';
  updateStepIndicator();
  if (p === 'review') populateReview();
}

function cap(s) { return s.charAt(0).toUpperCase() + s.slice(1); }

function updateStepIndicator() {
  var p = state.page;
  var n = p === 'wifi' ? 1 : p === 'tickers' ? 2 : 3;
  for (var i = 1; i <= 3; i++) {
    var circ = document.getElementById('s'+i+'circle');
    var name = document.getElementById('s'+i+'name');
    if (n > i)      { circ.style.background='#2c2c2c'; circ.style.color='#888'; circ.textContent='✓'; name.style.color='#8a8a8a'; }
    else if (n===i) { circ.style.background='#f0ece3'; circ.style.color='#0f0f0f'; circ.textContent=i; name.style.color='#f0ece3'; }
    else            { circ.style.background='#141414'; circ.style.color='#383838'; circ.textContent=i; name.style.color='#383838'; }
  }
  document.getElementById('c1').style.background = n > 1 ? '#444' : '#242424';
  document.getElementById('c2').style.background = n > 2 ? '#444' : '#242424';
}

function goBack() {
  if (state.page === 'tickers') showPage('wifi');
  else if (state.page === 'review') showPage('tickers');
}

function goToReview() {
  if (!canProceedTickers()) return;
  showPage('review');
}

// ── WiFi page ──────────────────────────────────────────────────────────────
function syncSSIDInput() {
  document.getElementById('ssidInput').addEventListener('input', function() {
    state.ssid = this.value; state.wifiOk = false; syncContinueBtn();
    document.getElementById('wifiErrBox').style.display = 'none';
  });
  document.getElementById('passInput').addEventListener('input', function() {
    state.pass = this.value;
  });
}

function syncContinueBtn() {
  document.getElementById('wifiContinueBtn').disabled = !state.ssid.trim();
}

function togglePass() {
  var inp = document.getElementById('passInput');
  inp.type = inp.type === 'password' ? 'text' : 'password';
}

function toggleScan() {
  if (state.showNetworks) {
    state.showNetworks = false;
    document.getElementById('netList').style.display = 'none';
    document.getElementById('scanBtn').textContent = '▸  Scan nearby networks';
    return;
  }
  document.getElementById('scanBtn').textContent = 'Scanning…';
  fetch('/scan').then(function(r){ return r.json(); }).then(function(nets) {
    state.showNetworks = true;
    document.getElementById('scanBtn').textContent = '▾  Scan nearby networks';
    var list = document.getElementById('netList');
    list.innerHTML = '';
    nets.forEach(function(n) {
      var row = document.createElement('div');
      row.className = 'net-item';
      row.innerHTML = '<span>' + escHtml(n.ssid) + '</span><span class="net-signal">' + rssiBar(n.rssi) + '</span>';
      row.onclick = function() {
        document.getElementById('ssidInput').value = n.ssid;
        state.ssid = n.ssid; state.wifiOk = false;
        syncContinueBtn();
        state.showNetworks = false;
        list.style.display = 'none';
        document.getElementById('scanBtn').textContent = '▸  Scan nearby networks';
      };
      list.appendChild(row);
    });
    list.style.display = 'block';
  }).catch(function() {
    document.getElementById('scanBtn').textContent = '▸  Scan nearby networks';
  });
}

function rssiBar(rssi) {
  if (rssi > -55) return '▂▄▆█';
  if (rssi > -70) return '▂▄▆░';
  if (rssi > -80) return '▂▄░░';
  return '▂░░░';
}

function testWifi() {
  var ssid = document.getElementById('ssidInput').value.trim();
  if (!ssid) return;
  document.getElementById('wifiErrBox').style.display = 'none';
  document.getElementById('testingBox').style.display = 'flex';
  document.getElementById('wifiContinueBtn').style.display = 'none';

  fetch('/wifi-test', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ssid: ssid, pass: document.getElementById('passInput').value})
  }).then(function(r){ return r.json(); }).then(function(res) {
    document.getElementById('testingBox').style.display = 'none';
    document.getElementById('wifiContinueBtn').style.display = '';
    if (res.ok) {
      state.ssid = ssid; state.pass = document.getElementById('passInput').value;
      state.wifiOk = true;
      showPage('tickers');
    } else {
      document.getElementById('wifiErrMsg').textContent = res.error || 'Connection failed.';
      document.getElementById('wifiErrBox').style.display = 'block';
    }
  }).catch(function() {
    document.getElementById('testingBox').style.display = 'none';
    document.getElementById('wifiContinueBtn').style.display = '';
    document.getElementById('wifiErrMsg').textContent = 'Request timed out. Try again.';
    document.getElementById('wifiErrBox').style.display = 'block';
  });
}

// ── Provider ───────────────────────────────────────────────────────────────
function setProvider(p) {
  state.provider = p;
  document.getElementById('btnTD').classList.toggle('active', p === 'twelve-data');
  document.getElementById('btnFH').classList.toggle('active', p === 'finnhub');
  document.getElementById('apiKeyLabel').textContent = p === 'twelve-data' ? 'TWELVE DATA API KEY' : 'FINNHUB API KEY';
  document.getElementById('apiKeyHelp').textContent  = p === 'twelve-data' ? 'Get a free key at twelvedata.com' : 'Get a free key at finnhub.io';
  document.getElementById('apiKeyInput').placeholder = p === 'twelve-data' ? 'e.g. abc12def34ghi56789' : 'e.g. abc12def34ghi56789jkl';
  resetApiTestStatus();
  syncTickersContinue();
}

function toggleApiKey() {
  var inp = document.getElementById('apiKeyInput');
  inp.type = inp.type === 'password' ? 'text' : 'password';
}

function resetApiTestStatus() {
  state.apiKeyOk = false;
  var s = document.getElementById('apiTestStatus');
  if (s) { s.textContent = 'Not tested'; s.style.color = '#8a8a8a'; }
  var e = document.getElementById('apiErrBox');
  if (e) { e.style.display = 'none'; }
}

function testApi() {
  var key = document.getElementById('apiKeyInput').value.trim();
  if (!key) return;
  if (!state.wifiOk) {
    document.getElementById('apiErrMsg').textContent = 'Test WiFi first.';
    document.getElementById('apiErrBox').style.display = 'block';
    return;
  }
  var btn = document.getElementById('apiTestBtn');
  var status = document.getElementById('apiTestStatus');
  document.getElementById('apiErrBox').style.display = 'none';
  btn.disabled = true;
  status.textContent = 'Testing…'; status.style.color = '#8a8a8a';

  fetch('/api-test', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      ssid: state.ssid, pass: state.pass,
      provider: state.provider, apiKey: key
    })
  }).then(function(r){ return r.json(); }).then(function(res) {
    btn.disabled = false;
    if (res.ok) {
      state.apiKeyOk = true;
      status.textContent = '✓ Key works'; status.style.color = '#4a8a4a';
    } else {
      state.apiKeyOk = false;
      status.textContent = 'Failed'; status.style.color = '#a85050';
      document.getElementById('apiErrMsg').textContent = res.error || 'Test failed.';
      document.getElementById('apiErrBox').style.display = 'block';
    }
    syncTickersContinue();
  }).catch(function() {
    btn.disabled = false;
    state.apiKeyOk = false;
    status.textContent = 'Failed'; status.style.color = '#a85050';
    document.getElementById('apiErrMsg').textContent = 'Request timed out. Try again.';
    document.getElementById('apiErrBox').style.display = 'block';
    syncTickersContinue();
  });
}

document.addEventListener('DOMContentLoaded', function() {
  document.getElementById('apiKeyInput').addEventListener('input', function() {
    state.apiKey = this.value;
    resetApiTestStatus();
    syncTickersContinue();
  });
});

// ── Tickers ────────────────────────────────────────────────────────────────
function addTickerFromConfig(sym, mode) {
  state.tickers.push({id: state.nextId++, symbol: sym.toUpperCase(), mode: mode || 'rotating'});
  renderTickers();
}

function addTicker() {
  var inp = document.getElementById('newTickerInput');
  var sym = inp.value.trim().toUpperCase();
  var err = document.getElementById('tickerErr');
  err.style.display = 'none';
  if (!sym) { err.textContent = 'Enter a ticker symbol'; err.style.display='block'; return; }
  if (state.tickers.find(function(t){ return t.symbol===sym; })) {
    err.textContent = sym + ' is already in your list'; err.style.display='block'; return;
  }
  state.tickers.push({id: state.nextId++, symbol: sym, mode: 'rotating'});
  inp.value = '';
  renderTickers();
}

function renderTickers() {
  var list = document.getElementById('tickerList');
  list.innerHTML = '';
  document.getElementById('emptyTickers').style.display = state.tickers.length ? 'none' : 'block';

  var hasRot = state.tickers.some(function(t, i){ return t.mode==='rotating' || i >= 5; });
  document.getElementById('rotSection').style.display = hasRot ? 'block' : 'none';

  // Insert a divider before the first auto-rotating ticker (index 5+)
  var shownDivider = false;
  state.tickers.forEach(function(t, i) {
    var autoRot = (i >= 5);

    if (autoRot && !shownDivider) {
      shownDivider = true;
      var div = document.createElement('div');
      div.style.cssText = 'font-family:monospace;font-size:9px;letter-spacing:.14em;color:#383838;text-transform:uppercase;padding:10px 2px 6px;';
      div.textContent = 'ROTATION BANK (positions 6+)';
      list.appendChild(div);
    }

    var modeBtns = autoRot
      ? '<div style="flex:1;padding:9px 0;font-family:monospace;font-size:10px;letter-spacing:.1em;color:#383838">ROTATING • AUTO</div>'
      : '<button class="btn-mode'+(t.mode==='static'?' active':'')+'" onclick="setMode('+t.id+',\'static\')">STATIC</button>' +
        '<button class="btn-mode'+(t.mode==='rotating'?' active':'')+'" onclick="setMode('+t.id+',\'rotating\')">ROTATING</button>';

    var card = document.createElement('div');
    card.className = 'ticker-card';
    card.innerHTML =
      '<div class="ticker-top">' +
        '<span class="ticker-sym">' + escHtml(t.symbol) + '</span>' +
        '<button class="btn-move" onclick="moveTicker('+t.id+',-1)"'+(i===0?' disabled':'')+'>↑</button>' +
        '<button class="btn-move" onclick="moveTicker('+t.id+',1)"'+(i===state.tickers.length-1?' disabled':'')+'>↓</button>' +
        '<button class="btn-remove" onclick="removeTicker('+t.id+')">✕</button>' +
      '</div>' +
      '<div class="ticker-btns">' + modeBtns + '</div>';
    list.appendChild(card);
  });

  validateRot();
  syncTickersContinue();
}

function removeTicker(id) {
  state.tickers = state.tickers.filter(function(t){ return t.id!==id; });
  renderTickers();
}

function setMode(id, mode) {
  var t = state.tickers.find(function(t){ return t.id===id; });
  if (t) t.mode = mode;
  renderTickers();
}

function moveTicker(id, dir) {
  var arr = state.tickers;
  var i = arr.findIndex(function(t){ return t.id===id; });
  var j = i + dir;
  if (j < 0 || j >= arr.length) return;
  var tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
  renderTickers();
}

function validateRot() {
  var h = parseInt(document.getElementById('rotH').value)||0;
  var m = parseInt(document.getElementById('rotM').value)||0;
  var hasRot = state.tickers.some(function(t, i){ return t.mode==='rotating' || i >= 5; });
  var err = document.getElementById('rotErr');
  var hint = document.getElementById('rotHint');
  if (hasRot && (h*60+m) < 15) {
    err.textContent = 'Minimum: 15 minutes total'; err.style.display='block'; hint.style.display='none';
  } else {
    err.style.display='none'; hint.style.display='block';
  }
  state.rotH = h; state.rotM = m;
  syncTickersContinue();
}

function canProceedTickers() {
  if (!state.tickers.length) return false;
  if (!document.getElementById('apiKeyInput').value.trim()) return false;
  if (!state.apiKeyOk) return false;
  var h = parseInt(document.getElementById('rotH').value)||0;
  var m = parseInt(document.getElementById('rotM').value)||0;
  var hasRot = state.tickers.some(function(t, i){ return t.mode==='rotating' || i >= 5; });
  if (hasRot && (h*60+m) < 15) return false;
  return true;
}

function syncTickersContinue() {
  document.getElementById('tickersContinueBtn').disabled = !canProceedTickers();
}

document.addEventListener('DOMContentLoaded', function() {
  document.getElementById('rotH').addEventListener('input', validateRot);
  document.getElementById('rotM').addEventListener('input', validateRot);
  document.getElementById('newTickerInput').addEventListener('input', function() { syncTickersContinue(); });
});

// ── Advanced settings ──────────────────────────────────────────────────────
function setBoard(b) {
  state.board = b;
  document.getElementById('btnXiao').classList.toggle('active', b==='xiao-panel');
  document.getElementById('btnRT1').classList.toggle('active', b==='reterminal-e1001');
  document.getElementById('panelVariantSection').style.display = b==='xiao-panel' ? 'block' : 'none';
  updateRefreshHint();
}

function setPanelVariant(v) {
  state.panelVariant = v;
  document.getElementById('btnPA').classList.toggle('active', v==='A');
  document.getElementById('btnPB').classList.toggle('active', v==='B');
}

function setPowerMode(m) {
  state.powerMode = m;
  document.getElementById('pcBatterySaver').classList.toggle('active', m==='battery-saver');
  document.getElementById('pcQuickWake').classList.toggle('active', m==='quick-wake');
}

function updateRefreshHint() {
  // Board C would get a 30-min min; for now both boards in scope use 15 min
  document.getElementById('refHint').textContent = 'Minimum 15 min. Recommended: 1 hour.';
}

function toggleAdv() {
  state.showAdvanced = !state.showAdvanced;
  document.getElementById('advInner').style.display = state.showAdvanced ? 'block' : 'none';
  document.getElementById('advIcon').textContent = state.showAdvanced ? '▾' : '▸';
}

// ── Review ─────────────────────────────────────────────────────────────────
function populateReview() {
  document.getElementById('rvSSID').textContent = state.ssid || '—';
  document.getElementById('rvProvider').textContent = state.provider === 'twelve-data' ? 'Twelve Data' : 'Finnhub';
  var key = document.getElementById('apiKeyInput').value;
  document.getElementById('rvKey').textContent = 'API key: ' + (key.length > 4 ? '••••••••' + key.slice(-4) : key.length ? '••••••••' : 'Not entered');

  var tlist = document.getElementById('rvTickerList');
  tlist.innerHTML = '';
  state.tickers.forEach(function(t) {
    var row = document.createElement('div');
    row.className = 'review-ticker-row';
    row.innerHTML = '<span class="review-ticker-sym">'+escHtml(t.symbol)+'</span><span class="review-ticker-mode">'+t.mode.toUpperCase()+'</span>';
    tlist.appendChild(row);
  });

  var hasRot = state.tickers.some(function(t, i){ return t.mode==='rotating' || i >= 5; });
  var rotSum = document.getElementById('rvRotSummary');
  if (hasRot) {
    var h = parseInt(document.getElementById('rotH').value)||0;
    var m = parseInt(document.getElementById('rotM').value)||0;
    rotSum.textContent = 'Rotation: every ' + (h ? h+'h ' : '') + m + ' min';
    rotSum.style.display = 'block';
  } else {
    rotSum.style.display = 'none';
  }

  validateRefreshInterval();
}

function validateRefreshInterval() {
  var h = parseInt(document.getElementById('refH').value)||0;
  var m = parseInt(document.getElementById('refM').value)||0;
  var err = document.getElementById('refErr');
  var hint = document.getElementById('refHint');
  if ((h*60+m) < 15) {
    err.textContent = 'Minimum: 15 minutes total'; err.style.display='block'; hint.style.display='none';
  } else {
    err.style.display='none'; hint.style.display='block';
  }
}

document.addEventListener('DOMContentLoaded', function() {
  document.getElementById('refH').addEventListener('input', validateRefreshInterval);
  document.getElementById('refM').addEventListener('input', validateRefreshInterval);
});

// ── Save ───────────────────────────────────────────────────────────────────
function saveAndRestart() {
  var refH = parseInt(document.getElementById('refH').value)||0;
  var refM = parseInt(document.getElementById('refM').value)||0;
  if ((refH*60+refM) < 15) { validateRefreshInterval(); return; }

  var payload = {
    ssid:         state.ssid,
    pass:         document.getElementById('passInput').value,
    provider:     state.provider,
    apiKey:       document.getElementById('apiKeyInput').value.trim(),
    tickers:      state.tickers.map(function(t, i){ return {symbol:t.symbol, mode: i >= 5 ? 'rotating' : t.mode}; }),
    rotH:         parseInt(document.getElementById('rotH').value)||0,
    rotM:         parseInt(document.getElementById('rotM').value)||0,
    refH:         refH,
    refM:         refM,
    board:        state.board,
    panelVariant: state.panelVariant,
    powerMode:    state.powerMode
  };

  showPage('saving');

  fetch('/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload)
  }).then(function(r){ return r.json(); }).then(function(res) {
    if (res.ok) {
      document.getElementById('savingBullet').textContent = '✔';
      document.getElementById('savingTitle').textContent  = 'Settings Saved';
      document.getElementById('savingDetail').textContent = 'Your device is rebooting and will join your home WiFi. You can safely close this page.';
      document.getElementById('savingDone').style.display = 'block';
    }
  }).catch(function(){
    // Device restarted before responding — that's fine, show success
    document.getElementById('savingBullet').textContent = '✔';
    document.getElementById('savingTitle').textContent  = 'Settings Saved';
    document.getElementById('savingDetail').textContent = 'Your device is rebooting. You can safely close this page.';
    document.getElementById('savingDone').style.display = 'block';
  });
}

// ── Utilities ──────────────────────────────────────────────────────────────
function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
</script>
</body>
</html>)rawhtml";
