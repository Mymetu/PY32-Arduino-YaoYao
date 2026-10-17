/*
 * IWatchdog - PY32F0xx 独立看门狗 (IWDG)
 *
 * 用法（与 STM32duino 的 IWatchdog 库一致）：
 *   #include <IWatchdog.h>
 *
 *   void setup() {
 *     Serial.begin(115200);
 *     if (IWatchdog.isReset(true)) { Serial.println("上次是看门狗复位"); }
 *     IWatchdog.begin(2000000);     // 2 秒超时，执行这句就开始计时
 *   }
 *   void loop() {
 *     IWatchdog.reload();           // 主循环里喂狗
 *     ...
 *   }
 *
 * 说明：
 *   - 时钟是内部 LSI 32.768kHz（Tools 里 LowSpeedClockSource 选 LSI），
 *     所以超时精度约 ±5~10%，只适合做"防死机"，不适合做精确定时。
 *   - 超时范围约 0.125ms ~ 32s（4096 x 256 / 32768）。
 *   - IWDG 一旦启动**硬件上无法关闭**，只能一直喂；调试时可在 loop 里暂停喂狗
 *     观察复位，或用 __HAL_DBGMCU_FREEZE_IWDG() 让调试器停住时不复位。
 */
#ifndef _IWATCHDOG_H_
#define _IWATCHDOG_H_

#include "Arduino.h"

class IWatchdogClass {
  public:
    IWatchdogClass();

    // 配置并启动看门狗（timeout_us 单位微秒）。返回后开始计时，必须周期 reload()
    void begin(uint32_t timeout_us);
    // 默认 2 秒
    void begin();

    // 喂狗：主循环里周期性调用
    void reload();

    // 运行中改超时（会重写 PR/RLR）
    void set(uint32_t timeout_us);

    // 当前实际超时（微秒）
    uint32_t getTimeout();

    // 看门狗是否已启动
    bool isEnabled();

    // 上次复位是不是看门狗引起的；clear=true 时顺便清掉复位标志
    bool isReset(bool clear = false);

    // 清所有复位标志
    void clearReset();

  private:
    bool     _enabled;
    uint32_t _timeout_us;
    bool     _compute(uint32_t timeout_us, uint32_t *prescaler, uint32_t *reload);
    void     _lsiReady();
};

extern IWatchdogClass IWatchdog;

#endif /* _IWATCHDOG_H_ */
