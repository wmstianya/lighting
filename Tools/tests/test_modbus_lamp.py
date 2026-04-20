"""
Modbus RTU 灯管控制回归测试

覆盖项:
  1. FC 0x02 读离散输入 — 验证修补后的传感器/模式状态可读
  2. FC 0x0F 写多个线圈 — 验证多灯同时控制
  3. 模式切换（手动/智能/应急）
  4. 场景切换（1~5）
  5. 调度命中（配置一段调度，推进时间验证自动切换）
  6. 仲裁拒绝（智能模式下手动写线圈应被拒）

依赖:
  pip install pymodbus pyserial

用法:
  python test_modbus_lamp.py --port COM3 --slave 1 --baud 115200

寄存器约定参考:
  Core/Doc/ModbusRegisterMap.md
  Core/Inc/modbus_reg_map.h
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass

from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException


# ---------------------------- 寄存器常量（与固件同步） ----------------------------
REG_HOLD_MODE              = 0x0000
REG_HOLD_LAMP_BITS_RW      = 0x0001
REG_HOLD_SCENE_REQ         = 0x0002
REG_HOLD_OVERRIDE_SEC      = 0x0003
REG_HOLD_SCENE_MASK_BASE   = 0x0010
REG_HOLD_TIME_HHMM         = 0x0020
REG_HOLD_TIME_DOW          = 0x0021
REG_HOLD_SCHEDULE_BASE     = 0x0030
REG_HOLD_SCHEDULE_ENABLE   = 0x0050
REG_HOLD_CMD               = 0x00F0

REG_INPUT_HEARTBEAT        = 0x0000
REG_INPUT_WATER_LEVEL      = 0x0005
REG_INPUT_ACTIVE_SCENE     = 0x0006
REG_INPUT_ACTIVE_MODE      = 0x0007
REG_INPUT_SCHEDULE_HIT     = 0x0008
REG_INPUT_LAMP_BITS_ACTUAL = 0x0009
REG_INPUT_TIME_SYNCED      = 0x000B
REG_INPUT_MAP_VERSION      = 0x001F

COIL_LAMP_BASE             = 0
COIL_LAMP_COUNT            = 5
COIL_BEEP_ONCE             = 5

LAMP_MODE_MANUAL           = 0
LAMP_MODE_SMART            = 1
LAMP_MODE_EMERGENCY        = 2

SCENE_ALL_ON               = 1
SCENE_HALF_ECO             = 2
SCENE_MAIN_ONLY            = 3
SCENE_EMERGENCY            = 4
SCENE_ALL_OFF              = 5

DOW_BIT_IGNORE             = 0x80

REG_MAP_VERSION_EXPECTED   = 0x0100


# ------------------------------ 测试结果结构 ------------------------------
@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str = ""


# ------------------------------ 核心测试用例 ------------------------------
class LampModbusTester:
    def __init__(self, port: str, slave: int, baud: int):
        self.slave = slave
        self.client = ModbusSerialClient(
            port=port,
            baudrate=baud,
            bytesize=8,
            stopbits=1,
            parity="N",
            timeout=1.0,
        )
        if not self.client.connect():
            raise RuntimeError(f"无法打开串口 {port}")
        self.results: list[TestResult] = []

    def close(self) -> None:
        self.client.close()

    # ----- 工具函数 -----
    def _read_hold(self, addr: int, count: int = 1) -> list[int]:
        rsp = self.client.read_holding_registers(address=addr, count=count, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"read_holding_registers({addr}) 失败: {rsp}")
        return list(rsp.registers)

    def _read_input(self, addr: int, count: int = 1) -> list[int]:
        rsp = self.client.read_input_registers(address=addr, count=count, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"read_input_registers({addr}) 失败: {rsp}")
        return list(rsp.registers)

    def _read_coils(self, addr: int, count: int = 1) -> list[bool]:
        rsp = self.client.read_coils(address=addr, count=count, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"read_coils({addr}) 失败: {rsp}")
        return list(rsp.bits)[:count]

    def _read_discrete(self, addr: int, count: int = 8) -> list[bool]:
        rsp = self.client.read_discrete_inputs(address=addr, count=count, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"read_discrete_inputs({addr}) 失败: {rsp}")
        return list(rsp.bits)[:count]

    def _write_single_reg(self, addr: int, value: int) -> None:
        rsp = self.client.write_register(address=addr, value=value, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"write_register({addr}) 失败: {rsp}")

    def _write_single_coil(self, addr: int, value: bool) -> None:
        rsp = self.client.write_coil(address=addr, value=value, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"write_coil({addr}) 失败: {rsp}")

    def _write_multi_coils(self, addr: int, values: list[bool]) -> None:
        rsp = self.client.write_coils(address=addr, values=values, slave=self.slave)
        if rsp.isError():
            raise ModbusException(f"write_coils({addr}) 失败: {rsp}")

    def _record(self, name: str, passed: bool, detail: str = "") -> None:
        status = "PASS" if passed else "FAIL"
        print(f"  [{status}] {name}" + (f" — {detail}" if detail else ""))
        self.results.append(TestResult(name, passed, detail))

    # ----- 测试用例 -----
    def test_reg_map_version(self) -> None:
        """寄存器表版本号 sanity check"""
        ver = self._read_input(REG_INPUT_MAP_VERSION, 1)[0]
        self._record(
            "REG_MAP_VERSION sanity",
            ver == REG_MAP_VERSION_EXPECTED,
            f"读到 0x{ver:04X}，期望 0x{REG_MAP_VERSION_EXPECTED:04X}",
        )

    def test_fc02_read_discrete(self) -> None:
        """FC 0x02 读离散输入：读 32 bits 覆盖 4 个状态字节"""
        try:
            bits = self._read_discrete(0, 32)
            self._record("FC 0x02 读离散输入", True, f"读到 {sum(bits)}/32 位为 1")
        except ModbusException as e:
            self._record("FC 0x02 读离散输入", False, str(e))

    def test_fc0f_write_multi_coils_manual(self) -> None:
        """FC 0x0F 写多个线圈（手动模式下）"""
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_MANUAL)
        time.sleep(0.05)
        pattern = [True, False, True, False, True]  # 0b10101 = 0x15
        self._write_multi_coils(COIL_LAMP_BASE, pattern)
        time.sleep(1.2)  # 等 1s 传感器刷新
        actual = self._read_input(REG_INPUT_LAMP_BITS_ACTUAL, 1)[0]
        expected_mask = 0x15
        self._record(
            "FC 0x0F 多线圈写入 (manual)",
            actual == expected_mask,
            f"硬件位图=0x{actual:02X}，期望=0x{expected_mask:02X}",
        )

    def test_mode_switch(self) -> None:
        """模式切换验证：手动 → 智能 → 手动"""
        for target in (LAMP_MODE_SMART, LAMP_MODE_MANUAL):
            self._write_single_reg(REG_HOLD_MODE, target)
            time.sleep(1.2)
            active = self._read_input(REG_INPUT_ACTIVE_MODE, 1)[0]
            name = {0: "MANUAL", 1: "SMART", 2: "EMERGENCY"}[target]
            self._record(
                f"模式切换到 {name}",
                active == target,
                f"REG_INPUT_ACTIVE_MODE={active}",
            )

    def test_scene_switch(self) -> None:
        """在手动模式下依次切换 5 个场景"""
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_MANUAL)
        time.sleep(0.1)
        expected_bits = {
            SCENE_ALL_ON:    0x1F,
            SCENE_HALF_ECO:  0x15,
            SCENE_MAIN_ONLY: 0x01,
            SCENE_EMERGENCY: 0x18,
            SCENE_ALL_OFF:   0x00,
        }
        for scene_id, exp_bits in expected_bits.items():
            self._write_single_reg(REG_HOLD_SCENE_REQ, scene_id)
            time.sleep(1.2)
            actual_bits = self._read_input(REG_INPUT_LAMP_BITS_ACTUAL, 1)[0]
            active_scene = self._read_input(REG_INPUT_ACTIVE_SCENE, 1)[0]
            ok = (actual_bits == exp_bits) and (active_scene == scene_id)
            self._record(
                f"场景 {scene_id} 切换",
                ok,
                f"bits=0x{actual_bits:02X}/期望 0x{exp_bits:02X}, active={active_scene}",
            )

    def test_arbitration_reject_in_smart(self) -> None:
        """智能模式下手动写线圈应被拒"""
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_MANUAL)
        self._write_single_reg(REG_HOLD_SCENE_REQ, SCENE_ALL_OFF)
        time.sleep(1.2)

        baseline = self._read_input(REG_INPUT_LAMP_BITS_ACTUAL, 1)[0]
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_SMART)
        time.sleep(0.1)
        # 在智能模式下尝试打开 DO1
        self._write_single_coil(0, True)
        time.sleep(1.2)
        after = self._read_input(REG_INPUT_LAMP_BITS_ACTUAL, 1)[0]
        self._record(
            "智能模式下手动写线圈被拒",
            after == baseline,
            f"baseline=0x{baseline:02X}, after=0x{after:02X}",
        )

    def test_schedule_hit(self) -> None:
        """配置调度段 → 授时命中 → 场景自动切换"""
        # 1) 切回手动避免干扰
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_MANUAL)
        self._write_single_reg(REG_HOLD_SCENE_REQ, SCENE_ALL_OFF)
        time.sleep(0.2)

        # 2) 配置段 0：08:00~10:00, 场景 1 "全开", 每天
        entry_base = REG_HOLD_SCHEDULE_BASE
        self._write_single_reg(entry_base + 0, 800)   # start
        self._write_single_reg(entry_base + 1, 1000)  # end
        self._write_single_reg(entry_base + 2, SCENE_ALL_ON)
        self._write_single_reg(entry_base + 3, DOW_BIT_IGNORE)

        # 3) 使能段 0
        self._write_single_reg(REG_HOLD_SCHEDULE_ENABLE, 0x01)

        # 4) 授时到 08:30（命中段内）
        self._write_single_reg(REG_HOLD_TIME_HHMM, 830)
        self._write_single_reg(REG_HOLD_TIME_DOW, 3)

        # 5) 切到智能模式
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_SMART)
        time.sleep(2.5)

        hit = self._read_input(REG_INPUT_SCHEDULE_HIT, 1)[0]
        scene = self._read_input(REG_INPUT_ACTIVE_SCENE, 1)[0]
        bits = self._read_input(REG_INPUT_LAMP_BITS_ACTUAL, 1)[0]
        ok = hit == 0 and scene == SCENE_ALL_ON and bits == 0x1F
        self._record(
            "调度命中 → 自动切场景",
            ok,
            f"hit={hit}, scene={scene}, bits=0x{bits:02X}",
        )

        # 6) 授时到 07:00（段外）
        self._write_single_reg(REG_HOLD_TIME_HHMM, 700)
        time.sleep(2.5)
        hit2 = self._read_input(REG_INPUT_SCHEDULE_HIT, 1)[0]
        self._record(
            "调度段外 → 未命中",
            hit2 == 0xFF,
            f"hit={hit2}",
        )

        # 7) 回手动 + 关灯清理
        self._write_single_reg(REG_HOLD_MODE, LAMP_MODE_MANUAL)
        self._write_single_reg(REG_HOLD_SCENE_REQ, SCENE_ALL_OFF)

    def test_time_synced_flag(self) -> None:
        """REG_INPUT_TIME_SYNCED 应为 1（上一个测试已授时）"""
        synced = self._read_input(REG_INPUT_TIME_SYNCED, 1)[0]
        self._record(
            "时间已被授时",
            synced == 1,
            f"time_synced={synced}",
        )

    # ----- 主流程 -----
    def run_all(self) -> bool:
        print(f"=== Modbus 灯管测试 (slave={self.slave}) ===")
        cases = [
            ("[0] 寄存器表版本", self.test_reg_map_version),
            ("[1] FC 0x02 读离散输入", self.test_fc02_read_discrete),
            ("[2] FC 0x0F 多线圈写入", self.test_fc0f_write_multi_coils_manual),
            ("[3] 模式切换", self.test_mode_switch),
            ("[4] 场景切换", self.test_scene_switch),
            ("[5] 智能模式仲裁拒绝", self.test_arbitration_reject_in_smart),
            ("[6] 调度命中", self.test_schedule_hit),
            ("[7] 授时标志", self.test_time_synced_flag),
        ]
        for title, fn in cases:
            print(f"\n{title}")
            try:
                fn()
            except Exception as exc:
                self._record(title, False, f"异常: {exc}")

        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        print("\n" + "=" * 50)
        print(f"结果: {passed}/{total} 通过")
        for r in self.results:
            if not r.passed:
                print(f"  FAIL: {r.name} — {r.detail}")
        return passed == total


def main() -> int:
    ap = argparse.ArgumentParser(description="Modbus 灯管控制回归测试")
    ap.add_argument("--port", required=True, help="串口设备，如 COM3 或 /dev/ttyUSB0")
    ap.add_argument("--slave", type=int, default=1, help="从机地址（默认 1）")
    ap.add_argument("--baud", type=int, default=115200, help="波特率（默认 115200）")
    args = ap.parse_args()

    tester = LampModbusTester(args.port, args.slave, args.baud)
    try:
        ok = tester.run_all()
    finally:
        tester.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
