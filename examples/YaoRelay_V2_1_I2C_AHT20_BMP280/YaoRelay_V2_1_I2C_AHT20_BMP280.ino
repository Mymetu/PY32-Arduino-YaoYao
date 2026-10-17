/* ============================================================================
 *  Yao-Relay V2.1   I2C 测试程序 (AHT20 + BMP280)
 *  芯片 : PY32F040C1BT6  @ Py32duino(YaoYao 0.2.7)  HSI 24MHz
 *  传感器: AHT20  (0x38) 温湿度  +  BMP280 (0x76/0x77) 温度 / 气压
 *
 *  接线 I2C1 :  PB6 = SCL      PB7 = SDA
 *               (板上没引出时改下面 SCL_PIN/SDA_PIN 为 PB8 / PB9)
 *  接线 串口 :  PA9 = TX -> USB-TTL 的 RX
 *               PA10= RX    (本程序不收)
 *               GND 共地,  115200 8N1
 *
 *  说明:
 *   1) 输出不用 Serial.print(), 改成直写 USART1 寄存器 —— 本项目内核的
 *      Serial.print 持续大量打印会卡死主循环。
 *   2) Wire.begin(a,b) 在本内核会走错重载, 所以先 setSDA/setSCL 再 begin()。
 *   3) AHT20 每次读取都会触发一次新测量(内部延时 80ms), 周期 >=200ms 才稳。
 *   4) BMP280 校准区(0x88..0x9F)是小端, 测量区(0xF7..)是大端, 字节序搞反全错。
 * ==========================================================================*/

#include <Wire.h>

/* ================= 用户可改区 ================= */
#define SDA_PIN        PB7          /* I2C1 SDA  备选 PB9 */
#define SCL_PIN        PB6          /* I2C1 SCL  备选 PB8 */

#define AHT20_ADDR     0x38
#define BMP_ADDR_A     0x76         /* SDO 接 GND */
#define BMP_ADDR_B     0x77         /* SDO 接 VCC */

#define UART_BAUD      115200
#define PERIOD_MS      1000         /* 打印周期, 不要小于 200 */

#define RELAY_PIN      PA11
#define BLINK_RELAY    0            /* 板子没 LED, 改 1 可用继电器做心跳(会响) */
/* ============================================= */

#define DBG_UART       USART1

/* ---------------------------------------------------------------------------
 * 1. 极简串口输出: 直写 USART1->DR, 轮询 TXE, 完全绕开内核 Serial 通道
 * -------------------------------------------------------------------------*/
static void upChar(char c)
{
  uint32_t t0 = millis();
  while ((DBG_UART->SR & USART_SR_TXE) == 0U) {
    if ((uint32_t)(millis() - t0) > 5U) { return; }   /* 5ms 超时, 不卡死 */
  }
  DBG_UART->DR = (uint8_t)c;
}

static void upStr(const char *s)
{
  while (*s != '\0') { upChar(*s++); }
}

static void upInt(int32_t v)
{
  char     b[12];
  uint8_t  i = 0;
  uint32_t u;

  if (v < 0) { upChar('-'); u = (uint32_t)(-(int64_t)v); }
  else       { u = (uint32_t)v; }

  if (u == 0U) { upChar('0'); return; }
  while ((u > 0U) && (i < 11U)) { b[i++] = (char)('0' + (u % 10U)); u /= 10U; }
  while (i > 0U) { upChar(b[--i]); }
}

/* v 以 10^dec 为单位: v=2531, dec=2 -> 25.31 */
static void upFixed(int32_t v, uint8_t dec)
{
  int32_t scale = 1;
  for (uint8_t i = 0; i < dec; i++) { scale *= 10; }
  if (v < 0) { upChar('-'); v = -v; }
  upInt(v / scale);
  if (dec == 0U) { return; }
  upChar('.');
  int32_t frac = v % scale;
  int32_t d    = scale / 10;
  while (d > 0) { upChar((char)('0' + (frac / d) % 10)); d /= 10; }
}

static void upFixedPad(int32_t v, uint8_t dec, uint8_t w)
{
  int32_t  a     = (v < 0) ? -v : v;
  int32_t  scale = 1;
  for (uint8_t i = 0; i < dec; i++) { scale *= 10; }
  int32_t  ip    = a / scale;
  uint8_t  n     = (v < 0) ? 1U : 0U;

  n++;
  while (ip >= 10) { ip /= 10; n++; }
  if (dec > 0U) { n += (uint8_t)(1U + dec); }
  while (n < w) { upChar(' '); n++; }
  upFixed(v, dec);
}

static void upHex8(uint8_t v)
{
  const char *h = "0123456789ABCDEF";
  upChar(h[(v >> 4) & 0x0FU]);
  upChar(h[v & 0x0FU]);
}

/* ---------------------------------------------------------------------------
 * 2. I2C 基本读写
 * -------------------------------------------------------------------------*/
static bool i2cWrite(uint8_t addr, const uint8_t *d, uint8_t n)
{
  Wire.beginTransmission(addr);
  for (uint8_t i = 0; i < n; i++) { Wire.write(d[i]); }
  return (Wire.endTransmission() == 0U);        /* 0 = ACK */
}

static bool i2cRead(uint8_t addr, uint8_t *d, uint8_t n)
{
  uint8_t got = Wire.requestFrom(addr, n);
  if (got != n) { return false; }
  for (uint8_t i = 0; i < n; i++) { d[i] = (uint8_t)Wire.read(); }
  return true;
}

static bool i2cWriteReg(uint8_t addr, uint8_t reg, const uint8_t *d, uint8_t n)
{
  Wire.beginTransmission(addr);
  Wire.write(reg);
  for (uint8_t i = 0; i < n; i++) { Wire.write(d[i]); }
  return (Wire.endTransmission() == 0U);
}

static bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t n)
{
  if (!i2cWrite(addr, &reg, 1U)) { return false; }
  return i2cRead(addr, d, n);
}

static uint8_t i2cScan(void)
{
  uint8_t found = 0;

  upStr("I2C scan (0x08-0x77):");
  for (uint8_t a = 0x08U; a <= 0x77U; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0U) {       /* 空写 = 探测 */
      upStr(" 0x"); upHex8(a);
      found++;
    }
  }
  if (found == 0U) { upStr("  <no device>"); }
  upStr("\r\n");
  return found;
}

/* ---------------------------------------------------------------------------
 * 3. AHT20  0x38   温湿度
 * -------------------------------------------------------------------------*/
static bool    ahtOk     = false;
static uint8_t ahtStatus = 0;

static uint8_t ahtCrc8(const uint8_t *d, uint8_t n)
{
  uint8_t crc = 0xFFU;
  for (uint8_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (uint8_t b = 0; b < 8U; b++) {
      if (crc & 0x80U) { crc = (uint8_t)((crc << 1) ^ 0x31U); }
      else             { crc = (uint8_t)(crc << 1); }
    }
  }
  return crc;
}

static bool ahtBegin(void)
{
  uint8_t st = 0;

  delay(40);                                  /* 上电 >=40ms 才能访问 */
  if (!i2cReadReg(AHT20_ADDR, 0x71U, &st, 1U)) { return false; }

  if ((st & 0x08U) == 0U) {                   /* bit3=0 -> 未校准, 发初始化 */
    const uint8_t cal[3] = { 0xBE, 0x08, 0x00 };
    if (!i2cWrite(AHT20_ADDR, cal, 3U)) { return false; }
    delay(20);
  }
  ahtStatus = st;
  return true;
}

/* t100 = 0.01'C,  rh100 = 0.01 %RH
 * 温度: T('C)  = raw / 2^20 * 200 - 50   -> 0.01'C 需 raw * 20000 / 2^20 - 5000
 * 湿度: RH(%) = raw / 2^20 * 100         -> 0.01%  需 raw * 10000 / 2^20        */
static bool ahtRead(int32_t *t100, int32_t *rh100)
{
  const uint8_t trig[3] = { 0xAC, 0x33, 0x00 };
  uint8_t       d[7];
  uint32_t      rh, raw;

  if (!i2cWrite(AHT20_ADDR, trig, 3U)) { return false; }
  delay(80);                                  /* 测量时间 */

  if (!i2cRead(AHT20_ADDR, d, 7U))     { return false; }
  ahtStatus = d[0];
  if (d[0] & 0x80U)            { return false; }   /* bit7 = busy */
  if (ahtCrc8(d, 6U) != d[6])  { return false; }   /* CRC 校验 */

  rh  = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | ((uint32_t)d[3] >> 4);
  raw = (((uint32_t)d[3] & 0x0FU) << 16) | ((uint32_t)d[4] << 8) | (uint32_t)d[5];

  *rh100 = (int32_t)(((int64_t)rh  * 10000) / 1048576);          /* 100% -> 10000 */
  *t100  = (int32_t)(((int64_t)raw * 20000) / 1048576) - 5000;   /* -50'C 偏移 */
  return true;
}

/* ---------------------------------------------------------------------------
 * 4. BMP280  0x76 / 0x77   温度 + 气压
 * -------------------------------------------------------------------------*/
static uint8_t  bmpAddr = 0;
static uint8_t  bmpId   = 0;
static int32_t  bmpAdcT = 0, bmpAdcP = 0;
static uint16_t digT1, digP1;
static int16_t  digT2, digT3, digP2, digP3, digP4, digP5, digP6, digP7, digP8, digP9;

/* BMP280 一个芯片里两种字节序:
 *   校准区 0x88..0x9F (dig_T1..dig_T3, dig_P1..dig_P9) : 低字节在前 = 小端
 *   测量区 0xF7..0xFC (adc_P / adc_T)                  : 高字节在前 = 大端
 * 搞反了温度会差十几度, 气压能差二十多倍。 */
static uint16_t u16le(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[1] << 8) | (uint16_t)p[0]);
}
static int16_t s16le(const uint8_t *p)
{
  return (int16_t)u16le(p);
}

static void bmpProbe(void)
{
  const uint8_t a[2] = { BMP_ADDR_A, BMP_ADDR_B };
  bool any = false;

  for (uint8_t i = 0; i < 2U; i++) {
    uint8_t id = 0;
    if (i2cReadReg(a[i], 0xD0U, &id, 1U)) {
      any = true;
      upStr("  probe 0x"); upHex8(a[i]);
      upStr(" -> chipID=0x"); upHex8(id);
      if (id == 0x58U)      { upStr(" (BMP280)"); }
      else if (id == 0x60U) { upStr(" (BME280)"); }
      else                  { upStr(" (不是 BMP280)"); }
      upStr("\r\n");
    }
  }
  if (!any) { upStr("  0x76/0x77 都无应答\r\n"); }
}

static bool bmpInit(uint8_t addr)
{
  uint8_t id = 0;
  uint8_t c[24];

  if (!i2cReadReg(addr, 0xD0U, &id, 1U)) { return false; }
  if ((id != 0x58U) && (id != 0x60U))    { return false; }

  /* 校准数据 0x88..0x9F 共 24 字节，BMP280 支持地址自增，一次读完即可。 */
  if (!i2cReadReg(addr, 0x88U, c, 24U)) { return false; }

  digT1 = u16le(&c[0]);
  digT2 = s16le(&c[2]);
  digT3 = s16le(&c[4]);
  digP1 = u16le(&c[6]);
  digP2 = s16le(&c[8]);
  digP3 = s16le(&c[10]);
  digP4 = s16le(&c[12]);
  digP5 = s16le(&c[14]);
  digP6 = s16le(&c[16]);
  digP7 = s16le(&c[18]);
  digP8 = s16le(&c[20]);
  digP9 = s16le(&c[22]);

  /* 校准值明显不合理只告警，不中断（方便定位接线/读数据问题） */
  if ((digT1 < 10000U) || (digT1 > 45000U) ||
      (digP1 < 10000U) || (digP1 > 45000U)) {
    upStr("  [!] 校准数据可疑: 检查 SDA/SCL 接线、上拉、供电\r\n");
  }
  upStr("  cal T1="); upInt((int32_t)digT1);
  upStr(" T2=");      upInt((int32_t)digT2);
  upStr(" T3=");      upInt((int32_t)digT3);
  upStr(" P1=");      upInt((int32_t)digP1);
  upStr("\r\n      P2="); upInt((int32_t)digP2);
  upStr(" P3=");      upInt((int32_t)digP3);
  upStr(" P4=");      upInt((int32_t)digP4);
  upStr(" P5=");      upInt((int32_t)digP5);
  upStr(" P6=");      upInt((int32_t)digP6);
  upStr(" P7=");      upInt((int32_t)digP7);
  upStr(" P8=");      upInt((int32_t)digP8);
  upStr(" P9=");      upInt((int32_t)digP9);
  upStr("\r\n");

  uint8_t cfg = 0x00U;      /* t_sb = 0.5ms, IIR 滤波关闭 */
  uint8_t ctl = 0x27U;      /* osrs_t = x1, osrs_p = x1, mode = normal */
  if (!i2cWriteReg(addr, 0xF5U, &cfg, 1U)) { return false; }
  if (!i2cWriteReg(addr, 0xF4U, &ctl, 1U)) { return false; }

  bmpAddr = addr;
  bmpId   = id;
  delay(10);
  return true;
}

/* t100 = 0.01'C,  p001hpa: 直接就是 Pa（见下方换算注释） */
static bool bmpRead(int32_t *t100, int32_t *p001hpa)
{
  uint8_t d[6];
  int32_t adcP, adcT, v1, v2, dt, tFine;
  int64_t p1, p2, p, q1, q2;

  if (bmpAddr == 0U) { return false; }

  if (!i2cReadReg(bmpAddr, 0xF7U, d, 6U)) { return false; }

  adcP = (int32_t)((((uint32_t)d[0] << 16) | ((uint32_t)d[1] << 8) | (uint32_t)d[2]) >> 4);
  adcT = (int32_t)((((uint32_t)d[3] << 16) | ((uint32_t)d[4] << 8) | (uint32_t)d[5]) >> 4);
  bmpAdcP = adcP;
  bmpAdcT = adcT;

  /* ---- 温度补偿 (数据手册 3.11.3) ---- */
  v1    = ((((adcT >> 3) - ((int32_t)digT1 << 1))) * (int32_t)digT2) >> 11;
  dt    = (adcT >> 4) - (int32_t)digT1;
  v2    = (((dt * dt) >> 12) * (int32_t)digT3) >> 14;
  tFine = v1 + v2;

  *t100 = (tFine * 5 + 128) >> 8;                   /* 0.01'C */

  /* ---- 气压补偿 (64 位, 手册给的整数算法) ---- */
  p1  = (int64_t)tFine - 128000;
  p2  = p1 * p1 * (int64_t)digP6;
  p2 += (p1 * (int64_t)digP5) * 131072LL;           /* << 17 */
  p2 += (int64_t)digP4 * 34359738368LL;             /* << 35 */
  p1  = ((p1 * p1 * (int64_t)digP3) >> 8) + ((p1 * (int64_t)digP2) * 4096LL);  /* << 12 */
  p1  = ((((int64_t)1 << 47) + p1) * (int64_t)digP1) >> 33;
  if (p1 == 0) { *p001hpa = 0; return false; }

  p   = 1048576 - adcP;
  p   = (((p << 31) - p2) * 3125) / p1;
  q1  = ((int64_t)digP9 * (p >> 13) * (p >> 13)) >> 25;
  q2  = ((int64_t)digP8 * p) >> 19;
  p   = ((p + q1 + q2) >> 8) + ((int64_t)digP7 * 16LL);        /* << 4 */

  /* p 是 Q24.8 的 Pa，即 p/256 = Pa，也就是 0.01 hPa 的数值
     -> 按 2 位小数打印 = hPa；原样打印 = Pa */
  *p001hpa = (int32_t)(p / 256);
  return true;
}

/* ---------------------------------------------------------------------------
 * 5. setup / loop
 * -------------------------------------------------------------------------*/
void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);              /* 继电器释放 */

  /* 让内核把 PA9/PA10 复用成 USART1 并配好波特率 */
  Serial.begin(UART_BAUD);
  delay(5);

  /* 之后 TX 全部直写寄存器, 关掉内核的中断收发 */
  DBG_UART->CR1 &= ~(USART_CR1_RXNEIE);
  DBG_UART->CR3 &= ~(USART_CR3_EIE);
  while (DBG_UART->SR & USART_SR_RXNE) { (void)DBG_UART->DR; }

  upStr("\r\n\r\n");
  upStr("==========================================\r\n");
  upStr(" Yao-Relay V2.1  I2C TEST\r\n");
  upStr(" AHT20 (0x38) + BMP280 (0x76/0x77)\r\n");
  upStr(" PY32F040C1BT6   HSI 24MHz\r\n");
  upStr(" I2C1 : SCL=PB6   SDA=PB7\r\n");
  upStr(" UART : PA9(TX)  ");
  upInt(UART_BAUD);
  upStr(" 8N1\r\n");
  upStr("==========================================\r\n");

  Wire.setSDA(SDA_PIN);                      /* 必须先设脚, 再 begin() */
  Wire.setSCL(SCL_PIN);
  Wire.begin();                              /* 100kHz, 主机模式 */

  delay(5);
  i2cScan();

  /* ---- AHT20 ---- */
  if (ahtBegin()) {
    ahtOk = true;
    upStr("AHT20 : init OK   status=0x"); upHex8(ahtStatus); upStr("\r\n");
  } else {
    ahtOk = false;
    upStr("AHT20 : 无应答 / 初始化失败 (查 0x38)\r\n");
  }

  /* ---- BMP280 ---- */
  bmpProbe();
  if (!bmpInit(BMP_ADDR_A)) { (void)bmpInit(BMP_ADDR_B); }
  if (bmpAddr != 0U) {
    upStr("BMP280: init OK   addr=0x"); upHex8(bmpAddr);
    upStr("   chipID=0x");             upHex8(bmpId);
    upStr("\r\n");
  } else {
    upStr("BMP280: 未找到 (CSB 要接 VCC, SDO 决定 0x76/0x77)\r\n");
  }
  upStr("------------------------------------------\r\n");
}

void loop()
{
  static uint32_t nextMs = 0;
  static uint32_t cnt    = 0;
  int32_t aT = 0, aH = 0;
  int32_t bT = 0, bP = 0;

  if ((int32_t)(millis() - nextMs) < 0) { return; }
  nextMs = millis() + PERIOD_MS;
  cnt++;

  upStr("\r\n#");
  upInt((int32_t)cnt);
  upStr("  t=");
  upInt((int32_t)millis());
  upStr("ms\r\n");

  /* ================= AHT20 ================= */
  if (!ahtOk) { ahtOk = ahtBegin(); }

  if (ahtOk && ahtRead(&aT, &aH)) {
    upStr("  AHT20 : T=");
    upFixedPad(aT, 2, 6);
    upStr("C   RH=");
    upFixedPad(aH, 2, 6);
    upStr("%   (st=0x"); upHex8(ahtStatus); upStr(")\r\n");
  } else {
    upStr("  AHT20 : 读取失败\r\n");
    ahtOk = false;
  }

  /* ================= BMP280 ================= */
  if (bmpAddr == 0U) {
    if (!bmpInit(BMP_ADDR_A)) { (void)bmpInit(BMP_ADDR_B); }
  }

  if ((bmpAddr != 0U) && bmpRead(&bT, &bP)) {
    upStr("  BMP280: T=");
    upFixedPad(bT, 2, 6);
    upStr("C   P=");
    upFixedPad(bP, 2, 9);
    upStr(" hPa (");
    upInt(bP);
    upStr(" Pa)\r\n");
    upStr("          raw T=");
    upInt(bmpAdcT);
    upStr(" P=");
    upInt(bmpAdcP);
    upStr("\r\n");
  } else if (bmpAddr != 0U) {
    upStr("  BMP280: 读取失败\r\n");
    bmpAddr = 0;
  }

#if BLINK_RELAY
  digitalWrite(RELAY_PIN, HIGH);
  delay(30);
  digitalWrite(RELAY_PIN, LOW);
#endif
}
