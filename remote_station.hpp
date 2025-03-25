#include <array>
// This is a general sensor variable.
typedef struct{
  // Reading is orignal*multipler
  int32_t reading;
  uint8_t multiplier;
  // Type maps to a type of sensor
  uint8_t type;
}sensor_t;

class LoRa_RS{
  public:
    sensor_t* get_sensor(int index){
      if ((index < 0) || (index >= number_of_sensors)) return nullptr;
      return &sensors[index];
    };

  private:
    // Each station can have 10 readings
    static constexpr int number_of_sensors = 10;
    std::array<sensor_t, number_of_sensors> sensors;
};