# Modbus RTU + UART + DMA 串口单元测试方案

**项目**: STM32F103 Modbus RTU Slave with RS485  
**版本**: 1.0.0  
**日期**: 2025-10-18  
**测试对象**: USART1 + DMA + IDLE中断 + Modbus RTU协议栈

---

## 一、测试环境配置

### 1.1 硬件配置

| 组件 | 配置 | 说明 |
|------|------|------|
| MCU | STM32F103C8T6 | 72MHz, 64KB Flash, 20KB RAM |
| USART | USART1 (PA9/PA10) | 9600 bps, 8N1 |
| RS485 | PA8 控制 DE/RE | MAX485/SP485芯片 |
| DMA | DMA1_Ch4 (TX), DMA1_Ch5 (RX) | 正常模式 |
| LED | PC13 | 测试用指示灯（保持寄存器0控制） |
| 调试 | UART1 或 SWD | 查看日志或调试 |

### 1.2 软件配置

```c
// 在 modbus_rtu_slave.h 中配置
#define MB_RTU_FRAME_MAX_SIZE       256U
#define MB_HOLDING_REGS_SIZE        100U
#define MB_INPUT_REGS_SIZE          100U
#define MB_COILS_SIZE               100U
#define MB_DISCRETE_INPUTS_SIZE     100U

// RS485 方向控制
#define MB_RS485_DE_GPIO_Port  GPIOA
#define MB_RS485_DE_Pin        GPIO_PIN_8
```

### 1.3 测试工具

1. **Modbus Poll** (Windows)  
   - 商业软件，功能强大
   - 支持RTU/ASCII/TCP
   - 下载：https://www.modbustools.com/

2. **Modbus Slave Simulator** (可选)  
   - 用于验证主站功能

3. **串口调试助手**  
   - 格西烽火 / SSCOM
   - 查看原始数据帧

4. **逻辑分析仪** (可选)  
   - 抓取RS485总线波形
   - 验证时序和电平

---

## 二、测试准备

### 2.1 连接方式

#### 方式1: USB-RS485转换器（推荐）

```
PC USB -----> USB-RS485 -----> STM32 RS485
                  A    <------>  A
                  B    <------>  B
                 GND   <------>  GND
```

#### 方式2: USB-TTL（仅测试UART，不测试RS485）

```
PC USB -----> USB-TTL -----> STM32 USART1
                 TX  ------>  PA10 (RX)
                 RX  <------  PA9  (TX)
                GND  ------>  GND

注意：需要在代码中注释掉 RS485 控制
```

### 2.2 初始化检查

烧录程序后，观察：

1. ✅ 电源指示灯亮
2. ✅ PC13 LED应该熄灭（初始值holdingRegs[0]=100，LED OFF）
3. ✅ 串口工具能识别设备

---

## 三、基础功能测试

### 3.1 测试用例 1: 读保持寄存器（功能码 0x03）

**目的**: 验证DMA接收、IDLE中断、CRC校验、数据返回

#### 测试步骤

1. 使用 **Modbus Poll** 配置：
   ```
   Connection: Serial RTU
   Port: COM3 (根据实际端口)
   Baud: 9600
   Data Bits: 8
   Parity: None
   Stop Bits: 1
   
   Slave ID: 1
   Function: 03 (Read Holding Registers)
   Start Address: 0
   Quantity: 3
   ```

2. 点击 **Connect**

3. 预期结果：
   ```
   Reg[0] = 100  (0x0064)
   Reg[1] = 200  (0x00C8)
   Reg[2] = 300  (0x012C)
   ```

#### 底层数据帧验证（可选）

**发送帧**（主站 → 从站）:
```
01 03 00 00 00 03 05 CB
│  │  │  │  │  │  └──┴─ CRC16 (LSB, MSB)
│  │  │  │  └──┴─ 数量 = 3
│  │  └──┴─ 起始地址 = 0
│  └─ 功能码 = 0x03
└─ 从站地址 = 0x01
```

**返回帧**（从站 → 主站）:
```
01 03 06 00 64 00 C8 01 2C XX XX
│  │  │  │  │  │  │  │  │  └──┴─ CRC16
│  │  │  │  │  │  │  └──┴─ Reg[2] = 300 (0x012C)
│  │  │  │  │  └──┴─ Reg[1] = 200 (0x00C8)
│  │  │  └──┴─ Reg[0] = 100 (0x0064)
│  │  └─ 字节数 = 6
│  └─ 功能码 = 0x03
└─ 从站地址 = 0x01
```

#### 预期行为

- [x] IDLE中断触发，接收到8字节
- [x] CRC校验通过
- [x] 快照读取（临界区保护）
- [x] DMA发送响应
- [x] RS485切换：TX → RX
- [x] DMA重启接收

---

### 3.2 测试用例 2: 读输入寄存器（功能码 0x04）

**目的**: 验证输入寄存器读取

#### 测试步骤

1. 配置 Modbus Poll:
   ```
   Function: 04 (Read Input Registers)
   Start Address: 0
   Quantity: 2
   ```

2. 预期结果：
   ```
   Reg[0] = 1000  (0x03E8)
   Reg[1] = 2000  (0x07D0)
   ```

#### 数据帧（简化）

**发送**: `01 04 00 00 00 02 XX XX`  
**返回**: `01 04 04 03 E8 07 D0 XX XX`

---

### 3.3 测试用例 3: 写单寄存器（功能码 0x06）+ LED控制

**目的**: 验证写操作、回调函数、LED控制

#### 测试步骤

1. 配置 Modbus Poll:
   ```
   Function: 06 (Write Single Register)
   Address: 0
   Value: 0  ← 将 PC13 LED 点亮
   ```

2. 预期现象：
   - ✅ **PC13 LED 亮起**（因为代码中 value=0 → GPIO_PIN_RESET → LED ON）
   - ✅ Modbus Poll 显示写入成功

3. 再次写入：
   ```
   Address: 0
   Value: 1  ← 将 PC13 LED 熄灭
   ```

4. 预期现象：
   - ✅ **PC13 LED 熄灭**

#### 数据帧

**发送**: `01 06 00 00 00 00 XX XX` (写入0)  
**返回**: `01 06 00 00 00 00 XX XX` (回显)

#### 验证回调函数

在 `main.c` 中的 `ModbusRTU_PostWriteCallback` 应该被调用：

```c
void ModbusRTU_PostWriteCallback(uint16_t addr, uint16_t value)
{
    if (addr == 0) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, (value > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}
```

---

### 3.4 测试用例 4: 写多寄存器（功能码 0x10）

**目的**: 验证批量写入、循环处理、内存安全

#### 测试步骤

1. 配置 Modbus Poll:
   ```
   Function: 16 (Write Multiple Registers)
   Start Address: 0
   Quantity: 3
   Values: [999, 888, 777]
   ```

2. 写入后，读取验证：
   ```
   Function: 03 (Read Holding Registers)
   Start Address: 0
   Quantity: 3
   ```

3. 预期结果：
   ```
   Reg[0] = 999
   Reg[1] = 888
   Reg[2] = 777
   ```

#### 数据帧

**发送**:
```
01 10 00 00 00 03 06 03 E7 03 78 03 09 XX XX
│  │  │  │  │  │  │  │  │  │  │  │  │  └──┴─ CRC
│  │  │  │  │  │  │  │  │  │  │  └──┴─ Value[2] = 777
│  │  │  │  │  │  │  │  │  └──┴─ Value[1] = 888
│  │  │  │  │  │  │  └──┴─ Value[0] = 999
│  │  │  │  │  │  └─ 字节数 = 6
│  │  │  │  └──┴─ 数量 = 3
│  │  └──┴─ 起始地址 = 0
│  └─ 功能码 = 0x10
└─ 从站地址 = 0x01
```

**返回**:
```
01 10 00 00 00 03 XX XX
```

---

## 四、压力测试

### 4.1 测试用例 5: 连续读取（轮询测试）

**目的**: 验证DMA重启、状态管理、无内存泄漏

#### 测试步骤

1. 在 Modbus Poll 中设置：
   ```
   Poll Rate: 100 ms  ← 每100ms读一次
   Duration: 10 minutes
   ```

2. 监控：
   - ✅ 所有请求都能正常响应
   - ✅ 无丢帧、无超时
   - ✅ LED无异常闪烁
   - ✅ 电流无异常波动

3. 预期结果：
   ```
   成功率 = 100%
   平均响应时间 < 10ms
   ```

---

### 4.2 测试用例 6: 快速读写交替

**目的**: 验证状态切换、RS485方向控制

#### 测试步骤

1. 使用脚本（Python + pymodbus）：

```python
from pymodbus.client.sync import ModbusSerialClient

client = ModbusSerialClient(
    method='rtu',
    port='COM3',
    baudrate=9600,
    timeout=1
)

if client.connect():
    for i in range(1000):
        # 读
        result = client.read_holding_registers(0, 3, unit=1)
        print(f"Read {i}: {result.registers}")
        
        # 写
        client.write_register(0, i % 2, unit=1)
        print(f"Write {i}: {i % 2}")
        
    client.close()
```

2. 预期结果：
   - ✅ 1000次读写全部成功
   - ✅ LED以约1Hz频率闪烁
   - ✅ 无死机、无乱码

---

### 4.3 测试用例 7: 边界条件测试

**目的**: 验证异常处理、CRC校验、地址越界保护

#### 测试7.1: 错误从站地址

```
发送: 02 03 00 00 00 03 XX XX  ← 从站地址 = 0x02（错误）
预期: 无响应（从站忽略）
```

#### 测试7.2: 错误功能码

```
发送: 01 09 00 00 00 03 XX XX  ← 功能码 = 0x09（不支持）
预期: 返回异常响应 01 89 01 XX XX
        └─ 0x89 = 0x09 | 0x80
           异常码 = 0x01 (非法功能)
```

#### 测试7.3: 地址越界

```
读取: 地址 = 99, 数量 = 5  ← 超出 MB_HOLDING_REGS_SIZE = 100
预期: 返回异常响应 01 83 02 XX XX
        异常码 = 0x02 (非法地址)
```

#### 测试7.4: 错误CRC

```
发送: 01 03 00 00 00 03 FF FF  ← 错误的CRC
预期: 无响应（从站丢弃帧）
```

#### 测试7.5: 数量越界

```
读取: 数量 = 126  ← 超出Modbus标准上限125
预期: 返回异常响应 01 83 03 XX XX
        异常码 = 0x03 (非法数据值)
```

---

## 五、RS485时序测试（可选，需逻辑分析仪）

### 5.1 测试用例 8: DE/RE切换时序

**目的**: 验证RS485半双工切换无毛刺

#### 测试步骤

1. 使用逻辑分析仪抓取：
   - CH1: PA8 (DE/RE)
   - CH2: PA9 (UART_TX)
   - CH3: PA10 (UART_RX)
   - CH4: RS485_A (差分信号)

2. 发送一次Modbus请求，观察波形

#### 预期时序

```
时间轴 ───────────────────────────────────────────>

DE/RE  ───┐                 ┌──────────────┐
          │                 │              │
          └─────────────────┘              └─────
          LOW              HIGH            LOW
         (接收)           (发送)          (接收)

UART_TX ─────────[数据]───────────────────────────

UART_RX [数据]────────────────────[响应数据]──────

关键验证点：
1. DE/RE 在 UART_TX 开始前切换为 HIGH（允许1-2us提前量）
2. DE/RE 在 UART_TX 结束后立即切换为 LOW（通过 HAL_UART_TxCpltCallback）
3. 切换无毛刺、无抖动
4. RX数据在 DE/RE=LOW 时正常接收
```

---

## 六、DMA状态监控测试

### 6.1 测试用例 9: DMA计数器检查

**目的**: 验证DMA正确重启、无泄漏

#### 测试代码（在main.c的while循环中添加）

```c
// 仅用于测试，生产环境应删除
uint32_t dmaCounter = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
uint32_t rxState = huart1.RxState;

// 通过调试器观察或通过另一个UART打印
// 预期: 
// - dmaCounter 应在 0 到 MB_RTU_FRAME_MAX_SIZE 之间变化
// - rxState 大部分时间为 HAL_UART_STATE_BUSY_RX
```

#### 验证点

- [x] 每次接收完成后，DMA计数器正确复位到 MB_RTU_FRAME_MAX_SIZE
- [x] `huart1.RxState` 在接收时为 `HAL_UART_STATE_BUSY_RX`
- [x] 无 `HAL_UART_STATE_READY` 长时间停留（表示DMA未重启）

---

## 七、IDLE中断触发测试

### 7.1 测试用例 10: IDLE中断响应时间

**目的**: 确认IDLE中断在帧结束后立即触发

#### 测试方法

1. 在 `USART1_IRQHandler` 中添加GPIO翻转：

```c
void USART1_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
  {
    // 测试用：翻转一个GPIO来标记IDLE触发
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);  // 需先初始化PB12
    
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    ModbusRTU_UartRxCallback(&g_mb);
  }
  HAL_UART_IRQHandler(&huart1);
}
```

2. 用示波器或逻辑分析仪观察 PB12 翻转

#### 预期结果

- PB12在每帧接收结束后立即翻转（延迟 < 10us）
- 频率与Modbus请求频率一致

---

## 八、异常恢复测试

### 8.1 测试用例 11: 噪声/干扰恢复

**目的**: 验证系统对错误帧的鲁棒性

#### 测试步骤

1. 使用串口助手发送随机数据（非Modbus帧）
2. 发送错误CRC的帧
3. 发送不完整的帧

#### 预期结果

- [x] 系统忽略错误帧
- [x] 不影响后续正确帧的处理
- [x] DMA正常重启
- [x] 无死锁、无崩溃

---

### 8.2 测试用例 12: 上电复位测试

**目的**: 验证初始化可靠性

#### 测试步骤

1. 烧录程序
2. 上电
3. 立即发送Modbus请求
4. 重复10次

#### 预期结果

- [x] 每次上电后第一帧都能正确响应
- [x] 无需等待、无需复位
- [x] LED状态正确

---

## 九、性能基准测试

### 9.1 测试用例 13: 响应时间测量

**目的**: 测量端到端延迟

#### 测试方法

使用 Modbus Poll 的 **Response Time** 功能：

```
Function: 03 (Read Holding Registers)
Quantity: 1
Poll Rate: 1000 ms
Duration: 5 minutes
```

#### 预期结果

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 平均响应时间 | < 5ms | 包括传输+处理 |
| 最大响应时间 | < 10ms | 偶尔的延迟 |
| 标准差 | < 2ms | 抖动小 |
| 成功率 | 100% | 无丢帧 |

---

### 9.2 测试用例 14: 吞吐量测试

**目的**: 测试最大数据传输速率

#### 测试步骤

1. 连续读取最大数量寄存器：
   ```
   Function: 03
   Quantity: 125  ← Modbus标准最大值
   Poll Rate: 1 ms  ← 尽快轮询
   ```

2. 计算吞吐量：
   ```
   每帧字节数 = 1 + 1 + 1 + 125*2 + 2 = 255 bytes
   波特率 = 9600 bps
   理论最大帧率 = 9600 / (255 * 10) ≈ 3.76 fps
   实际吞吐量 ≈ 255 * 3.76 ≈ 958 bytes/s
   ```

#### 预期结果

- [x] 实际吞吐量 ≥ 90% 理论值
- [x] 无错误、无丢帧

---

## 十、调试与问题排查

### 10.1 常见问题及解决方法

#### 问题1: 第一帧能响应，后续无响应

**原因**: `HAL_UART_TxCpltCallback` 未实现或RS485未切回接收模式

**排查**:
```c
// 在 HAL_UART_TxCpltCallback 中添加断点
// 验证是否被调用
```

**解决**: 确保实现了完整的 `HAL_UART_TxCpltCallback`（已在本方案中修复）

---

#### 问题2: 全部无响应

**排查步骤**:
1. 检查RS485连接（A-A, B-B）
2. 检查波特率（9600）
3. 检查从站地址（0x01）
4. 用示波器看UART_TX是否有波形
5. 检查 `__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE)` 是否调用

---

#### 问题3: CRC校验总是失败

**排查**:
```c
// 在 ModbusRTU_ProcessFrame 中添加调试代码
uint16_t crcRx = (uint16_t)((mb->rxBuffer[mb->rxCount - 1] << 8) | mb->rxBuffer[mb->rxCount - 2]);
uint16_t crcClc = ModbusRTU_CRC16(mb->rxBuffer, mb->rxCount - 2);

// 打印或观察这两个值
// 若不一致，检查：
// 1. rxCount 是否正确
// 2. 是否有数据丢失
// 3. CRC字节序是否正确（应为LSB在前）
```

---

#### 问题4: LED控制反向（0=灭，1=亮）

**原因**: PC13低电平点亮，逻辑需要反转

**解决**:
```c
// 当前代码已正确实现
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, (value > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
```

---

#### 问题5: DMA无法重启

**排查**:
```c
// 在 MB_RestartRx 中添加错误检查
HAL_StatusTypeDef status = HAL_UART_Receive_DMA(mb->huart, mb->rxBuffer, MB_RTU_FRAME_MAX_SIZE);
if (status != HAL_OK) {
    // DMA重启失败，可能是：
    // 1. huart->RxState 不正确
    // 2. DMA未完全停止
    // 3. UART有错误标志未清除
}
```

---

### 10.2 调试辅助代码（仅用于测试）

```c
// 在 main.c 的 while(1) 中添加（生产环境需删除）
static uint32_t lastDebugTime = 0;
if (HAL_GetTick() - lastDebugTime > 1000) {
    lastDebugTime = HAL_GetTick();
    
    // 通过调试器观察这些值
    uint16_t rxCnt = g_mb.rxCount;
    uint8_t rxCplt = g_mb.rxComplete;
    uint8_t frameRcv = g_mb.frameReceiving;
    uint32_t dmaCnt = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    
    // 或通过另一个UART打印
    // printf("RX:%d, Cplt:%d, Frame:%d, DMA:%ld\r\n", 
    //        rxCnt, rxCplt, frameRcv, dmaCnt);
}
```

---

## 十一、自动化测试脚本（Python）

### 11.1 完整功能测试脚本

```python
#!/usr/bin/env python3
"""
Modbus RTU Slave Automated Test Suite
Target: STM32F103 + Modbus RTU Slave (Slave ID = 0x01)
"""

from pymodbus.client.sync import ModbusSerialClient
import time
import sys

# 测试配置
SERIAL_PORT = 'COM3'  # 根据实际情况修改
BAUDRATE = 9600
SLAVE_ID = 1

# 测试统计
test_passed = 0
test_failed = 0

def test_init():
    """初始化Modbus客户端"""
    client = ModbusSerialClient(
        method='rtu',
        port=SERIAL_PORT,
        baudrate=BAUDRATE,
        timeout=1,
        parity='N',
        stopbits=1,
        bytesize=8
    )
    
    if not client.connect():
        print(f"❌ 无法连接到 {SERIAL_PORT}")
        sys.exit(1)
    
    print(f"✅ 已连接到 {SERIAL_PORT}")
    return client

def test_case(name, func):
    """测试用例装饰器"""
    global test_passed, test_failed
    print(f"\n▶ 测试: {name}")
    try:
        func()
        print(f"  ✅ 通过")
        test_passed += 1
    except AssertionError as e:
        print(f"  ❌ 失败: {e}")
        test_failed += 1
    except Exception as e:
        print(f"  ❌ 错误: {e}")
        test_failed += 1

# ========== 测试用例 ==========

def test_read_holding_registers(client):
    """测试用例1: 读保持寄存器（0x03）"""
    result = client.read_holding_registers(0, 3, unit=SLAVE_ID)
    assert not result.isError(), f"读取失败: {result}"
    assert result.registers[0] == 100, f"Reg[0] 应为 100，实际 {result.registers[0]}"
    assert result.registers[1] == 200, f"Reg[1] 应为 200，实际 {result.registers[1]}"
    assert result.registers[2] == 300, f"Reg[2] 应为 300，实际 {result.registers[2]}"
    print(f"  读取到: {result.registers}")

def test_read_input_registers(client):
    """测试用例2: 读输入寄存器（0x04）"""
    result = client.read_input_registers(0, 2, unit=SLAVE_ID)
    assert not result.isError(), f"读取失败: {result}"
    assert result.registers[0] == 1000, f"Reg[0] 应为 1000，实际 {result.registers[0]}"
    assert result.registers[1] == 2000, f"Reg[1] 应为 2000，实际 {result.registers[1]}"
    print(f"  读取到: {result.registers}")

def test_write_single_register(client):
    """测试用例3: 写单寄存器（0x06）"""
    # 写入 0（LED亮）
    result = client.write_register(0, 0, unit=SLAVE_ID)
    assert not result.isError(), f"写入失败: {result}"
    print(f"  已写入 0（LED应亮起）")
    time.sleep(0.5)
    
    # 验证写入
    result = client.read_holding_registers(0, 1, unit=SLAVE_ID)
    assert result.registers[0] == 0, f"验证失败，期望 0，实际 {result.registers[0]}"
    
    # 写入 1（LED灭）
    result = client.write_register(0, 1, unit=SLAVE_ID)
    assert not result.isError(), f"写入失败: {result}"
    print(f"  已写入 1（LED应熄灭）")

def test_write_multiple_registers(client):
    """测试用例4: 写多寄存器（0x10）"""
    values = [999, 888, 777]
    result = client.write_registers(0, values, unit=SLAVE_ID)
    assert not result.isError(), f"写入失败: {result}"
    
    # 验证写入
    result = client.read_holding_registers(0, 3, unit=SLAVE_ID)
    assert result.registers == values, f"验证失败: {result.registers} != {values}"
    print(f"  批量写入并验证成功: {values}")

def test_continuous_read(client, count=100):
    """测试用例5: 连续读取（压力测试）"""
    success = 0
    for i in range(count):
        result = client.read_holding_registers(0, 3, unit=SLAVE_ID)
        if not result.isError():
            success += 1
        else:
            print(f"  第 {i+1} 次读取失败")
    
    success_rate = (success / count) * 100
    assert success_rate >= 99, f"成功率过低: {success_rate}%"
    print(f"  成功率: {success_rate}% ({success}/{count})")

def test_read_write_alternate(client, count=50):
    """测试用例6: 读写交替"""
    for i in range(count):
        # 读
        result = client.read_holding_registers(0, 1, unit=SLAVE_ID)
        assert not result.isError(), f"第 {i+1} 次读取失败"
        
        # 写
        result = client.write_register(0, i % 2, unit=SLAVE_ID)
        assert not result.isError(), f"第 {i+1} 次写入失败"
    
    print(f"  {count} 次读写交替完成")

def test_invalid_slave_address(client):
    """测试用例7: 错误从站地址"""
    result = client.read_holding_registers(0, 3, unit=0x02)
    # 预期超时或无响应
    assert result.isError() or result is None, "应忽略错误从站地址"
    print(f"  正确忽略了错误从站地址")

def test_invalid_address(client):
    """测试用例8: 地址越界"""
    result = client.read_holding_registers(99, 5, unit=SLAVE_ID)  # 超出100
    # 预期返回异常码
    assert result.isError(), "应返回异常响应"
    print(f"  正确返回了异常响应")

def test_boundary_values(client):
    """测试用例9: 边界值测试"""
    # 最大数量读取
    result = client.read_holding_registers(0, 100, unit=SLAVE_ID)
    assert not result.isError(), "读取100个寄存器失败"
    assert len(result.registers) == 100, f"应返回100个寄存器"
    print(f"  边界值测试通过")

# ========== 主测试流程 ==========

def main():
    print("=" * 60)
    print("  STM32F103 Modbus RTU Slave 自动化测试套件")
    print("=" * 60)
    
    client = test_init()
    
    try:
        # 基础功能测试
        test_case("读保持寄存器 (0x03)", lambda: test_read_holding_registers(client))
        test_case("读输入寄存器 (0x04)", lambda: test_read_input_registers(client))
        test_case("写单寄存器 (0x06)", lambda: test_write_single_register(client))
        test_case("写多寄存器 (0x10)", lambda: test_write_multiple_registers(client))
        
        # 压力测试
        test_case("连续读取 (100次)", lambda: test_continuous_read(client, 100))
        test_case("读写交替 (50次)", lambda: test_read_write_alternate(client, 50))
        
        # 异常测试
        test_case("错误从站地址", lambda: test_invalid_slave_address(client))
        test_case("地址越界", lambda: test_invalid_address(client))
        test_case("边界值测试", lambda: test_boundary_values(client))
        
    finally:
        client.close()
        print("\n" + "=" * 60)
        print(f"  测试完成: ✅ {test_passed} 通过, ❌ {test_failed} 失败")
        print("=" * 60)
        
        if test_failed > 0:
            sys.exit(1)

if __name__ == '__main__':
    main()
```

### 11.2 运行测试

```bash
# 安装依赖
pip install pymodbus

# 运行测试
python modbus_test.py
```

---

## 十二、测试通过标准

### 12.1 必须通过的测试（关键）

- [x] 测试用例1: 读保持寄存器
- [x] 测试用例2: 读输入寄存器
- [x] 测试用例3: 写单寄存器 + LED控制
- [x] 测试用例4: 写多寄存器
- [x] 测试用例5: 连续读取（成功率 ≥ 99%）
- [x] 测试用例8: RS485时序（无毛刺）

### 12.2 建议通过的测试（增强）

- [ ] 测试用例6: 快速读写交替
- [ ] 测试用例7: 边界条件测试
- [ ] 测试用例11: 噪声恢复测试
- [ ] 测试用例13: 性能基准测试

---

## 十三、测试报告模板

```markdown
# Modbus RTU 测试报告

**测试日期**: YYYY-MM-DD  
**测试人员**: [姓名]  
**固件版本**: [版本号]  
**硬件版本**: STM32F103C8T6

## 测试环境
- 工具: Modbus Poll v9.0 / Python脚本
- 接口: USB-RS485转换器
- 波特率: 9600
- 测试时长: 2小时

## 测试结果

| 测试用例 | 状态 | 备注 |
|---------|------|------|
| 读保持寄存器 | ✅ 通过 | - |
| 读输入寄存器 | ✅ 通过 | - |
| 写单寄存器 | ✅ 通过 | LED控制正常 |
| 写多寄存器 | ✅ 通过 | - |
| 连续读取 | ✅ 通过 | 成功率 100% |
| 读写交替 | ✅ 通过 | 无死锁 |
| 异常处理 | ✅ 通过 | 正确返回异常码 |
| RS485时序 | ✅ 通过 | 切换延迟 < 5us |

## 性能指标
- 平均响应时间: 4.2 ms
- 最大响应时间: 8.5 ms
- 吞吐量: 920 bytes/s

## 问题记录
无

## 结论
✅ 系统通过所有测试，满足生产要求。
```

---

## 十四、持续集成（可选）

### 14.1 Jenkins集成

```groovy
pipeline {
    agent any
    stages {
        stage('Flash Firmware') {
            steps {
                sh 'st-flash write firmware.bin 0x08000000'
            }
        }
        stage('Run Tests') {
            steps {
                sh 'python3 modbus_test.py'
            }
        }
    }
}
```

---

## 附录A: 快速检查清单

在开始测试前，逐项检查：

- [ ] 固件已烧录
- [ ] LED初始状态正确（灭）
- [ ] RS485连接正确（A-A, B-B, GND-GND）
- [ ] 串口工具能识别设备
- [ ] 波特率配置正确（9600）
- [ ] 从站地址正确（0x01）
- [ ] 终端电阻已连接（如需要）
- [ ] 电源电压正常（3.3V/5V）
- [ ] 调试串口正常（如有）

---

## 附录B: 常用Modbus命令速查

| 功能 | 功能码 | 请求格式 | 示例 |
|------|--------|---------|------|
| 读保持寄存器 | 0x03 | `[地址][功能码][起始地址H][起始地址L][数量H][数量L][CRC_L][CRC_H]` | `01 03 00 00 00 03 05 CB` |
| 读输入寄存器 | 0x04 | 同上 | `01 04 00 00 00 02 71 CB` |
| 写单寄存器 | 0x06 | `[地址][功能码][寄存器地址H][寄存器地址L][值H][值L][CRC_L][CRC_H]` | `01 06 00 00 00 00 88 39` |
| 写多寄存器 | 0x10 | `[地址][功能码][起始地址H][起始地址L][数量H][数量L][字节数][数据...][CRC_L][CRC_H]` | `01 10 00 00 00 02 04 00 0A 00 14 XX XX` |

---

## 附录C: 故障排除流程图

```
无响应？
  ├─ 是 → 检查硬件连接
  │       ├─ RS485线序正确？
  │       ├─ 电源正常？
  │       └─ 波特率匹配？
  │
  └─ 否 → 第一帧能响应？
           ├─ 是 → 检查 HAL_UART_TxCpltCallback
           │       └─ 是否重启DMA？
           │
           └─ 否 → 检查CRC
                   ├─ 计算正确？
                   └─ 字节序正确？
```

---

## 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| 1.0.0 | 2025-10-18 | [姓名] | 初始版本 |

---

**测试完成后，请将此文档归档到项目仓库的 `/docs/testing/` 目录。**

