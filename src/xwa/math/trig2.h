#ifndef XWA_MATH_TRIG2_H
#define XWA_MATH_TRIG2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int trig2_xmovedist;
extern uint16_t trig2_xyangle;
extern int trig2_polardistance;
extern int trig2_xoffset;
extern int trig2_ymovedist;
extern uint16_t trig2_phi;
extern int trig2_rho;
extern int trig2_yoffset;
extern uint16_t trig2_signswap;
extern uint16_t trig2_signx;
extern uint16_t trig2_signy;
extern uint16_t trig2_signz;
extern int trig2_zmovedist;
extern int trig2_zoffset;
extern uint32_t trig2_divisorhilo;
extern uint16_t trig2_theta;
extern uint16_t trig2_angleplane;
extern uint16_t targetPitch;

void trig2_ctop(int dx, int dy, int dz);
void trig2_ctop2dim(int dx, int dy);
void trig2_movexyz(int distance, int pitch, int heading);
void trig2_ptoc3dim(void);
void trig2_calcarctan_core(int a, int b, int16_t* outRatio, int16_t* outAngle);
int trig2_arctan(int y, int x);
uint16_t trig2_arcsin(int sinQ15);
int trig2_w_arcsin(int sinQ15);
int trig2_getsignedsin(int angle);
int trig2_getsignedcos(int angle);
int trig2_calcsineofangle(int angle);
int trig2_sinewordmult(int value, int angle);
int trig2_cosinewordmult(int value, int angle);
int trig2_sinedwordmult(int value, int angle);
int trig2_cosinedwordmult(int value, int angle);

#ifdef __cplusplus
}
#endif

#endif
