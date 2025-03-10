// relay use only for demonstrating the ticking that COM pin is making when moving between NC (normally closed)
// and NO (normally open). when applying voltage to IN pin, the COM pin connects to NO, otherwise it is 
// connected to NC pin.
// this way we can control the voltage being applied on specific module in order to control its activity.
// the relay is necessary whenever we want to control a module which gets its power supply from an external
// voltage source, like a battery or when plugged to the outlet. 

#include <WiFi.h>    
#include "driver/uart.h"
#include "Plant.h"
// #include <WiFiClientSecure.h>
// #include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include "EEPROM.h"
#include "esp_task_wdt.h"

// Replace with your network credentials
const char* ssid     = "********";     // change this for your own network
const char* password = "********";  // change this for your own network

// Initialize Telegram BOT
#define BOTtoken "**********************************************"  // your Bot Token (Get from Botfather)
                
// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#ifndef CHAT_ID
  #define CHAT_ID "**********"
#endif
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);



#define RELAY_PIN_BIG_AVOCADO       23  //  Define Pin 1 of the Relay
#define RELAY_PIN_SMALL_AVOCADO     22  //  Define Pin 9 of the Relay
#define RELAY_PIN_POMELAS           21  //  Define Pin 3 of the Relay
#define RELAY_PIN_BIG_PINEAPPLE     19  //  Define Pin 5 of the Relay
#define RELAY_PIN_SMALL_PINAPPLE    18  //  Define Pin 6 of the Relay
#define RELAY_PIN_LOQUAT            5   //  Define Pin 4 of the Relay
#define RELAY_PIN_LEMON             17  //  Define Pin 7 of the Relay
#define RELAY_PIN_MONSTERAS         16  //  Define Pin 8 of the Relay

#define SMALL_AVOCADO_SENSOR_PIN    36  // 5 Volts pump
#define BIG_PINEAPPLE_SENSOR_PIN    34  // 5 Volts pump
#define MONSTERAS_SENSOR_PIN        35  // 12 Volts pump (2 pumps)
#define LEMON_SENSOR_PIN            32  // 5 Volts pump // shares relay with small pinapple due to lack of pins
#define SMALL_PINAPPLE_SENSOR_PIN   32  /*27*/  // 5 Volts pump // shares relay with lemon due to lack of pins
#define BIG_AVOCADO_SENSOR_PIN      33  // 5 Volts pump // shares relay with loquat due to lack of pins
#define LOQUAT_SENSOR_PIN           33  /*26*/  // 12 Volts pump // shares relay with big avocado due to lack of pins
#define POMELAS_SENSOR_PIN          39  // 12 Volts pump (2 pumps)

#define DELAY_IN_MICROS             60*30*1000000 // 1000000-us * 60 * 30 = 30 minutes. this means every plant is being sampled in 30*8 = 240 minutes intervals.

//Setup the EEPROM
#define ADDRESS 0          // We'll store the last position of code we reached before reset, in the first EEPROM byte
#define EEPROM_SIZE 4*17    // the flash will be written to whenever the plant is watered, so in the next reset the value of last water time will be kept
                           // one extra use for the flash is to monitor any resets caused by WTD timoeout. 17 is for 2 arrays of 8integers, and one integer of reset reason 
int g_addressesCyclesArray[8] = {4,8,12,16,20,24,28,32}; // addresses to keep the cycles left to wait for each plant before next possible watering
int g_addressesLastTimeWateredArray[8] = {36,40,44,48,52,56,60,64}; // addresses to keep the last time watered for each plant
int g_arrayIndex = 0;
Plant* g_plantsArray[8];
int g_resetCounter = 192; // 192 stands for 192 plants samples. why? because samples take place in 30 minutes intervals, meaning 30*192 = 4 days.
                        // the system will reset itself once in 4 days, and will keep track of last time each plant was watered. this way the timers
                        // will not overflow, and the system could run without disruptions.


// when waking up from reset, update every plant object with its remaining cycles (4 hours per cycles) to wait
// until next watering could be available. minimum time should be 3 days between one watering to the next 
void updateInitialCycles(int& cycles, int index){
  cycles = EEPROM.readInt(g_addressesCyclesArray[index]);
  if ((cycles < 0) || cycles > MINIMAL_WATER_CYCLES){
    cycles = 0;
  }
}

// read from flash the value of the last time plant was watered, update it in local variable for keeping track of time in 
// new round of system after forcing reset
void updateLastTimeWatered(int& lastTime, int index){
  lastTime = EEPROM.readInt(g_addressesLastTimeWateredArray[index]);
  if ((lastTime < 0) || lastTime > 240) { // 240 represents 30 days since last time waterd (240 minutes per cycle * 240 = 40 days)
    lastTime = 0;
  }
}


// write to flash the remaining cycles to wait for each of the plants. Those values will then be read 
// in the setup function after reset takes place 
void updateCyclesBeforeReset(){
    for (int i=0; i < 8; i++){
      EEPROM.writeInt(g_addressesCyclesArray[i], g_plantsArray[i]->getLastWaterCount());               
      EEPROM.commit();  // The value will not be stored if commit is not called.
    }
}


// write to flash the last time each of the plants was watered. Those values will then be read 
// with updateLastTimeWatered after reset takes place 
void updateLastTimeWateredBeforeReset(){
    for (int i=0; i < 8; i++){
      EEPROM.writeInt(g_addressesLastTimeWateredArray[i], g_plantsArray[i]->getLastTimeWatered());               
      EEPROM.commit();  // The value will not be stored if commit is not called.
    }
}


// update user for each of the plant's remaining cycles to wait before next watering is available.
// this should be in the range of 0 to MINIMAL_WATER_CYCLES
void botUpdateCyclesAfterReset(){
    for (int i=0; i < 8; i++){
      String botMessage = "Plant: " + g_plantsArray[i]->getPlantName() + " has " + g_plantsArray[i]->getLastWaterCount() + " cycles left.";
      bot.sendMessage(CHAT_ID, botMessage, "");
      delay(5000);
    }
}

enum programPositionEnum {
  Setup,
  StartOfLoop,
  WokeUp,
  TryingToConnectWifi,
  ConnectedToWifi
};


void setup(){

  esp_reset_reason_t resetReason = esp_reset_reason();

  // Read the last position of code we reached before reset
  EEPROM.begin(EEPROM_SIZE);
  int readFlash = EEPROM.readInt(ADDRESS);

  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::Setup);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.

  int initialWaterCycles;
  int lastTimeWateredUpdate;

  updateInitialCycles(initialWaterCycles, 0);
  updateLastTimeWatered(lastTimeWateredUpdate,0);
  g_plantsArray[0] = new Plant ("Big Avocado",
                              BIG_AVOCADO_SENSOR_PIN,
                              RELAY_PIN_BIG_AVOCADO,
                              4,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);
  
  updateInitialCycles(initialWaterCycles, 1);
  updateLastTimeWatered(lastTimeWateredUpdate,1);
  g_plantsArray[1] = new Plant ("Small Avocado",
                              SMALL_AVOCADO_SENSOR_PIN,
                              RELAY_PIN_SMALL_AVOCADO,
                              2,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);

  updateInitialCycles(initialWaterCycles, 2);
  updateLastTimeWatered(lastTimeWateredUpdate,2);
  g_plantsArray[2] = new Plant ("Pomelas",
                              POMELAS_SENSOR_PIN,
                              RELAY_PIN_POMELAS,
                              3,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);
  
  updateInitialCycles(initialWaterCycles, 3);
  updateLastTimeWatered(lastTimeWateredUpdate,3);
  g_plantsArray[3] = new Plant ("Big Pinapple",
                              BIG_PINEAPPLE_SENSOR_PIN,
                              RELAY_PIN_BIG_PINEAPPLE,
                              2,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);

  updateInitialCycles(initialWaterCycles, 4);
  updateLastTimeWatered(lastTimeWateredUpdate,4);
  g_plantsArray[4] = new Plant ("Small Pinapple",
                              SMALL_PINAPPLE_SENSOR_PIN,
                              RELAY_PIN_SMALL_PINAPPLE,
                              2,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);
  
  updateInitialCycles(initialWaterCycles, 5);
  updateLastTimeWatered(lastTimeWateredUpdate,5);
  g_plantsArray[5] = new Plant ("Loquat",
                              LOQUAT_SENSOR_PIN,
                              RELAY_PIN_LOQUAT,
                              3,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);

  updateInitialCycles(initialWaterCycles, 6);
  updateLastTimeWatered(lastTimeWateredUpdate,6);
  g_plantsArray[6] = new Plant ("Lemon",
                              LEMON_SENSOR_PIN,
                              RELAY_PIN_LEMON,
                              5,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);

  updateInitialCycles(initialWaterCycles, 7);
  updateLastTimeWatered(lastTimeWateredUpdate,7);
  g_plantsArray[7] = new Plant ("Monsteras",
                              MONSTERAS_SENSOR_PIN,
                              RELAY_PIN_MONSTERAS,
                              3,
                              initialWaterCycles,
                              false,
                              lastTimeWateredUpdate);

  delay(5000);

  pinMode(RELAY_PIN_BIG_AVOCADO,    OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_SMALL_AVOCADO,  OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_POMELAS,        OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_BIG_PINEAPPLE,  OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_SMALL_PINAPPLE, OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_LOQUAT,         OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_LEMON,          OUTPUT);         // Setting the Pin to output signal
  pinMode(RELAY_PIN_MONSTERAS,      OUTPUT);         // Setting the Pin to output signal

  pinMode(BIG_AVOCADO_RELAY_SENSOR_PIN,       OUTPUT);
  digitalWrite(BIG_AVOCADO_RELAY_SENSOR_PIN,  LOW);
  pinMode(LOQUAT_RELAY_SENSOR_PIN,            OUTPUT);
  digitalWrite(LOQUAT_RELAY_SENSOR_PIN,       LOW);
  pinMode(SMALL_PINAPPLE_RELAY_SENSOR_PIN,    OUTPUT);
  digitalWrite(LOQUAT_RELAY_SENSOR_PIN,       LOW);

  pinMode(SMALL_AVOCADO_SENSOR_PIN, INPUT);
  pinMode(SMALL_PINAPPLE_SENSOR_PIN, INPUT);
  pinMode(BIG_AVOCADO_SENSOR_PIN, INPUT);
  pinMode(BIG_PINEAPPLE_SENSOR_PIN, INPUT);
  pinMode(LEMON_SENSOR_PIN, INPUT);
  pinMode(LOQUAT_SENSOR_PIN, INPUT);
  pinMode(POMELAS_SENSOR_PIN, INPUT);
  pinMode(MONSTERAS_SENSOR_PIN, INPUT);

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  // connect to WiFi
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  botUpdateCyclesAfterReset(); // update user with the remaining time to wait (cycles) for each plant

  delay(2000);
  // when reseting the esp32 because of watchdog timeout, inform user where was the code last time before reseting
  if (resetReason == esp_reset_reason_t::ESP_RST_TASK_WDT){
    switch (readFlash){
      case programPositionEnum::Setup:
        bot.sendMessage(CHAT_ID, "Reset executed from setup", "");
        break;
      case programPositionEnum::StartOfLoop:
        bot.sendMessage(CHAT_ID, "Reset executed from start of loop", "");
        break;
      case programPositionEnum::WokeUp:
        bot.sendMessage(CHAT_ID, "Reset executed after waking up from light sleep", "");
        break;
      case programPositionEnum::TryingToConnectWifi:
        bot.sendMessage(CHAT_ID, "Reset executed while trying to connect WiFi", "");
        break;
      case programPositionEnum::ConnectedToWifi:
        bot.sendMessage(CHAT_ID, "Reset executed after connecting to WiFi", "");
        break;
      default:
        bot.sendMessage(CHAT_ID, "Reset reason unknown", "");
        break;
    }
  }
  delay(2000);
  bot.sendMessage(CHAT_ID, "Plants water system is now active", "");
}


esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,  // Timeout in milliseconds (e.g., 10 seconds)
    .idle_core_mask = 0,  // No idle core tasks are subscribed by default
    .trigger_panic = true // Trigger panic when WDT timeout occurs
};

void loop() {

  if (g_resetCounter == 0){
    // Store the current cycles count of plant before executing restart to the system
    updateCyclesBeforeReset();
    updateLastTimeWateredBeforeReset();
    esp_restart(); // restart the esp32 every 4 days. 
  }
  g_resetCounter--;

  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::StartOfLoop);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.

  Plant* currentPlant = g_plantsArray[g_arrayIndex % 8];

  // enter this section only when the plant's soil is dry enough to water it again
  if (currentPlant->isReadyForWater()) {
    currentPlant->waterThePlant();

    String plantInfo = "Plant: " + currentPlant->getPlantName() + " was watered after " + currentPlant->LastTimeWatered();

    bot.sendMessage(CHAT_ID, plantInfo, "");
  }
  esp_sleep_enable_timer_wakeup(DELAY_IN_MICROS); // Configure wake up timer for 30 minutes
  esp_light_sleep_start(); // Enter light sleep mode


  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::WokeUp);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.

  // Disable the Task Watchdog Timer for the entire program
  esp_task_wdt_deinit();

  // connect to WiFi after waking up
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    // Store the current program position in the EEPROM
    EEPROM.writeInt(ADDRESS, programPositionEnum::TryingToConnectWifi);                 
    EEPROM.commit();  // The value will not be stored if commit is not called.
    delay(500);
  }

  esp_task_wdt_init(&wdt_config);  // Re-enable WDT with a 10-second timeout

  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::ConnectedToWifi);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.

  delay(1000); // give time for system to stable after waking up

  g_arrayIndex++;
  if (g_arrayIndex == 24) { // log message once in 24 * 30 = 720 minutes = 12 hours
    bot.sendMessage(CHAT_ID, "--- System is working properly --- ", "");
    g_arrayIndex = 0; // keep index between 0 to 7
  }

}