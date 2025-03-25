
#include <WiFi.h>
#include <array>
#include "remote_station.hpp"
#include "LoRaWan_APP.h"



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
#define WIFI_SSID         "Boomklever Boven"
#define WIFI_PASSWORD     "ewbkshxd"
#define MQTT_HOST         IPAddress(192, 168, 2, 235)
#define MQTT_PORT         1883
#define WIFI_SSID         "Boomklever Boven"
#define WIFI_PASSWORD     "ewbkshxd"

class LoRa_GW{
  public:

    LoRa_GW(){
      lora_idle = false;
      rssi = 0;
      rx_size = 0;
      tx_number = 0;
    };


    // LORA
    LoRa_RS* get_remote_station(int index);
    static void OnRxDone( uint8_t *payload, uint16_t size, int16_t rx_rssi, int8_t rx_snr); // Declare the function here

    void set_lora_idle(void);
    void clear_lora_idle(void);
    bool get_lora_idle(void);

    // MQTT Func



    // Variables
    int16_t tx_number;
    int16_t rssi,rx_size;
    static RadioEvents_t RadioEvents;
    char txpacket[BUFFER_SIZE];
    char rxpacket[BUFFER_SIZE];

  private:
    static constexpr int number_of_remote_stations = 10;
    std::array<LoRa_RS, number_of_remote_stations> remote_stations;
    
    bool lora_idle;

};



