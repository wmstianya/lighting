# 模块化Modbus RTU设计说明

## 📁 文件结构

### 核心层（与硬件无关）
- `modbus_rtu_core.h/c` - Modbus RTU协议核心实现
  - CRC计算
  - 帧处理
  - 功能码处理（01,03,04,05,06,10h）
  - 数据管理

### 硬件抽象层
- `modbus_port.h` - 硬件接口定义
- `modbus_port_uart1.c` - UART1端口实现（PA9/PA10）
- `modbus_port_uart2.c` - UART2端口实现（PA2/PA3）

### 应用层
- `modbus_app.h/c` - 应用层管理
  - 多实例管理
  - 数据初始化
  - 回调处理

### 辅助文件
- `modbus_it_adapter.c` - 中断适配器
- `main_modbus_example.c` - 使用示例

## 🚀 快速开始

### 1. 添加文件到工程

在Keil项目中添加以下文件：
```
Core/Src/
  ├── modbus_rtu_core.c
  ├── modbus_port_uart1.c
  ├── modbus_port_uart2.c
  └── modbus_app.c

Core/Inc/
  ├── modbus_rtu_core.h
  ├── modbus_port.h
  └── modbus_app.h
```

### 2. 在main.c中初始化

```c
#include "modbus_app.h"

int main(void)
{
    // 系统初始化...
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    // 初始化Modbus
    ModbusApp_Init();
    
    while (1) {
        // 处理Modbus通信
        ModbusApp_Process();
        
        // 其他应用代码...
        HAL_Delay(1);
    }
}
```

### 3. 更新中断处理

在`stm32f1xx_it.c`中：

```c
#include "modbus_port.h"

void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        ModbusPort_UART1_IdleCallback();
        return;
    }
    HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        ModbusPort_UART2_IdleCallback();
        return;
    }
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        ModbusPort_UART1_TxCpltCallback();
    }
    else if (huart == &huart2) {
        ModbusPort_UART2_TxCpltCallback();
    }
}
```

## 📊 默认配置

### UART1 Modbus
- 从站地址：0x01
- 保持寄存器：100个（初始值1000-1099）
- 输入寄存器：50个（初始值2000-2049）
- 线圈：80个（前8个ON）
- 离散输入：40个
- RS485控制：PA8
- LED指示：PB0

### UART2 Modbus
- 从站地址：0x02
- 保持寄存器：100个（初始值3000-3099）
- 输入寄存器：50个（初始值4000-4049）
- 线圈：80个（前4个ON）
- 离散输入：40个
- RS485控制：PA4
- LED指示：PB1

## 🔧 自定义配置

### 修改从站地址
```c
ModbusRTU_t *mb = ModbusApp_GetUart1Instance();
ModbusRTU_SetSlaveAddr(mb, 0x10);
```

### 添加回调函数
```c
// 在modbus_app.c中
static void uart1_onCoilChanged(uint16_t addr, uint8_t value)
{
    if (addr < 5) {
        // 控制继电器
        relaySetState(addr, value);
    }
}

// 设置回调
modbusUart1.onCoilChanged = uart1_onCoilChanged;
```

### 访问数据
```c
ModbusRTU_t *mb = ModbusApp_GetUart1Instance();

// 读写寄存器
uint16_t value = ModbusRTU_ReadHoldingReg(mb, 0);
ModbusRTU_WriteHoldingReg(mb, 0, 0x1234);

// 控制线圈
ModbusRTU_WriteCoil(mb, 0, 1);  // ON
uint8_t state = ModbusRTU_ReadCoil(mb, 0);
```

## 🎯 架构优势

1. **高内聚低耦合**
   - 协议层独立于硬件
   - 端口层封装硬件细节
   - 应用层管理业务逻辑

2. **可复用性**
   - 一套协议代码，多个串口实例
   - 易于添加新串口

3. **可移植性**
   - 更换MCU只需修改端口层
   - 协议层可直接复用

4. **可扩展性**
   - 易于添加新功能码
   - 支持自定义回调

## 📋 支持的功能码

| 功能码 | 功能 | 说明 |
|--------|------|------|
| 0x01 | Read Coils | 读线圈 |
| 0x03 | Read Holding Registers | 读保持寄存器 |
| 0x04 | Read Input Registers | 读输入寄存器 |
| 0x05 | Write Single Coil | 写单个线圈 |
| 0x06 | Write Single Register | 写单个寄存器 |
| 0x10 | Write Multiple Registers | 写多个寄存器 |

## 🐛 调试提示

1. 检查LED指示灯
   - 接收数据时LED会闪烁
   - 心跳LED每秒闪烁

2. 查看统计信息
```c
ModbusStats_t *stats = ModbusRTU_GetStats(mb);
printf("RX: %d, TX: %d, CRC Err: %d\n", 
       stats->rxFrameCount, 
       stats->txFrameCount, 
       stats->crcErrorCount);
```

3. 波特率配置
   - 确保UART配置正确（默认115200）
   - 检查RS485方向控制

## 📞 联系支持

如有问题，请查看示例文件`main_modbus_example.c`或联系开发团队。

---
*Lighting Ultra Team - 2025-11-08*
