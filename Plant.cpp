#include "HardwareSerial.h"
#include "Plant.h"

#ifndef CHAT_ID
  #define CHAT_ID "********"
#endif

extern UniversalTelegramBot bot;

Plant::Plant(String name, uint8_t sensor, uint8_t relay, uint8_t portions, uint8_t waterCount, bool pump) :
              m_plantName(name), m_sensorNumber(sensor), m_relayNumber(relay), m_waterPortions(portions),
              m_lastWaterCount(waterCount), m_isSmallPump(pump) {}


// this function indicates if the plant is ready for the next watering round, or not.
// every plant needs at least 4 days of interval between every water round, and after that the sensor
// will check the moisture in the ground and according to that the plant will be watered or not.
bool Plant::isReadyForWater() 
{
  m_lastTimeWatered++;

  if (m_lastWaterCount > 0) {
    m_lastWaterCount--;
    return false;
  }

  else {
    // Big Avocado and Loquat share same pin, since there are no other pins left 
    // for sampling the plants. It is made by relay which connects both of them to pin
    // LOQUAT_SENSOR_PIN/BIG_AVOCADO_SENSOR_PIN (same one) and is controlled by pin number 1.
      if (m_plantName == "Big Avocado"){
        delay(1000); // the relay is mechanic switch, need to give it time to stabilize
        digitalWrite(BIG_AVOCADO_RELAY_SENSOR_PIN, HIGH); // Big avocado is connected to normally open (NO), so active in HIGH
        delay(500);
      }
      else if (m_plantName == "Loquat") {
        delay(1000); // the relay is mechanic switch, need to give it time to stabilize
        digitalWrite(LOQUAT_RELAY_SENSOR_PIN, LOW); // Loquat is connected to normally closed (NC), so active in LOW
        delay(500);
      }
      else if (m_plantName == "Small Pinapple") {
        delay(1000); // the relay is mechanic switch, need to give it time to stabilize
        digitalWrite(SMALL_PINAPPLE_RELAY_SENSOR_PIN, HIGH); // Small pinapple is connected to normally open (NO), so active in HIGH
        delay(500);
      }
      else if (m_plantName == "Lemon") {
        delay(1000); // the relay is mechanic switch, need to give it time to stabilize
        digitalWrite(LEMON_RELAY_SENSOR_PIN, LOW); // Lemon is connected to normally closed (NC), so active in LOW
        delay(500);
      }
    
    int moistureSample = analogRead(m_sensorNumber);

    if (moistureSample > DRY_SOIL) {
      String plantName = "Current Plant being sampled: " + m_plantName + " Moisture level " + String(moistureSample);
      bot.sendMessage(CHAT_ID, plantName, "");
    }

    if ((moistureSample > DRY_SOIL) && (moistureSample < 2700)) { // check that sensor measures valid result and does not go crazy
      // 1. check for reliable reading of the sensor (i.e at least 10 reads in the same range)
      uint16_t totalSamples = 0;
      for (int i = 0; i < 10; i++){
        delay(1000);
        totalSamples += analogRead(m_sensorNumber);
      }

      // the tolerance I decided is 100 gap between first sample and other 10 average samples
      if (((totalSamples / 10) > moistureSample + 100) || ((totalSamples / 10) < moistureSample - 100)){
        return false;
      }
      m_lastWaterCount = MINIMAL_WATER_CYCLES; // update the cycles back to minimal number, so at least 3 days of no water after that
      return true;
    }
    return false; // soil is not dry yet, or the sensor went crazy with invalid values.
  }
}



// water the plant according to its specific parameters
void Plant::waterThePlant() 
{
  // 3. if so - water the plant according to its own demands in the chart
    int delayTime = m_isSmallPump ? SMALL_PUMP_250ML_IN_MS : BIG_PUMP_250ML_IN_MS; // delayTime ms, representing 250ml of water for small or big pumps (experiment result)
    for (int j = 0; j < m_waterPortions ; j++){
      digitalWrite(m_relayNumber, HIGH); // water the plant
      delay(delayTime);                  // for delayTime ms, representing 250ml of water (experiment result)
      digitalWrite(m_relayNumber, LOW);  // stop watering the plant
      delay(60*1000);                    // wait for 1 minute before watering again to prevent flood
    }
}


String Plant::LastTimeWatered(){
  int hoursSinceLastWater = (m_lastTimeWatered*240); // each time a plant being sampled is once in 240 minutes
  String returnString = String(hoursSinceLastWater / 1440) + " Days."; // 1440 minutes is 24 hours, meaning one day
  m_lastTimeWatered = 0;
  return returnString;
}