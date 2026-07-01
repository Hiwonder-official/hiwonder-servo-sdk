# hiwonder-servo-sdk

**[English](README.md) | 中文**

Python / ESP32 / Raspberry Pi / STM32 SDK，适用于**幻尔 HX-30HM** 磁编码总线舵机。

- 协议：UART 单线半双工，1 Mbps，8N1，小端字节序
- 位置：12 位（0–4095 步，360° 范围，约 0.088°/步）
- 支持单总线多舵机同步读写

---

## 目录结构

```
hiwonder-servo-sdk/
├── docs/
│   ├── protocol.md          ← 通信协议（帧格式、指令、时序）
│   └── register_table.md   ← 完整寄存器表（ROM + SRAM，含单位与说明）
│
├── python/                  ← 纯 Python SDK（可 pip 安装）
│   ├── hiwonder/
│   │   ├── __init__.py
│   │   └── servo.py
│   ├── hiwonder_servo.py    ← 向后兼容 shim（from hiwonder import *）
│   ├── pyproject.toml
│   ├── setup.py
│   ├── requirements.txt
│   └── examples/
│       ├── basic_motion.py
│       ├── sync_control.py
│       └── scan_bus.py
│
├── esp32/                   ← ESP32 Arduino 库
│   ├── library.properties   ← Arduino Library Manager 元数据
│   ├── HiwonderServo.h
│   ├── HiwonderServo.cpp
│   └── examples/
│       ├── basic_motion/basic_motion.ino
│       └── sync_control/sync_control.ino
│
├── raspberrypi/             ← 树莓派驱动（扩展 Python SDK）
│   ├── hiwonder_servo_rpi.py
│   └── examples/
│       ├── basic_motion_rpi.py
│       └── sync_control_rpi.py
│
└── stm32/                   ← STM32 HAL C 库
    ├── hiwonder_servo_stm32.h
    ├── hiwonder_servo_stm32.c
    ├── README_STM32.md
    └── examples/
        ├── basic_motion.c
        └── sync_control.c
```

---

## 硬件接线

HX-30HM 使用**单线半双工 UART 总线**，所有舵机共用同一根 DATA 线。

```
主机 UART TX ──[ 1 kΩ ]──┬── 舵机总线 DATA
                           │
主机 UART RX ──────────────┘
GND ──────────────────────── 舵机 GND
11.1 V（3S 锂电池）─────────── 舵机 V+
```

多个舵机时，将所有 `DATA` 和 `GND` 并联即可。1 kΩ 电阻用于防止主机侧 TX/RX 总线冲突。

如果使用 RS-485 收发器芯片（MAX485 等），将 DE/RE 引脚连接到任意 GPIO 输出引脚，并使用对应 SDK 的方向引脚功能。

---

## Python SDK

### 环境要求

- Python 3.8+
- pyserial

### 安装

**方式 A — 从源码安装（推荐开发使用）**

```bash
cd python
pip install -e .
```

安装后，`hiwonder` 包在任何位置均可导入：

```python
from hiwonder import HiwonderBus, Servo, sync_move
```

**方式 B — 仅安装依赖**

```bash
pip install pyserial
```

### 串口配置

**Windows** — 插入 USB-UART 适配器后，在设备管理器中确认串口号（如 `COM3`、`COM4`）。

**Linux** — 插入后运行 `ls /dev/ttyUSB*` 或 `ls /dev/ttyACM*` 查看设备名。

波特率必须设置为 **1 000 000**（1 Mbps）。CH340、CP2102、FT232 等常见 USB 转串口芯片均支持此波特率。

### 控制单个舵机

```python
from hiwonder import HiwonderBus, Servo

bus   = HiwonderBus("COM3")         # Windows（Linux 用 "/dev/ttyUSB0"）
servo = Servo(bus, servo_id=1)

servo.move_to(2048, speed=500)      # 移动到中间位置，速度 500 步/s
print(servo.full_status())
bus.close()
```

### 读取舵机状态

```python
print(servo.position)       # 当前位置 0–4095
print(servo.temperature)    # 温度（°C）
print(servo.voltage)        # 电压（float，单位 V）
print(servo.current)        # 电流（mA）
print(servo.is_moving)      # True / False
```

### 多舵机同步运动

```python
from hiwonder import HiwonderBus, sync_move

bus = HiwonderBus("COM3")
# (舵机ID, 目标位置, 运动时间ms, 速度)
sync_move(bus, [
    (1, 1000, 0, 400),
    (2, 2000, 0, 400),
    (3, 3000, 0, 400),
])
bus.close()
```

`sync_move` 发送单条 SYNC WRITE 数据包，所有舵机同时启动。

### 修改舵机 ID

```python
# 将当前 ID 1 的舵机改为 ID 5
servo.set_id(5)
```

写入 NVS（掉电保存），立即生效。

### 运行示例

```bash
# 单舵机
python python/examples/basic_motion.py --port COM3 --id 1

# 四舵机同步
python python/examples/sync_control.py --port COM3 --ids 1 2 3 4

# 扫描总线上所有 ID
python python/examples/scan_bus.py --port COM3
```

### 向后兼容

原来直接使用 `hiwonder_servo.py` 的代码无需修改——`python/hiwonder_servo.py` 现在是一个 shim，会重新导出 `hiwonder` 包中的所有内容。

---

## ESP32（Arduino IDE / PlatformIO）

### 安装 — Arduino IDE

1. 在 Arduino IDE 中：**项目 → 加载库 → 添加 .ZIP 库…**  
   选择 `esp32/` 文件夹（或其压缩包）。

   也可以将整个 `esp32/` 文件夹复制到 Arduino 库目录：
   - Windows：`C:\Users\<用户名>\Documents\Arduino\libraries\HiwonderServo\`
   - macOS / Linux：`~/Arduino/libraries/HiwonderServo/`

2. 重启 Arduino IDE，在**项目 → 加载库**中可以看到 **HiwonderServo**。

3. 打开示例：**文件 → 示例 → HiwonderServo → basic_motion** 或 **sync_control**。

### 安装 — PlatformIO

在 `platformio.ini` 中添加：

```ini
lib_deps =
    https://github.com/Hiwonder-official/-hiwonder-servo-sdk.git
```

或将 `esp32/` 文件夹复制到项目的 `lib/` 目录。

### 接线（ESP32）

库的默认引脚分配：

| 信号 | GPIO |
|------|------|
| UART RX（接收总线数据）| GPIO 16 |
| UART TX → 1 kΩ → 总线 | GPIO 17 |

如需更改，在 `HiwonderBus` 构造函数中传入不同的引脚号。

### 最小示例

```cpp
#include <HiwonderServo.h>

HiwonderBus bus(Serial2, /*rx=*/16, /*tx=*/17, /*baud=*/1000000UL);
Servo servo(bus, /*id=*/1);

void setup() {
    Serial.begin(115200);

    servo.moveTo(2048, /*speed=*/500);   // 移动到中间位置
    delay(1500);

    ServoStatus st = servo.fullStatus();
    Serial.printf("pos=%d  volt=%.1fV  temp=%d°C\n",
                  st.position, st.voltage, st.temperature);
}

void loop() {}
```

### 多舵机同步（ESP32）

```cpp
uint8_t  ids[4]       = {1, 2, 3, 4};
uint16_t positions[4] = {1000, 2000, 3000, 2048};
uint16_t speeds[4]    = {400, 400, 400, 400};
uint16_t times[4]     = {0, 0, 0, 0};

syncMove(bus, ids, positions, speeds, times, 4);
```

---

## 树莓派

### 串口配置

**RPi 3 / 4 / 5** — 默认 UART（`/dev/ttyAMA0`）被蓝牙占用，需先禁用蓝牙：

```bash
# 追加到 /boot/config.txt（RPi 5 为 /boot/firmware/config.txt）
echo "dtoverlay=disable-bt" | sudo tee -a /boot/config.txt
sudo reboot
```

然后启用串口硬件：

```bash
sudo raspi-config
# 接口选项 → 串口 → 登录 shell 通过串口访问？→ 否 → 串口硬件是否启用？→ 是
```

重启后 `/dev/ttyAMA0` 即可使用。

也可以直接使用 USB-UART 适配器（`/dev/ttyUSB0`），无需任何配置。

### 安装

树莓派驱动复用 Python SDK，安装一次即可：

```bash
cd python
pip install -e .
```

### 运行示例

```bash
python raspberrypi/examples/basic_motion_rpi.py --port /dev/ttyAMA0 --id 1

python raspberrypi/examples/sync_control_rpi.py --port /dev/ttyAMA0 --ids 1 2 3 4
```

### RS-485 方向引脚（可选）

如果使用 RS-485 收发器（MAX485、SN75176）而非直接 1 kΩ 电阻接线，需传入方向控制引脚的 BCM GPIO 编号：

```bash
python raspberrypi/examples/basic_motion_rpi.py --port /dev/ttyAMA0 --id 1 --dir-pin 18
```

在代码中：

```python
from raspberrypi.hiwonder_servo_rpi import RpiServoBus, Servo

bus   = RpiServoBus("/dev/ttyAMA0", dir_pin=18)   # BCM GPIO 18
servo = Servo(bus, servo_id=1)
servo.move_to(2048, speed=400)
bus.close()
```

---

## STM32

详细的 CubeIDE 配置、接线说明和 API 参考见 [`stm32/README_STM32.md`](stm32/README_STM32.md)。

**快速上手：**

1. 将 `stm32/hiwonder_servo_stm32.h` 和 `stm32/hiwonder_servo_stm32.c` 复制到项目中。
2. 在 CubeMX 中配置一个 UART 外设：**1 000 000 bps，8N1**。
3. 在引用驱动头文件之前，先包含你的芯片系列 HAL 头文件：

   ```c
   #include "stm32f4xx_hal.h"    /* 按实际芯片型号修改 */
   #include "hiwonder_servo_stm32.h"
   ```

4. 创建总线句柄并调用 `hw_move_to`：

   ```c
   HW_Bus bus = { .huart = &huart2, .dir_port = NULL, .timeout = 10 };
   hw_move_to(&bus, /*id=*/1, /*position=*/2048, /*time_ms=*/0, /*speed=*/500);
   ```

---

## 支持的指令

| 指令 | 代码 | 描述 |
|------|------|------|
| PING | 0x01 | 查询舵机是否在线 |
| READ DATA | 0x02 | 读取寄存器 |
| WRITE DATA | 0x03 | 写入寄存器 |
| REG WRITE | 0x04 | 缓冲写（等待 ACTION 触发）|
| ACTION | 0x05 | 广播执行所有缓冲写 |
| RESET | 0x06 | 恢复出厂默认值 |
| SYNC READ | 0x82 | 从多个舵机同步读取相同寄存器 |
| SYNC WRITE | 0x83 | 向多个舵机同步写入相同寄存器 |

完整帧格式、校验算法及示例见 [`docs/protocol.md`](docs/protocol.md)。

---

## 关键寄存器速查

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| ID | 0x05 | 1 字节，0–253；写入需先解锁 NVS |
| 波特率 | 0x06 | 0=1Mbps（默认）；写入需先解锁 NVS |
| 目标位置 | 0x2A | 2 字节，0–4095；原子 6 字节块起始地址 |
| 运动时间 | 0x2C | 2 字节，ms |
| 运动速度 | 0x2E | 2 字节，步/s |
| NVS 写保护锁 | 0x37 | 0=解锁，1=上锁（默认）|
| 当前位置 | 0x38 | 2 字节，只读 |
| 当前速度 | 0x3A | 2 字节，只读 |
| 电压 | 0x3E | 2 字节，×0.1 V，只读 |
| 温度 | 0x3F | 1 字节，°C，只读 |
| 电流（mA）| 0x45 | 2 字节，只读 |

> **0x2A 与 0x28 的区别**：始终使用 0x2A（TARGET_POS）。它与 0x2C（时间）和 0x2E（速度）构成连续 6 字节原子块，一条写指令同时设置三个参数。分开写 0x28 和 0x29 存在竞态风险——舵机可能在第二次写入完成之前就已开始运动。详见 [`docs/register_table.md`](docs/register_table.md)。

完整寄存器表：[`docs/register_table.md`](docs/register_table.md)

---

## 常见问题

**Q：舵机完全没有响应。**  
A：先确认供电（需要 11.1 V / 3S 锂电池）。确认波特率精确设置为 1 000 000。如果不确定接线方向，尝试交换 TX 和 RX。确认 1 kΩ 电阻串联在 TX 侧，不是 RX 侧。

**Q：偶尔出现校验和错误。**  
A：通常是信号完整性问题——尝试缩短连接线缆，在舵机接头附近加 100 nF 去耦电容，或通过 `servo.set_baud_rate(1)` 将波特率降低到 500 kbps。

**Q：多个舵机不能全部响应。**  
A：所有舵机出厂默认 ID 均为 1。请逐个连接，使用 `servo.set_id(n)` 修改 ID 后再并联。

**Q：`pip install -e .` 报错"找不到 hiwonder 模块"。**  
A：请在 `python/` 目录下运行该命令，不要在仓库根目录下运行。

**Q：树莓派 UART 不可用或提示权限拒绝。**  
A：运行 `sudo raspi-config` → 接口选项 → 串口 → 禁用登录 shell，启用串口硬件。同时检查 `ls -l /dev/ttyAMA0`，当前用户需要属于 `dialout` 组（`sudo usermod -aG dialout $USER`）。

**Q：ESP32 草图编译通过但舵机不动。**  
A：确认 `Serial2` 在你的开发板上映射到 GPIO 16（RX）和 17（TX）。部分 ESP32-S3 开发板的 Serial2 引脚不同——请查阅开发板引脚图，并在 `HiwonderBus` 构造函数中相应调整。

---

## 许可证

MIT
