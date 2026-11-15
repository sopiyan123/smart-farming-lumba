#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ======================= DEKLARASI FUNGSI =======================
String soilStatus(int soil);
String tempStatus(float t);

// ======================= WIFI & MQTT =======================
const char* WIFI_SSID = "Wokwi-GUEST";        // GANTI
const char* WIFI_PASS = "";    // GANTI

const char* MQTT_HOST = "j07117bb.ala.asia-southeast1.emqxsl.com";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "Lumba_Lumba";    // GANTI
const char* MQTT_PASS = "lumba123";    // GANTI

const char* TOPIC_DATA   = "smartfarm/lumba/data";
const char* TOPIC_EVENT  = "smartfarm/lumba/event";
const char* TOPIC_STATUS = "smartfarm/lumba/status";
const char* TOPIC_FREQ   = "smartfarm/lumba/set/buzzer_freq";

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

unsigned long lastMqttTry = 0;

// ======================= PIN =======================
#define SOIL_PIN  34
#define DHTPIN    15
#define TRIG      5
#define ECHO      18
#define PUMP_PIN  26
#define BUZZER    25
#define SERVO_PIN 27

#define RELAY_ON  HIGH
#define RELAY_OFF LOW

DHT dht(DHTPIN, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

const int SOIL_DRY_PERCENT = 40;
const int SOIL_WET_PERCENT = 60;
const int TANK_EMPTY_CM    = 15;
const int TANK_FULL_CM     = 3;

bool pumpOn = false;
bool lastPump = false;
bool lastTankEmpty = false;

float t = 0, h = 0;

unsigned long lastDHT = 0;
unsigned long lastReport = 0;

// ======================= BUZZER =======================
const int BUZZER_CH      = 10;
int       buzzerFreq     = 2000;
const int BUZZER_BEEP_MS = 300;

unsigned long lastBeep = 0;
bool beepState = false;

void buzzerInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER, 2000, 8);
#else
  ledcSetup(BUZZER_CH, 2000, 8);
  ledcAttachPin(BUZZER, BUZZER_CH);
#endif
}

void buzzerTone(int freq) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER, freq);
#else
  ledcWriteTone(BUZZER_CH, freq);
#endif
}

void buzzerOff() {
  buzzerTone(0);
}

void alarmUpdate(bool active) {
  if (!active) {
    buzzerOff();
    beepState = false;
    return;
  }

  if (millis() - lastBeep >= BUZZER_BEEP_MS) {
    lastBeep = millis();
    beepState = !beepState;

    if (beepState) buzzerTone(buzzerFreq);
    else           buzzerOff();
  }
}

// ======================= MQTT =======================
void mqttEvent(const char* msg) {
  if (mqtt.connected()) mqtt.publish(TOPIC_EVENT, msg);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buf[16];
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  if (strcmp(topic, TOPIC_FREQ) == 0) {
    int f = atoi(buf);
    buzzerFreq = constrain(f, 100, 5000);
    Serial.printf("[MQTT] Frekuensi buzzer diubah: %d Hz\n", buzzerFreq);
    mqttEvent("Frekuensi buzzer diubah via MQTT");
  }
}

void wifiConnect() {
  Serial.printf("[WIFI] Menghubungkan ke %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Terhubung, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WIFI] Gagal, akan dicoba lagi di loop");
  }
}

// Non-blocking: dicoba tiap 5 detik, tidak menahan kontrol pompa
void mqttReconnect() {
  if (mqtt.connected()) return;
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Terputus, mencoba menyambung ulang...");
    WiFi.reconnect();
    return;
  }

  String clientId = "esp32-lumba-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("[MQTT] Menghubungkan... ");

  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                   TOPIC_STATUS, 1, true, "offline")) {
    Serial.println("terhubung");
    mqtt.publish(TOPIC_STATUS, "online", true);
    mqtt.subscribe(TOPIC_FREQ);
  } else {
    Serial.printf("gagal, rc=%d\n", mqtt.state());
  }
}

void mqttPublishData(int soil, float dist, int tank, bool tankEmpty) {
  if (!mqtt.connected()) return;

  char json[320];
  snprintf(json, sizeof(json),
           "{\"soil\":%d,\"soil_status\":\"%s\",\"temp\":%.1f,\"hum\":%.0f,"
           "\"dist\":%.0f,\"tank\":%d,\"tank_empty\":%s,"
           "\"pump\":%s,\"alarm\":%s,\"buzzer_hz\":%d}",
           soil, soilStatus(soil).c_str(), t, h,
           dist >= 999 ? -1.0 : dist, tank, tankEmpty ? "true" : "false",
           pumpOn ? "true" : "false", tankEmpty ? "true" : "false", buzzerFreq);

  mqtt.publish(TOPIC_DATA, json);
}

// ======================= FUNGSI BANTU =======================
String soilStatus(int soil) {
  if (soil < SOIL_DRY_PERCENT) return "Kering";
  if (soil <= SOIL_WET_PERCENT) return "Cukup";
  return "Basah";
}

String tempStatus(float t) {
  if (t < 22) return "Dingin";
  if (t < 28) return "Nyaman";
  if (t < 32) return "Hangat";
  return "PANAS";
}

float readDistanceCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  long d = pulseIn(ECHO, HIGH, 30000);

  if (d == 0) return 999;

  return d * 0.0343 / 2;
}

int tankPercent(float dist) {
  float p = (TANK_EMPTY_CM - dist) * 100.0 /
            (TANK_EMPTY_CM - TANK_FULL_CM);

  return constrain((int)p, 0, 100);
}

void lcdLine(int row, String s) {
  while (s.length() < 16) s += ' ';

  lcd.setCursor(0, row);
  lcd.print(s.substring(0, 16));
}

void printReport(int soil, float dist, int tank, bool tankEmpty) {
  Serial.println();
  Serial.println("============== LAPORAN SMART FARMING ==============");

  Serial.printf("Kelembapan tanah : %d %%  -> %s (kering < %d%%, basah > %d%%)\n",
                soil, soilStatus(soil).c_str(),
                SOIL_DRY_PERCENT, SOIL_WET_PERCENT);

  Serial.printf("Suhu udara       : %.1f C  -> %s\n",
                t, tempStatus(t).c_str());

  Serial.printf("Kelembapan udara : %.0f %%\n", h);

  if (dist >= 999) {
    Serial.println("Jarak ke air     : tidak terbaca");
  } else {
    Serial.printf("Jarak ke air     : %.0f cm\n", dist);
  }

  Serial.printf("Isi tandon       : %d %%  -> %s\n",
                tank, tankEmpty ? "KOSONG!" : "Aman");

  Serial.printf("Pompa air        : %s\n",
                pumpOn ? "MENYALA (menyiram)" : "MATI");

  Serial.printf("Alarm            : %s (%d Hz)\n",
                tankEmpty ? "BERBUNYI (tandon kosong)" : "Diam",
                buzzerFreq);

  Serial.printf("WiFi / MQTT      : %s / %s\n",
                WiFi.status() == WL_CONNECTED ? "OK" : "putus",
                mqtt.connected() ? "OK" : "putus");

  Serial.println("===================================================");
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, RELAY_OFF);

  dht.begin();

  lcd.init();
  lcd.backlight();

  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  buzzerInit();
  buzzerOff();

  lcdLine(0, "Smart Farming");
  lcdLine(1, "Connecting WiFi");

  Serial.println("--------------------------------------------------");
  Serial.println(" SMART FARMING - Penyiraman Otomatis (ESP32)");
  Serial.println("--------------------------------------------------");

  Serial.println("Cara kerja singkat:");
  Serial.println(" 1. Tanah KERING (< 40%) -> pompa menyala, tanaman disiram");
  Serial.println(" 2. Tanah BASAH (> 60%)  -> pompa mati");
  Serial.println(" 3. Antara 40-60%        -> pompa tetap di kondisi terakhir");
  Serial.println(" 4. Tandon KOSONG        -> pompa dipaksa mati + alarm bunyi");
  Serial.println("--------------------------------------------------");

  // WiFi + MQTT (TLS)
  wifiConnect();
  espClient.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);

  delay(1000);
  lcd.clear();

  float nt = dht.readTemperature();
  float nh = dht.readHumidity();

  if (!isnan(nt) && !isnan(nh)) {
    t = nt;
    h = nh;
  }
}

// ======================= LOOP =======================
void loop() {
  // --- MQTT ---
  mqttReconnect();
  mqtt.loop();

  // --- Baca sensor ---
  int raw = analogRead(SOIL_PIN);
  int soil = map(raw, 0, 4095, 0, 100);

  if (millis() - lastDHT > 2000) {
    float nt = dht.readTemperature();
    float nh = dht.readHumidity();

    if (!isnan(nt) && !isnan(nh)) {
      t = nt;
      h = nh;
    } else {
      Serial.println("[PERINGATAN] Sensor DHT22 gagal dibaca, cek kabel!");
    }

    lastDHT = millis();
  }

  float dist = readDistanceCm();
  bool tankEmpty = dist > TANK_EMPTY_CM;
  int tank = tankPercent(dist);

  // --- Logika pompa (hysteresis) ---
  if (tankEmpty) {
    pumpOn = false;
  } else if (soil < SOIL_DRY_PERCENT) {
    pumpOn = true;
  } else if (soil > SOIL_WET_PERCENT) {
    pumpOn = false;
  }

  // --- Kontrol servo ---
  if (tankEmpty) {
    myServo.write(0);
  } else if (soil < SOIL_DRY_PERCENT) {
    myServo.write(90);
  } else if (soil > SOIL_WET_PERCENT) {
    myServo.write(0);
  }

  digitalWrite(PUMP_PIN, pumpOn ? RELAY_ON : RELAY_OFF);

  // --- Alarm buzzer ---
  alarmUpdate(tankEmpty);

  // --- Event log (serial + MQTT) ---
  if (pumpOn != lastPump) {
    if (pumpOn) {
      Serial.printf("[EVENT] Tanah KERING (%d%%) -> Pompa MENYALA, tanaman disiram\n", soil);
      mqttEvent("Tanah kering, pompa MENYALA");
    } else if (tankEmpty) {
      Serial.println("[EVENT] Tandon kosong -> Pompa DIMATIKAN paksa agar tidak rusak");
      mqttEvent("Tandon kosong, pompa dimatikan paksa");
    } else {
      Serial.printf("[EVENT] Tanah sudah cukup basah (%d%%) -> Pompa MATI\n", soil);
      mqttEvent("Tanah cukup basah, pompa MATI");
    }

    lastPump = pumpOn;
  }

  if (tankEmpty != lastTankEmpty) {
    if (tankEmpty) {
      Serial.println("[ALARM!] Air di tandon HABIS -> buzzer berbunyi, isi ulang tandon!");
      mqttEvent("ALARM: tandon habis");
    } else {
      Serial.println("[INFO] Tandon sudah terisi air lagi -> alarm berhenti");
      mqttEvent("Tandon terisi lagi, alarm berhenti");
    }

    lastTankEmpty = tankEmpty;
  }

  // --- LCD (ganti halaman tiap 3 detik) ---
  int page = (millis() / 3000) % 2;

  if (page == 0) {
    lcdLine(0, "Tanah:" + String(soil) + "% " + soilStatus(soil));
    lcdLine(1, String("Pompa: ") + (pumpOn ? "MENYALA" : "MATI"));
  } else {
    lcdLine(0, "Suhu:" + String(t, 1) + "C " + String(h, 0) + "%");
    lcdLine(1, tankEmpty ? String("TANDON KOSONG!") : "Tandon: " + String(tank) + "%");
  }

  // --- Laporan serial + publish MQTT tiap 3 detik ---
  if (millis() - lastReport > 3000) {
    printReport(soil, dist, tank, tankEmpty);
    mqttPublishData(soil, dist, tank, tankEmpty);
    lastReport = millis();
  }

  delay(100);
}