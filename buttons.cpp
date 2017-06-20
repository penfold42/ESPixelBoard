#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <RotaryEncoder.h>
#include "ESPixelStick.h"

#define ROT_MAX 25  // 19
#define ROT_SCALE 10   // 13
#define ROT_UNITY (ROT_SCALE*ROT_MAX)   // 247

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


typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;

static hsv   rgb2hsv(rgb in);
static rgb   hsv2rgb(hsv in);



void handle_buttons() {

  encoder.tick();

  if (done_setup == 0) {
    done_setup = 1;
    selected_option = 0;
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
      { 0x400000, 0x400000, 0, 0, 0x400000, 0x400000 }, // red
      { 0x004000, 0x004000, 0, 0, 0x004000, 0x004000 }, // green
      { 0x000040, 0x000040, 0, 0, 0x000040, 0x000040 }, // blue
      { 0x404000, 0x404000, 0, 0, 0x404000, 0x404000 }, // yellow
      { 0x004040, 0x004040, 0, 0, 0x004040, 0x004040 }, // cyan
      { 0x400040, 0x400040, 0, 0, 0x400040, 0x400040 }, // magenta
      { 0x400000, 0x400000, 0x004000, 0x004000, 0x000040, 0x000040 }, // rgb
      { 0x404000, 0x404000, 0x004040, 0x004040, 0x400040, 0x400040 }, // ycm
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
      }
      if (button_duration == 40) {
        handle_long_release();
      }
      if (button_counter >= 500) {
        button_counter = 500;
      }
    } else {
      button_counter--;
      if (button_pushed == 1) {
        if ((button_duration - button_counter >= 10) || (button_counter == 1)) {
          button_pushed = 0;
//          LOG_PORT.printf("button released. button_duration %d\n", button_duration);
          if (button_duration >= 40) {
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
//    LOG_PORT.printf("button mode: %d\n", button_mode);
    switch (button_mode) {
      case 0:
        config.testmode = TestMode::DISABLED;
        break;
      case 1:
        config.testmode = TestMode::STATIC;
        anim_mode = 6; anim_step = 0;   // RGBRGB
        selected_option = 0;
        encoder.setPosition(manual_rgb[selected_option]/ROT_SCALE);
        break;
      case 2:
        config.testmode = TestMode::STATIC;
        anim_mode = 7; anim_step = 0;   // HSVHSV
        selected_option = 0;
        encoder.setPosition(manual_hsv[selected_option]/ROT_SCALE);
        break;
    }
}

void handle_rotary_encoder() {
    rgb temp_rgb;
    hsv temp_hsv;
    int newPos = encoder.getPosition();
    if (newPos > ROT_MAX) {
      encoder.setPosition(ROT_MAX);
    }
    if (newPos < 0) {
      encoder.setPosition(0);
    }
    newPos = encoder.getPosition();
    if (pos != newPos) {
//      LOG_PORT.printf("newPos: %d. button_mode: %d. selected_option: %d.\n",newPos, button_mode, selected_option);
      pos = newPos;
      if (button_pushed == 0) {
        switch (button_mode) {
          case 0:
            break;
          case 1:
            manual_rgb[selected_option] = pos * ROT_SCALE;

            temp_rgb.r = (double) manual_rgb[0] / ROT_UNITY;
            temp_rgb.g = (double) manual_rgb[1] / ROT_UNITY;
            temp_rgb.b = (double) manual_rgb[2] / ROT_UNITY;

            temp_hsv = rgb2hsv(temp_rgb);

            manual_hsv[0] = temp_hsv.h*ROT_UNITY / 359;
            manual_hsv[1] = temp_hsv.s*ROT_UNITY;
            manual_hsv[2] = temp_hsv.v*ROT_UNITY;
//LOG_PORT.printf("R %d G %d B %d H %d S %d V %d\n",manual_rgb[0],manual_rgb[1],manual_rgb[2],manual_hsv[0],manual_hsv[1],manual_hsv[2]);
            break;

          case 2:
            manual_hsv[selected_option] = pos * ROT_SCALE;

            temp_hsv.h = (double) manual_hsv[0] * 359 / ROT_UNITY;
            temp_hsv.s = (double) manual_hsv[1] / ROT_UNITY;
            temp_hsv.v = (double) manual_hsv[2] / ROT_UNITY;

            temp_rgb = hsv2rgb(temp_hsv);

            manual_rgb[0] = temp_rgb.r * ROT_UNITY;
            manual_rgb[1] = temp_rgb.g * ROT_UNITY;
            manual_rgb[2] = temp_rgb.b * ROT_UNITY;
//LOG_PORT.printf("R %d G %d B %d H %d S %d V %d\n",manual_rgb[0],manual_rgb[1],manual_rgb[2],manual_hsv[0],manual_hsv[1],manual_hsv[2]);
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


hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}


rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}
