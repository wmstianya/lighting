# Lighting Ultra System Architecture

## 系统概述

本系统是基于STM32F103C8T6的工业照明控制系统，采用模块化设计，支持Modbus RTU通信、多路继电器控制、传感器采集和智能监控。

## 硬件平台

- **MCU**: STM32F103C8T6 (64KB Flash, 20KB RAM, 72MHz)
- **外设**: UART1, UART2, ADC1, TIM1, DMA
- **外部芯片**: TPS3823-33DBVR (外部看门狗)

## 模块架构

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
├─────────────────────────────────────────────────────────┤
│ modbus_app.c  - Modbus应用层（双串口管理）                │
└─────────────────────────────────────────────────────────┘
         ↓                              ↓
┌──────────────────────┐    ┌──────────────────────────┐
│   Communication      │    │   System Services         │
├──────────────────────┤    ├──────────────────────────┤
│ modbus_rtu_core.c    │    │ config_manager.c (Flash) │
│ modbus_port_uart1.c  │    │ error_handler.c (Flash)  │
│ modbus_port_uart2.c  │    │ task_scheduler.c (TODO)  │
└──────────────────────┘    └──────────────────────────┘
         ↓                              ↓
┌──────────────────────────────────────────────────────────┐
│                    Driver Layer                          │
├──────────────────────────────────────────────────────────┤
│ Hardware Drivers      │  Sensor Drivers   │ Utils       │
│ ────────────────────  │  ────────────────  │ ──────────  │
│ relay.c (5路继电器)    │  pressure_sensor.c│ kalman.c    │
│ led.c (4路LED指示)     │  water_level.c    │             │
│ beep.c (PWM蜂鸣器)     │                   │             │
│ watchdog.c (外部看门狗)│                   │             │
└──────────────────────────────────────────────────────────┘
         ↓
┌──────────────────────────────────────────────────────────┐
│                    HAL Layer                             │
├──────────────────────────────────────────────────────────┤
│ STM32 HAL Driver (GPIO, UART, DMA, ADC, TIM, Flash)     │
└──────────────────────────────────────────────────────────┘
```

## 驱动模块清单

### 1. 硬件驱动 (Hardware Drivers)

| 模块 | 文件 | 功能 | 引脚 |
|------|------|------|------|
| **relay** | relay.h/c | 5路继电器控制 | PB4/PB3/PA15/PA12/PA11 |
| **led** | led.h/c | 4路LED指示灯（低电平点亮） | PB1/PB15/PB5/PB6 |
| **beep** | beep.h/c | 无源蜂鸣器PWM驱动（2700Hz） | PB14 (TIM1_CH2N) |
| **watchdog** | watchdog.h/c | 外部看门狗TPS3823-33DBVR | PC14 (WDI) |

### 2. 传感器驱动 (Sensor Drivers)

| 模块 | 文件 | 功能 | 引脚 |
|------|------|------|------|
| **pressure_sensor** | pressure_sensor.h/c | 4-20mA压力变送器（0-1.6MPa） | PB0 (ADC12_IN8) |
| **water_level** | water_level.h/c | 三点式水位检测（低/中/高） | PA0/PA1/PA5 (DI1/DI2/DI3) |

### 3. 算法工具 (Utils)

| 模块 | 文件 | 功能 |
|------|------|------|
| **kalman** | kalman.h/c | 一维卡尔曼滤波器 |

### 4. 系统服务 (System Services)

| 模块 | 文件 | 功能 |
|------|------|------|
| **config_manager** | config_manager.h/c | Flash配置存储和管理 |
| **error_handler** | error_handler.h/c | 统一错误处理和Flash日志 |

### 5. 通信协议 (Communication)

| 模块 | 文件 | 功能 |
|------|------|------|
| **modbus_rtu_core** | modbus_rtu_core.h/c | Modbus RTU核心协议 |
| **modbus_port_uart1** | modbus_port_uart1.h/c | UART1硬件端口适配 |
| **modbus_port_uart2** | modbus_port_uart2.h/c | UART2硬件端口适配 |
| **modbus_app** | modbus_app.h/c | Modbus应用层管理 |

## Flash分区布局

```
STM32F103C8T6 Flash (64KB = 32页 × 2KB)
┌────────────────────────────────────┐
│ 0x08000000                         │
│ ├─ 程序代码区 (约60KB)              │
│ │  Page 0-29                       │
│ │  (应用程序 .text .data .rodata)  │
│ │                                  │
│ ├─ 0x0800F000 (Page 30, 2KB)      │
│ │  错误日志区                       │
│ │  - 循环日志（最近62条）           │
│ │  - 32字节/条                     │
│ │                                  │
│ ├─ 0x0800F800 (Page 31, 2KB)      │
│ │  系统配置区                       │
│ │  - Modbus地址                    │
│ │  - 传感器参数                     │
│ │  - CRC32校验                     │
│ └─ 0x0800FFFF                      │
└────────────────────────────────────┘
```

## Modbus寄存器映射

### 输入寄存器 (Input Registers - 只读)

| 地址 | 寄存器 | 数据 | 单位 | 说明 |
|------|--------|------|------|------|
| 0 | 30001 | 计数器 | - | 运行计数 |
| 1 | 30002 | 压力值 | 0.001 MPa | 压力×1000 |
| 2 | 30003 | 电流值 | 0.01 mA | 电流×100 |
| 3 | 30004 | ADC原始值 | - | 0-4095 |
| 4 | 30005 | 压力有效标志 | - | 1=有效, 0=无效 |
| 5 | 30006 | 错误掩码（低16位） | - | bit对应错误码 |
| 6 | 30007 | 错误掩码（高16位） | - | bit对应错误码 |
| 7 | 30008 | 错误日志总数 | - | 0-100 |

### 保持寄存器 (Holding Registers - 可读写)

| 地址 | 寄存器 | 功能 | 说明 |
|------|--------|------|------|
| 0 | 40001 | DO控制 | bit0-4控制DO1-DO5 |
| 1-99 | 40002-40100 | 用户数据 | 可自定义 |

### 线圈 (Coils - 可读写)

| 地址 | 功能 |
|------|------|
| 0-4 | DO1-DO5控制 |

### 离散输入 (Discrete Inputs - 只读)

**字节0 (地址10001-10008)：压力传感器状态**
- Bit 0: 压力数据有效
- Bit 1: 压力 > 1.0 MPa
- Bit 2: 压力 > 1.4 MPa (报警)

**字节1 (地址10009-10016)：水位探针原始状态**
- Bit 0: DI1低水位探针有水
- Bit 1: DI2中水位探针有水
- Bit 2: DI3高水位探针有水

**字节2 (地址10017-10024)：水位状态编码**
- 0x00: 无水
- 0x01: 低水位
- 0x02: 中水位
- 0x03: 高水位
- 0xFF: 异常
- Bit 7: 水位稳定标志

## 信号处理流程

### 压力传感器采集
```
4-20mA变送器 → 250Ω采样电阻 → PB0(ADC12_IN8)
    ↓
ADC采样(12位) → 10次平均 → 电压(V)
    ↓
电流计算(mA) → 有效性检查(3.5-20.5mA)
    ↓
线性转换 → 压力值(MPa)
    ↓
卡尔曼滤波 → 平滑输出
    ↓
Modbus输入寄存器
```

### 水位检测流程
```
3个水位探针(PA0/PA1/PA5) → GPIO上拉输入（低电平有效）
    ↓
50ms周期采样 → 状态变化检测
    ↓
200ms防抖处理 → 稳定状态确认
    ↓
水位逻辑判断 → 异常检测
    ↓
Modbus离散输入
```

## 配置管理

### Flash存储配置项

```c
SystemConfig_t {
    // Modbus
    modbus1SlaveAddr    // UART1地址 (1-247)
    modbus2SlaveAddr    // UART2地址 (1-247)
    modbus1Baudrate     // UART1波特率
    modbus2Baudrate     // UART2波特率
    
    // 传感器
    pressureMin         // 压力下限 (MPa)
    pressureMax         // 压力上限 (MPa)
    
    // 系统
    enableWatchdog      // 看门狗使能
    enableBeep          // 蜂鸣器使能
    
    // 校验
    checksum            // CRC32校验和
}
```

### 配置API
```c
configManagerInit();          // 初始化（从Flash加载）
const SystemConfig_t* cfg = configGet();  // 获取配置
configSetModbus(addr1, addr2);  // 修改Modbus地址
configSetPressure(min, max);  // 修改压力量程
configSave();                 // 保存到Flash
configResetToDefault();       // 恢复出厂设置
```

## 错误处理框架

### 错误分类

```
系统错误 (0x00-0x0F)
通信错误 (0x10-0x1F)
传感器错误 (0x20-0x2F)
水位错误 (0x30-0x3F)
硬件错误 (0x40-0x4F)
配置错误 (0x50-0x5F)
```

### 错误处理API

```c
// 错误报告
ERROR_REPORT_INFO(code, "message");
ERROR_REPORT_WARNING(code, "message");
ERROR_REPORT_ERROR(code, "message");
ERROR_REPORT_CRITICAL(code, "message");

// 错误查询
bool isActive = errorIsActive(ERROR_ADC_INIT_FAILED);
uint32_t mask = errorGetActiveMask();
ErrorStats_t stats = errorGetStats(code);

// 错误清除
errorClear(code);
errorClearAll();

// Flash日志
errorLogWrite(code, level, "message");
uint16_t count = errorLogRead(buffer, 10);  // 读最近10条
uint16_t total = errorLogGetCount();
errorLogClear();
```

## 系统初始化流程

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    // 硬件初始化
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    // 系统服务初始化
    configManagerInit();      // ← 1. 加载配置
    errorHandlerInit();       // ← 2. 错误处理
    
    // 获取配置
    const SystemConfig_t* config = configGet();
    
    // 驱动模块初始化
    beepInit();
    ledInit();
    watchdogInit();
    pressureSensorInit(config->pressureMin, config->pressureMax);  // 使用配置
    waterLevelInit();
    
    // Modbus初始化（使用配置地址）
    ModbusApp_Init();
    
    // 系统就绪提示
    beepSetTime(200);
    
    // 主循环
    while (1) {
        ModbusApp_Process();
        beepProcess();
        pressureSensorProcess();
        waterLevelProcess();
        watchdogFeed();
        
        // 定期更新Modbus数据
        ModbusApp_UpdateSensorData();  // 1秒周期
        
        HAL_Delay(1);
    }
}
```

## 关键技术特性

### 1. 非阻塞设计
- ✅ 所有模块基于时间戳轮询
- ✅ 无阻塞式延时（HAL_Delay仅用于主循环1ms）
- ✅ Modbus通信不受影响

### 2. 信号处理
- ✅ ADC多次采样平均（10次）
- ✅ 卡尔曼滤波平滑噪声
- ✅ 水位200ms防抖

### 3. 可靠性保障
- ✅ 外部看门狗监控（1.6s超时）
- ✅ Flash配置CRC32校验
- ✅ Flash日志循环存储（100条）
- ✅ 传感器故障检测

### 4. 可配置性
- ✅ Modbus地址可配置
- ✅ 传感器参数可配置
- ✅ Flash掉电保存
- ✅ 支持恢复出厂设置

## 编码规范

### 命名规范
- **函数/变量**: camelCase (如: `pressureSensorInit`)
- **常量**: UPPER_CASE (如: `ADC_RESOLUTION`)
- **禁止数字后缀**: ❌ `uart1Init`, ✅ `uartMainInit`

### 模块规范
- **头文件**: Include guards, 函数声明, 注释
- **源文件**: 私有函数static, 不超过20行/函数
- **注释**: Doxygen格式，包含参数、返回值、注意事项

### 错误处理
- ❌ 禁止: `while(1);`
- ✅ 使用: `ERROR_REPORT_XXX()` 宏
- ✅ 返回错误码，允许恢复

## 性能指标

| 指标 | 值 | 说明 |
|------|-----|------|
| Modbus响应时间 | <10ms | 中断+DMA |
| 压力采样周期 | 100ms | 10次平均+卡尔曼 |
| 水位采样周期 | 50ms | 200ms防抖 |
| 看门狗喂狗周期 | 500ms | 超时1.6s |
| CPU占用率 | <30% | 估算值 |

## 后续扩展计划

### 近期（2周内）
- [ ] 任务调度器 (task_scheduler.c)
- [ ] 系统状态机 (system_state.c)
- [ ] 水位异常细分

### 长期优化
- [ ] Flash日志查询接口（通过Modbus）
- [ ] 性能监控（CPU使用率）
- [ ] 自检诊断功能

## 开发者注意事项

1. **Flash寿命**: Flash写入次数有限（10K次），不要频繁保存配置
2. **看门狗**: 确保主循环1ms能执行一次，否则会复位
3. **ADC精度**: 采样电阻必须是250Ω ±1%精度
4. **蜂鸣器**: 无源蜂鸣器，必须PWM驱动，谐振频率2700Hz
5. **水位探针**: 上拉输入，低电平有效

## 版本信息

- **版本**: v1.0.0
- **日期**: 2025-11-11
- **作者**: Lighting Ultra Team
- **MCU**: STM32F103C8T6
- **编译器**: Keil MDK-ARM v5

