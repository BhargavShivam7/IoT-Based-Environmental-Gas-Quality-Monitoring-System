/*
 * AetherSense Pro: Environmental Monitor & Alarm System
 * Hardware: NodeMCU (ESP8266), DHT11, MQ-135, 5V Active Buzzer (High-Impedance Hack)
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"

// =================================================================
// --- CLOUD & NETWORK CONFIGURATION ---
// =================================================================
const char* ssid = "DarkHunterOP";         
const char* password = "g5abehq8";         
const char* deviceID = "ESP_001"; 
const char* serverUrl = "https://iot-based-environmental-gas-quality.onrender.com/api/post_data"; 

// =================================================================
// --- PHYSICAL ALARM THRESHOLDS ---
// =================================================================
const int LOCAL_GAS_WARNING = 200; // Edge-Case Testing Threshold
const int LOCAL_GAS_HAZARD = 300;

// =================================================================
// --- HARDWARE PINS ---
// =================================================================
#define DHTPIN D3     
#define MQ_PIN A0     
#define BUZZER_PIN D5 

#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

// --- BACKGROUND TIMER ---
unsigned long previousMillis = 0; 
const long interval = 30000; 
bool firstRun = true; 

void setup() {
  Serial.begin(115200); 
  Serial.println("\n[AetherSense Node Booting...]");
  
  // THE HACK: Setting to INPUT disconnects the pin and forces silence
  pinMode(BUZZER_PIN, INPUT); 
  
  dht.begin();          
  setup_wifi();         
}

void loop() {
  // =================================================================
  // 1. INSTANT ALARM SYSTEM
  // =================================================================
  int currentGas = analogRead(MQ_PIN);

  if (currentGas >= LOCAL_GAS_HAZARD) {
    // RED HAZARD: Switch to OUTPUT and pull LOW to scream
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100); 
    // Switch to INPUT to silence
    pinMode(BUZZER_PIN, INPUT); 
    delay(100);
  } 
  else if (currentGas >= LOCAL_GAS_WARNING) {
    // ORANGE WARNING
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500); 
    pinMode(BUZZER_PIN, INPUT); 
    delay(1000);
  } 
  else {
    // GREEN SAFE: Ensure pin is disconnected (Silent)
    pinMode(BUZZER_PIN, INPUT); 
  }

  // =================================================================
  // 2. BACKGROUND DATA UPLOAD
  // =================================================================
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval || firstRun) {
    previousMillis = currentMillis;
    firstRun = false;

    float h = dht.readHumidity();
    float t = dht.readTemperature(); 

    if (isnan(h) || isnan(t)) {
      Serial.println("DHT read failed! Check wiring.");
      return; 
    }

    Serial.println("\n--- Packaging Data ---");
    Serial.print("Temp: "); Serial.print(t); Serial.println(" °C");
    Serial.print("Humidity: "); Serial.print(h); Serial.println(" %");
    Serial.print("Gas Level: "); Serial.println(currentGas);

    StaticJsonDocument<200> doc;
    doc["device_id"] = deviceID; 
    doc["temperature"] = t;
    doc["humidity"] = h;
    doc["gas_level"] = currentGas;

    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    // --- AGGRESSIVE AUTO-RECONNECT FAILSAFE ---
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Wi-Fi dropped! Forcing re-authentication");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid, password); 
      
      int retries = 0;
      while(WiFi.status() != WL_CONNECTED && retries < 20) { 
        delay(500);
        Serial.print(".");
        retries++;
      }
      Serial.println();
    }

    // =================================================================
    // RADIO SILENCE PROTOCOL (Using the Input Hack)
    // =================================================================
    pinMode(BUZZER_PIN, INPUT); 
    delay(1000); 

    // Send to Render
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Sending to Render Cloud...");
      
      WiFiClientSecure secureClient;
      secureClient.setBufferSizes(512, 512); 
      
      HTTPClient http;
      secureClient.setInsecure(); 
      http.begin(secureClient, serverUrl);
      
      http.setTimeout(60000); 
      http.addHeader("Content-Type", "application/json"); 
      http.addHeader("Connection", "close"); 
      
      int httpCode = http.POST(jsonBuffer); 
      
      if (httpCode > 0) {
        Serial.print("Server Response: "); Serial.println(httpCode);
      } else {
        Serial.print("Error sending POST: "); Serial.println(http.errorToString(httpCode));
      }
      http.end(); 
    } else {
      Serial.println("Wi-Fi disconnected. Waiting for next cycle.");
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.setSleepMode(WIFI_NONE_SLEEP); 
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}