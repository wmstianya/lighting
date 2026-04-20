# Modbus 灯管控制回归测试

## 依赖

```bash
pip install pymodbus pyserial
```

## 用法

```bash
python test_modbus_lamp.py --port COM3 --slave 1 --baud 115200
```

Linux/Mac：

```bash
python test_modbus_lamp.py --port /dev/ttyUSB0 --slave 1 --baud 115200
```

## 测试覆盖

| 编号 | 测试项 | 覆盖内容 |
|------|-------|----------|
| 0 | 寄存器表版本 | 读 `REG_INPUT_MAP_VERSION`，验证 = 0x0100 |
| 1 | FC 0x02 读离散输入 | 修补后的功能码可用 |
| 2 | FC 0x0F 写多个线圈 | 批量控制 5 路灯 (0b10101) |
| 3 | 模式切换 | 手动 ↔ 智能，验证 `REG_INPUT_ACTIVE_MODE` 镜像 |
| 4 | 场景切换 | 依次写场景 1~5，验证 DO 位图与 `REG_INPUT_ACTIVE_SCENE` |
| 5 | 仲裁拒绝 | 智能模式下手动写线圈应无效 |
| 6 | 调度命中 | 配置一段 08:00~10:00 全开，授时到 08:30 应自动切到场景 1；授时到 07:00 应未命中 |
| 7 | 授时标志 | `REG_INPUT_TIME_SYNCED` 在授时后应为 1 |

## 预期输出（全通过）

```
=== Modbus 灯管测试 (slave=1) ===

[0] 寄存器表版本
  [PASS] REG_MAP_VERSION sanity — 读到 0x0100，期望 0x0100

[1] FC 0x02 读离散输入
  [PASS] FC 0x02 读离散输入 — 读到 X/32 位为 1

...

==================================================
结果: 8/8 通过
```

## 双 UART 测试

本项目支持双 UART（UART1=0x01, UART2=0x02）。将两个测试分别跑一遍：

```bash
# UART1
python test_modbus_lamp.py --port COM3 --slave 1

# UART2
python test_modbus_lamp.py --port COM4 --slave 2
```

两路结果应完全一致（`ENABLE_DUAL_UART_SYNC=1` 时）。

## 故障排查

- **所有测试 FAIL 且报超时**：检查波特率、串口号、从站地址；确认 RS485 DE 方向控制正常
- **FC 0x02 FAIL**：固件未包含 Step 1 的修补
- **场景切换 FAIL**：检查继电器物理接线，`relayGetAllStates()` 是否正确回读
- **调度命中 FAIL**：检查 `HAL_GetTick()` 是否正常增长；授时是否被正确接收（读 `REG_INPUT_TIME_SYNCED`）
