#ifndef __JOG_CONFIG_H
#define __JOG_CONFIG_H
/* 只用原摇杆：PA0/PA1双轴ADC，PA2自带按键按下接地；无需新增按键。 */
#define JOG_SWAP_XY 0
#define JOG_INVERT_X 0
#define JOG_INVERT_Y 1
#define JOG_DEAD_ZONE 300
#define JOG_SAMPLE_MS 5
#define JOG_SAMPLE_TIMEOUT_MS 20
#define JOG_FEEDBACK_TIMEOUT_MS 100
#define JOG_DRAW_MS 100
#define JOG_LONG_PRESS_MS 800
#define JOG_DEFAULT_SPEED 100                 /* 0.1mm/s单位，默认10mm/s */
#endif
