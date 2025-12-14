
#include <WiFi.h>
#include <array>
#include "remote_station.hpp"
#include "LoRaWan_APP.h"
#include <vector>



#define RF_FREQUENCY                                868000000 // Hz

#define TX_OUTPUT_POWER                             10        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 30 // Define the payload size here

#define _ASYNC_MQTT_LOGLEVEL_               1
#define WIFI_SSID         "Ziggo1459309"
#define WIFI_PASSWORD     "dRez4aj2wcnf"
#define MQTT_HOST         IPAddress(192, 168, 178, 127) //192.168.178.181
#define MQTT_PORT         1883

#define INVALID_INT16 0xFFFF

#define MAX_NUM_OF_SENSORS 10

#define SENSOR_READING_ID_TEMPERATURE 1
#define SENSOR_READING_ID_HUMIDITY 2
#define SENSOR_READING_ID_CO2 3
#define SENSOR_READING_ID_TVOC 4
#define SENSOR_READING_ID_PRESSURE 5

class LoRa_GW{
  public:

    LoRa_GW(){
      this->rssi = 0;
      this->battery_voltage = 0;
      this->registered = false;

      this->sensor_ids.reserve(MAX_NUM_OF_SENSORS);

      // Get UUID
      this->uuid = ESP.getEfuseMac();

      // Print the value as a single decimal integer
      Serial.print("Chip ID (Decimal): ");
      Serial.println((uint64_t)chipId);
    };

    // Variables
    int16_t rssi;
    uint16_t battery_voltage;
    uint64_t uuid;
    bool registered;
    
    void add_sensor_id(int value)
    {
      if (sensor_count < MAX_NUM_OF_SENSORS){
        sensor_ids.push_back(value);
        sensor_count++;
      }
    }

    int get_sensor_id(size_t index) const
    {
      return sensor_ids[index];
    }

    size_t size_of_sensor_ids() const
    {
      return sensor_ids.size();
    }

  private:
    std::vector<int> sensor_ids;
    uint16_t sensor_count = 0;

};

struct SensorData{
  uint16_t humidity = INVALID_INT16;
  int16_t temperature = INVALID_INT16;
  uint16_t co2_ppm = INVALID_INT16;
  uint16_t tvoc = INVALID_INT16;
  uint16_t pressure = INVALID_INT16;

};



