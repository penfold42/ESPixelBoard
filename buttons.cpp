#include "buttons.h"

#include <Arduino.h>
#include <RotaryEncoder.h>
#include <ArduinoJson.h>
#include "ESPixelStick.h"
#include "rgbhsv.h"

// GPIO for rotary encoder
#define BUTTON    13
#define ROTARY_A  12
#define ROTARY_B  14

#define ROT_MAX 40  // 20 is one rotation

RotaryEncoder encoder(ROTARY_A, ROTARY_B);

// ESPixelStick globals
extern  config_t        config;
extern  testing_t       testing;
extern  uint32_t        pwm_valid_gpio_mask;

int done_setup = 0;

//  0 = normal mode, 1 = RGB selector,  2 = HSV selector
int button_mode = 0;

// (0,1,2) = (r,g,b) or (h,s,v)
int selected_option = 0;

// button counter thresholds in 0.010 s
#define BUTTON_DEBOUNCE 10
#define BUTTON_LONG_THRESHOLD 40
#define BUTTON_MAX 50

// globals to hold current colour
rgb global_rgb;
hsv global_hsv;

unsigned long last_millis;

int rotary_pos;   // current position of the rotary encoder

int button_counter;   // debounce and duration counter
int button_pushed;    // 1 = pushed, 0 = not pushed
int button_duration;  // how long was it held?

int anim_step;  // which step of the animation to display
int anim_mode;  // which animation sequence

void setup_buttons() {
  selected_option = 0;
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(ROTARY_A, INPUT_PULLUP);
  pinMode(ROTARY_B, INPUT_PULLUP);
  pwm_valid_gpio_mask &= ~( 1<<BUTTON | 1<<ROTARY_A | 1<<ROTARY_B );
}

void handle_buttons() {

  encoder.tick();   // rotary encoder update

  if  ((millis() - last_millis ) >= 10 ) {
    last_millis = millis();

    debounce_buttons();
    handle_rotary_encoder();
    if (button_mode >= 1) {
      set_testing_led( global_rgb.r*255, global_rgb.g*255, global_rgb.b*255 );
      do_button_animations();
    }
  }
}

#define BRITE 0x40
#define BLK (0)
#define RED (BRITE<<16)
#define GRN (BRITE<<8)
#define BLU (BRITE<<0)
#define YEL (RED+GRN)
#define CYN (GRN+BLU)
#define PUR (RED+BLU)

// animation sequnces, each with 6 steps
#define ANIM_STEPS 6

int anim_colours[][ANIM_STEPS] = {
      { RED, RED, BLK, BLK, RED, RED }, // red
      { GRN, GRN, BLK, BLK, GRN, GRN }, // green
      { BLU, BLU, BLK, BLK, BLU, BLU }, // blue
      { YEL, YEL, BLK, BLK, YEL, YEL }, // yellow
      { CYN, CYN, BLK, BLK, CYN, CYN }, // cyan
      { PUR, PUR, BLK, BLK, PUR, PUR }, // magenta
      { RED, RED, GRN, GRN, BLU, BLU }, // rgb
      { YEL, YEL, CYN, CYN, PUR, PUR }, // ycm
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

      if ((button_pushed == 0) && (button_counter == BUTTON_DEBOUNCE)) {
        button_pushed = 1;
      }
      if (button_duration == BUTTON_LONG_THRESHOLD) {
        handle_long_press();
      }
      if (button_counter >= BUTTON_MAX) {
        button_counter = BUTTON_MAX;
      }
    } else {
      button_counter--;
      if (button_pushed == 1) {
        if ((button_duration - button_counter >= BUTTON_DEBOUNCE) || (button_counter == 1)) {
          button_pushed = 0;
          if (button_duration >= BUTTON_LONG_THRESHOLD) {
//            handle_long_release();
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

      if (config.testmode == TestMode::STATIC) {
        if (++selected_option > 2) {
          selected_option = 0;
        }
        anim_mode = (button_mode-1)*3 + selected_option; 
        anim_step = 0; // R, G or B
        update_pos_from_rgbhsv();
      }
}

void handle_long_press(){

    button_mode++;
    if (button_mode >= 3) {
      button_mode = 0;
    }

    switch (button_mode) {
      case 0:
        config.testmode = TestMode::DISABLED;
        break;
      case 1:
      case 2:
        config.testmode = TestMode::STATIC;
        selected_option = 0;
        anim_mode = button_mode+5; anim_step = 0;   // 6=RRGGBB 7=YYCCPP
        update_pos_from_rgbhsv();
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
    if (rotary_pos != newPos) {
//      LOG_PORT.printf("newPos: %d. button_mode: %d. selected_option: %d.\n",newPos, button_mode, selected_option);
      rotary_pos = newPos;
      if (button_pushed == 0) {
        update_rgbhsv_from_pos();
      }
    }
}

void update_rgbhsv_from_pos() {

      int combo = (button_mode-1)*3 + selected_option;
      switch (combo) {
        case 0: // Red
          global_rgb.r = (double) rotary_pos / ROT_MAX;
          global_hsv = rgb2hsv(global_rgb);
          break;
        case 1: // Green
          global_rgb.g = (double) rotary_pos / ROT_MAX;
          global_hsv = rgb2hsv(global_rgb);
          break;
        case 2: // Blue
          global_rgb.b = (double) rotary_pos / ROT_MAX;
          global_hsv = rgb2hsv(global_rgb);
          break;

        case 3: // Hue
          global_hsv.h = (double) 360 * rotary_pos / ROT_MAX;
          global_rgb = hsv2rgb(global_hsv);
          break;
        case 4: // Saturation
          global_hsv.s = (double) rotary_pos / ROT_MAX;
          global_rgb = hsv2rgb(global_hsv);
          break;
        case 5: // Value
          global_hsv.v = (double) rotary_pos / ROT_MAX;
          global_rgb = hsv2rgb(global_hsv);
          break;
      }
}

void update_pos_from_rgbhsv() {

      int combo = (button_mode-1)*3 + selected_option;

      if ((combo >= 0) && (combo <= 5)) {

        switch (combo) {
          case 0: // Red
            rotary_pos = global_rgb.r * ROT_MAX;
            break;
          case 1: // Green
            rotary_pos = global_rgb.g * ROT_MAX;
            break;
          case 2: // Blue
            rotary_pos = global_rgb.b * ROT_MAX;
            break;
          case 3: // Hue
            rotary_pos = global_hsv.h * ROT_MAX / 360;
            break;
          case 4: // Saturation
            rotary_pos = global_hsv.s * ROT_MAX;
            break;
          case 5: // Value
            rotary_pos = global_hsv.v * ROT_MAX;
            break;
        }
        encoder.setPosition(rotary_pos);
      }
}

void set_testing_led(int r, int g, int b) {
      testing.r = r;
      testing.g = g;
      testing.b = b;
}

