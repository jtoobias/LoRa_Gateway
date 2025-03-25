#include "gateway.hpp"

LoRa_RS* LoRa_GW::get_remote_station(int index) {
  return (index >= 0 && index < this->number_of_remote_stations) ? &remote_stations[index] : nullptr;
}

void LoRa_GW::set_lora_idle() { this->lora_idle = true; }
void LoRa_GW::clear_lora_idle() { this->lora_idle = false; }
bool LoRa_GW::get_lora_idle() { return this->lora_idle; }




