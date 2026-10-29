#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "DHTesp.h"

const int dhtPin = 15;
const int ldrPin = 34;
const int fanPin = 26;
const int lampPin = 27;

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* apiKey = "--REDACTED--";

DHTesp dht;

bool fanActive = false;
bool lampActive = false;

unsigned long lastSend = 0;
const unsigned long sendInterval = 20000;

void setup() {
  Serial.begin(115200);

  pinMode(fanPin, OUTPUT);
  pinMode(lampPin, OUTPUT);

  digitalWrite(fanPin, LOW);
  digitalWrite(lampPin, LOW);

  analogReadResolution(12);
  dht.setup(dhtPin, DHTesp::DHT22);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Joining network");

  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 30) {
    delay(500);
    Serial.print(".");
    count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nNetwork connected.");
    Serial.print("Device IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNetwork connection failed.");
  }
}

void loop() {
  TempAndHumidity values = dht.getTempAndHumidity();

  if (isnan(values.temperature) || isnan(values.humidity)) {
    Serial.println("Unable to read DHT22.");
    delay(2000);
    return;
  }

  float temp = values.temperature;
  float humid = values.humidity;

  int ldrValue = analogRead(ldrPin);
  float brightness = 100.0 - (ldrValue * 100.0 / 4095.0);

  if (temp >= 30.0) {
    fanActive = true;
  } else if (temp <= 28.0) {
    fanActive = false;
  }

  if (brightness < 35.0) {
    lampActive = true;
  } else if (brightness > 45.0) {
    lampActive = false;
  }

  digitalWrite(fanPin, fanActive ? HIGH : LOW);
  digitalWrite(lampPin, lampActive ? HIGH : LOW);

  Serial.println("\nGreenhouse Status");
  Serial.printf("Temperature: %.1f C\n", temp);
  Serial.printf("Humidity: %.1f %%\n", humid);
  Serial.printf("Brightness: %.1f %%\n", brightness);
  Serial.printf("Cooling fan: %s\n", fanActive ? "Running" : "Stopped");
  Serial.printf("Grow lamp: %s\n", lampActive ? "On" : "Off");

  if (lastSend == 0 || millis() - lastSend >= sendInterval) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to network...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);

      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED &&
             millis() - start < 10000) {
        delay(500);
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();

      HTTPClient https;
      String url = "https://api.thingspeak.com/update?api_key=";
      url += apiKey;
      url += "&field1=" + String(temp, 1);
      url += "&field2=" + String(humid, 1);
      url += "&field3=" + String(brightness, 1);
      url += "&field4=" + String(fanActive ? 1 : 0);
      url += "&field5=" + String(lampActive ? 1 : 0);

      if (https.begin(client, url)) {
        int result = https.GET();
        String reply = https.getString();

        if (result == 200 && reply.toInt() > 0) {
          Serial.println("Data sent to cloud. Record: " + reply);
        } else {
          Serial.printf("Upload error: HTTP %d\n", result);
        }

        https.end();
      }
    } else {
      Serial.println("Cloud update skipped: no network.");
    }

    lastSend = millis();
  }

  delay(2000);
}