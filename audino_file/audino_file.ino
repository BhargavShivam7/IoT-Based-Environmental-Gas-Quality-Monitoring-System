/*
 * IoT Environmental Monitor Firmware (PRO VERSION)
 * * Hardware: NodeMCU (ESP8266), DHT11, MQ-135
 * * Libraries: "DHT sensor library", "ArduinoJson"
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"

// =================================================================
// --- CONFIGURATION - YOU MUST EDIT THESE VALUES ---
// =================================================================
const char* ssid = "DarkHunterOP";         // Your Wi-Fi name
const char* password = "g5abehq8";         // Your Wi-Fi password

// *** NEW: MULTI-DEVICE SUPPORT ***
// This must match EXACTLY what you type into the "Add Device" form on the dashboard.
const char* deviceID = "ESP_001"; 

// *** NEW: API ENDPOINT URL ***
// If testing LOCALLY (VS Code), use your computer's local IP address from the terminal (e.g., http://192.168.0.103:5000/api/post_data). 
// If deployed to Render, use: https://iot-based-environmental-gas-quality.onrender.com/api/post_data
const char* serverUrl = "https://iot-based-environmental-gas-quality.onrender.com/api/post_data"; 
// =================================================================

// --- SENSOR PINS ---
#define DHTPIN D3     // Digital pin D3
#define MQ_PIN A0     // Analog pin A0
#define DHTTYPE DHT11 // We are using the DHT11 sensor

// --- GLOBAL OBJECTS ---
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200); 
  Serial.println("\n[IoT Monitor Starting Up]");
  dht.begin();          
  setup_wifi();         
}

void loop() {
  // 1. Read sensor data
  float h = dht.readHumidity();
  float t = dht.readTemperature(); 
  int g = analogRead(MQ_PIN);

  // 2. Check if sensor reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(5000);
    return;
  }

  // Print values to the Serial Monitor 
  Serial.println("--------------------");
  Serial.print("Device ID: "); Serial.println(deviceID);
  Serial.print("Temperature: "); Serial.print(t); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(h); Serial.println(" %");
  Serial.print("Gas Level: "); Serial.println(g);

  // 3. Create JSON document
  StaticJsonDocument<200> doc;
  
  // *** NEW: Add Device ID to the payload ***
  doc["device_id"] = deviceID; 
  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["gas_level"] = g;

  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  // 4. Send the data
  Serial.println("Sending data to server...");
  
  WiFiClient client;
  WiFiClientSecure secureClient;
  HTTPClient http;

  // Check if URL is HTTPS (Render) or HTTP (Local testing)
  if (String(serverUrl).startsWith("https")) {
    secureClient.setInsecure(); // Accept self-signed certificates if needed
    http.begin(secureClient, serverUrl);
  } else {
    http.begin(client, serverUrl);
  }
  
  http.addHeader("Content-Type", "application/json"); 
  
  int httpCode = http.POST(jsonBuffer); 
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("HTTP Response code: "); Serial.println(httpCode);
    Serial.print("Server Message: "); Serial.println(payload);
  } else {
    Serial.print("Error sending POST: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end(); 
  Serial.println("--------------------");

  // Wait for 2 minutes (120000 ms) before next reading
  // Change to 5000 (5 seconds) temporarily if you are actively testing the alarm!
  delay(120000); 
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}