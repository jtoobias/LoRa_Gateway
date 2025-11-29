/* 
 * Project made by Jonathan Tobias, builr on top of the Heltect examples
 * this project also realess in GitHub:
 * https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series
 * */
 
#include "Arduino.h"
#include "gateway.hpp"
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMQTT_ESP32.h>
#include "lora_sx1262.hpp"
#include <SPI.h>
#include "Adafruit_CCS811.h"

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 30 // Define the payload size here

#define LED_PIN 35
#define CJMCU_CO2_ADDR 0x5A   //When addr line is low 0x5B when High

#define SDA_PIN 6
#define SCL_PIN 7
#define IS_MASTER 1
#define DIO1 14

Adafruit_CCS811 ccs;
// Use VSPI with custom pins (Heltec V3)
SPIClass spiLoRa(1); // 1 = HSPI, 2 = VSPI (but macro may not be defined)

JtSX1262 radio(
  8,   // NSS
  12,  // RST
  13,  // BUSY
  14,  // DIO1
  spiLoRa
);

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];


const char *PubTopic  = "sensor_data/sensors";               // Topic to publish

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void setup() {
    Serial.begin(115200);

    while (!Serial && millis() < 5000);

#if IS_MASTER
    Wire.begin(SDA_PIN, SCL_PIN);
#endif
    pinMode(LED_PIN,OUTPUT);
    digitalWrite(LED_PIN,LOW);
    delay(500);

    Serial.print("\nStarting MQTT FullyFeature_ESP32 on ");
    Serial.println(ARDUINO_BOARD);
    Serial.println(ASYNC_MQTT_ESP32_VERSION);

    // Use custom SPI pins
    spiLoRa.begin(9, 11, 10, 8); // SCK, MISO, MOSI, NSS
    radio.begin();
    delay(50);
    // Set frequency to 868 MHz
    radio.setFrequency(868000000);  // Set RF frequency to 868 MHz
    // Set modulation parameters (e.g., SF7, 125 kHz, 4/5 coding rate)
    radio.setModulationParams(7, 0x70, 0x01);  // Spreading factor 7, 125 kHz, 4/5 coding rate
    // Set TX power to 14 dBm
    radio.setTxPower(14);  // Transmission power in dBm

    // Set the mode to LoRa (standby first, then RX)
    uint8_t standbyMode = 0x00;  // Standby mode
    radio.writeCommand(0x80, &standbyMode, 1);  // Set to standby
    delay(50);  // Wait for mode change

    uint8_t rxMode = 0x03;  // LoRa RX mode
    radio.writeCommand(0x82, &rxMode, 1);  // Switch to RX mode

    // Enable interrupts for RX_DONE
    uint8_t irq_mask[] = {0x00, 0x20};  // RX_DONE interrupt enabled
    radio.writeCommand(0x11, irq_mask, 2);  // Set IRQ mask
    pinMode(DIO1, INPUT);  // DIO1 as input
    attachInterrupt(digitalPinToInterrupt(DIO1), irqHandler, FALLING);  // Trigger interrupt on falling edge

    delay(500);
    // Debug mode
    uint8_t opMode = 0;
    radio.readCommand(0x01, &opMode, 1);  // Read OpMode register
    Serial.print("Current OpMode: 0x");
    Serial.println(opMode, HEX);

#if IS_MASTER
    if (!ccs.begin()) {
        Serial.println("Failed to start CCS811 sensor! Check wiring.");
        while (1);
    }

    Serial.println("CCS811 sensor initialized.");
#endif
    // uint8_t msg[] = "Hello!";
    // radio.transmit(msg, sizeof(msg));
    // mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
    //                                   reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    // wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
    //                                   reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    // WiFi.onEvent(WiFiEvent);

    // mqttClient.onConnect(onMqttConnect);
    // mqttClient.onDisconnect(onMqttDisconnect);
    // mqttClient.onSubscribe(onMqttSubscribe);
    // mqttClient.onUnsubscribe(onMqttUnsubscribe);
    // mqttClient.onMessage(onMqttMessage);
    // mqttClient.onPublish(onMqttPublish);

    // mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    // connectToWifi();

    // Serial.print("\nStarting FullyFeature_ESP32 on ");
    // Serial.println(ARDUINO_BOARD);
    // Serial.println(ASYNC_MQTT_ESP32_VERSION);

                              
}

void loop()
{
  
  // static uint32_t count = 0;
  // count ++;
  // static unsigned long lastMsg = 0;
  // if (millis() - lastMsg > 5000) {
  //   lastMsg = millis();
  //   uint16_t packetIdPub2 = mqttClient.publish(PubTopic, 2, true, "test 3");
  //   Serial.print("Publishing at QoS 2, packetId: ");
  //   Serial.println(packetIdPub2);
  // }


#if IS_MASTER
  uint16_t co2_value = 0;
  uint16_t tvoc_value = 0;
  if (ccs.available()) {
    if (!ccs.readData()) {
        co2_value = ccs.geteCO2();
        tvoc_value = ccs.getTVOC();
    } else {
      Serial.println("Error reading CCS811 sensor data.");
    }
  }
  digitalWrite(LED_PIN,HIGH);
      // build packet
  sprintf(txpacket,"%u,%u",co2_value,tvoc_value); 
  Serial.print("Sending: ");
  Serial.println(txpacket);
  radio.transmit((uint8_t*)txpacket, strlen(txpacket));
  delay(1000); 
  digitalWrite(LED_PIN,LOW);
  delay(3000);  // Wait before next message
#else
  uint8_t buffer[255];
  uint8_t length = 0;
  delay(1000);

  uint8_t status = radio.readStatus();
  if ((status & 0x80) == 0x80) {  // Check if LoRa mode is enabled
    Serial.println("LoRa mode active.");
  } else {
      Serial.println("LoRa mode not active.");
  } 
  Serial.print("SX1262 status: 0x");
  Serial.println(status, HEX);
  if (radio.receive(buffer, &length)) {
    digitalWrite(LED_PIN,HIGH);
    Serial.print("Received: ");
    for (uint8_t i = 0; i < length; i++) {
      Serial.write(buffer[i]);
      Serial.print("Received buffer: ");
    }
    Serial.println();

    Serial.println("Write to rx Mode: ");
    // restart receive mode
    uint8_t rx_params[] = { 0x00, 0x00, 0x00 };  
    radio.writeCommand(0x82, rx_params, 3); // SetRx with no timeout
    radio.readCommand(0x82, buffer,3); // SetRx with no timeout

    for (uint8_t i = 0; i < length; i++) {
      Serial.print(buffer[i]);
      Serial.print("Received: ");
    }
    digitalWrite(LED_PIN,LOW);
  }
  Serial.println("Test");
  delay(500);
#endif
}


void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
#if USING_CORE_ESP32_CORE_V200_PLUS

    case ARDUINO_EVENT_WIFI_READY:
      Serial.println("WiFi ready");
      break;

    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("WiFi STA starting");
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi STA connected");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("WiFi lost IP");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#else

    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#endif

    default:
      break;
  }
}

void printSeparationLine()
{
  Serial.println("************************************************");
}

void onMqttConnect(bool sessionPresent)
{
  Serial.print("Connected to MQTT broker: ");
  Serial.print(MQTT_HOST);
  Serial.print(", port: ");
  Serial.println(MQTT_PORT);
  Serial.print("PubTopic: ");
  Serial.println(PubTopic);

  printSeparationLine();
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  uint16_t packetIdSub = mqttClient.subscribe(PubTopic, 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);

  mqttClient.publish(PubTopic, 0, true, "ESP32 Test");
  Serial.println("Publishing at QoS 0");

  uint16_t packetIdPub1 = mqttClient.publish(PubTopic, 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);

  uint16_t packetIdPub2 = mqttClient.publish(PubTopic, 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2);

  printSeparationLine();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  (void) reason;

  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(const uint16_t& packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total)
{
  (void) payload;

  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
}

void onMqttPublish(const uint16_t& packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void irqHandler() {
    // Handle the interrupt, check the received message or status
    Serial.println("IRQ Triggered, packet received!");
}

