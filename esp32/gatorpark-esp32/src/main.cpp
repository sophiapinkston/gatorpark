#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_MMC56x3.h>

//Wifi and RDTB Configuration, as well as spot paths for the simulated parking spots. 
const char* WIFI_SSID = "B308-3000@ENCLAVE";
const char* WIFI_PASS = "PG22f#rhv1Kb";
const char* RTDB_BASE = "https://gatorpark-1d593-default-rtdb.firebaseio.com";
String spotPath_A1 = "/garages/A/spots/A1.json";
String spotPath_A2 = "/garages/A/spots/A2.json";
String spotPath_A3 = "/garages/A/spots/A3.json";

// Pin specifications and sensor object
#define TRIG_PIN 33
#define ECHO_PIN 32
#define I2C_SDA  21
#define I2C_SCL  22

Adafruit_MMC5603 mmc;

// Debouncing and sampling parameters for reading sensors 
const uint32_t SAMPLE_MS    = 100;          // slightly slower loop for stability (was 80)
const uint32_t HEARTBEAT_MS = 5000;

float US_ENTER_CM  = 25.0f;                 // widened hysteresis for steadier enter
float US_EXIT_CM   = 12.0f;                 // widened hysteresis for steadier exit
const int HIT_UP   = 5;
const int HIT_DOWN = 10;                    // require more agreement to begin clearing
int hitCounter = 0;
bool occupied = false;

// Baseline establishment for sensor data 
bool baselineDone = false;
uint32_t baseT0 = 0;
float usAccum = 0;  uint32_t usCount = 0;
float usBaseline = NAN;
float magEMA = NAN, magBaseline = NAN;

// RTDB state tracking (utilized for change detection for data analytics & for lastBoot timestamp)
uint32_t lastPush = 0;
bool lastOccupied = false;
uint32_t lastSample = 0;

// ---- additions for stability (no comment text above changed) ----
const uint32_t EXIT_LINGER_MS = 3000;       // require 3s of consistent "free" before clearing
uint32_t clearStartTs = 0;
int enterHits = 0;
int exitHits  = 0;

// 5-sample median filter for ultrasonic
static const int MED_N = 5;
float usBuf[MED_N];
int   usIdx = 0;
bool  usFilled = false;
static float median5(float a[], int n) {
  float b[MED_N];
  for (int i=0;i<n;i++) b[i]=a[i];
  for (int i=1;i<n;i++){ float key=b[i]; int j=i-1; while(j>=0 && b[j]>key){ b[j+1]=b[j]; j--; } b[j+1]=key; }
  return b[n/2];
}
inline void usFilterAdd(float v) { if (v >= 0) { usBuf[usIdx++] = v; if (usIdx>=MED_N){ usIdx=0; usFilled=true; } } }
inline bool usFilterReady()      { return usFilled || usIdx>2; }
inline float usFiltered()        { int n = usFilled ? MED_N : max(usIdx,1); return median5(usBuf,n); }

// WiFi connection establishment, utilized for testing and ensuring connectivity
void wifiConnect(uint32_t timeout_ms = 15000) {
  Serial.print("Wi-Fi: connecting to \""); Serial.print(WIFI_SSID); Serial.println("\" …");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout_ms) {
    delay(300);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWi-Fi connected ✓  IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi FAILED (timeout). Continuing anyway...");
  }
}

bool rtdbPatch(const String& path, const String& jsonBody) {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("RTDB: skip send (Wi-Fi down)");
      return false;
    }
  }
  HTTPClient http;
  String url = String(RTDB_BASE) + path;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(jsonBody);
  Serial.print("RTDB PATCH "); Serial.print(url); Serial.print(" -> "); Serial.println(code);
  http.end();
  return (code >= 200 && code < 300);
}

/* SENSORS 
Read distance in cm from ultrasonic sensor, and then from the I2c bus for the magnetometer
*/ 
float readUltrasonicCM() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (dur == 0) return -1.0f;
  return (dur * 0.0343f) / 2.0f;
}

void i2cScan() {
  Serial.println("I2C scan:");
  for (uint8_t a=3; a<0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission()==0) {
      Serial.print("  - 0x"); if (a<16) Serial.print('0'); Serial.println(a, HEX);
    }
  }
}

// Updating RTDB with spot status and sensor readings, as well as lastBoot for state tracking
void publishSpot(const String& path, bool occ, float us_cm, float mag_uT, bool isBoot=false) {
  uint32_t now = millis();
  String body = "{";
  body += "\"occupied\":"; body += (occ ? "true" : "false"); body += ",";
  if (!isnan(us_cm)) { body += "\"us_cm\":"; body += String(us_cm, 1); body += ","; }
  if (!isnan(mag_uT)) { body += "\"mag_uT\":"; body += String(mag_uT, 1); body += ","; }
  body += "\"lastBoot\":"; body += String((unsigned long)now);
  body += "}";
  rtdbPatch(path, body);
}

//MCU Setup and Main loop
/* SETUP:
  - Initialize Serial for debugging
  - Setup pins for ultrasonic sensor
  - Initialize I2C and scan for devices
  - Initialize MMC5603 magnetometer
  - Connect to WiFi
  - Simulate spots A2 and A3 as unoccupied in RTDB 
  - Begin baseline establishment for spot A1 (A1 is the sensor driven spot)
*/
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nGatorPark — Garage A (A1 sensor + simulated A2/A3)");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  i2cScan();
  Serial.print("MMC5603: "); Serial.println(mmc.begin() ? "OK" : "FAIL");

  wifiConnect();

  // Hardcoding A2 & A3 as open (occupied = false)
  Serial.println("Publishing simulated A2 & A3 as unoccupied...");
  publishSpot(spotPath_A2, false, NAN, NAN, true);
  publishSpot(spotPath_A3, false, NAN, NAN, true);

  // baseline
  baselineDone = false;
  baseT0 = millis();
  Serial.println("Keep the spot empty for ~5s to baseline...");
}

/*
LOOP:
  - Sample ultrasonic and magnetometer data at defined intervals
  - Establish baselines if not done
  - Determine occupancy based on sensor readings with debouncing
  - Print sensor readings and occupancy status periodically
  - Update RTDB on occupancy change or lastBoot interval (heartbeat)
*/
void loop() {
  uint32_t now = millis();
  if (now - lastSample < SAMPLE_MS) return;
  lastSample = now;

  float us = readUltrasonicCM();
  if (us >= 0) usFilterAdd(us);                      // feed median filter (ignore NoEcho)
  float usStab = usFilterReady() ? usFiltered() : us;

  sensors_event_t evt; mmc.getEvent(&evt);
  float magAbs = sqrtf(evt.magnetic.x*evt.magnetic.x +
                       evt.magnetic.y*evt.magnetic.y +
                       evt.magnetic.z*evt.magnetic.z);

  if (!baselineDone) {
    if (us >= 0) { usAccum += us; usCount++; }
    if (isnan(magEMA)) magEMA = magAbs;
    else magEMA = 0.9f*magEMA + 0.1f*magAbs;

    if (now - baseT0 >= 5000) {
      if (usCount) usBaseline = usAccum / usCount;
      magBaseline = magEMA;
      baselineDone = true;
      Serial.println("\n--- Baselines locked ---");
      Serial.print("US baseline: "); Serial.println(usBaseline, 1);
      Serial.print("|B| baseline: "); Serial.println(magBaseline, 1);
      publishSpot(spotPath_A1, false, usBaseline, magBaseline, true);
    }
    return;
  }

  float dUS = (isnan(usBaseline) || usStab < 0) ? NAN : (usBaseline - usStab);
  bool haveUS = (usStab >= 0);
  bool usEnter = (!isnan(dUS) && dUS >= US_ENTER_CM);
  bool usExit  = (!isnan(dUS) && dUS <= US_EXIT_CM);

  if (!occupied) {
    // accumulate enters; decay slowly on counter-evidence
    if (usEnter) enterHits++; else if (haveUS) enterHits = max(0, enterHits - 1);
    exitHits = 0; clearStartTs = 0;
    if (enterHits >= HIT_UP) { occupied = true; enterHits = 0; }
  } else {
    // need many exits AND 3s of continuous exit before clearing
    if (usExit) {
      if (exitHits < HIT_DOWN) exitHits++;
      if (exitHits >= HIT_DOWN) {
        if (clearStartTs == 0) clearStartTs = now;
        if (now - clearStartTs >= EXIT_LINGER_MS) {
          occupied = false; exitHits = 0; clearStartTs = 0;
        }
      }
    } else if (haveUS) {
      exitHits = 0; clearStartTs = 0; // any counter evidence cancels clear
    }
  }

  // keep your debug print intact; map hitCounter to the active counter
  hitCounter = occupied ? exitHits : enterHits;

  static uint32_t lastPrint = 0;
  if (now - lastPrint > 500) {
    lastPrint = now;
    Serial.print("US="); if (usStab < 0) Serial.print("NoEcho"); else { Serial.print(usStab,1); Serial.print("cm"); }
    Serial.print(" ΔUS="); if (isnan(dUS)) Serial.print("-"); else Serial.print(dUS,1);
    Serial.print("  OCCUPIED="); Serial.print(occupied ? "YES" : "no");
    Serial.print("  (hc="); Serial.print(hitCounter); 
    if (clearStartTs) { Serial.print(", clearing_in≈"); Serial.print(max<int>(0,(EXIT_LINGER_MS - (now - clearStartTs))/1000)); Serial.print("s"); }
    Serial.println(")");
  }

  //Update RTDB on change or periodic
  bool change = (occupied != lastOccupied);
  bool heartbeat = (now - lastPush >= HEARTBEAT_MS);
  if (change || heartbeat) {
    publishSpot(spotPath_A1, occupied, usStab, magAbs);
    lastPush = now;
    lastOccupied = occupied;
  }
}
