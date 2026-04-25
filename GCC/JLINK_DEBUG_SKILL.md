---
name: lighting-ultra-jlink
description: >-
  Build, flash, and debug the lighting_ultra STM32F103C8T6 project via J-Link SWD.
  Use when the user wants to compile firmware, flash to hardware, or debug with GDB.
---

# lighting_ultra J-Link Flash & Debug

## Hardware

- **MCU**: STM32F103C8T6 (Cortex-M3, 64K Flash, 20K RAM)
- **Debugger**: J-Link SWD
- **SWD Pinout**: SWDIO, SWDCLK, GND, VCC (3.3V)

## Tool Paths (This Machine)

```
J-Link GDB Server : D:\jlink\JLinkGDBServerCL.exe
J-Link Commander  : D:\jlink\JLink.exe
arm-none-eabi-gdb : C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-gdb.exe
ELF output        : E:\data\lighting_ultra\lighting_ultra\build\output\lighting_ultra.elf
Project root      : E:\data\lighting_ultra\lighting_ultra
```

## Quick Commands

### 一键编译 + 烧录
```bat
cd E:\data\lighting_ultra\lighting_ultra
jlink_flash.bat
```

### 仅烧录（跳过编译）
```bat
jlink_flash.bat norebuild
```

### 交互式 GDB 调试
```bat
jlink_debug.bat
```
进入 GDB 后执行：
```gdb
target remote localhost:2331
monitor halt
print modbusUart1.stats
monitor go
```

### 一键读取关键变量（非交互）
```bat
jlink_debug.bat read     -- 心跳 + 传感器 + 错误码
jlink_debug.bat modbus   -- Modbus 统计 + 保持寄存器
jlink_debug.bat lamp     -- 灯管管理器 + 调度器内部状态
jlink_debug.bat stop     -- 关闭 GDB Server
```

### Cursor / VSCode 图形调试
安装 `Cortex-Debug` 插件后，按 F5 选择：
- **J-Link Flash & Debug** — 编译 + 烧录 + 在 main() 停住
- **J-Link Attach** — 附加到正在运行的目标，不重烧

## GDB 常用命令速查

```gdb
# 连接 & 控制
target remote localhost:2331   -- 连接 GDB Server
monitor halt                    -- 暂停 CPU
monitor go                      -- 继续运行
monitor reset                   -- 复位（保持调试连接）

# 打印变量（需要 ELF 符号）
print modbusUart1.stats               -- Modbus UART1 统计
print modbusUart2.stats               -- Modbus UART2 统计
print s_mode                          -- 灯管模式 (lamp_manager.c)
print s_activeScene                   -- 当前场景
print s_lastCommittedBits             -- 最后提交的 DO 位图
print s_curHHMM                       -- 调度器当前时间
print s_timeSynced                    -- 是否已授时
print s_entries                       -- 调度表（8段）

# 读内存
x/16xh &uart1_holdingRegs             -- 保持寄存器 0~15
x/12xh &uart1_inputRegs               -- 输入寄存器 0~11

# STM32F103 常用外设寄存器
x/1xw 0x40013800   -- USART1->SR
x/1xw 0x40013804   -- USART1->DR
x/1xw 0x40004400   -- USART2->SR
x/1xw 0x40004404   -- USART2->DR
x/1xw 0x40010C00   -- GPIOA->CRL (PA0~PA7 模式)
x/1xw 0x40010C08   -- GPIOA->IDR (输入状态)
x/1xw 0x40010C0C   -- GPIOA->ODR (输出状态)
x/1xw 0x40010C10   -- GPIOB->CRL

# 断点
break modbus_app.c:dispatchRegWrite   -- 在寄存器写入回调设断点
break lamp_manager.c:commitBitsToHardware -- 在 DO 输出时停住
continue                              -- 继续运行到断点
next                                  -- 单步（不进入函数）
step                                  -- 单步（进入函数）

# 退出
quit                                  -- 退出 GDB（Server 继续运行）
```

## Workflow: 烧录并调试一次完整流程

```
1. jlink_flash.bat          ← 编译 + 烧录到板子，自动复位运行
2. jlink_debug.bat modbus   ← 读 Modbus 统计确认通信正常
3. jlink_debug.bat lamp     ← 读灯管状态确认场景/模式正确
4. jlink_debug.bat stop     ← 关闭 GDB Server
```

## 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| `Cannot connect to target` | SWD 接线问题 / 板子未上电 | 检查 SWDIO/SWDCLK/GND/VCC |
| `Flash erase failed` | Flash 写保护 | 用 JLink Commander 执行 `unlock kinetis` 或 BOOT0 跳线 |
| GDB 打印变量显示地址而不是值 | ELF 不匹配当前固件 | 重新 `jlink_flash.bat` 使 ELF 与板子同步 |
| GDB Server 端口占用 | 上次未关闭 | `jlink_debug.bat stop` |
