#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include "structures.h"
#include <RotaryEncoder.h>

#define BUTTON_DEBOUNCE_DELAY 20
#define ROT_SCALE 8
#define ROT_MAX 256/ROT_SCALE-1

#define BUTTON    13
#define ROTARY_A  12
#define ROTARY_B  14

RotaryEncoder encoder(ROTARY_A, ROTARY_B);

extern  config_t        config;
extern  testing_t       testing;

int done_setup = 0;

int selected_option = 0;
int manual_rgb[3];

int pos;

unsigned long last_millis;
int button_counter;
int button_pushed;

void handle_buttons() {

  if (done_setup == 0) {
    done_setup = 1;
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(ROTARY_A, INPUT_PULLUP);
    pinMode(ROTARY_B, INPUT_PULLUP);
  }

  if  ((millis() - last_millis ) >= 10 ) {
    last_millis = millis();
    if (digitalRead(BUTTON) == 0) {
      button_counter++;
      if ((button_pushed == 0) && (button_counter == 10)) {
        button_pushed = 1;
        Serial.print("button pushed\n");
        if (config.testmode == TestMode::DISABLED) {
          config.testmode = TestMode::STATIC;
          testing.step = 0;
          selected_option = 0;
        }
        else if (config.testmode == TestMode::STATIC) {
          selected_option++;
          if (selected_option == 3) {
            config.testmode = TestMode::DISABLED;
          }
        }
//        encoder.setPosition(manual_rgb[selected_option]/ROT_SCALE);
        switch (selected_option) {
          case 0:
            encoder.setPosition(testing.r/ROT_SCALE);
            break;
          case 1:
            encoder.setPosition(testing.g/ROT_SCALE);
            break;
          case 2:
            encoder.setPosition(testing.b/ROT_SCALE);
            break;
        }

      }
      if (button_counter >= 12) {
        button_counter = 11;
      }
    } else {
      button_counter--;
      if ((button_pushed == 1) && (button_counter == 1)) {
        button_pushed = 0;
        Serial.print("button released\n");
      }
      if (button_counter < 0) {
        button_counter = 0;
      }
    }
  }

  encoder.tick();
  int newPos = encoder.getPosition();
  if (newPos > ROT_MAX) {
    encoder.setPosition(ROT_MAX);
  }
  if (newPos < 0) {
    encoder.setPosition(0);
  }
  newPos = encoder.getPosition();
  if (pos != newPos) {
    Serial.print("    ");
    Serial.print(newPos);
    Serial.println();
    Serial.print ("testmode: "); Serial.println ((int)config.testmode);
    pos = newPos;
    if (config.testmode == TestMode::STATIC) {
/*
      manual_rgb[selected_option] = pos * ROT_SCALE;
      testing.r = manual_rgb[0];
      testing.g = manual_rgb[1];
      testing.b = manual_rgb[2];
*/
      switch (selected_option) {
        case 0:
          testing.r = pos * ROT_SCALE;
          break;
        case 1:
          testing.g = pos * ROT_SCALE;
          break;
        case 2:
          testing.b = pos * ROT_SCALE;
          break;
      }
    }
  }

  /*
  int pins = 0;
  pins |= digitalRead(BUTTON)<<2;
  pins |= digitalRead(ROTARY_A)<<1;
  pins |= digitalRead(ROTARY_B)<<0;
  
  Serial.print(pins, HEX);
  Serial.print(": pins\n");
  */
  
  int size = 0;
  if (size == 42) {

    /* Set the data */
    int i=0;
    for (i = 0; i < _min(size,config.channel_count); i++) {
#if defined(ESPS_MODE_PIXEL)
        pixels.setValue(i, 42);
#elif defined(ESPS_MODE_SERIAL)
        serial.setValue(i, 42);
#endif
    }
    /* fill the rest with 0s*/
    for (      ; i < config.channel_count; i++) {
#if defined(ESPS_MODE_PIXEL)
        pixels.setValue(i, 0);
#elif defined(ESPS_MODE_SERIAL)
        serial.setValue(i, 0);
#endif
    }
  }
}


