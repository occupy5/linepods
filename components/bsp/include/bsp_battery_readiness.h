// components/bsp/include/bsp_battery_readiness.h
// CW2017 读数可信度判定。纯逻辑，不依赖 ESP-IDF/LVGL，便于主机测试覆盖
// 深睡唤醒瞬间的瞬时值。
#pragma once

#include <stdbool.h>

// 单节锂电的合理电压窗口。超出窗口说明读回的是垃圾值，而不是真实电压。
#define BSP_BATTERY_MV_MIN 2500
#define BSP_BATTERY_MV_MAX 4500

// 空电判定电压。CW2017 被 0x30 快速启动后会重新做首次 SOC 换算，换算出结果
// 之前 SOC 寄存器会读到 0；此时电芯电压仍然健康。只有电压已经贴近空电时，
// 0% 才是真实读数，否则它只是"还没算完"。
#define BSP_BATTERY_EMPTY_MV 3300

// 该 SOC/电压组合能否作为真实电量显示。返回 false 时上层必须继续显示占位符，
// 不能把瞬时值当成真实电量。
bool bsp_battery_reading_usable(int soc, int mv);
