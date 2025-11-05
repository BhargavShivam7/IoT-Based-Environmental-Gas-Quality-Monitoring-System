/*
 * IoT Environmental Monitor Firmware
 * * This code reads data from a DHT11 and an MQ-135 sensor,
 * connects to Wi-Fi, and sends the data as a JSON object
 * to a custom web server via an HTTP POST request.
 * * Hardware:
 * - NodeMCU (ESP8266)
 * - DHT11 Sensor -> Pin D4
 * - MQ-135 Sensor -> Pin A0
 * * Libraries to install via Library Manager:
 * 1. "DHT sensor library" by Adafruit
 * 2. "ArduinoJson" by Benoit Blanchon
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// =================================================================
// --- CONFIGURATION - YOU MUST EDIT THESE 3 VALUES ---
// =================================================================
const char* ssid = "YOUR_WIFI_SSID";         // Your Wi-Fi name
const char* password = "YOUR_WIFI_PASSWORD";  // Your Wi-Fi password
const char* serverUrl = "http://your-app-name.onrender.com/data"; // Your Render app URL
// =================================================================

// --- SENSOR PINS ---
#define DHTPIN D4     // Digital pin D4
#define MQ_PIN A0     // Analog pin A0
#define DHTTYPE DHT11 // We are using the DHT11 sensor

// --- GLOBAL OBJECTS ---
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
HTTPClient http;

// --- ONE-TIME SETUP ---
void setup() {
  Serial.begin(115200); // Start serial for debugging
  Serial.println("\n[IoT Monitor Starting Up]");
  
  dht.begin();          // Initialize the DHT sensor
  
  setup_wifi();         // Connect to Wi-Fi
}

// --- MAIN LOOP ---
void loop() {
  // Read sensor data
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Read as Celsius
  int g = analogRead(MQ_PIN);

  // Check if sensor reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    // Wait 5 seconds and try again
    delay(5000);
    return;
  }

  // Print values to the Serial Monitor (for testing)
  Serial.println("--------------------");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println(" %");
  Serial.print("Gas Level: ");
  Serial.println(g);

  // Create JSON document
  StaticJsonDocument<200> doc;
  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["gas_level"] = g;

  // Serialize JSON to a string
  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  // Send the data
  Serial.println("Sending data to server...");
  http.begin(client, serverUrl); // Specify URL
  http.addHeader("Content-Type", "application/json"); // Set content type
  
  int httpCode = http.POST(jsonBuffer); // Send the POST request
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    Serial.print("Response payload: ");
    Serial.println(payload);
  } else {
    Serial.print("Error sending POST: ");
    Serial.println(httpCode);
  }
  
  http.end(); // Free resources
  Serial.println("--------------------");

  // Wait for 2 minutes before sending next reading
  // (ThingSpeak free tier limit is 15s, but 2 mins is good for a dashboard)
  delay(120000); 
}

// --- CONNECT TO WIFI ---
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
