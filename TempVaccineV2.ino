#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_task_wdt.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include <vector>

#define WDT_TIMEOUT 30

// ================= การตั้งค่าเครือข่าย (แก้ไขตรงนี้ให้ตรงกับของคุณ) =================
const char* ssid = "******";
const char* password = "******";
const String GOOGLE_SCRIPT_URL = "*******"; 

// ================= ตั้งค่าฮาร์ดแวร์ =================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(4);
DallasTemperature sensors(&oneWire);
WebServer server(80);

// ================= ตัวแปรระบบ =================
float currentTemp = 0.0;
float tempOffset = 0.0; 
String scheduleTimes = "08:00,12:00,18:00";
String lastSentMinute = "";

struct DataLog {
  String timestamp;
  float temperature;
};
std::vector<DataLog> offlineBuffer;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;

// ================= หน้าเว็บ HTML (ฝังใน PROGMEM) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Temperature Dashboard</title>
    <style>
        :root { --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --accent: #38bdf8; --danger: #fb7185; }
        body { font-family: 'Segoe UI', Tahoma, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .header { width: 100%; max-width: 800px; display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
        .clock { font-size: 1.2rem; color: #94a3b8; }
        .gear-btn { font-size: 1.8rem; cursor: pointer; background: none; border: none; color: var(--accent); transition: transform 0.3s; }
        .gear-btn:hover { transform: rotate(90deg); }
        .card { background: var(--card); padding: 40px; border-radius: 20px; text-align: center; width: 100%; max-width: 400px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
        .temp-value { font-size: 5rem; font-weight: bold; color: var(--accent); margin: 20px 0; }
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.7); justify-content: center; align-items: center; }
        .modal-content { background: var(--card); padding: 30px; border-radius: 15px; width: 350px; }
        .modal-content label { display: block; margin-top: 15px; font-size: 0.9rem; color: #cbd5e1; }
        .modal-content input { width: 90%; padding: 10px; margin-top: 5px; border-radius: 8px; border: 1px solid #334155; background: #0f172a; color: white; }
        .btn-save { margin-top: 20px; width: 100%; padding: 12px; background: var(--accent); border: none; border-radius: 8px; color: #0f172a; font-weight: bold; cursor: pointer; }
    </style>
</head>
<body>
    <div class="header">
        <button class="gear-btn" onclick="openSettings()">⚙️</button>
        <div class="clock" id="liveClock">กำลังโหลดเวลา...</div>
    </div>
    <div class="card">
        <div>อุณหภูมิปัจจุบัน (IP: <span id="ipDisplay">Loading...</span>)</div>
        <div class="temp-value" id="tempDisplay">--</div>
        <div>°C (ชดเชยค่าแล้ว)</div>
    </div>
    <div class="modal" id="settingsModal">
        <div class="modal-content">
            <h3>⚙️ ตั้งค่าระบบ</h3>
            <label>ชดเชยอุณหภูมิ (Offset) เช่น -0.5 หรือ +1.2:</label>
            <input type="number" step="0.1" id="tempOffset" value="0.0">
            <label>เวลาส่งเข้า Google Sheets (เช่น 08:00,12:00,18:00):</label>
            <input type="text" id="scheduleTimes" value="08:00,12:00,18:00">
            <button class="btn-save" onclick="saveSettings()">บันทึกการตั้งค่า</button>
            <button class="btn-save" style="background:#475569; margin-top:10px;" onclick="closeSettings()">ยกเลิก</button>
        </div>
    </div>

    <script>
        // ดึง IP มาแสดงอัตโนมัติจาก URL ที่เปิดอยู่
        document.getElementById('ipDisplay').innerText = window.location.hostname;

        // นาฬิกา Real-time
        setInterval(() => {
            const now = new Date();
            const options = { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric', hour: '2-digit', minute:'2-digit', second:'2-digit' };
            document.getElementById('liveClock').innerText = now.toLocaleDateString('th-TH', options);
        }, 1000);

        // ดึงข้อมูลอุณหภูมิทุก 2 วินาที
        setInterval(async () => {
            try {
                const res = await fetch('/api/data');
                const data = await res.json();
                document.getElementById('tempDisplay').innerText = data.temperature.toFixed(2);
            } catch (err) {
                document.getElementById('tempDisplay').innerText = "Err";
            }
        }, 2000);

        function openSettings() { document.getElementById('settingsModal').style.display = 'flex'; }
        function closeSettings() { document.getElementById('settingsModal').style.display = 'none'; }
        
        async function saveSettings() {
            const offset = parseFloat(document.getElementById('tempOffset').value);
            const schedules = document.getElementById('scheduleTimes').value;
            try {
                await fetch('/api/settings', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ offset: offset, schedules: schedules })
                });
                alert("บันทึกค่าสำเร็จ!");
                closeSettings();
            } catch(e) {
                alert("เกิดข้อผิดพลาดในการบันทึก");
            }
        }
    </script>
</body>
</html>
)rawliteral";

// ================= ฟังก์ชันจัดการเครือข่ายและเวลา =================
void setupNTP() { 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
}

bool sendToGoogleSheets(String timestamp, float temp) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  // เพิ่มการตั้งค่าเพื่อรองรับ HTTPS ของ Google
  WiFiClientSecure client;
  client.setInsecure(); // ข้ามการตรวจสอบ SSL Certificate
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // จำเป็นมากเพราะ Google มีการ Redirect
  http.begin(client, GOOGLE_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<200> doc;
  doc["timestamp"] = timestamp;
  doc["temperature"] = temp;
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpResponseCode = http.POST(requestBody);
  http.end();
  
  // Google มักจะตอบกลับเป็นโค้ด 200 หรือ 302 ถือว่าสำเร็จ
  return (httpResponseCode == 200 || httpResponseCode == 302);
}

// ================= API Endpoints =================
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{\"temperature\":" + String(currentTemp) + "}";
  server.send(200, "application/json", json);
}

void handleSettings() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("plain") == false) { 
    server.send(400, "text/plain", "Body not received"); 
    return; 
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  deserializeJson(doc, body);
  
  tempOffset = doc["offset"];
  scheduleTimes = doc["schedules"].as<String>();
  
  server.send(200, "application/json", "{\"status\":\"success\"}");
}

// ================= ฟังก์ชันหลัก (Setup) =================
void setup() {
  Serial.begin(115200);
  
  // 1. ตั้งค่า Watchdog Timer (รองรับ ESP32 Core v3.x)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  // 2. เริ่มการทำงานจอและเซนเซอร์
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sensors.begin();
  sensors.setWaitForConversion(false); 

  // 3. กระบวนการเชื่อมต่อ Wi-Fi ที่เสถียร
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.print("Connecting WiFi...");
  display.display();

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset(); // เลี้ยง Watchdog
    wifiAttempts++;
    
    // รีสตาร์ทถ้ารอเกิน 15 วินาที
    if (wifiAttempts > 30) {
      display.clearDisplay();
      display.setCursor(0, 20);
      display.print("WiFi Failed!");
      display.setCursor(0, 40);
      display.print("Rebooting...");
      display.display();
      delay(2000);
      ESP.restart();
    }
  }

  // 4. แสดง IP บนจอค้างไว้ 3 วินาที
  display.clearDisplay();
  display.setCursor(0, 10);  display.print("WiFi Connected!");
  display.setCursor(0, 30);  display.print("IP: "); display.print(WiFi.localIP());
  display.setCursor(0, 50);  display.print("Starting Server...");
  display.display();
  Serial.println("\nIP: " + WiFi.localIP().toString());
  delay(3000); 

  // 5. เปิด Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/settings", HTTP_POST, handleSettings);
  
  // จัดการ CORS OPTIONS เผื่อทดสอบ API ภายนอก
  server.on("/api/settings", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204); 
  });
  
  server.begin();
}

// ================= ฟังก์ชันการทำงาน (Loop) =================
void loop() {
  esp_task_wdt_reset(); // รายงานตัวว่าบอร์ดไม่ค้าง
  unsigned long currentMillis = millis();

  // 1. จัดการ Server & NTP
  if (WiFi.status() == WL_CONNECTED) {
    static bool timeConfigured = false;
    if(!timeConfigured) { 
      setupNTP(); 
      timeConfigured = true; 
    }
    server.handleClient(); // รอรับคำสั่งจากหน้าเว็บ
  }

  // 2. อ่านเซนเซอร์ (ทุก 2 วินาที) แบบ Non-blocking
  static unsigned long previousTempMillis = 0;
  static bool isRequestingTemp = false;
  
  if (!isRequestingTemp && (currentMillis - previousTempMillis >= 2000)) {
    sensors.requestTemperatures();
    previousTempMillis = currentMillis;
    isRequestingTemp = true;
  }
  
  if (isRequestingTemp && (currentMillis - previousTempMillis >= 750)) {
    // บวกค่า Offset ที่ตั้งมาจากหน้าเว็บ
    currentTemp = sensors.getTempCByIndex(0) + tempOffset;
    isRequestingTemp = false;
    
    // อัปเดตจอ OLED แบบชัดเจน
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0); 
    
    if (WiFi.status() == WL_CONNECTED) {
      display.print("IP:"); display.print(WiFi.localIP().toString());
    } else {
      display.print("WiFi: Offline");
    }
    
    display.setTextSize(2); 
    display.setCursor(0, 25); 
    display.print(currentTemp, 1); 
    display.print(" C");
    
    display.setTextSize(1);
    display.setCursor(0, 55); 
    display.print("Buffer: "); 
    display.print(offlineBuffer.size());
    
    display.display();
  }

  // 3. จัดการ Google Sheets & Offline Buffer (เช็คเวลาทุก 1 วินาที)
  static unsigned long lastTimeCheck = 0;
  if (currentMillis - lastTimeCheck >= 1000) {
    lastTimeCheck = currentMillis;
    struct tm timeinfo;
    
    if (getLocalTime(&timeinfo)) {
      char timeStringBuff[10]; 
      strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
      String currentTime = String(timeStringBuff);
      
      char fullTimeBuff[30];
      strftime(fullTimeBuff, sizeof(fullTimeBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
      String fullTimestamp = String(fullTimeBuff);

      // ถ้าถึงเวลาในตาราง และยังไม่ได้ส่งในนาทีนี้
      if (scheduleTimes.indexOf(currentTime) >= 0 && lastSentMinute != currentTime) {
        lastSentMinute = currentTime;
        
        if (WiFi.status() == WL_CONNECTED) {
          bool success = sendToGoogleSheets(fullTimestamp, currentTemp);
          
          // ทยอยส่งข้อมูลที่ค้างอยู่ใน Buffer หากส่งค่าล่าสุดสำเร็จ
          if(success && offlineBuffer.size() > 0) {
            for(int i=0; i<offlineBuffer.size(); i++) {
              sendToGoogleSheets(offlineBuffer[i].timestamp, offlineBuffer[i].temperature);
              delay(500); // หน่วงเวลาเล็กน้อยป้องกัน Google Block
            }
            offlineBuffer.clear();
          }
        } else {
          // เก็บข้อมูลลง Buffer หากออฟไลน์ (สูงสุด 50 ชุด)
          if(offlineBuffer.size() < 50) {
            offlineBuffer.push_back({fullTimestamp, currentTemp});
          }
        }
      }
    }
  }
}
