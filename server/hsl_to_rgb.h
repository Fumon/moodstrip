#ifndef HSL_TO_RGB_H
#define HSL_TO_RGB_H
#include <stdint.h>

void HSL_to_RGB(double H, double S, double L, uint8_t *R, uint8_t *G, uint8_t *B);
double Hue_2_RGB(double v1, double v2, double vH );

#endif
