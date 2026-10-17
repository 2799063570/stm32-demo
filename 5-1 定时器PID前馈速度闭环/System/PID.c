#include "PID.h"

/** 函数：浮点限幅，内部使用 */
static float PID_Limit(float Value, float Limit)
{
	if (Value > Limit) return Limit;
	if (Value < -Limit) return -Limit;
	return Value;
}

/**
  * 函    数：清除控制器历史量，保留增益和周期配置
  * 参    数：PID 控制器地址
  * 返 回 值：无
  */
void PID_Reset(PID_TypeDef *PID)
{
	PID->Integral = 0;
	PID->PreviousMeasurement = 0;
	PID->Derivative = 0;
	PID->HasPrevious = 0;
}

/**
  * 函    数：位置式PID+速度/静摩擦前馈+条件积分抗饱和
  * 参    数：Target目标RPM；Measurement反馈RPM；PID内Dt必须为正
  * 返 回 值：带正负号的PWM千分比，范围±Limit
  */
float PID_Calculate(PID_TypeDef *PID, float Target, float Measurement)
{
	float Error, FeedForward, NewIntegral, RawOutput, DMeasurement;
	if (PID->Dt <= 0 || PID->Limit <= 0) return 0;
	Error = Target - Measurement;
	/*对测量值微分，避免目标阶跃产生微分冲击；再对微分量低通*/
	DMeasurement = PID->HasPrevious ? (Measurement - PID->PreviousMeasurement) / PID->Dt : 0;
	PID->Derivative += PID->DAlpha * (DMeasurement - PID->Derivative);
	PID->PreviousMeasurement = Measurement;
	PID->HasPrevious = 1;
	FeedForward = PID->Kv * Target;
	if (Target > 0) FeedForward += PID->Ks;
	if (Target < 0) FeedForward -= PID->Ks;
	NewIntegral = PID_Limit(PID->Integral + PID->Ki * Error * PID->Dt, PID->Limit);
	RawOutput = FeedForward + PID->Kp * Error + NewIntegral - PID->Kd * PID->Derivative;
	/*已经饱和且误差继续推向同一方向时，不再积累积分；误差反向时允许退饱和*/
	if (!((RawOutput > PID->Limit && Error > 0) || (RawOutput < -PID->Limit && Error < 0)))
		PID->Integral = NewIntegral;
	RawOutput = FeedForward + PID->Kp * Error + PID->Integral - PID->Kd * PID->Derivative;
	return PID_Limit(RawOutput, PID->Limit);
}
