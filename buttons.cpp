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

//  0 = normal mode, 1 = RGB selector,  2 = HSV selector
int button_mode = 0;

// (0,1,2) = (r,g,b) or (h,s,v)
int selected_option = 0;

int manual_rgb[3];
int manual_hsv[3];

int pos;

unsigned long last_millis;

int button_counter;   // debounce and duration counter
int button_pushed;    // 1 = pushed, 0 = not pushed
int button_duration;  // how long was it held?

/* forward declarations */
void update_led_from_pos(int selected_option, int pos);
void set_testing_led(int r, int g, int b);
void debounce_buttons();
void handle_rotary_encoder();
void do_button_animations();
void handle_short_release();
void handle_long_release();


void handle_buttons() {

  encoder.tick();

  if (done_setup == 0) {
    done_setup = 1;
    selected_option = -1;
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(ROTARY_A, INPUT_PULLUP);
    pinMode(ROTARY_B, INPUT_PULLUP);
  }
  
  if  ((millis() - last_millis ) >= 10 ) {
    last_millis = millis();

    debounce_buttons();
    handle_rotary_encoder();
    if (button_mode >= 1) {
      set_testing_led( manual_rgb[0], manual_rgb[1], manual_rgb[2]);
      do_button_animations();
    }
  }
}

int anim_step;
int anim_mode;
int anim_r, anim_g, anim_b;

// animation sequnces, each with 6 steps
#define ANIM_STEPS 6
int anim_colours[][ANIM_STEPS] = { 
      { 0xff0000, 0, 0xff0000, 0, 0xff0000, 0 }, // red
      { 0x00ff00, 0, 0x00ff00, 0, 0x00ff00, 0 }, // green
      { 0x0000ff, 0, 0x0000ff, 0, 0x0000ff, 0 }, // blue
      { 0xffff00, 0, 0xffff00, 0, 0xffff00, 0 }, // yellow
      { 0x00ffff, 0, 0x00ffff, 0, 0x00ffff, 0 }, // cyan
      { 0xff00ff, 0, 0xff00ff, 0, 0xff00ff, 0 }, // magenta
      { 0xff0000, 0x00ff00, 0x0000ff, 0xff0000, 0x00ff00, 0x0000ff }, // rgb
      { 0xffff00, 0x00ffff, 0xff00ff, 0xffff00, 0x00ffff, 0xff00ff }, // ycm
};

void do_button_animations() {
  if (anim_step >= 0) {
    if (++anim_step >= ANIM_STEPS*10) {
      anim_step = -1;
    } else {
      int val = anim_colours[anim_mode][anim_step/10];
      set_testing_led( val>>16&0xff, val>>8&0xff, val>>0&0xff );
    }    
  }
}

void debounce_buttons() {
    
    if (digitalRead(BUTTON) == 0) {
      button_counter++;
      button_duration = button_counter;

      if ((button_pushed == 0) && (button_counter == 10)) {
        button_pushed = 1;
        LOG_PORT.print("button pushed\n");        
        LOG_PORT.printf("selected_option: %d\n",selected_option);
      }
      if (button_counter >= 500) {
        button_counter = 500;
      }
    } else {
      button_counter--;
      if (button_pushed == 1) {
        if ((button_duration - button_counter >= 10) || (button_counter == 1)) {
          button_pushed = 0;
          LOG_PORT.printf("button released. button_duration %d\n", button_duration);
          if (button_duration >= 40) {
            handle_long_release();
          } else {
            handle_short_release();
          }
        }
      }
      if (button_counter < 0) {
        button_counter = 0;
      }
    }  
}

void handle_short_release() {
  // Short press
      if (config.testmode == TestMode::STATIC) {
        if (++selected_option > 2) {
          selected_option = 0;
        }
        switch (button_mode) {
          case 0:
            break;
          case 1:
            anim_mode = selected_option; anim_step = 0;
            encoder.setPosition(manual_rgb[selected_option]/ROT_SCALE);
            break;
          case 2:
            anim_mode = selected_option + 3; anim_step = 0;
            encoder.setPosition(manual_hsv[selected_option]/ROT_SCALE);
            break;
        }
      }
}

void handle_long_release(){
// Long press
    button_mode++;
    if (button_mode >= 3) {
      button_mode = 0;
    }
    LOG_PORT.printf("button mode: %d\n", button_mode);
    switch (button_mode) {
      case 0:
        config.testmode = TestMode::DISABLED;
        break;
      case 1:
        config.testmode = TestMode::STATIC;
        anim_mode = 6; anim_step = 0;
        selected_option = 0;
        encoder.setPosition(manual_rgb[selected_option]/ROT_SCALE);
        break;
      case 2:
        config.testmode = TestMode::STATIC;
        anim_mode = 7; anim_step = 0;
        selected_option = 0;
        encoder.setPosition(manual_hsv[selected_option]/ROT_SCALE);
        break;
    }
}

void handle_rotary_encoder() {
    int newPos = encoder.getPosition();
    if (newPos > ROT_MAX) {
      encoder.setPosition(ROT_MAX);
    }
    if (newPos < 0) {
      encoder.setPosition(0);
    }
    newPos = encoder.getPosition();
    if (pos != newPos) {
      LOG_PORT.printf("    %d\n",newPos);
      LOG_PORT.printf("button_mode: %d. selected_option: %d.\n",button_mode, selected_option);
      pos = newPos;
      if (button_pushed == 0) {
        switch (button_mode) {
          case 0:
            break;
          case 1:
            manual_rgb[selected_option] = pos * ROT_SCALE;
            break;
          case 2:
            manual_hsv[selected_option] = pos * ROT_SCALE;
            break;
        }
      }
    }
}

void update_led_from_pos(int selected_option, int pos) {
      set_testing_led( manual_rgb[0], manual_rgb[1], manual_rgb[2]);
}

void set_testing_led(int r, int g, int b) {
      testing.r = r;
      testing.g = g;
      testing.b = b;
}

