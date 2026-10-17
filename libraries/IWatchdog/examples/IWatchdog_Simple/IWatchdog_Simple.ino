/*
 * 看门狗最小示例：2 秒超时，正常喂狗；把 loop 里 while(1) 放开就能验证复位。
 */
#include <IWatchdog.h>

void setup() {
  Serial.begin(115200);
  delay(200);

  if (IWatchdog.isReset(true)) {      /* 顺便清复位标志 */
    Serial.println("last reset: WATCHDOG");
  } else {
    Serial.println("last reset: power-on / other");
  }

  IWatchdog.begin(2000000);           /* 2 秒 */
  Serial.print("IWDG enabled, timeout(us) = ");
  Serial.println(IWatchdog.getTimeout());
}

void loop() {
  IWatchdog.reload();                 /* 喂狗 */
  Serial.println("alive");
  delay(500);

  /* 想验证看门狗：把下面两行放开，第 6 次循环后停止喂狗，2 秒后芯片复位 */
  /* static uint8_t n = 0;  if (++n > 6) { while (1) {} } */
}
