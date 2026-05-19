#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>
#include "time.h"
#include <vector>
#include <esp_wifi.h> 

#define WDT_TIMEOUT 30

// ================= ตั้งค่าเครือข่าย & API =================
const char* ssid = "*****";
const char* password = "*****";
const String GOOGLE_SCRIPT_URL = "*****"; 
const String LINE_CHANNEL_TOKEN = "*****="; 
const String LINE_TARGET_ID = "*****";

// ================= ฮาร์ดแวร์ & ตัวแปร =================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(4);
DallasTemperature sensors(&oneWire);
WebServer server(80);

float currentTemp = 0.0, tempOffset = 0.0;
float tempMin = 1.0, tempMax = 9.0;
String scheduleTimes = "08:30,15:30", lastSentMinute = "";
unsigned long lastAlertCheckMillis = 0, lastLineNotifyMillis = 0;

bool enableLineNotify = true; 

struct DataLog { String timestamp; float temperature; };
std::vector<DataLog> offlineBuffer;

// ================= หน้าเว็บ =================
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Vaccine Monitor Pro</title><style>:root{--bg:#0f172a;--card:#1e293b;--text:#f8fafc;--accent:#38bdf8;--danger:#fb7185}body{font-family:sans-serif;background:var(--bg);color:var(--text);margin:0;padding:20px;display:flex;flex-direction:column;align-items:center}.header{width:100%;max-width:800px;display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}.clock{font-size:1.1rem;color:#94a3b8}.gear-btn{font-size:1.8rem;cursor:pointer;background:0 0;border:0;color:var(--accent)}.card{background:var(--card);padding:40px;border-radius:20px;text-align:center;width:100%;max-width:400px;box-shadow:0 10px 25px rgba(0,0,0,.5);border:2px solid #334155;transition:border-color .3s}.temp-value{font-size:5rem;font-weight:700;color:var(--accent);margin:15px 0}.modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.8);justify-content:center;align-items:center;z-index:100}.modal-content{background:var(--card);padding:25px;border-radius:15px;width:350px;border:1px solid var(--accent);max-height:85vh;overflow-y:auto}label{display:block;margin-top:15px;font-size:.85rem;color:#94a3b8}input{width:90%;padding:10px;margin-top:5px;border-radius:8px;border:1px solid #334155;background:#0f172a;color:#fff}.btn-save{margin-top:20px;width:100%;padding:12px;background:var(--accent);border:0;border-radius:8px;color:#0f172a;font-weight:700;cursor:pointer}.toggle-btn{display:flex;align-items:center;gap:8px;padding:6px 16px;border:none;border-radius:20px;font-family:inherit;font-size:.85rem;font-weight:700;cursor:pointer;transition:all .3s;background:#475569;color:#fff}.toggle-btn.on{background:#10b981}.toggle-btn.off{background:#fb7185}.status-dot{width:10px;height:10px;border-radius:50%;background:#fff;transition:transform .3s}.toggle-btn.on .status-dot{transform:scale(1.2);box-shadow:0 0 5px rgba(255,255,255,.8)}.toggle-btn.off .status-dot{opacity:.6}</style></head><body><div class="header"><button class="gear-btn" onclick="openSettings()">⚙️</button><div id="liveClock" class="clock">...</div></div><div class="card" id="mainCard"><div style="font-size:.9rem;color:#94a3b8">อุณหภูมิปัจจุบัน</div><div class="temp-value" id="tempDisplay">--</div><div style="font-size:1rem">°C</div><div style="margin-top:15px;font-size:.8rem;color:var(--danger)" id="limitInfo">ช่วงวิกฤต: -- ถึง -- °C</div><div style="margin-top:25px;display:flex;align-items:center;justify-content:center;gap:12px;border-top:1px solid #334155;padding-top:20px"><span style="font-size:.85rem;color:#94a3b8">แจ้งเตือน LINE:</span><button id="btnNotify" class="toggle-btn" onclick="toggleNotify()"><span class="status-dot"></span><span id="txtNotify">กำลังโหลด...</span></button></div></div><div class="modal" id="settingsModal"><div class="modal-content"><h2 style="margin-top:0">⚙️ ตั้งค่าระบบ</h2><label>1. อุณหภูมิต่ำสุด:</label><input type="number" step="0.1" id="tempMin" value="1.0"><label>2. อุณหภูมิสูงสุด:</label><input type="number" step="0.1" id="tempMax" value="9.0"><label>3. ชดเชยค่า (Offset):</label><input type="number" step="0.1" id="tempOffset" value="0.0"><label>4. รอบเวลา (HH:MM):</label><input type="text" id="scheduleTimes" value="08:30,15:30"><button class="btn-save" onclick="saveSettings()">บันทึก</button><button class="btn-save" style="background:#475569;margin-top:10px" onclick="closeSettings()">ปิด</button></div></div><script>setInterval(()=>{document.getElementById("liveClock").innerText=new Date().toLocaleDateString("th-TH",{hour:"2-digit",minute:"2-digit",second:"2-digit"})},1e3);setInterval(async()=>{try{let e=await fetch("/api/data"),t=await e.json();document.getElementById("tempDisplay").innerText=t.temperature.toFixed(1);document.getElementById("limitInfo").innerText="ช่วงวิกฤต: "+t.tempMin.toFixed(1)+" - "+t.tempMax.toFixed(1)+" °C";document.getElementById("mainCard").style.borderColor=(t.temperature<t.tempMin||t.temperature>t.tempMax)?"#fb7185":"#334155";let n=t.lineNotifyEnabled;let a=document.getElementById("btnNotify"),l=document.getElementById("txtNotify");if(n){a.className="toggle-btn on";l.innerText="เปิด (ON)"}else{a.className="toggle-btn off";l.innerText="ปิด (OFF)"}}catch(e){}},2e3);function openSettings(){document.getElementById("settingsModal").style.display="flex"}function closeSettings(){document.getElementById("settingsModal").style.display="none"}async function saveSettings(){let min=document.getElementById("tempMin").value,max=document.getElementById("tempMax").value,t=document.getElementById("tempOffset").value,n=document.getElementById("scheduleTimes").value;try{await fetch("/api/settings",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:`tempMin=${min}&tempMax=${max}&offset=${t}&schedules=${n}`});alert("บันทึกสำเร็จ!");closeSettings()}catch(e){alert("ล้มเหลว")}}async function toggleNotify(){let e=document.getElementById("btnNotify"),t=e.classList.contains("on"),n=!t;e.className=n?"toggle-btn on":"toggle-btn off";document.getElementById("txtNotify").innerText=n?"เปิด (ON)":"ปิด (OFF)";try{await fetch("/api/settings",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:`enableLineNotify=${n}`})}catch(a){console.error(a)}}</script></body></html>)rawliteral";

// ================= ฟังก์ชันส่งข้อมูลภายนอก =================
void sendLineBotMessage(String msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  msg.replace("\n", "\\n"); 
  String payload = "{\"to\":\"" + LINE_TARGET_ID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + msg + "\"}]}";
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + LINE_CHANNEL_TOKEN);
  http.POST(payload);
  http.end();
}

bool sendToGoogleSheets(String timestamp, float temp) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  // [FIX] ตัดทศนิยมเหลือ 1 ตำแหน่งให้เป็นมาตรฐานเดียวกับจอและป้องกันข้อมูลยาวเกิน
  String payload = "{\"timestamp\":\"" + timestamp + "\",\"temperature\":" + String(temp, 1) + "}";
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  
  // [FIX] เอาคำสั่ง http.setFollowRedirects ออก! 
  // เพื่อไม่ให้มันเด้งตามหน้า Redirect ของ Google ซึ่งเป็นสาเหตุให้สคริปต์โดนเรียกซ้ำ 2 รอบ
  
  http.begin(client, GOOGLE_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  http.end();
  
  // [FIX] Google Apps Script มักจะตอบ HTTP 302 เมื่อทำงานสำเร็จ เราจะถือว่า 200 หรือ 302 คือสำเร็จ
  return (code == 200 || code == 302); 
}

// ================= API & การตั้งค่าผ่านหน้าเว็บ =================
void handleData() {
  server.send(200, "application/json", 
    "{\"temperature\":" + String(currentTemp) + 
    ",\"tempMin\":" + String(tempMin) + 
    ",\"tempMax\":" + String(tempMax) + 
    ",\"lineNotifyEnabled\":" + (enableLineNotify ? "true" : "false") + "}");
}

void handleSettings() {
  if (server.hasArg("tempMin")) tempMin = server.arg("tempMin").toFloat();
  if (server.hasArg("tempMax")) tempMax = server.arg("tempMax").toFloat();
  if (server.hasArg("offset")) tempOffset = server.arg("offset").toFloat();
  if (server.hasArg("schedules")) scheduleTimes = server.arg("schedules");
  if (server.hasArg("enableLineNotify")) enableLineNotify = (server.arg("enableLineNotify") == "true");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ================= Setup & Loop =================
void setup() {
  setCpuFrequencyMhz(80);
  esp_task_wdt_config_t wdt_config = { .timeout_ms = WDT_TIMEOUT * 1000, .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, .trigger_panic = true };
  esp_task_wdt_init(&wdt_config); esp_task_wdt_add(NULL);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sensors.begin(); sensors.setWaitForConversion(false); 
  
  WiFi.mode(WIFI_STA); WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); esp_task_wdt_reset(); }

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  configTime(7 * 3600, 0, "pool.ntp.org");
  
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", index_html); });
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.begin();
}

void loop() {
  esp_task_wdt_reset();
  unsigned long currentMillis = millis();
  if (WiFi.status() == WL_CONNECTED) server.handleClient();
  
  // 1. อ่านเซนเซอร์
  static unsigned long prevTempM = 0;
  static bool isRequestingTemp = false;
  if (!isRequestingTemp && (currentMillis - prevTempM >= 2000)) {
    sensors.requestTemperatures();
    prevTempM = currentMillis;
    isRequestingTemp = true;
  }
  
  if (isRequestingTemp && (currentMillis - prevTempM >= 750)) {
    currentTemp = sensors.getTempCByIndex(0) + tempOffset;
    isRequestingTemp = false;
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0,0); 
    display.print("IP: "); display.print(WiFi.localIP());
    display.setTextSize(2); display.setCursor(0,25); 
    display.print(currentTemp, 1); display.print(" C");
    display.setTextSize(1); display.setCursor(0,55); 
    display.print("Rng:"); display.print((int)tempMin);
    display.print("-"); display.print((int)tempMax); 
    display.print(" Buf:"); display.print(offlineBuffer.size());
    display.display();
  }

  // [FIX] ตัวแปรตรวจสอบว่าอุณหภูมิตอนนี้ ไม่ใช่ค่า Error จากเซนเซอร์
  bool isTempValid = (currentTemp != 0.0 && currentTemp != -127.00 && currentTemp != 85.00);

  // 2. แจ้งเตือน LINE Bot
  if (isTempValid) {
    if (currentMillis - lastAlertCheckMillis >= 2 * 60 * 1000) {
      lastAlertCheckMillis = currentMillis;
      if (currentTemp < tempMin || currentTemp > tempMax) {
        if (enableLineNotify) {
          if (lastLineNotifyMillis == 0 || (currentMillis - lastLineNotifyMillis >= 30 * 60 * 1000)) {
            String msg = "🚨 แจ้งเตือนอุณหภูมิวัคซีนผิดปกติ!\nปัจจุบัน: " + String(currentTemp, 1) + " °C\nช่วงที่ปลอดภัย: " + String(tempMin, 1) + " - " + String(tempMax, 1) + " °C";
            sendLineBotMessage(msg);
            lastLineNotifyMillis = currentMillis; 
          }
        }
      } else { 
        lastLineNotifyMillis = 0; 
      }
    }
  }

  // 3. จัดการ Google Sheets
  static unsigned long lastCheckT = 0;
  if (currentMillis - lastCheckT >= 1000) {
    lastCheckT = currentMillis;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[10], fullTime[30];
      strftime(timeStr, 10, "%H:%M", &timeinfo);
      strftime(fullTime, 30, "%Y-%m-%d %H:%M:%S", &timeinfo);
      String curTime = String(timeStr);
      
      // [FIX] ทำให้การเช็คเวลาตรงเป๊ะ (ป้องกัน 8:30 ไปตรงกับ 18:30)
      String matchTime = "," + curTime + ",";
      String allTimes = "," + scheduleTimes + ",";
      
      if (allTimes.indexOf(matchTime) >= 0 && lastSentMinute != curTime) {
        lastSentMinute = curTime; // ล็อคทันทีไม่ให้ส่งซ้ำใน 1 นาทีเดิม
        
        if (isTempValid) { // [FIX] กรองไม่ให้เซฟค่า 0.0, -127.0 ลงในชีท
          if (WiFi.status() == WL_CONNECTED) {
            
            if(sendToGoogleSheets(String(fullTime), currentTemp)) {
              // [FIX] ปรับปรุงตรรกะ Buffer หากส่งค้างสำเร็จถึงจะลบออกทีละตัว (ป้องกันข้อมูลสูญหายถ้ายิงไม่ผ่าน)
              auto it = offlineBuffer.begin();
              while (it != offlineBuffer.end()) {
                if (sendToGoogleSheets(it->timestamp, it->temperature)) {
                  it = offlineBuffer.erase(it); // ส่งผ่านแล้วลบออก
                  delay(500); 
                } else {
                  break; // ยิงไม่ผ่านให้หยุดทำ รอส่งใหม่รอบหน้า
                }
              }
            } else if(offlineBuffer.size() < 50) {
               // ถ้าเน็ตหลุดกระทันหันตอนกำลังส่ง ให้เก็บใส่ Buffer
               offlineBuffer.push_back({String(fullTime), currentTemp});
            }
            
          } else if(offlineBuffer.size() < 50) {
            offlineBuffer.push_back({String(fullTime), currentTemp});
          }
        }
      }
    }
  }

  delay(10); // คืนเวลาให้ OS ประหยัดพลังงาน
}