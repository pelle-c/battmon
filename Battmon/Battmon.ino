#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "BattmonConfig.h"

namespace {
using namespace BattmonConfig;

struct Reading {
  const char* name;
  const char* unit;
  int16_t raw;
  float measuredVolts;
  float calibratedValue;
  bool available;
};

struct HistoryPoint {
  uint32_t sampledAtMs;
  float voltageV;
  float powerW;
  bool available;
};

constexpr uint32_t kHistoryDurationMs = 60UL * 60UL * 1000UL;
constexpr size_t kHistoryCapacity =
    kHistoryDurationMs / kHistorySampleIntervalMs;

Adafruit_ADS1015 ads48;
Adafruit_ADS1015 ads49;
Adafruit_ADS1015 ads4a;
WebServer server(80);
uint32_t lastSampleMs = 0;
uint32_t lastCurrentIntegrationMs = 0;
double coulombCounterAh = 0.0;
float previousCurrentA = 0.0f;
bool previousCurrentValid = false;
bool ads48Ready = false;
bool ads49Ready = false;
bool ads4aReady = false;
bool otaUploadStarted = false;
bool otaUploadSucceeded = false;
HistoryPoint history[kHistoryCapacity] = {};
size_t historyCount = 0;
size_t historyNextIndex = 0;
uint32_t lastHistorySampleMs = 0;

Reading readings[] = {
    {kReadingName0, "V", 0, 0.0f, 0.0f, false},
    {kReadingName1, "V", 0, 0.0f, 0.0f, false},
    {kReadingName2, "V", 0, 0.0f, 0.0f, false},
    {kReadingName3, "V", 0, 0.0f, 0.0f, false},
    {kReadingName4, "V", 0, 0.0f, 0.0f, false},
    {kReadingName5, "A", 0, 0.0f, 0.0f, false},
};

constexpr char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#101820">
  <title>Battmon</title>
  <style>
    :root{color-scheme:dark;--bg:#101820;--panel:#192630;--line:#2d414d;--muted:#9eb0b9;--text:#edf5f5;--accent:#70e1c1;--bad:#ff9a8c}
    *{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 50% -20%,#24433f 0,transparent 55%),var(--bg);color:var(--text);font:16px/1.45 system-ui,sans-serif;min-height:100vh}
    main{width:min(960px,100%);margin:0 auto;padding:34px 20px 50px}.top{display:flex;align-items:flex-end;justify-content:space-between;gap:20px;margin-bottom:26px}
    h1{font-size:clamp(2rem,6vw,3rem);line-height:1;margin:0;letter-spacing:-.05em}.sub{margin:10px 0 0;color:var(--muted)}.live{color:var(--accent);font-size:.9rem;white-space:nowrap}.live.offline{color:var(--bad)}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(245px,1fr));gap:14px}.card{background:linear-gradient(145deg,#1b2b35,#17232c);border:1px solid var(--line);border-radius:18px;padding:20px;box-shadow:0 12px 35px #0002}
    .summary-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin-bottom:14px}.summary-card{display:flex;align-items:center;justify-content:space-between;gap:14px;min-height:132px}.ah-label{font-size:.9rem;color:var(--muted)}.ah-value{font-size:clamp(1.8rem,5vw,2.5rem);font-weight:650;letter-spacing:-.04em;margin-top:2px}.ah-value span{font-size:.48em;color:var(--accent);letter-spacing:0}.reset{border:1px solid #668077;background:#223b38;color:var(--text);border-radius:10px;padding:11px 15px;font:inherit;font-size:.9rem;cursor:pointer}.reset:hover{background:#2d4b46}.reset:disabled{opacity:.55;cursor:wait}.power-detail{margin-top:3px}
    .chart-card{margin-bottom:14px}.chart-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;margin-bottom:8px}.chart-title{font-size:1.05rem;font-weight:600}.chart-subtitle{font-size:.82rem;color:var(--muted);margin-top:3px}.legend{display:flex;gap:16px;flex-wrap:wrap;font-size:.82rem;color:var(--muted)}.legend span{white-space:nowrap}.legend i{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px}.voltage-dot{background:#70e1c1}.power-dot{background:#ffbf69}#historyChart{display:block;width:100%;height:auto;overflow:visible}.chart-grid{stroke:#2d414d;stroke-width:1}.chart-axis{fill:#9eb0b9;font:12px system-ui,sans-serif}.voltage-line{fill:none;stroke:#70e1c1;stroke-width:2.5;stroke-linejoin:round;stroke-linecap:round}.power-line{fill:none;stroke:#ffbf69;stroke-width:2.5;stroke-linejoin:round;stroke-linecap:round}.chart-empty{fill:#9eb0b9;font:14px system-ui,sans-serif;text-anchor:middle}
    .label{font-size:.92rem;color:var(--muted)}.value{font-size:clamp(2rem,6vw,2.8rem);font-weight:650;letter-spacing:-.05em;margin:15px 0 10px}.unit{font-size:.48em;color:var(--accent);letter-spacing:0}.detail{font: .8rem ui-monospace,monospace;color:var(--muted)}.state{font-size:.72rem;margin-top:15px;color:var(--accent);text-transform:uppercase;letter-spacing:.12em}.state.missing{color:var(--bad)}
    .update-link{color:var(--accent);font-size:.88rem;text-decoration:none;border:1px solid var(--line);border-radius:9px;padding:8px 11px;white-space:nowrap}.update-link:hover{border-color:var(--accent)}
    footer{margin-top:24px;color:var(--muted);font-size:.82rem}code{color:var(--text)}
    @media(max-width:560px){main{padding:25px 14px 36px}.top{align-items:flex-start;flex-direction:column;gap:14px}.grid,.summary-grid{grid-template-columns:1fr}.summary-card{min-height:unset}}
  </style>
</head>
<body>
  <main>
    <header class="top"><div><h1>Battmon</h1><p class="sub">Electric quadricycle</p></div><div style="display:flex;align-items:center;gap:14px"><a class="update-link" href="/update">Firmware OTA</a><div id="live" class="live">Connecting…</div></div></header>
    <section class="summary-grid">
      <article class="card summary-card"><div><div class="ah-label">Accumulated energy</div><div class="ah-value"><span id="ah">0.000000</span> <span>Ah</span></div></div><button id="resetAh" class="reset" type="button">Reset Ah</button></article>
      <article class="card summary-card"><div><div class="ah-label">Actual power</div><div class="ah-value"><span id="powerValue">—</span> <span>W</span></div><div id="powerDetail" class="detail power-detail">Waiting for readings</div></div></article>
    </section>
    <section class="card chart-card">
      <div class="chart-head"><div><div class="chart-title">Last hour</div><div class="chart-subtitle">Total voltage and total power</div></div><div class="legend"><span><i class="voltage-dot"></i>Total voltage (V)</span><span><i class="power-dot"></i>Total power (W)</span></div></div>
      <svg id="historyChart" viewBox="0 0 900 310" role="img" aria-label="Total voltage and power over the last hour"></svg>
    </section>
    <section id="readings" class="grid" aria-live="polite"></section>
    <footer>2026 Per Carlén</footer>
  </main>
  <script>
    const grid=document.getElementById('readings'),live=document.getElementById('live'),chart=document.getElementById('historyChart');
    function renderHistory(points){
      const W=900,H=310,L=64,R=64,T=16,B=38,PW=W-L-R,PH=H-T-B;
      const valid=points.filter(p=>p.available&&Number.isFinite(p.v)&&Number.isFinite(p.w));
      const voltageMax=Math.max(1,...valid.map(p=>p.v))*1.08;
      let powerMin=valid.length?Math.min(0,...valid.map(p=>p.w)):0;
      let powerMax=valid.length?Math.max(1,...valid.map(p=>p.w)):1;
      const powerPad=(powerMax-powerMin)*.08||1;powerMin-=powerPad;powerMax+=powerPad;
      const x=age=>L+PW*(1-age/3600),yv=v=>T+PH*(1-v/voltageMax),yp=w=>T+PH*(1-(w-powerMin)/(powerMax-powerMin));
      let svg='';
      for(let i=0;i<=4;i++){
        const ratio=i/4,yy=T+PH*ratio,v=voltageMax*(1-ratio),w=powerMax-(powerMax-powerMin)*ratio;
        svg+=`<line class="chart-grid" x1="${L}" y1="${yy}" x2="${W-R}" y2="${yy}"/><text class="chart-axis" x="${L-9}" y="${yy+4}" text-anchor="end">${v.toFixed(0)}</text><text class="chart-axis" x="${W-R+9}" y="${yy+4}">${w.toFixed(0)}</text>`;
      }
      for(let h=0;h<=6;h++){
        const xx=L+PW*h/6;svg+=`<line class="chart-grid" x1="${xx}" y1="${T}" x2="${xx}" y2="${T+PH}"/><text class="chart-axis" x="${xx}" y="${H-10}" text-anchor="middle">${60-h*10}m</text>`;
      }
      const pathFor=(key,map)=>{
        let d='',pen=false;
        for(const p of points){if(!p.available||!Number.isFinite(p[key])){pen=false;continue}const px=x(p.age_s),py=map(p[key]);d+=`${pen?'L':'M'}${px.toFixed(1)},${py.toFixed(1)} `;pen=true}
        return d;
      };
      if(valid.length){svg+=`<path class="voltage-line" d="${pathFor('v',yv)}"/><path class="power-line" d="${pathFor('w',yp)}"/>`}
      else svg+='<text class="chart-empty" x="450" y="160">Waiting for complete sensor readings…</text>';
      chart.innerHTML=svg;
    }
    async function refreshHistory(){
      try{const response=await fetch('/api/history',{cache:'no-store'});if(!response.ok)throw new Error('HTTP '+response.status);const data=await response.json();renderHistory(data.points)}catch(error){}
    }
    async function refresh(){
      try{
        const response=await fetch('/api/values',{cache:'no-store'});
        if(!response.ok)throw new Error('HTTP '+response.status);
        const data=await response.json();
        document.getElementById('ah').textContent=Number(data.coulomb_ah).toFixed(6);
        document.getElementById('powerValue').textContent=data.power_available?Number(data.power_w).toFixed(3):'—';
        document.getElementById('powerDetail').textContent=data.power_available
          ? '('+Number(data.voltage_sum_v).toFixed(3)+' V total) × '+Number(data.current_a).toFixed(3)+' A'
          : 'Requires all voltage and current readings';
        grid.innerHTML=data.readings.map(r=>`<article class="card"><div class="label">${r.name}</div><div class="value">${r.available?Number(r.value).toFixed(3):'—'} <span class="unit">${r.unit}</span></div><div class="detail">${r.available?`ADC ${Number(r.measured).toFixed(3)} V · raw ${r.raw}`:'No reading'}</div><div class="state ${r.available?'':'missing'}">${r.available?'Online':'Sensor unavailable'}</div></article>`).join('');
        live.textContent='● Live · '+new Date().toLocaleTimeString();live.classList.remove('offline');
      }catch(error){live.textContent='● Connection lost';live.classList.add('offline')}
    }
    document.getElementById('resetAh').addEventListener('click',async()=>{
      const button=document.getElementById('resetAh');button.disabled=true;
      try{const response=await fetch('/api/reset-ah',{method:'POST'});if(!response.ok)throw new Error('HTTP '+response.status);await refresh()}
      catch(error){live.textContent='Reset failed';live.classList.add('offline')}
      finally{button.disabled=false}
    });
    refresh();refreshHistory();setInterval(refresh,1000);setInterval(refreshHistory,10000);
  </script>
</body>
</html>
)HTML";

constexpr char kUpdateHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#101820">
  <title>Battmon · Firmware update</title>
  <style>
    :root{color-scheme:dark;--bg:#101820;--panel:#192630;--line:#2d414d;--muted:#9eb0b9;--text:#edf5f5;--accent:#70e1c1}
    *{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 50% -20%,#24433f 0,transparent 55%),var(--bg);color:var(--text);font:16px/1.5 system-ui,sans-serif;min-height:100vh}
    main{width:min(620px,100%);margin:0 auto;padding:34px 20px}a{color:var(--accent);text-decoration:none}.card{margin-top:25px;background:linear-gradient(145deg,#1b2b35,#17232c);border:1px solid var(--line);border-radius:18px;padding:24px}h1{margin:0;letter-spacing:-.04em}p{color:var(--muted)}input{display:block;width:100%;margin:22px 0;padding:12px;border:1px solid var(--line);border-radius:10px;background:#101820;color:var(--text)}button{border:1px solid #668077;background:#223b38;color:var(--text);border-radius:10px;padding:12px 16px;font:inherit;cursor:pointer}button:hover{background:#2d4b46}button:disabled{opacity:.55;cursor:wait}#status{margin-top:16px;min-height:1.5em}
  </style>
</head>
<body>
  <main>
    <a href="/">← Back to dashboard</a>
    <section class="card">
      <h1>Firmware update</h1>
      <p>Select the compiled ESP32-C6 <code>.bin</code> firmware file. Keep this device powered and connected while it uploads.</p>
      <form id="upload" method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="firmware" accept=".bin,application/octet-stream" required>
        <button id="submit" type="submit">Upload and install</button>
      </form>
      <div id="status" role="status"></div>
    </section>
  </main>
  <script>
    const form=document.getElementById('upload'),button=document.getElementById('submit'),status=document.getElementById('status');
    form.addEventListener('submit',async event=>{
      event.preventDefault();button.disabled=true;status.textContent='Uploading firmware…';
      try{const response=await fetch('/update',{method:'POST',body:new FormData(form)});const message=await response.text();status.textContent=message;if(!response.ok)button.disabled=false}
      catch(error){status.textContent='Upload connection lost. The device may be restarting.'}
    });
  </script>
</body>
</html>
)HTML";

void samplePair(Adafruit_ADS1015& adc, uint8_t readingIndex, bool pair01,
                float factor, float offsetVolts = 0.0f) {
  Reading& reading = readings[readingIndex];
  reading.raw = pair01 ? adc.readADC_Differential_0_1()
                       : adc.readADC_Differential_2_3();
  reading.measuredVolts = adc.computeVolts(reading.raw);
  reading.calibratedValue = (reading.measuredVolts - offsetVolts) * factor;
  reading.available = true;
  Serial.printf("%s: raw=%d, measured=%.3f V, calibrated=%.3f %s\n",
                reading.name, reading.raw, reading.measuredVolts,
                reading.calibratedValue, reading.unit);
}

void sampleAdc(Adafruit_ADS1015& adc, bool ready, uint8_t firstReadingIndex,
               float factor01, float factor23) {
  readings[firstReadingIndex].available = ready;
  readings[firstReadingIndex + 1].available = ready;
  if (!ready) {
    Serial.printf("%s not available; check power, address, SDA and SCL.\n",
                  readings[firstReadingIndex].name);
    return;
  }
  samplePair(adc, firstReadingIndex, true, factor01);
  samplePair(adc, firstReadingIndex + 1, false, factor23,
             firstReadingIndex + 1 == 5 ? kAds4AA2A3CurrentOffsetVolts : 0.0f);
}

void integrateCurrent(uint32_t now) {
  const Reading& current = readings[5];
  if (!current.available) {
    previousCurrentValid = false;
    lastCurrentIntegrationMs = now;
    return;
  }

  const float currentA = current.calibratedValue;
  if (previousCurrentValid) {
    const uint32_t elapsedMs = now - lastCurrentIntegrationMs;
    const double averageCurrentA = (previousCurrentA + currentA) * 0.5;
    coulombCounterAh += averageCurrentA * elapsedMs / 3600000.0;
  }

  previousCurrentA = currentA;
  lastCurrentIntegrationMs = now;
  previousCurrentValid = true;
}

void recordHistory(uint32_t now) {
  if (now - lastHistorySampleMs < kHistorySampleIntervalMs) return;
  lastHistorySampleMs = now;

  double voltageSum = 0.0;
  bool available = readings[5].available;
  for (size_t i = 0; i < 5; ++i) {
    available = available && readings[i].available;
    voltageSum += readings[i].calibratedValue;
  }

  HistoryPoint& point = history[historyNextIndex];
  point.sampledAtMs = now;
  point.voltageV = voltageSum;
  point.powerW = voltageSum * readings[5].calibratedValue;
  point.available = available;
  historyNextIndex = (historyNextIndex + 1) % kHistoryCapacity;
  if (historyCount < kHistoryCapacity) ++historyCount;
}

void handleIndex() {
  server.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleHistory() {
  const uint32_t now = millis();
  const size_t first = (historyNextIndex + kHistoryCapacity - historyCount) %
                       kHistoryCapacity;
  String json;
  json.reserve(historyCount * 72 + 32);
  json = "{\"points\":[";
  bool firstPoint = true;
  for (size_t i = 0; i < historyCount; ++i) {
    const HistoryPoint& point = history[(first + i) % kHistoryCapacity];
    const uint32_t ageMs = now - point.sampledAtMs;
    if (ageMs > kHistoryDurationMs) continue;
    if (!firstPoint) json += ',';
    firstPoint = false;
    json += "{\"age_s\":" + String(ageMs / 1000) + ",\"available\":";
    json += point.available ? "true" : "false";
    if (point.available) {
      json += ",\"v\":" + String(point.voltageV, 3) +
              ",\"w\":" + String(point.powerW, 3) + '}';
    } else {
      json += ",\"v\":null,\"w\":null}";
    }
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleValues() {
  double voltageSum = 0.0;
  bool powerAvailable = readings[5].available;
  for (size_t i = 0; i < 5; ++i) {
    if (!readings[i].available) {
      powerAvailable = false;
    }
    voltageSum += readings[i].calibratedValue;
  }
  const double currentA = readings[5].calibratedValue;
  const double actualPowerW = voltageSum * currentA;

  String json;
  json.reserve(700);
  json = "{\"uptime_ms\":" + String(millis()) + ",\"coulomb_ah\":" +
         String(coulombCounterAh, 6) + ",\"power_available\":" +
         String(powerAvailable ? "true" : "false") +
         ",\"voltage_sum_v\":" + String(voltageSum, 5) +
         ",\"current_a\":" + String(currentA, 5) +
         ",\"power_w\":" + String(actualPowerW, 5) + ",\"readings\":[";
  for (size_t i = 0; i < sizeof(readings) / sizeof(readings[0]); ++i) {
    if (i) json += ',';
    const Reading& reading = readings[i];
    json += "{\"name\":\"";
    json += reading.name;
    json += "\",\"unit\":\"";
    json += reading.unit;
    json += "\",\"available\":";
    json += reading.available ? "true" : "false";
    json += ",\"raw\":" + String(reading.raw);
    json += ",\"measured\":" + String(reading.measuredVolts, 5);
    json += ",\"value\":" + String(reading.calibratedValue, 5) + '}';
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleResetAh() {
  coulombCounterAh = 0.0;
  lastCurrentIntegrationMs = millis();
  previousCurrentValid = readings[5].available;
  if (previousCurrentValid) {
    previousCurrentA = readings[5].calibratedValue;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"coulomb_ah\":0}");
}

void handleUpdatePage() {
  if (!server.authenticate(kOtaUsername, kApPassword)) {
    server.requestAuthentication();
    return;
  }
  server.send_P(200, "text/html; charset=utf-8", kUpdateHtml);
}

void handleUpdateUpload() {
  if (!server.authenticate(kOtaUsername, kApPassword)) {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA: receiving %s\n", upload.filename.c_str());
    otaUploadSucceeded = false;
    otaUploadStarted = Update.begin(UPDATE_SIZE_UNKNOWN);
    if (!otaUploadStarted) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaUploadStarted && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      otaUploadStarted = false;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    otaUploadSucceeded = otaUploadStarted && Update.end(true);
    if (otaUploadSucceeded) {
      Serial.printf("OTA: installed %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    otaUploadStarted = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaUploadStarted = false;
    otaUploadSucceeded = false;
    Serial.println("OTA: upload aborted");
  }
}

void handleUpdateResult() {
  if (!server.authenticate(kOtaUsername, kApPassword)) {
    server.requestAuthentication();
    return;
  }
  if (otaUploadSucceeded) {
    server.send(200, "text/plain", "Firmware installed. The device is restarting.");
    delay(500);
    ESP.restart();
    return;
  }
  server.send(500, "text/plain", "Firmware update failed. Check that the selected .bin is an ESP32-C6 firmware image.");
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(kApSsid, kApPassword)) {
    Serial.printf("Wi-Fi AP: %s\n", kApSsid);
    Serial.printf("Open http://%s/\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("Failed to start Wi-Fi access point.");
  }
}
}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(200);
  Serial.println("Battmon starting on ESP32-C6");

  Wire.begin(kSdaPin, kSclPin);
  ads48.setGain(kAdcGain);  // Input pins must stay within supply rails.
  ads49.setGain(kAdcGain);
  ads4a.setGain(kAdcGain);
  ads48Ready = ads48.begin(kAds48Address, &Wire);
  ads49Ready = ads49.begin(kAds49Address, &Wire);
  ads4aReady = ads4a.begin(kAds4AAddress, &Wire);
  Serial.printf("I2C: SDA=%u, SCL=%u; ADS1015 0x%02X %s, 0x%02X %s, 0x%02X %s\n",
                kSdaPin, kSclPin, kAds48Address,
                ads48Ready ? "detected" : "not detected",
                kAds49Address,
                ads49Ready ? "detected" : "not detected",
                kAds4AAddress,
                ads4aReady ? "detected" : "not detected");

  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/values", HTTP_GET, handleValues);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/reset-ah", HTTP_POST, handleResetAh);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  startAccessPoint();
  server.begin();
  Serial.println("HTTP server started on port 80");

  sampleAdc(ads48, ads48Ready, 0, kAds48A0A1VoltageFactor, kAds48A2A3VoltageFactor);
  sampleAdc(ads49, ads49Ready, 2, kAds49A0A1VoltageFactor, kAds49A2A3VoltageFactor);
  sampleAdc(ads4a, ads4aReady, 4, kAds4AA0A1VoltageFactor, kAds4AA2A3AmpsPerVolt);
  integrateCurrent(millis());
  lastHistorySampleMs = millis() - kHistorySampleIntervalMs;
  recordHistory(millis());
}

void loop() {
  server.handleClient();

  const uint32_t now = millis();
  if (now - lastSampleMs >= kSampleIntervalMs) {
    lastSampleMs = now;
    sampleAdc(ads48, ads48Ready, 0,
              kAds48A0A1VoltageFactor, kAds48A2A3VoltageFactor);
    sampleAdc(ads49, ads49Ready, 2,
              kAds49A0A1VoltageFactor, kAds49A2A3VoltageFactor);
    sampleAdc(ads4a, ads4aReady, 4,
              kAds4AA0A1VoltageFactor, kAds4AA2A3AmpsPerVolt);
    integrateCurrent(millis());
    recordHistory(millis());
    Serial.printf("Accumulated charge: %.6f Ah\n", coulombCounterAh);
  }
}
