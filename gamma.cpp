#include <Arduino.h>

uint16_t GAMMA_TABLE[256] = { 0 };

void updateGammaTable(float gammaVal, float briteVal) {
  for (int i=0; i<256; i++) {
    GAMMA_TABLE[i] = (uint16_t) min((65535.0 * pow(i * briteVal /255.0, gammaVal) + 0.5), 65535.0);
  }
}

