#include <Arduino.h>
#include "ESPixelStick.h"
#include "pwm.h"

extern  config_t    config;         // Current configuration

// PWM globals

uint16_t last_pwm[NUM_GPIO];   // 0-1023, 0=dark

// GPIO 6-11 are for flash chip
#if defined (ESPS_MODE_PIXEL) || ( defined(ESPS_MODE_SERIAL) && (SEROUT_UART == 1))
// { 0,1,  3,4,5,12,13,14,15,16 };  // 2 is WS2811 led data
uint32_t pwm_valid_gpio_mask = 0b11111000000111011;

#elif defined(ESPS_MODE_SERIAL) && (SEROUT_UART == 0)
// { 0,  2,3,4,5,12,13,14,15,16 };  // 1 is serial TX for DMX data
uint32_t pwm_valid_gpio_mask = 0b11111000000111101;
#endif


#if defined(ESPS_SUPPORT_PWM)
void setupPWM () {
  if ( config.pwm_global_enabled ) {
    if ( (config.pwm_freq >= 100) && (config.pwm_freq <= 1000) ) {
      analogWriteFreq(config.pwm_freq);
    }
    for (int gpio=0; gpio < NUM_GPIO; gpio++ ) {
      if ( ( pwm_valid_gpio_mask & 1<<gpio ) && (config.pwm_gpio_enabled & 1<<gpio) ) {
        pinMode(gpio, OUTPUT);
        last_pwm[gpio] = 65535;  // invalid value to force initial update
      }
    }
    handlePWM();
  }
}

void handlePWM() {

  uint16_t pwm_val = 0;
  if ( config.pwm_global_enabled ) {
    for (int gpio=0; gpio < NUM_GPIO; gpio++ ) {
      if ( ( pwm_valid_gpio_mask & 1<<gpio ) && (config.pwm_gpio_enabled & 1<<gpio) ) {

        uint16_t gpio_dmx = config.pwm_gpio_dmx[gpio];
        if ((gpio_dmx < config.channel_count) || (gpio_dmx == 65535)) {

          if (gpio_dmx < config.channel_count) {
#if defined (ESPS_MODE_PIXEL)
            pwm_val = (config.pwm_gamma) ? GAMMA_TABLE[pixels.getData()[gpio_dmx]]>>6 : pixels.getData()[gpio_dmx]<<2;
#elif defined(ESPS_MODE_SERIAL)
            pwm_val = (config.pwm_gamma) ? GAMMA_TABLE[serial.getData()[gpio_dmx]]>>6 : serial.getData()[gpio_dmx]<<2;
#endif
          } else {
            pwm_val = 0;  // dmx channel 65535 forces 0 pwm value
          }

          // relays dont like pwm, force output high or low if "digital"
          if (config.pwm_gpio_digital & 1<<gpio) {
            if ( pwm_val >= 512) {
              pwm_val = 1023;
            } else {
              pwm_val = 0;
            }
          }

          // inverted pwm output
          if (config.pwm_gpio_invert & 1<<gpio) {
            pwm_val = 1023-pwm_val;  // 0..1023 => 1023..0
          }

          if ( pwm_val != last_pwm[gpio]) {
            last_pwm[gpio] = pwm_val;
            analogWrite(gpio, pwm_val);
          }
        }
      }
    }
  }
}
#endif

