#include "IWatchdog.h"

#if defined(HAL_IWDG_MODULE_ENABLED) && defined(IWDG)

/* PY32F0 的 LSI 标称 32.768kHz */
#define IWDG_LSI_FREQ     32768UL
#define IWDG_RLR_MAX      0x0FFFU

IWatchdogClass IWatchdog;

static IWDG_HandleTypeDef hiwdg;

static const uint32_t iwdg_presc_reg[7] = {
  IWDG_PRESCALER_4, IWDG_PRESCALER_8, IWDG_PRESCALER_16, IWDG_PRESCALER_32,
  IWDG_PRESCALER_64, IWDG_PRESCALER_128, IWDG_PRESCALER_256
};
static const uint32_t iwdg_presc_div[7] = { 4, 8, 16, 32, 64, 128, 256 };

IWatchdogClass::IWatchdogClass()
{
  _enabled    = false;
  _timeout_us = 0;
}

/* LSI 打开并等待稳定（IWDG 启动后硬件也会强制打开 LSI） */
void IWatchdogClass::_lsiReady()
{
  __HAL_RCC_LSI_ENABLE();
#ifdef RCC_FLAG_LSIRDY
  uint32_t t0 = millis();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET) {
    if ((millis() - t0) > 100U) {
      break;
    }
  }
#endif
}

/*
 * 选一组 (prescaler, reload) 使超时最接近要求值。
 * IWDG 超时 = (RLR + 1) x prescaler / f_LSI
 */
bool IWatchdogClass::_compute(uint32_t timeout_us, uint32_t *prescaler, uint32_t *reload)
{
  if (timeout_us == 0U) {
    timeout_us = 1U;
  }
  for (uint8_t i = 0; i < 7U; i++) {
    uint64_t rlr = (((uint64_t)timeout_us * IWDG_LSI_FREQ) / ((uint64_t)iwdg_presc_div[i] * 1000000ULL));
    if (rlr > 0ULL) {
      rlr--;                       /* (RLR + 1) 计数 */
    }
    if (rlr <= (uint64_t)IWDG_RLR_MAX) {
      *prescaler = iwdg_presc_reg[i];
      *reload    = (uint32_t)rlr;
      return true;
    }
  }
  /* 要求比最大超时还长：用最大档 */
  *prescaler = IWDG_PRESCALER_256;
  *reload    = IWDG_RLR_MAX;
  return true;
}

void IWatchdogClass::begin(uint32_t timeout_us)
{
  uint32_t presc, rlr;

  _lsiReady();
  _compute(timeout_us, &presc, &rlr);

  hiwdg.Instance       = IWDG;
  hiwdg.Init.Prescaler = presc;
  hiwdg.Init.Reload    = rlr;

  /*
   * 注意：HAL_IWDG_Init() 是"先启动看门狗(KR=0xCCCC)，再写 PR/RLR，最后等 SR 同步"。
   * 也就是说不管它返回 HAL_OK 还是 HAL_TIMEOUT，**狗都已经在跑了**。
   * 所以这里一律置 _enabled = true 并且立刻重装计数；
   * 否则一旦 HAL 返回非 OK，reload() 就不喂狗 -> 变成约 0.5 秒一次的复位死循环，
   * 现象就是"刷完像死了一样、串口一点输出都没有"。
   */
  (void)HAL_IWDG_Init(&hiwdg);
  _enabled = true;

  /* 若 SR 同步没等到，直接再写一次并立刻重装 */
#if defined(IWDG_SR_PVU) || defined(IWDG_SR_RVU)
  {
    uint32_t sr = IWDG->SR;
    uint32_t mask = 0U;
#ifdef IWDG_SR_PVU
    mask |= IWDG_SR_PVU;
#endif
#ifdef IWDG_SR_RVU
    mask |= IWDG_SR_RVU;
#endif
    if ((sr & mask) != 0U) {
      IWDG->KR  = 0x00005555U;   /* 允许写 PR/RLR */
      IWDG->PR  = presc;
      IWDG->RLR = rlr;
    }
  }
#endif
  __HAL_IWDG_RELOAD_COUNTER(&hiwdg);

  _timeout_us = getTimeout();
}

void IWatchdogClass::begin()
{
  begin(2000000U);   /* 默认 2 秒 */
}

void IWatchdogClass::reload()
{
  if (_enabled) {
    HAL_IWDG_Refresh(&hiwdg);
  }
}

void IWatchdogClass::set(uint32_t timeout_us)
{
  uint32_t presc, rlr;

  _compute(timeout_us, &presc, &rlr);
  hiwdg.Instance       = IWDG;
  hiwdg.Init.Prescaler = presc;
  hiwdg.Init.Reload    = rlr;
  (void)HAL_IWDG_Init(&hiwdg);
  _enabled = true;
  __HAL_IWDG_RELOAD_COUNTER(&hiwdg);
  _timeout_us = getTimeout();
}

uint32_t IWatchdogClass::getTimeout()
{
  uint32_t div = 4U;

  for (uint8_t i = 0; i < 7U; i++) {
    if (iwdg_presc_reg[i] == (IWDG->PR & IWDG_PR_PR_Msk)) {
      div = iwdg_presc_div[i];
      break;
    }
  }
  return (uint32_t)(((uint64_t)((IWDG->RLR & IWDG_RLR_RL_Msk) + 1U) * div * 1000000ULL) / IWDG_LSI_FREQ);
}

bool IWatchdogClass::isEnabled()
{
  return _enabled;
}

bool IWatchdogClass::isReset(bool clear)
{
  bool res = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET);
  if (clear) {
    clearReset();
  }
  return res;
}

void IWatchdogClass::clearReset()
{
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

#else /* no IWDG */

IWatchdogClass IWatchdog;

IWatchdogClass::IWatchdogClass() { _enabled = false; _timeout_us = 0; }
void IWatchdogClass::begin(uint32_t) {}
void IWatchdogClass::begin() {}
void IWatchdogClass::reload() {}
void IWatchdogClass::set(uint32_t) {}
uint32_t IWatchdogClass::getTimeout() { return 0; }
bool IWatchdogClass::isEnabled() { return false; }
bool IWatchdogClass::isReset(bool) { return false; }
void IWatchdogClass::clearReset() {}
bool IWatchdogClass::_compute(uint32_t, uint32_t *, uint32_t *) { return false; }
void IWatchdogClass::_lsiReady() {}

#endif
