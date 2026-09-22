// components/bsp/src/bsp_battery_readiness.c
#include "bsp_battery_readiness.h"

bool bsp_battery_reading_usable(int soc, int mv)
{
    if (soc < 0 || soc > 100) return false;
    if (mv < BSP_BATTERY_MV_MIN || mv > BSP_BATTERY_MV_MAX) return false;

    /* 深睡唤醒后电量计以 0x30 快速启动，首个换算完成前会读回 0%，
     * 但电芯电压仍然在放电平台上。真实空电会先让电压塌到空电阈值以下。 */
    if (soc == 0 && mv >= BSP_BATTERY_EMPTY_MV) return false;

    return true;
}
