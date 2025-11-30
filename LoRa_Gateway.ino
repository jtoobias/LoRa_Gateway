#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMQTT_ESP32.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"

// ==== CONFIG ====
// #define MQTT_HOST       "192.168.1.10"   // your broker IP
#define MQTT_PORT       1883
#define WIFI_SSID         "Ziggo1459309"
#define WIFI_PASSWORD     "dRez4aj2wcnf"
#define MQTT_HOST         IPAddress(192, 168, 178, 181) //192.168.178.181

const char* MQTT_TOPIC = "sensor_data/ccs811";

// ==== GLOBALS ====
AsyncMqttClient mqttClient;
Adafruit_CCS811 ccs;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// ==== WIFI + MQTT CONNECT ====
void connectToWifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection. Reconnecting...");
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;

    default:
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT!");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("debug 1");

  // Initialize CCS811
  Wire.begin(4,5);
  if (!ccs.begin()) {
    Serial.println("Failed to start CCS811 sensor!");
    while (true);
  }
  Serial.println("CCS811 ready.");

  // Create reconnect timers
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000),
                                    pdFALSE, (void*)0,
                                    (TimerCallbackFunction_t)connectToMqtt);

  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000),
                                    pdFALSE, (void*)0,
                                    (TimerCallbackFunction_t)connectToWifi);

  // Setup callbacks
  WiFi.onEvent(WiFiEvent);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Start WiFi
  connectToWifi();
}

void loop() {
  static unsigned long lastSensorTime = 0;

  if (millis() - lastSensorTime > 1000) {
    lastSensorTime = millis();
    Serial.print("test");
    if (ccs.available() && !ccs.readData()) {
      uint16_t co2  = ccs.geteCO2();
      uint16_t tvoc = ccs.getTVOC();

      char payload[64];
      sprintf(payload, "{\"co2\": %u, \"tvoc\": %u}", co2, tvoc);

      Serial.print("Publishing: ");
      Serial.println(payload);

      mqttClient.publish(MQTT_TOPIC, 1, false, payload);
    } else {
      Serial.println("Sensor not ready.");
    }
  }
}
