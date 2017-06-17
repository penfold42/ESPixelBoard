#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <RotaryEncoder.h>
#include "ESPixelStick.h"

#define BUTTON_DEBOUNCE_DELAY 20
#define ROT_SCALE 8
#define ROT_MAX 256/ROT_SCALE-1

// GPIO for rotary encoder
#define BUTTON    13
#define ROTARY_A  12
#define ROTARY_B  14

RotaryEncoder encoder(ROTARY_A, ROTARY_B);

extern  config_t        config;
extern  testing_t       testing;

int done_setup = 0;

int selected_option = 0;
int manual_rgb_hsv[6];

int pos;

unsigned long last_millis;
int button_counter;
int button_pushed;

void update_led_from_pos(int selected_option, int pos);
void set_testing_led(int r, int g, int b);

void handle_buttons() {

  if (done_setup == 0) {
    done_setup = 1;
    selected_option=3;
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
        LOG_PORT.print("button pushed\n");
        if (++selected_option > 5) {
          selected_option = -1;
        }
        LOG_PORT.printf("selected_option: %d\n",selected_option);
        switch (selected_option) {
          case -1:
              config.testmode = TestMode::DISABLED;
            break;
          case 0:
              set_testing_led( 64, 0, 0);
//              testing.r = 64; testing.g =  0; testing.b =  0;
//              encoder.setPosition(manual_rgb_hsv[selected_option]/ROT_SCALE);
              config.testmode = TestMode::STATIC;
            break;
          case 1:
              set_testing_led( 0, 64, 0);
//              testing.r =  0; testing.g = 64; testing.b =  0;
//              encoder.setPosition(manual_rgb_hsv[selected_option]/ROT_SCALE);
              config.testmode = TestMode::STATIC;
            break;
          case 2:
              set_testing_led( 0, 0, 64);
//              testing.r =  0; testing.g =  0; testing.b = 64;
//              encoder.setPosition(manual_rgb_hsv[selected_option]/ROT_SCALE);
              config.testmode = TestMode::STATIC;
            break;
          case 3:
              set_testing_led( 64, 64, 0);
              config.testmode = TestMode::STATIC;
            break;
          case 4:
              set_testing_led( 0, 64, 64);
              config.testmode = TestMode::STATIC;
            break;
          case 5:
              set_testing_led( 64, 0, 64);
              config.testmode = TestMode::STATIC;
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
        LOG_PORT.print("button released\n");
        if (config.testmode == TestMode::STATIC) {
          encoder.setPosition(manual_rgb_hsv[selected_option]/ROT_SCALE);
          set_testing_led( manual_rgb_hsv[0], manual_rgb_hsv[1], manual_rgb_hsv[2]);
        }
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
    LOG_PORT.print("    ");
    LOG_PORT.print(newPos);
    LOG_PORT.println();
    pos = newPos;
    if ( (config.testmode == TestMode::STATIC) && (button_pushed == 0) ) {
      update_led_from_pos(selected_option, pos);
    }
  }
}

void update_led_from_pos(int selected_option, int pos) {
      manual_rgb_hsv[selected_option] = pos * ROT_SCALE;
      testing.r = manual_rgb_hsv[0];
      testing.g = manual_rgb_hsv[1];
      testing.b = manual_rgb_hsv[2];
}

void set_testing_led(int r, int g, int b) {
      testing.r = r;
      testing.g = g;
      testing.b = b;
}

