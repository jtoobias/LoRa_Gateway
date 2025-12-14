#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMQTT_ESP32.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include "SparkFun_SCD4x_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_SCD4x
#include "gateway.hpp"

// ==== CONFIG ====
// Heltec WiFi LoRa 32 V3 onboard LED pin
#define LED_PIN 35
#define UPDATE_PERIOD (1000*60*5)
#define FW_MAJOR  0
#define FW_MINOR  1
#define FW_PATCH  0
#define DEVICE_TYPE 0

#define MAX_BUFFER_SIZE 100

String MQTT_DATA_TOPIC = "sensor_device/data";
String MQTT_REGISTRATION_TOPIC = "sensor_device/registration";
String MQTT_REGISTRATION_RESPONSE_TOPIC = "sensor_device/registration_response";

// ==== GLOBALS ====
char global_array[MAX_BUFFER_SIZE];

AsyncMqttClient mqttClient;
Adafruit_CCS811 ccs;
SCD4x mySensor;

LoRa_GW sensor_gateway;

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
  sensor_gateway.add_sensor_id(SENSOR_READING_ID_CO2)
  sensor_gateway.add_sensor_id(SENSOR_READING_ID_TVOC)
  sensor_gateway.add_sensor_id(SENSOR_READING_ID_TEMPERATURE)
  sensor_gateway.add_sensor_id(SENSOR_READING_ID_HUMIDITY)

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
  mqttClient.subscribe

  // Start WiFi
  connectToWifi();

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  static unsigned long lastSensorTime = 0;
  SensorData sensor_data;

  if ((millis() - lastSensorTime) >= UPDATE_PERIOD) {

    if (ccs.available() && !ccs.readData()) {

      digitalWrite(LED_PIN, HIGH); 
      sensor_data.co2_ppm  = ccs.geteCO2();
      sensor_data.tvoc = ccs.getTVOC();

    } else {
      Serial.println("Sensor not ready.");
    }

    if (mySensor.readMeasurement()) // readMeasurement will return true when fresh data is available
    {
      co2_scd = mySensor.getCO2();
      sensor_data.temperature = int16_t(mySensor.getTemperature()*10);
      sensor_data.humidity = uint16_t(mySensor.getHumidity()*10);
    }


      char payload[64];
      sprintf(payload, "%u,%u,%u,%d,%u", co2, tvoc,co2_scd,temp_scd,rh_scd);

      Serial.print("Publishing: ");
      Serial.println(payload);

      mqttClient.publish(MQTT_DATA_TOPIC, 1, false, payload);

    lastSensorTime = millis();
    digitalWrite(LED_PIN, LOW); 
  }
}

void send_mqtt_payload(char &payload,String topic){

  mqttClient.publish(topic, 1, false, &global_array);
}

void sensor_device_data(SensorData &sensor_data){

  // Format all struct elements directly into the string buffer
  snprintf(global_array, MAX_BUFFER_SIZE, "%llu,%u,%u,%d,%u",
           sensor_gateway.uuid, 
           sensor_data.co2, 
           sensor_data.tvoc,
           sensor_data.temperature,
           sensor_data.humidity);
  send_mqtt_payload(global_array,MQTT_DATA_TOPIC);
}

void register_device(void){
  // Use 'idx' to track the number of characters written so far.
  int idx = snprintf(global_array, MAX_BUFFER_SIZE, "%llu,%u,%u,%u,%u", 
                     sensor_gateway.uuid, FW_MAJOR, FW_MINOR, FW_PATCH, DEVICE_TYPE);
  
  // Check for snprintf errors (negative return value or index exceeding buffer size)
  if (idx < 0 || idx >= MAX_BUFFER_SIZE) {
    Serial.println("Error or buffer overflow during initial snprintf!");
    return; 
  }

  // --- Step 2: Loop through the vector and append each element ---
  for (size_t i = 0; i < sensor_gateway.size_of_sensor_ids(); ++i) {
    // Append a comma, then the number. 
    // We pass (global_array + idx) as the destination, and update idx with the return value.
    int written = snprintf(global_array + idx, MAX_BUFFER_SIZE - idx, ",%u", sensor_readings);

    // Check again for potential errors or buffer overflow during append
    if (written < 0 || idx + written >= MAX_BUFFER_SIZE) {
      Serial.println("Error or buffer overflow during vector append!");
      // Optionally, break the loop if you can't fit more data
      break; 
    }
    idx += written; // Update index for the next append operation
  }

  // The string is automatically null-terminated by the last snprintf call
  Serial.print("Final Formatted String: ");
  Serial.println(global_array);

  send_mqtt_payload(global_array,MQTT_REGISTRATION_TOPIC);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    
    String incomingTopic = String(topic);
    // Extract the payload as a string for easier comparison
    String incomingPayload = String(payload).substring(0, len); 

    Serial.println("Message received on topic: " + incomingTopic);
    Serial.println("Payload: " + incomingPayload);

    // Filter 1: Check if it's the registration topic we care about
    if (incomingTopic == MQTT_REGISTRATION_RESPONSE_TOPIC) {
        
        // Filter 2: Check if the payload is *specifically* acknowledging our ID
        // The backend should reply with just the decimal uint64_t ID you sent
        String expectedAck = String(sensor_gateway.uuid); 

        if (incomingPayload == "ACK_" + expectedAck) {
            sensor_gateway.registration = true;
            Serial.println("REGISTRATION ACKNOWLEDGED for this device!");
            // We stay subscribed to the topic in this scenario
        } else if (incomingPayload.startsWith("NACK")) {
             Serial.println("Registration NACK received. Check backend logic.");
        }
    }
}