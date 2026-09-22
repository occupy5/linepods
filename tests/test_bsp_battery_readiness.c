#include <assert.h>

#include "bsp_battery_readiness.h"

int main(void)
{
    /* 深睡唤醒后电量计正在换算：SOC 读到 0，但电压还健康。 */
    assert(!bsp_battery_reading_usable(0, 3900));
    assert(!bsp_battery_reading_usable(0, 4200));
    assert(!bsp_battery_reading_usable(0, BSP_BATTERY_EMPTY_MV));

    /* 真实空电：0% 且电压已经贴近空电。 */
    assert(bsp_battery_reading_usable(0, BSP_BATTERY_EMPTY_MV - 1));
    assert(bsp_battery_reading_usable(0, 3000));

    /* 首个换算未完成时寄存器可能是 0xFF 或其它越界值。 */
    assert(!bsp_battery_reading_usable(255, 3900));
    assert(!bsp_battery_reading_usable(101, 3900));
    assert(!bsp_battery_reading_usable(-1, 3900));

    /* 电压不可信时，任何 SOC 都不能用。 */
    assert(!bsp_battery_reading_usable(73, 0));
    assert(!bsp_battery_reading_usable(73, BSP_BATTERY_MV_MIN - 1));
    assert(!bsp_battery_reading_usable(73, BSP_BATTERY_MV_MAX + 1));

    /* 常规读数。 */
    assert(bsp_battery_reading_usable(1, 3400));
    assert(bsp_battery_reading_usable(73, 3900));
    assert(bsp_battery_reading_usable(100, 4200));
    assert(bsp_battery_reading_usable(100, BSP_BATTERY_MV_MAX));
    return 0;
}
