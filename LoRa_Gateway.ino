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

LoRa_GW lora_gateway;
static RadioEvents_t RadioEvents;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
AsyncMqttClient mqttClient;
const char *PubTopic  = "sensor_data/sensors";               // Topic to 


void wifiEvent(WiFiEvent_t event);

void connectToWifi();
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos);
void onMqttUnsubscribe(const uint16_t& packetId);
void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties, size_t len, size_t index, size_t total);

void onMqttPublish(const uint16_t& packetId);

void setup() {

    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);


    while (!Serial && millis() < 5000);

    delay(500);

    Serial.print("\nStarting FullyFeature_ESP32 on ");
    Serial.println(ARDUINO_BOARD);
    Serial.println(ASYNC_MQTT_ESP32_VERSION);


    WiFi.onEvent(wifiEvent);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onPublish(onMqttPublish);

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    connectToWifi();
    

    mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));

    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));

      // Set callback to a lambda function that binds the `this` pointer
    RadioEvents.RxDone = OnRxDone;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                              LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                              LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                              0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
}


void loop()
{
  
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();
    uint16_t packetIdPub2 = mqttClient.publish(PubTopic, 2, true, "test 3");
    Serial.print("Publishing at QoS 2, packetId: ");
    Serial.println(packetIdPub2);
  }

  if(lora_gateway.get_lora_idle())
  {
    lora_gateway.clear_lora_idle();
    Serial.println("into RX mode");
    Radio.Rx(0);
  }
  Radio.IrqProcess( );

}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rx_rssi, int8_t rx_snr )
{
  lora_gateway.rssi=rx_rssi;
  lora_gateway.rx_size=size;
  memcpy(lora_gateway.rxpacket, payload, lora_gateway.rx_size );
  lora_gateway.rxpacket[size]='\0';
  Radio.Sleep();
  Serial.printf("\r\nReceived packet \"%s\" with rssi %d , length %d\r\n",lora_gateway.rxpacket,lora_gateway.rssi, lora_gateway.rx_size);
  lora_gateway.set_lora_idle();
}


void wifiEvent(WiFiEvent_t event) {
  switch (event) {
#if USING_CORE_ESP32_CORE_V200_PLUS
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
#else
    case SYSTEM_EVENT_STA_GOT_IP:
#endif
      Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
      connectToMqtt();
      break;

#if USING_CORE_ESP32_CORE_V200_PLUS
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#else
    case SYSTEM_EVENT_STA_DISCONNECTED:
#endif
      Serial.println("WiFi lost connection.");
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;

    default: break;
  }
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.printf("Connected to MQTT broker at %s:%d\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("Publishing to topic: %s\n", PubTopic);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) xTimerStart(mqttReconnectTimer, 0);
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

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties, size_t len, size_t index, size_t total) {
  Serial.printf("MQTT Message | Topic: %s | QoS: %d | Retain: %d | Len: %d/%d\n", 
                topic, properties.qos, properties.retain, index, total);
}

void onMqttPublish(const uint16_t& packetId) {
  Serial.printf("Publish acknowledged. Packet ID: %d\n", packetId);
}