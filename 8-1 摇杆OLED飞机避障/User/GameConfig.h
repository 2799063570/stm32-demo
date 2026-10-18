#ifndef __GAME_CONFIG_H
#define __GAME_CONFIG_H

/*每5ms读摇杆，每40ms更新游戏。屏幕原点在左上角，右/下为正。*/
#define GAME_FRAME_MS          40
#define STICK_SAMPLE_MS        5
#define STICK_CAL_SAMPLES      32
#define STICK_CAL_SPAN         200
#define STICK_DEAD_ZONE        300
#define STICK_FAST_ZONE        1100
#define STICK_SWAP_XY          0       // 横竖轴接反或安装旋转时设1
#define STICK_INVERT_X         0       // 左右相反时切换0/1
#define STICK_INVERT_Y         1       // 上下相反时切换0/1
#define STICK_DEBOUNCE_SAMPLES 4       // 稳定20ms才确认按下/松开

#endif
