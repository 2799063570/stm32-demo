#ifndef __PID_H
#define __PID_H

typedef struct
{
	float Kp, Ki, Kd, Kv, Ks;
	float Dt, Limit, DAlpha;
	float Integral, PreviousMeasurement, Derivative;
	unsigned char HasPrevious;
} PID_TypeDef;

void PID_Reset(PID_TypeDef *PID);
float PID_Calculate(PID_TypeDef *PID, float Target, float Measurement);
#endif
