#ifndef PWM_H_
#define PWM_H_

#include "gamma.h"

#define NUM_GPIO 17    // 0 .. 16 inclusive

extern uint32_t pwm_valid_gpio_mask;

#if defined(ESPS_MODE_PIXEL)
extern PixelDriver     pixels;         // Pixel object
#elif defined(ESPS_MODE_SERIAL)
extern SerialDriver    serial;         // Serial object
#endif

void setupPWM ();
void handlePWM ();

#endif // PWM_H_
