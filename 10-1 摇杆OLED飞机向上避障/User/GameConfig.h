#ifndef __GAME_CONFIG_H
#define __GAME_CONFIG_H

/*每5ms读摇杆，每40ms更新游戏。屏幕原点在左上角，右/下为正。*/
#define GAME_FRAME_MS          40
#define GAME_BUILD_TAG         "UP FIX 1020-3" // 固定可见版本号，核对实板实际运行的固件
#define STICK_SAMPLE_MS        5
#define STICK_CAL_SAMPLES      32
#define STICK_CAL_SPAN         200
#define STICK_DEAD_ZONE        300
#define STICK_FAST_ZONE        1100
#define STICK_SWAP_XY          0       // 横竖轴接反或安装旋转时设1
#define STICK_INVERT_X         1       // 用户确认只有左右反向，因此只修正X轴
#define STICK_INVERT_Y         1       // 恢复原版已确认正确的上下方向，不额外反转Y
#define STICK_DEBOUNCE_SAMPLES 4       // 稳定20ms才确认按下/松开
#define GAME_INPUT_DEBUG       1       // 暂在顶部显示Y采样/方向/飞机位置；设0恢复计分栏

#endif
