#include <Arduino.h>
#include <String>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

#define DRY_SOIL              1700    // based on experiments done with sensors, this is the value of dry soil returned by sensor 
#define MINIMAL_WATER_CYCLES  18      // this number comes from 240 minutes for each sampling interval, and 18*240 = 4320 minutes, or 3 days. this is the minimal watering interval for each of the plants.

#define BIG_AVOCADO_RELAY_SENSOR_PIN      1 // 2 -  it works in NC method, meaning it will be LOW when sampling.
#define LOQUAT_RELAY_SENSOR_PIN           1 // 33 - it works in NO method, meaning it will be HIGH when sampling.
#define LEMON_RELAY_SENSOR_PIN            3 // 32 - it works in NC method, meaning it will be LOW when sampling.
#define SMALL_PINAPPLE_RELAY_SENSOR_PIN   3 // 27 - it works in NO method, meaning it will be HIGH when sampling.

// the times commented out are the real ones. the time currently used is a reduced time 
// for testing purposes to not overflood the plants. this should be tested and corrected if necessary.
#define SMALL_PUMP_250ML_IN_MS  11000/*13500*/ 
#define BIG_PUMP_250ML_IN_MS    4000/*5000*/


class Plant {
  private:
    String  m_plantName;
    uint8_t m_sensorNumber;
    uint8_t m_relayNumber;
    uint8_t m_waterPortions;
    uint8_t m_lastWaterCount;
    bool    m_isSmallPump;
    uint8_t m_lastTimeWatered {0};

  public:

    Plant(String name, uint8_t sensor, uint8_t relay, uint8_t portions, uint8_t waterCount, bool pump);
    bool minimalTimePassed {false};
    bool isReadyForWater();
    void waterThePlant();
    String getPlantName() {return m_plantName;}
    String LastTimeWatered();
    
};
