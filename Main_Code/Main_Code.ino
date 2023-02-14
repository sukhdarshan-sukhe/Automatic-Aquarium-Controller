#include <Arduino.h>
#include <EEPROM.h>
#include <NoDelay.h>
#include <Wire.h>
#include <menu.h> 
#include <menuIO/liquidCrystalOutI2C.h>
#include <menuIO/encoderIn.h>
#include <menuIO/keyIn.h>
#include <menuIO/chainStream.h>

using namespace Menu;

#define MAX_DEPTH 3

LiquidCrystal_I2C lcd(0x3F, 16, 2);

// Encoder /////////////////////////////////////
#define encA 12
#define encB 4
// this encoder has a button here
#define encBtn 2


encoderIn<encA, encB> encoder; // simple quad encoder driver
#define ENC_SENSIVITY 4
encoderInStream<encA, encB> encStream(encoder, ENC_SENSIVITY); // simple quad encoder fake Stream

// a keyboard with only one key as the encoder button
keyMap encBtn_map[] = {{-encBtn, defaultNavCodes[enterCmd].ch}}; // negative pin numbers use internal pull-up, this is on when low
keyIn<1> encButton(encBtn_map);                                  // 1 is the number of keys

// serialIn serial(Serial);

// input from the encoder + encoder button
menuIn *inputsList[] = {&encStream, &encButton};
chainStream<2> in(inputsList); // 3 is the number of inputs

#define LEDPIN 7

int eeAddress = 100;


struct AquaConfig
{
  bool led;
  bool filter;
  bool feeder;
  bool wavemaker;

  uint16_t ledOnTime[2];
  uint16_t ledOffTime[2];

  uint16_t feederOnTime[2];
  uint16_t feederDuration;
};

AquaConfig aqua_config;

result showEvent(eventMask e, navNode &nav, prompt &item)
{
  // Serial.print("show event: ");
  // Serial.println(e);
  return proceed;
}

result saveConfig(eventMask e, navNode &nav, prompt &item)
{
  if(e == 4) {
    // save config to eeprom
    EEPROM.put(eeAddress, aqua_config);
  }
  // Serial.flush();
  return proceed;
}

TOGGLE(aqua_config.led, setLed, "Led: ", saveConfig, anyEvent, noStyle //,doExit,enterEvent,noStyle
       ,
       VALUE("On", HIGH, doNothing, noEvent),
       VALUE("Off", LOW, doNothing, noEvent));

PADMENU(ledOnTimeField, "On ", saveConfig, anyEvent, noStyle,
        FIELD(aqua_config.ledOnTime[0], "", "h", 0, 23, 1, 0, doNothing, anyEvent, noStyle),
        FIELD(aqua_config.ledOnTime[1], "", "m", 0, 59, 5, 0, doNothing, anyEvent, noStyle));

PADMENU(ledOffTimeField, "Off", saveConfig, anyEvent, noStyle,
        FIELD(aqua_config.ledOffTime[0], "", "h", 0, 23, 1, 0, doNothing, anyEvent, noStyle),
        FIELD(aqua_config.ledOffTime[1], "", "m", 0, 59, 5, 0, doNothing, anyEvent, noStyle));

PADMENU(feederOnField, "On ", saveConfig, anyEvent, noStyle,
        FIELD(aqua_config.feederOnTime[0], "", "h", 0, 23, 1, 0, doNothing, anyEvent, noStyle),
        FIELD(aqua_config.feederOnTime[1], "", "m", 0, 59, 5, 0, doNothing, anyEvent, noStyle));

MENU(ledMenu, "LED", showEvent, anyEvent, noStyle,
     SUBMENU(ledOnTimeField),
     SUBMENU(ledOffTimeField),
     EXIT("<Back"));

MENU(feederMenu, "Feeder", showEvent, anyEvent, noStyle,
     SUBMENU(feederOnField),
     FIELD(aqua_config.feederDuration, "Duration", "sec", 1, 60, 1, 0, saveConfig, anyEvent, wrapStyle),
     EXIT("<Back"));


MENU(mainMenu, "Main menu", doNothing, noEvent, wrapStyle,
     SUBMENU(setLed),
     SUBMENU(ledMenu),
     SUBMENU(feederMenu),
     EXIT("<Back"));


MENU_OUTPUTS(
    out,
    MAX_DEPTH,
    LIQUIDCRYSTAL_OUT(lcd, {0, 0, 16, 2}),
    NONE);
NAVROOT(nav, mainMenu, MAX_DEPTH, in, out); // the navigation root object

result alert(menuOut &o, idleEvent e)
{
  if (e == idling)
  {
    o.setCursor(0, 0);
    o.print("alert test");
    o.setCursor(0, 1);
    o.print("[select] to continue...");
  }
  return proceed;
}

result idle(menuOut &o, idleEvent e)
{
  switch (e)
  {
  case idleStart:
    o.print("suspending menu!");
    nav.idleOn();
    break;
  case idling:
    o.print("suspended...");
    o.clear();
    break;
  case idleEnd:
    o.print("resuming menu.");
    break;
  }
  return proceed;
}

void setup()
{
  pinMode(encBtn, INPUT_PULLUP);
  pinMode(LEDPIN, OUTPUT);

  // = {LOW, LOW, LOW, LOW, {8, 0}, {20, 0}, {9,0}, 3}
  EEPROM.get(eeAddress, aqua_config);

  // It this is first time run then put default values
  if (aqua_config.feederDuration == 0)
  {
    aqua_config = {LOW, LOW, LOW, LOW, {8, 0}, {20, 0}, {9, 0}, 3};
    EEPROM.put(eeAddress, aqua_config);
  }



  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("Aquarium Controller"));
  Serial.flush();
  encoder.begin();
  lcd.init();
  lcd.backlight();
  nav.idleTask = idle; // point a function to be used when menu is suspended
  // mainMenu[1].enabled = disabledStatus;
  nav.showTitle = false;
  nav.timeOut = 30;
  lcd.setCursor(0, 0);
  lcd.print("Aquarium Ctrl.");
  lcd.setCursor(0, 1);
  lcd.print("V1.0.0");

  Serial.println(aqua_config.feederDuration);

  // delay(1000);
  nav.idleOn();
}

void LEDHandler() {
  digitalWrite(LEDPIN, aqua_config.led);
}

void homeScreenHandler() {
  lcd.setCursor(0, 0);
  lcd.print("LED");
  lcd.setCursor(0, 1);
  lcd.print(aqua_config.led);
}

noDelay homeScreenTask(2000, homeScreenHandler);
noDelay LEDTask(1000, LEDHandler);

void loop()
{
  nav.poll();
  LEDTask.update();
  
  if (nav.sleepTask)
  {
    homeScreenTask.update();
  }
}