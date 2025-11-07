# 双串口回环测试指南

**项目**: STM32F103 Modbus RTU双串口系统  
**版本**: 1.0.0  
**日期**: 2025-10-18

---

## 📋 目录

1. [测试概述](#测试概述)
2. [硬件连接](#硬件连接)
3. [测试模式](#测试模式)
4. [快速开始](#快速开始)
5. [集成到项目](#集成到项目)
6. [测试结果判读](#测试结果判读)
7. [故障排查](#故障排查)

---

## 📝 测试概述

回环测试用于验证双串口Modbus RTU系统的硬件和软件功能是否正常。

### 测试覆盖范围

| 测试项 | 验证内容 | 是否必须 |
|--------|---------|---------|
| ✅ RS485方向控制 | PA4/PA8切换是否正常 | **必须** |
| ⚙️ 硬件回环 | UART+DMA基础功能 | 可选 |
| 🔄 双串口交叉 | 两个串口互发互收 | 可选 |
| 📦 Modbus功能 | 完整协议栈测试 | 推荐 |
| 💪 压力测试 | 稳定性和性能 | 推荐 |

---

## 🔌 硬件连接

### 测试1: RS485方向控制（无需额外连线）

```
只需要正常的RS485连接：
- 串口1: PA4 控制 DE/RE
- 串口2: PA8 控制 DE/RE

无需短接或交叉连线
```

### 测试2: 硬件回环（需要短接）

```
串口1 (USART2):
  PA2 (TX) ──┐
              ├── 短接
  PA3 (RX) ──┘

或

串口2 (USART1):
  PA9 (TX)  ──┐
               ├── 短接
  PA10 (RX) ──┘
```

### 测试3: 双串口交叉（需要交叉连线）

```
USART1          USART2
PA9 (TX) ────── PA3 (RX)
PA10(RX) ────── PA2 (TX)
GND ──────────── GND
```

---

## 🧪 测试模式

### 模式1: RS485方向控制测试 ⭐ **推荐首选**

**目的**: 验证PA4和PA8的DE/RE切换功能

**硬件要求**: 无需额外连线

**测试内容**:
1. 设置为发送模式 (HIGH)
2. 验证GPIO状态
3. 设置为接收模式 (LOW)
4. 验证GPIO状态
5. 快速切换100次

**预期结果**: 
- GPIO能正确读写
- 切换无延迟或毛刺

**使用方法**:
```c
TestResult result = {0};
loopbackTestInit(&huart1, &huart2);
bool passed = loopbackTestRun(TEST_MODE_RS485_DIRECTION, &result);
```

---

### 模式2: 硬件回环测试

**目的**: 验证UART+DMA基础收发功能

**硬件要求**: 需要TX-RX短接

**测试内容**:
1. 生成32字节测试数据（递增模式）
2. 通过UART发送
3. 同时启动DMA接收
4. 验证接收数据与发送数据一致

**预期结果**: 
- 发送和接收的数据完全一致
- 无超时、无丢失

**使用方法**:
```c
TestResult result = {0};
loopbackTestInit(&huart1, &huart2);
bool passed = loopbackTestRun(TEST_MODE_HARDWARE_LOOPBACK, &result);
```

---

### 模式3: 双串口交叉测试

**目的**: 验证两个串口可以互相通信

**硬件要求**: 需要交叉连线

**测试内容**:
1. 串口1发送 → 串口2接收
2. 验证数据正确性
3. 串口2发送 → 串口1接收
4. 验证数据正确性

**预期结果**: 
- 双向通信正常
- 数据无损失

**使用方法**:
```c
TestResult result = {0};
loopbackTestInit(&huart1, &huart2);
bool passed = loopbackTestRun(TEST_MODE_DUAL_UART_CROSSOVER, &result);
```

---

### 模式4: Modbus功能测试

**目的**: 验证完整的Modbus RTU协议栈

**硬件要求**: 根据测试方式选择

**测试内容**:
1. 构造Modbus读保持寄存器请求 (0x03)
2. 发送到从站
3. 接收响应
4. 验证响应格式和CRC

**预期结果**: 
- 能正确解析Modbus帧
- CRC校验通过
- 响应数据正确

**使用方法**:
```c
TestResult result = {0};
loopbackTestInit(&huart1, &huart2);
bool passed = loopbackTestRun(TEST_MODE_MODBUS_LOOPBACK, &result);
```

---

### 模式5: 压力测试

**目的**: 测试系统稳定性和性能

**硬件要求**: 同硬件回环测试

**测试内容**:
1. 连续发送接收1000次
2. 使用不同的数据模式
3. 统计成功率

**预期结果**: 
- 成功率 ≥ 99%
- 无内存泄漏
- 无死锁

**使用方法**:
```c
TestResult result = {0};
loopbackTestInit(&huart1, &huart2);
bool passed = loopbackTestRun(TEST_MODE_STRESS_TEST, &result);
// 或指定次数
bool passed = loopbackTestStress(5000, &result);
```

---

## 🚀 快速开始

### 步骤1: 添加文件到项目

将以下文件添加到Keil项目中：
- `Core/Inc/uart_loopback_test.h`
- `Core/Src/uart_loopback_test.c`
- `Core/Src/main_test_example.c` (参考)

### 步骤2: 最小测试代码

在 `main.c` 的 `main()` 函数中添加：

```c
#include "uart_loopback_test.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    /* ========== 执行快速测试 ========== */
    TestResult result = {0};
    loopbackTestInit(&huart1, &huart2);
    
    // 只测试RS485方向控制（最基础、最重要）
    if (loopbackTestRun(TEST_MODE_RS485_DIRECTION, &result)) {
        // 测试通过：LED快闪3次
        for (int i = 0; i < 3; i++) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(200);
        }
    } else {
        // 测试失败：LED常亮
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        while(1);  // 停止运行
    }
    
    /* 继续正常的Modbus初始化 */
    ModbusRTU_Init(&g_mb,  &huart1, 0x01);
    ModbusRTU_Init(&g_mb2, &huart2, 0x02);
    
    while (1) {
        ModbusRTU_Process(&g_mb);
        ModbusRTU_Process(&g_mb2);
        HAL_Delay(1);
    }
}
```

### 步骤3: 编译并烧录

```bash
# Keil MDK
按 F7 编译
按 F8 烧录
```

### 步骤4: 观察LED指示

| LED状态 | 含义 |
|--------|------|
| 快闪3次 | 测试通过 ✅ |
| 常亮 | 测试失败 ❌ |
| 不亮 | 未执行测试或硬件问题 |

---

## 🔧 集成到项目

### 方案1: 启动时自动测试（推荐）

```c
int main(void)
{
    // ... 初始化代码 ...
    
    #ifdef DEBUG
    // 仅在调试版本中执行测试
    loopbackTestExample();
    #endif
    
    // ... Modbus初始化和主循环 ...
}
```

### 方案2: 按键触发测试

```c
/* 在主循环中检测按键 */
while (1) {
    if (HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin) == GPIO_PIN_RESET) {
        // 按下用户按键，执行测试
        loopbackTestExample();
        HAL_Delay(1000);  // 防抖
    }
    
    ModbusRTU_Process(&g_mb);
    ModbusRTU_Process(&g_mb2);
    HAL_Delay(1);
}
```

### 方案3: 条件编译

```c
/* 在编译选项中定义 ENABLE_LOOPBACK_TEST */
#ifdef ENABLE_LOOPBACK_TEST
    loopbackTestExample();
#endif
```

---

## 📊 测试结果判读

### TestResult结构体

```c
typedef struct {
    uint32_t totalTests;      // 总测试数
    uint32_t passedTests;     // 通过数
    uint32_t failedTests;     // 失败数
    uint32_t timeoutErrors;   // 超时错误
    uint32_t crcErrors;       // CRC错误
    uint32_t txCount;         // 发送字节数
    uint32_t rxCount;         // 接收字节数
    char     lastError[64];   // 最后一个错误信息
} TestResult;
```

### 通过标准

| 测试项 | 通过条件 |
|--------|---------|
| RS485方向 | GPIO读写一致，无异常 |
| 硬件回环 | 发送数据 == 接收数据 |
| 双串口交叉 | 双向通信均成功 |
| Modbus功能 | 响应格式正确，CRC通过 |
| 压力测试 | 成功率 ≥ 99% |

### 示例输出（通过调试器查看）

```
正常情况:
  totalTests   = 1
  passedTests  = 1
  failedTests  = 0
  lastError    = ""

失败情况:
  totalTests   = 1
  passedTests  = 0
  failedTests  = 1
  lastError    = "TX mode failed"
```

---

## 🔍 故障排查

### 问题1: RS485方向测试失败

**现象**: `lastError = "TX mode failed"` 或 `"RX mode failed"`

**可能原因**:
1. PA4或PA8未配置为推挽输出
2. GPIO时钟未使能
3. 引脚被其他功能占用

**解决方法**:
```c
// 检查 main.c 的 MX_GPIO_Init() 是否包含：
HAL_GPIO_WritePin(MB_USART1_RS485_DE_GPIO_Port, MB_USART1_RS485_DE_Pin, GPIO_PIN_RESET);
GPIO_InitStruct.Pin   = MB_USART1_RS485_DE_Pin;
GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  // 必须是推挽输出
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(MB_USART1_RS485_DE_GPIO_Port, &GPIO_InitStruct);
```

---

### 问题2: 硬件回环测试超时

**现象**: `lastError = "RX timeout"`

**可能原因**:
1. TX和RX未短接
2. UART配置错误（波特率、数据位等）
3. DMA未正确初始化

**解决方法**:
1. 用万用表或示波器检查TX-RX是否短接
2. 检查波特率配置（应该是9600）
3. 检查DMA通道分配

---

### 问题3: 数据不匹配

**现象**: `lastError = "Data mismatch"`

**可能原因**:
1. 数据丢失
2. 接收缓冲区被覆盖
3. DMA配置错误

**解决方法**:
```c
// 检查DMA配置
hdma_usart1_rx.Init.Mode = DMA_NORMAL;  // 必须是NORMAL模式
hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;  // 必须使能内存递增
```

---

### 问题4: 双串口交叉测试失败

**现象**: `lastError = "UART1->UART2 TX failed"`

**可能原因**:
1. 交叉连线错误
2. 两个串口GPIO冲突
3. 波特率不一致

**解决方法**:
1. 仔细检查连线：
   - PA9 (USART1 TX) → PA3 (USART2 RX)
   - PA2 (USART2 TX) → PA10 (USART1 RX)
2. 确保两个串口波特率相同
3. 确保GND共地

---

### 问题5: 压力测试成功率低

**现象**: 成功率 < 99%

**可能原因**:
1. 系统负载过高
2. 中断优先级配置不当
3. DMA冲突

**解决方法**:
1. 检查中断优先级（UART中断应该比其他高）
2. 减少主循环中的延迟
3. 增加测试数据间隔

---

## 📱 LED指示含义

### 启动时测试

| LED闪烁模式 | 含义 |
|-----------|------|
| 快闪3次 → 暂停 → 快闪N次 | 开始测试 → 测试完成 → N个测试通过 |
| 快闪3次 → 暂停 → 常亮 | 开始测试 → 测试完成 → 全部失败 |
| 不亮 | 测试未执行或系统未启动 |

### 单项测试

| LED闪烁模式 | 含义 |
|-----------|------|
| 快闪2次 | 单项测试通过 ✅ |
| 常亮2秒 | 单项测试失败 ❌ |

---

## 🎯 推荐测试流程

### 第一次使用

1. **基础测试** - RS485方向控制
   - 无需额外连线
   - 验证GPIO基础功能
   - **必须通过**

2. **可选测试** - 硬件回环
   - 短接TX-RX
   - 验证UART+DMA
   - 可选，但推荐

3. **集成测试** - Modbus功能
   - 正常连接RS485
   - 完整协议测试
   - 推荐

### 生产测试

1. 快速测试（5秒）
   - `loopbackTestQuick()`
   - 仅测RS485方向

2. 完整测试（30秒）
   - `loopbackTestRunAll()`
   - 全部测试项目

3. 压力测试（可选，5分钟）
   - `loopbackTestBenchmark()`
   - 验证长期稳定性

---

## 📚 参考资料

- [STM32F103 数据手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [HAL库用户手册](https://www.st.com/resource/en/user_manual/dm00105879.pdf)
- [Modbus RTU协议规范](https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)

---

## ✅ 快速检查清单

在开始测试前，确认：

- [ ] 硬件连接正确（根据测试模式）
- [ ] 固件已烧录到STM32
- [ ] LED能正常工作（可以看到闪烁）
- [ ] 串口已正确初始化（MX_USART1/2_UART_Init）
- [ ] RS485使能引脚已配置（PA4, PA8）
- [ ] DMA已启用并配置正确
- [ ] 测试代码已添加到main()函数
- [ ] 编译无错误、无警告

---

**测试完成后，请保存测试结果记录！** 📝


