# PY32 Arduino 适配（YaoYao 修改版）v0.2.7 &nbsp;·&nbsp; 附 Yao-Relay V2.1 三接口继电器示例

基于 [regimantas/Arduino-PY32](https://github.com/regimantas/Arduino-PY32)（PY32Duino **0.2.62**）修改，**补齐并修正 PY32F040C1BT6 的多串口 / I2C / SPI 适配**，可直接在 Arduino IDE 里当普通 Arduino 板使用；同时附带一个完整可用的 **三接口（TTL / RS485 / RS232）Modbus RTU 继电器**示例工程。

- 目标芯片：**PY32F040C1BT6**（LQFP48，128 KB Flash / 16 KB SRAM）
- 本仓库版本：**0.2.7**（上游 0.2.62 + 下列修改）
- 实测时钟：**HSI 24 MHz / HCLK 24 MHz**
- 许可证：沿用上游 **BSD-3-Clause**（见 `LICENSE`）

---

## 1. 相对上游 0.2.62 改了什么

| # | 文件 | 修改内容 | 原因 |
|---|---|---|---|
| 1 | `variants/.../PeripheralPins.c` | **USART2 的 PA2/PA3 复用从 AF4 改为 AF1** | 上游值不对，`Serial2` 收发不到数据 |
| 2 | `variants/.../variant_generic.h` | 新增 `PIN_SERIAL3_TX/RX`(PB10/PB11)、`PIN_SERIAL4_TX/RX`(PA0/PA1) 与 `HAVE_HWSERIAL3/4` | 上游只映射了 1 个串口，芯片实际有 4 个 |
| 3 | `variants/.../PeripheralPins.c` | 补 USART3（AF4）、USART4（AF4）的 TX/RX 引脚映射 | 让 `Serial3` / `Serial4` 能正确分配引脚 |
| 4 | `libraries/SrcWrapper/src/air/uart.c` | 补 USART3/USART4 的 RCC 时钟/复位宏；增加 `UART3_IRQN_USE`/`UART4_IRQN_USE`；`uart_init` 增 USART3/USART4 分支；新增共享中断 `USART3_4_IRQHandler`（F040 上 USART3/USART4 共用 `USART3_4_IRQn=29`） | 否则 `Serial3/Serial4` 初始化直接返回 |
| 5 | `variants/.../variant_generic.cpp` | 增加 `__attribute__((constructor))`：**上电即打开 USART3/USART4 时钟并释放复位** | 实测 `Serial4.begin()` 对寄存器写入会全部丢失（`CR1` 恒为 0），根因是时钟未开 |
| 6 | `variants/.../PeripheralPins.c` | **I2C1 的 PB6/PB7 复用从 AF6 改为 AF1**；补 I2C2（PB10/PB11） | 上游值不对，Wire 无法工作 |
| 7 | `variants/.../PeripheralPins.c` + `variant_generic.h` | **SPI1 改为 PB3(SCK)/PB4(MISO)/PB5(MOSI)/PA4(NSS)，AF0**；补 SPI2 | 上游沿用 PY32F030 的 PA4/PA5/PA6/PA7，在 F040 上不成立 |
| 8 | `libraries/IWatchdog/*` | 加固 `begin()`：无论 HAL 初始化是否超时都立即置位使能并喂狗；`architectures` 改为 `PY32` | 原实现会留下"狗已开但没人喂"的状态，表现为刷完无限复位 |
| 9 | `boards.txt` | `GenF040` 打开 **`-DHAL_FLASH_MODULE_ENABLED`** | 让 sketch 能调用 `HAL_FLASH_*` 做参数掉电保存 |
| 10 | `boards.txt` | **补上 F040 段缺失的菜单显示名** `menu.ClockSourceAndFrequency.HSI4M_HCLK4M` | 缺这一行时 Arduino IDE **不显示「时钟频率」菜单** |
| 11 | `platform.txt` | 增加 12 条**菜单中文标题**（芯片型号 / 时钟频率 / 上传方式 / 上传速率 / 串口 / 供电电压 / 低速时钟 / BOR / 写入 BOR / 编译优化 / 调试 / 启动引脚），版本号 `0.2.7` | 让菜单稳定出现且可读 |
| 12 | `variants/.../ldscript.ld` | 新增 **`.RamFunc` 段**（放 RAM、从 Flash 载入）+ `_siramfunc/_sramfunc/_eramfunc` | PY32 整页 flash 编程代码必须在 RAM 执行 |

> 上游原始 README 保留在 `README-upstream.md`。

---

## 2. 已适配 / 可用的 Arduino 调用方法

（针对 **PY32F040C1BT6**，HSI 24 MHz。✅ = 本仓库实测通过，⚠️ = 已适配未逐项实测）

### 2.1 数字 IO
| API | 说明 |
|---|---|
| `pinMode(pin, INPUT / OUTPUT / INPUT_PULLUP / INPUT_PULLDOWN)` | ✅ 全部 GPIO |
| `digitalWrite()` / `digitalRead()` | ✅ |
| 引脚命名 | `PA0…PA15`、`PB0…PB15`、`PF0…PF9`、`PC13/PC14/PC15`；也支持 `A0…A9`（模拟引脚） |
| `attachInterrupt(digitalPinToInterrupt(pin), isr, RISING/FALLING/CHANGE)` / `detachInterrupt()` | ⚠️ EXTI 0-1 / 2-3 / 4-15 共享，多引脚同时中断需注意 |
| `tone()` / `noTone()` | ⚠️ 随内核提供（Tone.cpp） |

### 2.2 时间
`millis()` / `micros()` ✅ &nbsp;·&nbsp; `delay(ms)` / `delayMicroseconds(us)` ✅

### 2.3 串口（4 路全可用）
| 对象 | 外设 | 默认引脚 | 状态 |
|---|---|---|---|
| `Serial` | USART1 | PA9(TX) / PA10(RX) | ✅ |
| `Serial2` | USART2 | PA2(TX) / PA3(RX) | ✅（本次修正 AF1） |
| `Serial3` | USART3 | PB10(TX) / PB11(RX) | ⚠️ 引脚/时钟已适配 |
| `Serial4` | USART4 | PA0(TX) / PA1(RX) | ✅（启动时已开时钟） |

方法：`begin(baud[, config])`、`end()`、`read()`、`readBytes()`、`available()`、`peek()`、`flush()`、`write()`、`print()/println()` 等；`config` 支持 `SERIAL_8N1`（默认）/`SERIAL_8E1`/`SERIAL_8O1`/`SERIAL_8N2`。

### 2.4 模拟
| API | 说明 |
|---|---|
| `analogRead(A0…A7)` | ✅ 对应 PA0…PA7 |
| `analogRead(A8/A9)` | ⚠️ 对应 PB0/PB1 |
| `analogWrite(pin, 0…255)` | ⚠️ 走 TIM 通道（可用引脚见 `PeripheralPins.c` 的 `PinMap_TIM`） |
| `analogReadResolution()` / `analogWriteResolution()` | ⚠️ 随内核提供 |

### 2.5 I2C（Wire）
| 对象 | 外设 | 引脚 | 状态 |
|---|---|---|---|
| `Wire` | I2C1 | SDA = PB7 / SCL = PB6 | ⚠️ 本次修正 AF1 |
| 第二路 | I2C2 | SDA = PB11 / SCL = PB10 | ⚠️ 已映射 |

`Wire.begin()`、`beginTransmission()/write()/endTransmission()`、`requestFrom()`、`read()`、`setClock()` 等标准接口。

### 2.6 SPI
| 对象 | 外设 | 引脚 | 状态 |
|---|---|---|---|
| `SPI` | SPI1 | SCK = PB3 / MISO = PB4 / MOSI = PB5 / SS = PA4 | ⚠️ 本次修正 |
| SPI2 | — | 见 `PinMap_SPI_*`（AF8） | ⚠️ 未实测 |

`SPI.begin()`、`beginTransaction(SPISettings(...))`、`transfer()`、`endTransaction()`；默认对象无硬件 NSS，建议软件 CS。

### 2.7 其它
| 项 | 说明 |
|---|---|
| `IWatchdog`（本仓库） | ✅ `begin(us)`/`begin()`、`reload()`、`set()`、`getTimeout()`、`isEnabled()`、`isReset(true)`；LSI 32768 Hz，约 125 µs ~ 32 s |
| `EEPROM`（上游库） | ⚠️ flash 模拟 |
| `Servo` / `SoftwareSerial` / `EasyInterval` / `PY32LowPower` / `HalI2CSlave` | 随内核提供 |
| `HAL_FLASH_*` | ✅ 已打开 `HAL_FLASH_MODULE_ENABLED`，可做参数掉电保存 |
| USB | ❌ F040 无 USB 外设 |

---

## 3. 安装板卡（**两种办法，按你的网络选一个**）

### 3.1 国内加速安装（推荐国内用户 · 全部资源在国内服务器，下载快）

> 服务器已托管：板卡包 + 编译器 + CMSIS + AirISP + Flasher，**全部从国内服务器下载**，不依赖 GitHub。

**第 1 步** 填索引地址
```
Arduino IDE → 文件 → 首选项 → 附加开发板管理器网址，加入：
https://root.yaoyao.ink/Yao-Relay_V2_1/package_mymetu_index.json
```
**第 2 步** 安装
```
工具 → 开发板 → 开发板管理器 → 搜索 "PY32"
→ 选 "PY32 (YaoYao)" 0.2.7 → 安装
（会自动下载：板卡包 4.2 MB + 编译器 292 MB + Flasher 38.7 MB + CMSIS 24.9 MB + AirISP 5.4 MB，一次性约 365 MB）
```
**第 3 步** 选板卡与选项（见 §3.4）→ 打开示例 → 编译通过即成功。

> 这套资源也在本仓库 `cn_speed up/` 目录里（`package_mymetu_index.json` + `py32-yaoyao-0.2.7.zip` + `tools/` 四个包）——如果你自己有服务器，整个目录原样上传、把索引里的地址改成你的域名即可。

### 3.2 国外 / GitHub 安装（不依赖国内服务器）

**第 1 步** 加**两个**索引地址（一行一条）
```
https://regsens.com/package_py32_index.json
https://github.com/Mymetu/PY32-Arduino-YaoYao/raw/main/package_mymetu_index_github.json
```
- 第一条是**官方 PY32Duino 索引**：提供编译器 / CMSIS / AirISP / Flasher 四个工具（从官方 GitHub Releases 下载）；
- 第二条是**本仓库索引**：只提供板卡包（从本仓库 `dist/` 下载），工具依赖官方那条。

**第 2 步** 开发板管理器 → 搜索 `PY32` → 安装 **PY32 (YaoYao) 0.2.7**（若提示工具缺失，先在同一个管理器里装一次官方 **PY32Duino 0.2.62**）。

**第 3 步** 选板卡与选项（§3.4）→ 编译示例。

### 3.3 手工安装（不想用开发板管理器）

1. 开发板管理器里先装官方 **PY32Duino 0.2.62**（只为拿编译器和上传工具）；
2. **关闭 Arduino IDE**；
3. 把本仓库内容覆盖到：`C:\Users\<你>\AppData\Local\Arduino15\packages\PY32Duino\hardware\PY32\0.2.62\`
   （或在 `<文档>\Arduino\hardware\Mymetu\PY32\` 放一份独立版本，两者可共存，FQBN 为 `Mymetu:PY32:GenF040`）；
4. 删掉编译缓存 `C:\Users\<你>\AppData\Local\arduino`，重新打开 IDE。

### 3.4 安装后必须选择的选项（工具菜单）

| 菜单 | 选择 |
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

## 4. 示例工程：Yao-Relay V2.1（三接口 Modbus RTU 继电器）

`examples/YaoRelay_V2_1/`（装好板卡后：文件 → 示例 → PY32 (YaoYao) → YaoRelay_V2_1）

### 4.1 GPIO 定义

| 功能 | 引脚 | 说明 |
|---|---|---|
| 继电器输出 | **PA11** | 输出；默认低电平 = 释放 |
| 干接点输入 DI | **PA12** | 上拉输入；**短接 GND = 低电平 = 闭合（读 1）**，悬空 = 高电平 = 断开（读 0） |
| TTL 串口 | **PA9 = TX，PA10 = RX** | `Serial`（USART1） |
| RS485 串口 | **PA2 = TX，PA3 = RX**，方向 **PA4**（高 = 发送） | `Serial2`（USART2）；PA4 建议 10 kΩ 下拉 |
| RS232 串口 | **PA0 = TX，PA1 = RX** | `Serial4`（USART4）；接 PC 需 MAX3232 电平转换 |
| 恢复出厂 | **PF8（BOOT0）** | 上拉保持 5 秒 → 恢复出厂参数（ID1/9600/8N1），继电器闪 5 下后重启 |
| 时钟 | 内部 HSI 24 MHz | 无外置晶振 |

三个串口是**同一个 Modbus RTU 从站**（同一套参数、同一个继电器），出厂默认 **地址 1 / 9600 / 8N1**；地址、波特率、校验位**掉电保存**（写入后设备自动重启生效）。

### 4.2 功能与寄存器

| 功能码 | 作用 |
|---|---|
| `0x01` | 读线圈（继电器状态） |
| `0x02` | 读离散输入（DI 状态） |
| `0x05` | 写单线圈：`FF00` 闭合（锁定）/ `0000` 释放 |
| `0x06` | 写单寄存器：`0x0006` 点动脉冲毫秒 · `0x0005` 从站地址 · `0x0007` 波特率（索引 0~7）· `0x000D` 校验格式 · `0x000B` 恢复出厂（`0x55AA`） |

**115200 必须用索引 7 写**（`115200 = 0x1C200` 超出 16 位寄存器）。完整命令表见 sketch 文件头注释 与 `设备手册_ModbusRTU.md`。

### 4.3 烧录（串口 ISP / AirISP）

接线：USB-TTL 的 **RX → PA9**、**TX → PA10**、**GND 共地**，3.3 V 供电；Arduino 里选好 §3.4 的选项和 COM 口。

**进刷机模式（务必按顺序）**：
1. **按住 RST 不撒手**
2. **按住 BOOT 不撒手**
3. **松开 RST**
4. **再松开 BOOT**

此时点"上传"即可；上传失败就把这四步重做一遍再点（也可把 UploadSpeed 降到 57600）。

---

## 5. 配套工具与文档

| 资源 | 位置 | 说明 |
|---|---|---|
| **设备协议手册** | `设备手册_ModbusRTU.md`（仓库根目录 / 项目资料目录） | 接口定义、寄存器表、命令速查、参数保存、故障排查 |
| **PC 上位机** | 项目资料 `485_232_ttl_Relay_Demo\windows_software\` | `YaoRelay上位机.exe`（双击即用，.NET Framework 4）：串口选择、设备ID、闭合/释放/点动、读继电器/读DI、改波特率/校验位/地址；**收发记录可选中复制** |
| **数据手册 / SDK** | 项目资料 `github_arduino_support\PY32F040C1BT6_documents\` | 芯片数据手册、参考手册、厂商 SDK（仅本地参考，不入 Git） |
| **国内加速资源** | 项目资料 `github_arduino_support\cn_speed up\` | 索引 + 板卡包 + 4 个工具包（365 MB），可直接部署到任意服务器 |

---

## 6. 已知问题

1. **`Serial.print()` 持续大量打印会卡死主循环**
   内核 TX 环形缓冲 + HAL 发送中断存在竞态（现象：输出打到一半停住）。**建议**：少量输出，或像示例那样**直写 `USARTx->DR`**。
2. **`Serial3/Serial4` 中断收发不可靠**
   `uart_init` 的 USART3/4 分支未能可靠写入 `obj->index/irq`。**建议**：这两个口用**主循环轮询**（示例即如此，四路都稳定）。
3. **改内核后必须删缓存** `C:\Users\<你>\AppData\Local\arduino`，否则旧对象文件让修改不生效。
4. 编译时若出现 `IWatchdog claims to run on PY32 ... py32` 的黄色警告，只是大小写不一致，**不影响编译和使用**。

---

## 7. 致谢与许可

- 上游：[regimantas/Arduino-PY32](https://github.com/regimantas/Arduino-PY32)（PY32Duino 0.2.62，BSD-3-Clause）
- 工具链：xpack-dev-tools arm-none-eabi-gcc 13.2.1-1.1、stm32duino CMSIS 5.7.0、Air-duino AirISP、PY32Flasher
- 本仓库修改：多串口 / I2C / SPI 适配修正、IWatchdog 加固、菜单修复、flash 参数保存支持、Modbus RTU 继电器示例
- 许可：**BSD-3-Clause**，详见 `LICENSE`
