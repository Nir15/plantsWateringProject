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
#define EEPROM_SIZE 4  // We only need one byte, 255 are available

enum programPositionEnum {
  Setup,
  StartOfLoop,
  WokeUp,
  TryingToConnectWifi,
  ConnectedToWifi
};

int arrayIndex = 0;
Plant* plantsArray[8];

void setup(){

  esp_reset_reason_t resetReason = esp_reset_reason();

  // Read the last position of code we reached before reset
  EEPROM.begin(EEPROM_SIZE);
  int readFlash = EEPROM.readInt(ADDRESS);

  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::Setup);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.


  plantsArray[0] = new Plant ("Big Avocado",
                              BIG_AVOCADO_SENSOR_PIN,
                              RELAY_PIN_BIG_AVOCADO,
                              4,
                              0,
                              true);
  
  plantsArray[1] = new Plant ("Small Avocado",
                              SMALL_AVOCADO_SENSOR_PIN,
                              RELAY_PIN_SMALL_AVOCADO,
                              2,
                              0,
                              true);

  plantsArray[2] = new Plant ("Pomelas",
                              POMELAS_SENSOR_PIN,
                              RELAY_PIN_POMELAS,
                              3,
                              0,
                              true);
  
  plantsArray[3] = new Plant ("Big Pinapple",
                              BIG_PINEAPPLE_SENSOR_PIN,
                              RELAY_PIN_BIG_PINEAPPLE,
                              2,
                              0,
                              true);

  plantsArray[4] = new Plant ("Small Pinapple",
                              SMALL_PINAPPLE_SENSOR_PIN,
                              RELAY_PIN_SMALL_PINAPPLE,
                              2,
                              0,
                              true);
  
  plantsArray[5] = new Plant ("Loquat",
                              LOQUAT_SENSOR_PIN,
                              RELAY_PIN_LOQUAT,
                              3,
                              0,
                              true);

  plantsArray[6] = new Plant ("Lemon",
                              LEMON_SENSOR_PIN,
                              RELAY_PIN_LEMON,
                              5,
                              0,
                              true);
  
  plantsArray[7] = new Plant ("Monsteras",
                              MONSTERAS_SENSOR_PIN,
                              RELAY_PIN_MONSTERAS,
                              3,
                              0,
                              true);

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

  // Store the current program position in the EEPROM
  EEPROM.writeInt(ADDRESS, programPositionEnum::StartOfLoop);                 
  EEPROM.commit();  // The value will not be stored if commit is not called.

  Plant* currentPlant = plantsArray[arrayIndex];

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

  arrayIndex++;
  if (arrayIndex == 8) {
    bot.sendMessage(CHAT_ID, "--- Finished a loop over all plants --- ", "");
    arrayIndex = 0; // keep index between 0 to 7
  }

}