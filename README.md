# TempVaccine

1. ไลบรารีที่ต้องติดตั้งเพิ่ม (External Libraries)
กลุ่มนี้คุณต้องเข้าไปที่ Library Manager (Ctrl + Shift + I) ใน Arduino IDE เพื่อค้นหาและกด Install ครับ:

OneWire (โดย Paul Stoffregen)

ใช้สำหรับสื่อสารกับเซนเซอร์ผ่านสายสัญญาณเพียงเส้นเดียว

DallasTemperature (โดย Miles Burton)

ใช้สำหรับแปลงสัญญาณจากเซนเซอร์ DS18B20 ให้เป็นค่าอุณหภูมิองศาเซลเซียส

Adafruit GFX Library (โดย Adafruit)

ไลบรารีพื้นฐานสำหรับการวาดกราฟิกและตัวอักษร

Adafruit SSD1306 (โดย Adafruit)

ไลบรารีเฉพาะสำหรับควบคุมหน้าจอ OLED ขนาด 0.96 นิ้ว

(ตอนกด Install หากมีหน้าต่างถามหา "Dependencies" ให้เลือก Install All)

2. ไลบรารีมาตรฐานที่ติดมากับบอร์ด (Built-in ESP32 Libraries)
กลุ่มนี้ไม่ต้องติดตั้งเพิ่มครับ ระบบจะเรียกใช้งานได้ทันทีเมื่อคุณเลือกบอร์ดเป็น ESP32 ในโปรแกรม Arduino IDE:

WiFi.h และ WiFiClientSecure.h

ใช้จัดการการเชื่อมต่อ Wi-Fi และระบบความปลอดภัย HTTPS สำหรับ LINE และ Google

WebServer.h

ใช้สร้างระบบ Web Server เพื่อแสดงผลหน้าแดชบอร์ดในตัวบอร์ด

HTTPClient.h

ใช้ส่งข้อมูล (POST Request) ไปยัง Google Sheets และ LINE Messaging API

Wire.h

ใช้สื่อสารกับหน้าจอ OLED ผ่านพอร์ต I2C (SDA/SCL)

esp_task_wdt.h

ระบบ Watchdog Timer ป้องกันบอร์ดค้าง

time.h

ใช้ดึงเวลาปัจจุบันจากอินเทอร์เน็ต (NTP Server) เพื่อระบุเวลาที่บันทึกลง Google Sheets

vector

ใช้สร้างหน่วยความจำชั่วคราว (Buffer) สำหรับเก็บข้อมูลเมื่ออินเทอร์เน็ตขัดข้อง
