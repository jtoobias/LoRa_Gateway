#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMQTT_ESP32.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include "SparkFun_SCD4x_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_SCD4x


// ==== CONFIG ====
// #define MQTT_HOST       "192.168.1.10"   // your broker IP
#define MQTT_PORT       1883
#define WIFI_SSID         "Ziggo1459309"
#define WIFI_PASSWORD     "dRez4aj2wcnf"
#define MQTT_HOST         IPAddress(192, 168, 178, 127) //192.168.178.181
// Heltec WiFi LoRa 32 V3 onboard LED pin
#define LED_PIN 35
#define UPDATE_PERIOD (1000*60*5)

const char* MQTT_TOPIC = "sensor_data/sensor_data";

// ==== GLOBALS ====
AsyncMqttClient mqttClient;
Adafruit_CCS811 ccs;
SCD4x mySensor;

uint64_t uuid;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void setup_scd40(void){

  mySensor.enableDebugging(); // Uncomment this line to get helpful debug messages on Serial

  //.begin will start periodic measurements for us (see the later examples for details on how to override this)
  if (mySensor.begin() == false)
  {
    Serial.println(F("Sensor not detected. Please check wiring. Freezing..."));
    while (1)
      ;
  }

 //We need to stop periodic measurements before we can change the sensor signal compensation settings
  if (mySensor.stopPeriodicMeasurement() == true)
  {
    Serial.println(F("Periodic measurement is disabled!"));
  }  

  //Now we can change the sensor settings.
  //There are three signal compensation commands we can use: setTemperatureOffset; setSensorAltitude; and setAmbientPressure

  Serial.print(F("Temperature offset is currently: "));
  Serial.println(mySensor.getTemperatureOffset(), 2); // Print the temperature offset with two decimal places
  mySensor.setTemperatureOffset(20); // Set the temperature offset to 5C
  Serial.print(F("Temperature offset is now: "));
  Serial.println(mySensor.getTemperatureOffset(), 2); // Print the temperature offset with two decimal places

  Serial.print(F("Sensor altitude is currently: "));
  Serial.println(mySensor.getSensorAltitude()); // Print the sensor altitude
  mySensor.setSensorAltitude(10); // Set the sensor altitude to 1000m
  Serial.print(F("Sensor altitude is now: "));
  Serial.println(mySensor.getSensorAltitude()); // Print the sensor altitude

  //There is no getAmbientPressure command
  bool success = mySensor.setAmbientPressure(98700); // Set the ambient pressure to 98700 Pascals
  if (success)
  {
    Serial.println(F("setAmbientPressure was successful"));
  }

  //The signal compensation settings are stored in RAM by default and will reset if reInit is called
  //or if the power is cycled. To store the settings in EEPROM we can call:
  mySensor.persistSettings(); // Uncomment this line to store the sensor settings in EEPROM

  //Just for giggles, while the periodic measurements are stopped, let's read the sensor serial number
  char serialNumber[13]; // The serial number is 48-bits. We need 12 bytes plus one extra to store it as ASCII Hex
  if (mySensor.getSerialNumber(serialNumber) == true)
  {
    Serial.print(F("The sensor's serial number is: 0x"));
    Serial.println(serialNumber);
  }

  //Finally, we need to restart periodic measurements
  if (mySensor.startPeriodicMeasurement() == true)
  {
    Serial.println(F("Periodic measurements restarted!"));
  }
}

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

  Wire.begin(4, 5);
  Wire.setClock(100000);

  // CCS811
  while (true) {
      if (!ccs.begin(0x5A, &Wire)) {   // <-- correct call
          Serial.println("Failed to start CCS811 sensor!");
          delay(1000);
      } else {
          break;
      }
  }
  Serial.println("CCS811 ready.");

  setup_scd40();

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

  // Get UUID
  uuid = ESP.getEfuseMac();

  // Print the value as a single decimal integer
  Serial.print("Chip ID (Decimal): ");
  Serial.println((uint64_t)chipId);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  static unsigned long lastSensorTime = 0;
  uint16_t co2_scd = 0,co2 = 0,tvoc = 0;
  uint16_t rh_scd = 0;
  int16_t temp_scd = 0;

  if ((millis() - lastSensorTime) >= UPDATE_PERIOD) {

    if (ccs.available() && !ccs.readData()) {

      digitalWrite(LED_PIN, HIGH); 
      co2  = ccs.geteCO2();
      tvoc = ccs.getTVOC();

    } else {
      Serial.println("Sensor not ready.");
    }

    if (mySensor.readMeasurement()) // readMeasurement will return true when fresh data is available
    {
      co2_scd = mySensor.getCO2();
      temp_scd = int16_t(mySensor.getTemperature()*10);
      rh_scd = uint16_t(mySensor.getHumidity()*10);
    }


      char payload[64];
      sprintf(payload, "%u,%u,%u,%d,%u", co2, tvoc,co2_scd,temp_scd,rh_scd);

      Serial.print("Publishing: ");
      Serial.println(payload);

      mqttClient.publish(MQTT_TOPIC, 1, false, payload);

    lastSensorTime = millis();
    digitalWrite(LED_PIN, LOW); 
  }
}
