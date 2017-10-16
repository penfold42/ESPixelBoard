#ifndef GAMMA_H_
#define GAMMA_H_

/* Gamma correction table */
extern uint16_t GAMMA_TABLE[];

#define GAMMA_SHIFT 8

void updateGammaTable(float gammaVal);

#endif /* GAMMA_H_ */
