// =====================================================================
//  Yao-Relay V2.1  ——  三接口 Modbus RTU 继电器（简单粗暴版）
//  PY32F040C1BT6 @ PY32Duino 0.2.62      HSI 24MHz
//
//  三个物理口是同一个 Modbus RTU 从站（同一套参数、同一个继电器）：
//    TTL (USART1)  PA9(TX) PA10(RX)
//    485 (USART2)  PA2(TX) PA3(RX)   DE = PA4（高=发送）
//    232 (USART4)  PA0(TX) PA1(RX)
//  继电器 PA11（输出）     DI PA12（上拉输入：高电平=0，低电平=1）
//
//  ==================== 常用命令速查（HEX，串口助手直接发，无需回车换行）====================
//  （默认出厂：地址 1 / 波特率 9600 / 8N1。参数改完设备会自动重启并保存。）
//
//   【ID1（地址 01）常用命令】
//    读继电器          01 01 00 00 00 01 FD CA       回 01 01 01 01 90 48=吸合 / 01 01 01 00 51 88=释放
//    读DI           01 02 00 00 00 01 B9 CA       回 01 02 01 01 60 48=低电平(1) / 01 02 01 00 A1 88=高电平(0)
//    闭合(锁定)        01 05 00 00 FF 00 8C 3A       应答=原样回显
//    释放            01 05 00 00 00 00 CD CA       应答=原样回显
//    脉冲100ms       01 06 00 06 00 64 68 20
//    脉冲200ms       01 06 00 06 00 C8 68 5D
//    脉冲500ms       01 06 00 06 01 F4 69 DC
//    脉冲1000ms      01 06 00 06 03 E8 69 75
//    脉冲2000ms      01 06 00 06 07 D0 6A 67
//    脉冲5000ms      01 06 00 06 13 88 64 9D
//    立即释放          01 06 00 06 00 00 69 CB       取消脉冲
//    波特率1200       01 06 00 07 00 00 38 0B
//    波特率2400       01 06 00 07 00 01 F9 CB
//    波特率4800       01 06 00 07 00 02 B9 CA
//    波特率9600       01 06 00 07 00 03 78 0A
//    波特率19200      01 06 00 07 00 04 39 C8
//    波特率38400      01 06 00 07 00 05 F8 08
//    波特率57600      01 06 00 07 00 06 B8 09
//    波特率115200     01 06 00 07 00 07 79 C9       ★115200 只能用索引(7)，写成数值会超出 16 位
//    校验8N1         01 06 00 0D 00 00 18 09       无校验 1 停止位（默认）
//    校验8E1         01 06 00 0D 00 01 D9 C9       偶校验
//    校验8O1         01 06 00 0D 00 02 99 C8       奇校验
//    校验8N2         01 06 00 0D 00 03 58 08       无校验 2 停止位
//    恢复出厂          01 06 00 0B 55 AA 47 27       回 ID1+9600+8N1，继电器闪 5 下后重启
//
//   【ID1 切换成 ID2】 01 06 00 05 00 02 18 0A      发完设备重启，之后用地址 02 通讯
//
//   【ID2（地址 02）常用命令】
//    读继电器          02 01 00 00 00 01 FD F9
//    读DI           02 02 00 00 00 01 B9 F9
//    闭合(锁定)        02 05 00 00 FF 00 8C 09
//    释放            02 05 00 00 00 00 CD F9
//    脉冲100ms       02 06 00 06 00 64 68 13
//    脉冲200ms       02 06 00 06 00 C8 68 6E
//    脉冲500ms       02 06 00 06 01 F4 69 EF
//    脉冲1000ms      02 06 00 06 03 E8 69 46
//    脉冲2000ms      02 06 00 06 07 D0 6A 54
//    脉冲5000ms      02 06 00 06 13 88 64 AE
//    立即释放          02 06 00 06 00 00 69 F8
//    波特率1200       02 06 00 07 00 00 38 38          波特率2400  02 06 00 07 00 01 F9 F8
//    波特率4800       02 06 00 07 00 02 B9 F9          波特率9600  02 06 00 07 00 03 78 39
//    波特率19200      02 06 00 07 00 04 39 FB          波特率38400 02 06 00 07 00 05 F8 3B
//    波特率57600      02 06 00 07 00 06 B8 3A          波特率115200 02 06 00 07 00 07 79 FA
//    校验8N1         02 06 00 0D 00 00 18 3A          校验8E1     02 06 00 0D 00 01 D9 FA
//    校验8O1         02 06 00 0D 00 02 99 FB          校验8N2     02 06 00 0D 00 03 58 3B
//    恢复出厂          02 06 00 0B 55 AA 47 14
//
//   【ID2 切换回 ID1】 02 06 00 05 00 01 58 38      发完设备重启，之后用地址 01 通讯
//
//  命令规律：
//    写点动脉冲 = 01(地址) 06 00 06 <ms高字节> <ms低字节> <CRC>   例 1000ms=0x03E8
//    写波特率   = 地址 06 00 07 00 <索引0..7> <CRC>
//    写校验位   = 地址 06 00 0D 00 <0..3> <CRC>
//    写从站地址 = 地址 06 00 05 00 <1..247> <CRC>
//    异常应答   = 地址 <功能码|0x80> <异常码> <CRC>
//                 异常码 01=非法功能 02=非法地址 03=非法数据
//
//  =====================================================================================
//
//  ---- 参数怎么保存（简单粗暴）----
//   地址/波特率/格式 一旦被修改：先发应答 → 写进 flash → **立刻重启** →
//   开机时从 flash 读回来加载。所以改完之后设备会自动重启一次，
//   主站把参数改成一样的就能继续通讯。（点动脉冲不重启，立即执行。）
//   参数区 = 128K flash 的最后一页 0x0801FF00，magic+CRC 校验，
//   没写过/写坏 -> 自动用默认值（地址1 / 9600 / 8N1），不影响程序运行。
//   恢复出厂两种方式：写 0x000B=0x55AA，或把 PF8 上拉 5 秒（继电器闪 5 下）。
//
//  ---- 实现要点（踩坑换来的，别随手改）----
//   1) 收发全部"直写寄存器 + 主循环轮询"：Arduino 的 Serial 发送通道持续打印会卡死
//      主循环；内核里 USART4 的 uart_init 分支还会覆盖 USART1 的句柄槽。轮询全部绕开。
//   2) USART4 时钟由内核 variant_generic.cpp 的 constructor 在 main() 之前打开。
//   3) Modbus 分帧：空闲 >=3ms 算一帧结束；帧内 >=60ms 未结束直接丢弃。
//   4) 485 发送：DE 整帧只拉高一次，先给 50us 使能，发完等 TC 再拉低并丢自身回显。
//   5) 波特率/格式只在开机时生效（改完就重启），不做运行时切换。
//   6) 开机不打任何串口信息。
//   7) 看门狗 2 秒，主循环末尾喂狗。
// =====================================================================

#include <IWatchdog.h>
#include <string.h>

/* ================= 用户配置 ================= */

#define DEF_SLAVE_ADDR   1             /* 出厂从站地址 */
#define DEF_BAUD_IDX     3             /* 出厂波特率：3 = 9600 */
#define DEF_FMT          0             /* 出厂 8N1 */

#define EN_TTL           1
#define EN_485           1
#define EN_232           1

#define TTL_PORT         Serial        /* USART1 */
#define RS485_PORT       Serial2       /* USART2 */
#define U232_PORT        Serial4       /* USART4 */

#define RELAY_PIN        PA11
#define RELAY_ACTIVE_LOW 0
#define IN_PIN           PA12          /* DI：高电平=0，低电平=1 */

#define BOOT0_PIN        PF8
#define BOOT0_HOLD_MS    5000UL        /* 上拉持续 5 秒 -> 恢复出厂 */

#define RS485_DE_PIN     PA4
#define RS485_TX_LEVEL   HIGH
#define RS485_IDLE_LEVEL (RS485_TX_LEVEL == HIGH ? LOW : HIGH)
#define RS485_DE_PRECHARGE_US 50

#define MB_GAP_MS           3UL        /* 帧间隔 */
#define MB_FRAME_TIMEOUT_MS 60UL       /* 帧内超时 */
#define MB_BUF              64

#define USE_WATCHDOG     1             /* 1 = 2 秒看门狗 */
#define WATCHDOG_MS      2000000UL

#define RELAY_ON_LEVEL   (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF_LEVEL  (RELAY_ACTIVE_LOW ? HIGH : LOW)

/* ================= 端口结构体（必须在第一个函数之前：Arduino 会自动插函数原型） ==== */

struct Port {
  const char    *name;
  USART_TypeDef *inst;
  void         (*beginFn)();
  bool           halfDuplex;
  bool           enabled;
  bool           started;
  uint8_t        buf[MB_BUF];
  uint16_t       len;
  uint32_t       frameStart;
  uint32_t       lastRxMs;
};

/* ================= 参数与状态 ================= */

static const uint32_t baudTable[8] = { 1200UL, 2400UL, 4800UL, 9600UL,
                                       19200UL, 38400UL, 57600UL, 115200UL };

static uint16_t slaveAddr  = DEF_SLAVE_ADDR;
static uint8_t  baudIdx    = DEF_BAUD_IDX;
static uint8_t  frameFmt   = DEF_FMT;      /* 0=8N1 1=8E1 2=8O1 3=8N2 */
static uint32_t modbusBaud = 9600UL;

static bool     relayOn     = false;
static bool     pulseActive = false;
static uint32_t pulseEnd    = 0;

static bool     needRestart = false;       /* 参数改过：应答发完后写 flash + 重启 */
static bool     needFactory = false;       /* 恢复出厂：应答发完后执行 */

/* ================= 直写寄存器收发 ================= */

static void rawTxCharTo(USART_TypeDef *u, char c)
{
  uint32_t t0 = millis();
  while ((u->SR & USART_SR_TXE) == 0U) {
    if ((millis() - t0) > 5U) {
      return;
    }
  }
  u->DR = (uint8_t)c;
}

static void drainRx(USART_TypeDef *u)
{
  uint8_t guard = 8;
  while ((u->SR & USART_SR_RXNE) && (guard-- > 0U)) {
    (void)u->DR;
  }
}

static void waitTc(USART_TypeDef *u)
{
  uint32_t t0 = millis();
  while ((u->SR & USART_SR_TC) == 0U) {
    if ((millis() - t0) > 5U) {
      return;
    }
  }
}

/* ================= 端口表 ================= */

static void beginTTL() { TTL_PORT.begin(modbusBaud); }
static void begin485() { RS485_PORT.begin(modbusBaud); }
static void begin232() { U232_PORT.begin(modbusBaud); }

static Port ports[] = {
  { "TTL/USART1", USART1, beginTTL, false, EN_TTL, false, {0}, 0, 0, 0 },
  { "485/USART2", USART2, begin485, true,  EN_485, false, {0}, 0, 0, 0 },
  { "232/USART4", USART4, begin232, false, EN_232, false, {0}, 0, 0, 0 },
};
static const uint8_t PORT_COUNT = sizeof(ports) / sizeof(ports[0]);

/* ================= 继电器 / DI ================= */

static inline void relayWrite(bool on)
{
  digitalWrite(RELAY_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

static void relaySet(bool on)
{
  relayOn = on;
  relayWrite(on);
}

static void relayPulse(uint32_t ms)
{
  if (ms == 0U) {
    pulseActive = false;
    relaySet(false);
    return;
  }
  relaySet(true);
  pulseEnd = millis() + ms;
  pulseActive = true;
}

static void relayPulseTask()
{
  if (pulseActive && ((int32_t)(millis() - pulseEnd) >= 0)) {
    pulseActive = false;
    relaySet(false);
  }
}

/* DI：高电平=0，低电平=1 */
static bool diValue()
{
  return (digitalRead(IN_PIN) == LOW);
}

/* ================= 发送一帧（485 整帧只切一次方向） ================= */

static void sendFrame(Port &p, const uint8_t *buf, uint16_t len)
{
  if (len == 0U) {
    return;
  }
  if (p.halfDuplex) {
    digitalWrite(RS485_DE_PIN, RS485_TX_LEVEL);
    delayMicroseconds(RS485_DE_PRECHARGE_US);
  }
  for (uint16_t i = 0; i < len; i++) {
    rawTxCharTo(p.inst, (char)buf[i]);
  }
  if (p.halfDuplex) {
    waitTc(p.inst);
    digitalWrite(RS485_DE_PIN, RS485_IDLE_LEVEL);
    drainRx(p.inst);
  }
}

/* ================= Modbus CRC16 ================= */

static uint16_t mbCrc16(const uint8_t *buf, uint16_t len)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (uint16_t)buf[i];
    for (uint8_t b = 0; b < 8U; b++) {
      crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

static uint16_t appendCrc(uint8_t *buf, uint16_t len)
{
  uint16_t crc = mbCrc16(buf, len);
  buf[len++] = (uint8_t)(crc & 0xFFU);
  buf[len++] = (uint8_t)(crc >> 8);
  return len;
}

/* ================= flash 参数区（最后一页） ================= */

#define CFG_ADDR   0x0801FF00UL
#define CFG_MAGIC  0x59524C34UL

#include "py32f0xx_hal_flash.h"

static uint8_t cfgPage[128] __attribute__((aligned(4)));

extern "C" {
  extern uint32_t _siramfunc[];
  extern uint32_t _sramfunc[];
  extern uint32_t _eramfunc[];
}

/* PY32 的整页突发编程代码必须在 RAM 里跑：把厂商 HAL 的 .RamFunc 段从 flash 拷进 RAM */
static void copyRamFuncs()
{
  uint32_t *s = _siramfunc;
  for (uint32_t *d = _sramfunc; d < _eramfunc; ) {
    *d++ = *s++;
  }
}

struct CfgBlob {
  uint32_t magic;
  uint16_t addr;
  uint8_t  baudIdx;
  uint8_t  fmt;
  uint16_t crc;
  uint16_t pad;
};

static bool cfgLoad()
{
  const CfgBlob *c = (const CfgBlob *)CFG_ADDR;
  if (c->magic != CFG_MAGIC) {
    return false;
  }
  if (mbCrc16((const uint8_t *)c, 8U) != c->crc) {
    return false;
  }
  if ((c->addr < 1U) || (c->addr > 247U) || (c->baudIdx > 7U) || (c->fmt > 3U)) {
    return false;
  }
  slaveAddr = c->addr;
  baudIdx   = c->baudIdx;
  frameFmt  = c->fmt;
  return true;
}

static void cfgSave()
{
  memset(cfgPage, 0xFF, sizeof(cfgPage));
  CfgBlob *c = (CfgBlob *)cfgPage;
  c->magic   = CFG_MAGIC;
  c->addr    = slaveAddr;
  c->baudIdx = baudIdx;
  c->fmt     = frameFmt;
  c->crc     = mbCrc16(cfgPage, 8U);

  copyRamFuncs();

  if (HAL_FLASH_Unlock() == HAL_OK) {
    FLASH_EraseInitTypeDef ei;
    uint32_t pageErr = 0U;
    ei.TypeErase   = FLASH_TYPEERASE_PAGEERASE;
    ei.PageAddress = CFG_ADDR;
    ei.NbPages     = 1U;
    if (HAL_FLASHEx_Erase(&ei, &pageErr) == HAL_OK) {
      HAL_FLASH_PageProgram(CFG_ADDR, (uint32_t *)cfgPage);
    }
    HAL_FLASH_Lock();
  }
}

/* 恢复出厂参数 + 保存 + 重启 */
static void factoryDefaults()
{
  slaveAddr  = DEF_SLAVE_ADDR;
  baudIdx    = DEF_BAUD_IDX;
  frameFmt   = DEF_FMT;
  pulseActive = false;
  relaySet(false);
  cfgSave();

  for (uint8_t i = 0; i < 5U; i++) {           /* 闪 5 下 = 已恢复出厂 */
    relayWrite(true);
    delay(80);
    relayWrite(false);
    delay(120);
  }
  NVIC_SystemReset();
}

/* ================= Modbus 处理（01/02/05/06） ================= */

static void mbReply(Port &p, const uint8_t *b, uint16_t n)
{
  sendFrame(p, b, n);
}

static void mbException(Port &p, uint8_t addr, uint8_t fc, uint8_t code)
{
  uint8_t r[5];
  r[0] = addr;
  r[1] = (uint8_t)(fc | 0x80U);
  r[2] = code;
  mbReply(p, r, appendCrc(r, 3));
}

/* 写寄存器：0=成功 1=成功(需保存+重启) 2=非法地址 3=非法数据 4=恢复出厂 */
static uint8_t regWrite(uint16_t reg, uint16_t v)
{
  if (reg == 0x0005U) {                          /* 从站地址 */
    if ((v < 1U) || (v > 247U)) {
      return 3U;
    }
    slaveAddr = v;
    return 1U;
  }

  if (reg == 0x0006U) {                          /* 点动脉冲 ms（立即执行，不重启） */
    if (v > 60000U) {
      return 3U;
    }
    relayPulse(v);
    return 0U;
  }

  if (reg == 0x0007U) {                          /* 波特率 */
    uint8_t idx = 0xFFU;
    if (v <= 7U) {
      idx = (uint8_t)v;
    } else {
      for (uint8_t i = 0; i < 8U; i++) {
        if (baudTable[i] == (uint32_t)v) {
          idx = i;
          break;
        }
      }
    }
    if (idx == 0xFFU) {
      return 3U;
    }
    baudIdx = idx;
    return 1U;
  }

  if (reg == 0x000DU) {                          /* 串口格式（校验位/停止位） */
    if (v > 3U) {
      return 3U;
    }
    frameFmt = (uint8_t)v;
    return 1U;
  }

  if (reg == 0x000BU) {                          /* 恢复出厂 */
    if (v != 0x55AAU) {
      return 3U;
    }
    return 4U;                                    /* 交给上层走恢复出厂流程 */
  }

  return 2U;                                      /* 非法地址 */
}

static void handleFrame(Port &p, const uint8_t *f, uint16_t n)
{
  if (n < 4U) {
    return;
  }
  uint16_t crcRx = (uint16_t)f[n - 2] | ((uint16_t)f[n - 1] << 8);
  if (mbCrc16(f, (uint16_t)(n - 2)) != crcRx) {
    return;                                       /* CRC 错：静默丢弃 */
  }

  uint8_t addr = f[0];
  if ((addr != (uint8_t)slaveAddr) && (addr != 0U)) {
    return;                                       /* 不是本机地址 */
  }
  bool broadcast = (addr == 0U);
  uint8_t fc = f[1];
  uint8_t r[8];

  if ((fc == 0x01U) || (fc == 0x02U)) {           /* 读继电器 / 读 DI */
    if (n < 6U) { mbException(p, addr, fc, 0x03); return; }
    uint16_t start = (uint16_t)(((uint16_t)f[2] << 8) | f[3]);
    uint16_t qty   = (uint16_t)(((uint16_t)f[4] << 8) | f[5]);
    if ((start != 0U) || (qty != 1U)) { mbException(p, addr, fc, 0x02); return; }
    bool v = (fc == 0x01U) ? relayOn : diValue();
    r[0] = addr; r[1] = fc; r[2] = 1U; r[3] = v ? 0x01U : 0x00U;
    if (!broadcast) { mbReply(p, r, appendCrc(r, 4)); }
    return;
  }

  if (fc == 0x05U) {                              /* 写单线圈：闭合 / 释放 */
    if (n != 8U) { mbException(p, addr, fc, 0x03); return; }
    uint16_t coil = (uint16_t)(((uint16_t)f[2] << 8) | f[3]);
    uint16_t val  = (uint16_t)(((uint16_t)f[4] << 8) | f[5]);
    if (coil != 0U) { mbException(p, addr, fc, 0x02); return; }
    if (val == 0xFF00U) {
      pulseActive = false;
      relaySet(true);
    } else if (val == 0x0000U) {
      pulseActive = false;
      relaySet(false);
    } else {
      mbException(p, addr, fc, 0x03);
      return;
    }
    if (!broadcast) { mbReply(p, f, 8U); }
    return;
  }

  if (fc == 0x06U) {                              /* 写单寄存器 */
    if (n != 8U) { mbException(p, addr, fc, 0x03); return; }
    uint16_t reg = (uint16_t)(((uint16_t)f[2] << 8) | f[3]);
    uint16_t val = (uint16_t)(((uint16_t)f[4] << 8) | f[5]);

    uint8_t res = regWrite(reg, val);
    if (res == 2U) { mbException(p, addr, fc, 0x02); return; }
    if (res == 3U) { mbException(p, addr, fc, 0x03); return; }

    if (!broadcast) { mbReply(p, f, 8U); }         /* 先把应答发出去 */
    if (res == 1U) {
      needRestart = true;                          /* 应答发完 -> 存 flash 并重启 */
    } else if (res == 4U) {
      needFactory = true;                          /* 应答发完 -> 恢复出厂(保存+重启) */
    }
    return;
  }

  mbException(p, addr, fc, 0x01);
}

/* ================= 端口初始化（纯轮询） ================= */

static void attachPolling(Port &p)
{
  p.inst->CR1 &= ~USART_CR1_RXNEIE;
  p.inst->CR3 &= ~USART_CR3_EIE;
  drainRx(p.inst);
  p.len = 0;
}

static void initPorts()
{
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    Port &p = ports[i];
    if (!p.enabled) {
      continue;
    }
    p.started = true;
    p.beginFn();
    attachPolling(p);
  }
}

/* ================= 收帧：轮询 + 帧间隔 + 帧内超时 ================= */

static void pollPort(Port &p)
{
  if (!p.enabled || !p.started) {
    return;
  }

  uint32_t now = millis();

  while (p.inst->SR & USART_SR_RXNE) {
    uint8_t c = (uint8_t)p.inst->DR;
    if (p.len == 0U) {
      p.frameStart = now;
    }
    p.lastRxMs = now;
    if (p.len < (uint16_t)(MB_BUF - 1U)) {
      p.buf[p.len++] = c;
    } else {
      p.len = 0;
    }
  }

  if (p.len > 0U) {
    if ((uint32_t)(now - p.lastRxMs) >= MB_GAP_MS) {
      uint16_t n = p.len;
      p.len = 0;
      handleFrame(p, p.buf, n);
    } else if ((uint32_t)(now - p.frameStart) > MB_FRAME_TIMEOUT_MS) {
      p.len = 0;
    }
  }
}

/* ================= BOOT0(PF8) 恢复出厂 ================= */

static void boot0CheckAtBoot()
{
  pinMode(BOOT0_PIN, INPUT);                     /* 板上已有下拉，不加内部上下拉 */
  if (digitalRead(BOOT0_PIN) != HIGH) {
    return;
  }
  uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < BOOT0_HOLD_MS) {
    if (digitalRead(BOOT0_PIN) != HIGH) {
      return;                                    /* 中途松开：不恢复 */
    }
  }
  factoryDefaults();                             /* 内部会保存 + 重启 */
}

static void boot0CheckRuntime()
{
  /* 只有"先低后高"才算真正被上拉，避免引脚一直高导致反复恢复出厂 */
  static bool     armed     = false;
  static uint32_t highSince = 0;

  if (digitalRead(BOOT0_PIN) == HIGH) {
    if (!armed) {
      highSince = 0U;
      return;
    }
    if (highSince == 0U) {
      highSince = millis();
      if (highSince == 0U) {
        highSince = 1U;
      }
    } else if ((uint32_t)(millis() - highSince) >= BOOT0_HOLD_MS) {
      highSince = 0U;
      armed     = false;
      factoryDefaults();                         /* 保存 + 闪 5 下 + 重启 */
    }
  } else {
    armed     = true;
    highSince = 0U;
  }
}

/* ================= setup / loop ================= */

void setup()
{
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, RS485_IDLE_LEVEL);

  pinMode(RELAY_PIN, OUTPUT);
  relayWrite(false);

  pinMode(IN_PIN, INPUT_PULLUP);

  copyRamFuncs();                                /* flash 编程函数搬到 RAM */
  cfgLoad();                                     /* 读回保存的参数（没写过用默认） */
  boot0CheckAtBoot();                            /* 上电被上拉 5 秒 -> 恢复出厂并重启 */

  modbusBaud = baudTable[baudIdx];
  initPorts();
  if (frameFmt != 0U) {                          /* 保存的串口格式（默认 8N1 不用动） */
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
      Port &p = ports[i];
      if (!p.enabled || !p.started) {
        continue;
      }
      USART_TypeDef *u = p.inst;
      u->CR1 &= ~USART_CR1_UE;
      uint32_t cr1 = u->CR1 & ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS);
      uint32_t cr2 = u->CR2 & ~USART_CR2_STOP;
      if (frameFmt == 1U) {
        cr1 |= (USART_CR1_M | USART_CR1_PCE);
      } else if (frameFmt == 2U) {
        cr1 |= (USART_CR1_M | USART_CR1_PCE | USART_CR1_PS);
      } else if (frameFmt == 3U) {
        cr2 |= USART_CR2_STOP_1;
      }
      u->CR1 = cr1;
      u->CR2 = cr2;
      u->CR1 |= USART_CR1_UE;
      drainRx(u);
      p.len = 0;
    }
  }

#if USE_WATCHDOG
  IWatchdog.begin(WATCHDOG_MS);
#endif
}

void loop()
{
  for (uint8_t i = 0; i < PORT_COUNT; i++) {
    pollPort(ports[i]);
  }

  if (needFactory) {                             /* 恢复出厂：保存 + 闪 5 下 + 重启 */
    needFactory = false;
    factoryDefaults();
  }

  if (needRestart) {                             /* 参数改过：存 flash 后重启 */
    needRestart = false;
    cfgSave();
    NVIC_SystemReset();
  }

  relayPulseTask();
  boot0CheckRuntime();

#if USE_WATCHDOG
  IWatchdog.reload();
#endif
}
