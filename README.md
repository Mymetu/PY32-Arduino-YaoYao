# PY32 Arduino 适配（YaoYao 修改版）v0.2.7

基于 [regimantas/Arduino-PY32](https://github.com/regimantas/Arduino-PY32)（PY32Duino **0.2.62**）修改而来，
**针对 PY32F040C1BT6（LQFP48，128 KB Flash / 16 KB SRAM）补齐并修正了多串口、I2C、SPI 的适配**，
可直接在 Arduino IDE 里当普通 Arduino 板使用。

- 目标芯片：**PY32F040C1BT6**
- 本仓库版本：**0.2.7**（上游 0.2.62 + 下列修改）
- 实测时钟：**HSI 24 MHz / HCLK 24 MHz**（内部 RC）
- 许可证：沿用上游 **BSD-3-Clause**（见 `LICENSE`）

---

## 1. 相对上游 0.2.62 改了什么

| # | 文件 | 修改内容 | 原因 |
|---|---|---|---|
| 1 | `variants/.../PeripheralPins.c` | **USART2 的 PA2/PA3 复用从 AF4 改为 AF1** | 上游值不对，`Serial2` 收发不到数据 |
| 2 | `variants/.../variant_generic.h` | 新增 `PIN_SERIAL3_TX/RX`(PB10/PB11)、`PIN_SERIAL4_TX/RX`(PA0/PA1) 与 `HAVE_HWSERIAL3/4` | 上游只有 1 个串口，硬件实际有 4 个 |
| 3 | `variants/.../PeripheralPins.c` | 补 USART3（AF4）、USART4（AF4）的 TX/RX 引脚映射 | 让 `Serial3`/`Serial4` 能正确分配引脚 |
| 4 | `libraries/SrcWrapper/src/air/uart.c` | 补 USART3/USART4 的 RCC 时钟/复位宏；增加 `UART3_IRQN_USE`/`UART4_IRQN_USE`；`uart_init` 增加 USART3/USART4 分支；新增共享中断 `USART3_4_IRQHandler`（F040 上 USART3/USART4 共用 `USART3_4_IRQn = 29`） | 否则 `Serial3/Serial4` 初始化直接返回、中断落到默认死循环 |
| 5 | `variants/.../variant_generic.cpp` | 新增 `__attribute__((constructor))`：**上电即打开 USART3/USART4 时钟并释放复位** | 实测 `Serial4.begin()` 对寄存器写入会全丢（`CR1` 始终为 0），根因是时钟未开 |
| 6 | `variants/.../PeripheralPins.c` | **I2C1 的 PB6/PB7 复用从 AF6 改为 AF1**；增加 I2C2（PB10/PB11）映射 | 上游值不对，Wire 无法工作 |
| 7 | `variants/.../PeripheralPins.c` + `variant_generic.h` | **SPI1 引脚改为 PB3(SCK)/PB4(MISO)/PB5(MOSI)/PA4(NSS)，AF0**；增加 SPI2（AF8，PA1 用 AF9） | 上游沿用 PY32F030 的 PA4/PA5/PA6/PA7，在 F040 上不成立 |
| 8 | `libraries/IWatchdog/*` | 加固 `begin()`/`set()`：无论 HAL 初始化是否超时都立即置位使能并喂狗；`library.properties` 的 `architectures` 改为 `PY32` | 原实现会留下"狗已开但没人喂"的状态，表现为刷完就无限复位 |
| 9 | `boards.txt` | 给 `GenF040` 打开 **`-DHAL_FLASH_MODULE_ENABLED`** | 让 sketch 能调用 `HAL_FLASH_*` 做参数掉电保存 |
| 10 | `variants/.../ldscript.ld` | 新增 **`.RamFunc` 段**（放 RAM、从 Flash 载入）+ `_siramfunc/_sramfunc/_eramfunc` 符号 | PY32 的整页 flash 编程代码必须在 RAM 中执行（厂商 HAL 已用 `__RAM_FUNC` 标记） |
| 11 | `platform.txt` | 版本号改为 **0.2.7**，平台名标注 YaoYao 修改版 | 便于区分上游 |

> 上游原始 README 保留在 `README-upstream.md`。

---

## 2. 已适配 / 可用的 Arduino 调用方法

以下均针对 **PY32F040C1BT6**，在 HSI 24 MHz 下验证或按代码路径可直接使用（✅ = 本仓库实测通过，⚠️ = 已适配但未逐项实测）。

### 2.1 数字 IO
| API | 说明 |
|---|---|
| `pinMode(pin, INPUT / OUTPUT / INPUT_PULLUP / INPUT_PULLDOWN)` | ✅ 全部 GPIO |
| `digitalWrite(pin, HIGH/LOW)` / `digitalRead(pin)` | ✅ |
| 引脚命名 | `PA0…PA15`、`PB0…PB15`、`PF0…PF9`、`PC13/PC14/PC15`；也支持数字序号与 `A0…A9`（模拟引脚） |
| `attachInterrupt(digitalPinToInterrupt(pin), isr, RISING/FALLING/CHANGE)` / `detachInterrupt()` | ⚠️ EXTI 通道 0-1 / 2-3 / 4-15 共享，多个引脚同时中断需注意 |

### 2.2 时间
| API | 说明 |
|---|---|
| `millis()` / `micros()` | ✅ 由 SysTick 提供 |
| `delay(ms)` / `delayMicroseconds(us)` | ✅ |

### 2.3 串口（4 路全可用）
| 对象 | 外设 | 默认引脚 | 状态 |
|---|---|---|---|
| `Serial` | USART1 | PA9(TX) / PA10(RX) | ✅ 收发可用 |
| `Serial2` | USART2 | PA2(TX) / PA3(RX) | ✅ 收发可用（本次修正 AF1） |
| `Serial3` | USART3 | PB10(TX) / PB11(RX) | ⚠️ 引脚/时钟已适配 |
| `Serial4` | USART4 | PA0(TX) / PA1(RX) | ✅ 收发可用（时钟在启动时已打开） |

可用方法：`begin(baud[, config])`、`end()`、`read()`、`readBytes()`、`available()`、`peek()`、`flush()`、`write()`、`print()/println()`、`setTimeout()` 等 `Stream`/`Print` 全套接口。
`config` 支持 `SERIAL_8N1`（默认）、`SERIAL_8E1`、`SERIAL_8O1`、`SERIAL_8N2` 等。

### 2.4 模拟
| API | 说明 |
|---|---|
| `analogRead(A0…A7)` | ✅ 对应 PA0…PA7（ADC IN0…IN7） |
| `analogRead(A8/A9)` | ⚠️ 对应 PB0/PB1（IN8/IN9），未逐项实测 |
| `analogWrite(pin, 0…255)` | ⚠️ 走 TIM 通道（可用的 PWM 引脚见 `PeripheralPins.c` 的 `PinMap_TIM`） |
| `analogReadResolution()` / `analogWriteResolution()` | ⚠️ 随内核提供 |

### 2.5 I2C（Wire）
| 对象 | 外设 | 引脚 | 状态 |
|---|---|---|---|
| `Wire` | I2C1 | SDA = PB7 / SCL = PB6 | ⚠️ 本次修正 AF1，代码路径可用 |
| 第二路 I2C2 | — | SDA = PB11 / SCL = PB10 | ⚠️ 引脚已映射 |

支持 `Wire.begin()`、`beginTransmission()/write()/endTransmission()`、`requestFrom()`、`read()`、`setClock()` 等标准接口。

### 2.6 SPI
| 对象 | 外设 | 引脚 | 状态 |
|---|---|---|---|
| `SPI` | SPI1 | SCK = PB3 / MISO = PB4 / MOSI = PB5 / SS = PA4 | ⚠️ 本次修正 |
| SPI2 | — | 见 `PinMap_SPI_*`（AF8） | ⚠️ 未实测 |

支持 `SPI.begin()`、`beginTransaction(SPISettings(...))`、`transfer()`、`endTransaction()`；默认对象不带硬件 NSS，建议用软件 CS。

### 2.7 其它外设与库
| 项 | 说明 |
|---|---|
| `IWatchdog`（本仓库自带） | ✅ `IWatchdog.begin(us)` / `begin()` / `reload()` / `set()` / `getTimeout()` / `isEnabled()` / `isReset(true)`；LSI 32768 Hz，超时约 125 µs ~ 32 s |
| `EEPROM`（上游自带库） | ⚠️ 可用（flash 模拟），未逐项实测 |
| `Servo` / `SoftwareSerial` / `EasyInterval` / `PY32LowPower` / `HalI2CSlave` | 上游自带，随内核一起提供 |
| `HAL_FLASH_*` | ✅ 已打开 `HAL_FLASH_MODULE_ENABLED`，可做参数掉电保存（配合 `.RamFunc`，厂商 HAL 已处理好 RAM 执行） |
| USB | ❌ F040 无 USB 外设 |

---

## 3. 已知问题（重要）

1. **`Serial.print()` 持续打印会卡死主循环**
   长时间/连续打印时，内核的 TX 环形缓冲 + HAL 发送中断会出现竞态，`write()` 永久等待（现象：串口输出打到一半停住、之后程序像死了一样）。
   **建议**：要么少量输出，要么像示例那样**直写 `USARTx->DR`**（示例 `YaoRelay_V2_1` 就是直写实现，稳定运行）。
2. **`Serial3/Serial4` 的中断收发不可靠**
   `uart_init` 里 USART3/USART4 分支未能可靠写入 `obj->index/irq`，会连带影响 `uart_handlers[0]`（USART1 的句柄槽）。
   **建议**：这两个口用**主循环轮询**（示例即用轮询，四路都稳定）；纯 Arduino 风格的 `Serial4.read()` 在示例场景下工作正常，但不要依赖其接收中断。
3. **改内核后必须删 IDE 缓存**：`C:\Users\<你>\AppData\Local\arduino`，否则旧对象文件会让修改不生效。

---

## 4. 安装这个开发板包

### 方式 A：覆盖式（推荐，最省事）
1. Arduino IDE → 开发板管理器 → 搜索 **PY32Duino** → 安装 **0.2.62**（它会把编译器和上传工具一起装好）；
2. 关闭 Arduino IDE；
3. 把 **本仓库根目录的全部内容**覆盖到安装目录：
   - Windows 默认：`C:\Users\<你的用户名>\AppData\Local\Arduino15\packages\PY32Duino\hardware\PY32\0.2.62\`
   - 若你在 IDE 里改过"数据目录"，则对应 `<你的数据目录>\packages\PY32Duino\hardware\PY32\0.2.62\`
4. 删除编译缓存 `C:\Users\<你的用户名>\AppData\Local\arduino`；
5. 重新打开 Arduino IDE。

### 方式 B：独立安装（两个版本可共存）
1. 同样先安装官方 **PY32Duino 0.2.62**（只需要它的编译器/上传工具）；
2. 把本仓库放到**项目文件夹下的 hardware 目录**，目录层级必须是：
   ```
   <Arduino 项目文件夹>\hardware\Mymetu\PY32\<本仓库文件…>
   ```
   （`<Arduino 项目文件夹>` 默认是 `C:\Users\<你的用户名>\Documents\Arduino`）
3. 重启 IDE，开发板菜单会出现 **Mymetu PY32** 系列；
   此时 FQBN 变成 `Mymetu:PY32:GenF040`（方式 A 则是 `PY32Duino:PY32:GenF040`）。

### 安装后必须选择的选项（Tools 菜单）
| 菜单项 | 选择 |
|---|---|
| 开发板 | **Generic PY32F040 Series** |
| 芯片型号 (pnum) | **PY32F040C1BT6** |
| 时钟 (ClockSourceAndFrequency) | **HSI 24 MHz, HCLK 24 MHz** |
| 上传方式 (upload_method) | **AirISP (serial)** |
| 上传速率 (UploadSpeed) | 115200 |
| 供电电压 (SupplyVoltage) | 3.3V |
| 低速时钟 (LowSpeedClockSource) | LSI 32.768 kHz |
| BOR / BORWrite | Unchanged / Do not write option bytes |

---

## 5. 示例：`examples/YaoRelay_V2_1`

一个完整的三接口（TTL / RS485 / RS232）**Modbus RTU 从站继电器**示例固件，可直接编译烧录。

### 5.1 硬件与 GPIO 定义

| 功能 | 引脚 | 说明 |
|---|---|---|
| 继电器输出 | **PA11** | 输出；默认**低电平 = 释放**（`RELAY_ACTIVE_LOW 0` 时高电平 = 吸合） |
| 干接点输入 DI | **PA12** | **上拉输入**；短接 GND = 低电平 = **闭合（读 1）**；悬空 = 高电平 = **断开（读 0）** |
| TTL 串口 | **PA9 = TX，PA10 = RX** | `Serial`（USART1） |
| RS485 串口 | **PA2 = TX，PA3 = RX**，方向 **PA4**（高 = 发送） | `Serial2`（USART2）；建议 PA4 加 10 kΩ 下拉 |
| RS232 串口 | **PA0 = TX，PA1 = RX** | `Serial4`（USART4）；PA0/PA1 是 TTL 电平，接 PC 需 MAX3232 等电平转换 |
| 恢复出厂 | **PF8（BOOT0）** | 上拉保持 **5 秒** → 恢复出厂参数（ID1 / 9600 / 8N1），继电器闪 5 下后重启 |
| 时钟 | 内部 HSI 24 MHz | 无外置晶振 |

> 三个串口是**同一个 Modbus RTU 从站**（同一套参数、同一个继电器）。出厂默认：**地址 1 / 9600 / 8N1**。
> 参数（地址、波特率、校验位）掉电保存在 flash 最后一页，**写入后设备会自动重启**。

### 5.2 主要功能
- 01 读线圈（继电器状态）、02 读离散输入（DI 状态）
- 05 写单线圈：`FF00` 闭合（锁定）/ `0000` 释放
- 06 写单寄存器：`0x0006` 点动脉冲毫秒、`0x0005` 从站地址、`0x0007` 波特率（索引 0~7）、`0x000D` 校验位格式、`0x000B` 恢复出厂
- 硬件看门狗 2 秒、开机无任何串口输出
- 全部寄存器与命令示例见 sketch 文件头注释

### 5.3 烧录（串口 ISP / AirISP）
接线：USB-TTL 的 **RX → PA9**、**TX → PA10**、**GND 共地**、3.3 V 供电；Arduino 里选好上面的选项和 COM 口。

**进入刷机模式的标准操作顺序（务必按顺序）：**

1. **按住 RST 不撒手**
2. **按住 BOOT 不撒手**
3. **松开 RST**
4. **再松开 BOOT**

此时芯片已停留在 ISP 模式，点 Arduino 的"上传"即可（上传完成后会自动复位运行）。

> 如果上传报错，重复上面四步再点一次上传；也可把 UploadSpeed 降到 57600 再试。

---

## 6. 致谢与许可

- 上游项目：[regimantas/Arduino-PY32](https://github.com/regimantas/Arduino-PY32)（PY32Duino 0.2.62，BSD-3-Clause）
- 本仓库的修改：多串口 / I2C / SPI 适配修正、IWatchdog 加固、flash 参数保存支持
- 芯片资料：PY32F040 系列数据手册（引脚复用表为准）

许可：**BSD-3-Clause**，详见 `LICENSE`。
