#!/usr/bin/env python3
"""
STM32 UART测试脚本
用于自动化测试STM32F103C8的串口通信功能

依赖安装：
pip install pyserial pymodbus colorama

使用方法：
python uart_test.py --port COM3 --baudrate 9600 --test all
"""

import serial
import time
import argparse
import sys
import struct
from colorama import init, Fore, Style

# 初始化colorama
init(autoreset=True)

class UARTTester:
    """UART测试类"""
    
    def __init__(self, port, baudrate=9600, timeout=1):
        """初始化测试器
        
        Args:
            port: 串口号 (如 COM3 或 /dev/ttyUSB0)
            baudrate: 波特率
            timeout: 超时时间（秒）
        """
        try:
            self.ser = serial.Serial(port, baudrate, timeout=timeout)
            print(f"{Fore.GREEN}✓ 串口已打开: {port} @ {baudrate} bps")
            time.sleep(2)  # 等待串口稳定
        except Exception as e:
            print(f"{Fore.RED}✗ 无法打开串口: {e}")
            sys.exit(1)
            
        self.test_results = {
            'passed': 0,
            'failed': 0,
            'total': 0
        }
    
    def __del__(self):
        """析构函数，关闭串口"""
        if hasattr(self, 'ser') and self.ser.is_open:
            self.ser.close()
    
    def test_loopback(self, data="Hello STM32!", iterations=10):
        """环回测试
        
        Args:
            data: 测试数据
            iterations: 测试次数
            
        Returns:
            bool: 测试是否通过
        """
        print(f"\n{Fore.CYAN}=== 环回测试 ===")
        print(f"测试数据: '{data}'")
        print(f"测试次数: {iterations}")
        
        success_count = 0
        
        for i in range(iterations):
            # 清空接收缓冲区
            self.ser.reset_input_buffer()
            
            # 发送数据
            self.ser.write(data.encode())
            
            # 等待响应
            time.sleep(0.1)
            
            # 接收数据
            received = self.ser.read(len(data))
            
            if received.decode('utf-8', errors='ignore') == data:
                print(f"{Fore.GREEN}  [{i+1}/{iterations}] ✓ 通过")
                success_count += 1
            else:
                print(f"{Fore.RED}  [{i+1}/{iterations}] ✗ 失败")
                print(f"    期望: {data}")
                print(f"    收到: {received.decode('utf-8', errors='ignore')}")
        
        success_rate = (success_count / iterations) * 100
        self.test_results['total'] += iterations
        self.test_results['passed'] += success_count
        self.test_results['failed'] += (iterations - success_count)
        
        if success_rate >= 95:
            print(f"{Fore.GREEN}✓ 环回测试通过 (成功率: {success_rate:.1f}%)")
            return True
        else:
            print(f"{Fore.RED}✗ 环回测试失败 (成功率: {success_rate:.1f}%)")
            return False
    
    def test_pattern(self, pattern=0x55, length=64, iterations=5):
        """模式测试
        
        Args:
            pattern: 测试模式 (0x55, 0xAA, 0xFF, 0x00)
            length: 数据长度
            iterations: 测试次数
            
        Returns:
            bool: 测试是否通过
        """
        print(f"\n{Fore.CYAN}=== 模式测试 ===")
        print(f"测试模式: 0x{pattern:02X}")
        print(f"数据长度: {length} 字节")
        
        # 生成测试数据
        if pattern == 0x55:
            data = bytes([0x55 if i % 2 == 0 else 0xAA for i in range(length)])
        elif pattern == 0xAA:
            data = bytes([0xAA if i % 2 == 0 else 0x55 for i in range(length)])
        else:
            data = bytes([pattern] * length)
        
        success_count = 0
        
        for i in range(iterations):
            self.ser.reset_input_buffer()
            self.ser.write(data)
            time.sleep(0.2)
            
            received = self.ser.read(length)
            
            if received == data:
                print(f"{Fore.GREEN}  [{i+1}/{iterations}] ✓ 通过")
                success_count += 1
            else:
                print(f"{Fore.RED}  [{i+1}/{iterations}] ✗ 失败")
                errors = sum(1 for a, b in zip(data, received) if a != b)
                print(f"    错误字节: {errors}/{length}")
        
        self.test_results['total'] += iterations
        self.test_results['passed'] += success_count
        self.test_results['failed'] += (iterations - success_count)
        
        return success_count == iterations
    
    def test_modbus_read(self, slave_addr=0x01, start_addr=0, count=3):
        """Modbus读保持寄存器测试
        
        Args:
            slave_addr: 从站地址
            start_addr: 起始地址
            count: 寄存器数量
            
        Returns:
            bool: 测试是否通过
        """
        print(f"\n{Fore.CYAN}=== Modbus读寄存器测试 ===")
        print(f"从站地址: 0x{slave_addr:02X}")
        print(f"起始地址: {start_addr}")
        print(f"寄存器数: {count}")
        
        # 构建Modbus RTU请求帧
        # [从站地址][功能码03][起始地址H][起始地址L][寄存器数H][寄存器数L][CRC16]
        request = bytearray([
            slave_addr,
            0x03,  # 读保持寄存器
            (start_addr >> 8) & 0xFF,
            start_addr & 0xFF,
            (count >> 8) & 0xFF,
            count & 0xFF
        ])
        
        # 计算CRC16
        crc = self.modbus_crc16(request)
        request.extend([crc & 0xFF, (crc >> 8) & 0xFF])
        
        # 发送请求
        self.ser.reset_input_buffer()
        self.ser.write(request)
        
        # 等待响应
        time.sleep(0.1)
        
        # 预期响应长度：地址(1) + 功能码(1) + 字节数(1) + 数据(count*2) + CRC(2)
        expected_length = 5 + count * 2
        response = self.ser.read(expected_length)
        
        if len(response) == expected_length:
            # 验证响应
            if response[0] == slave_addr and response[1] == 0x03:
                # 提取数据
                byte_count = response[2]
                if byte_count == count * 2:
                    values = []
                    for i in range(count):
                        value = (response[3 + i*2] << 8) | response[4 + i*2]
                        values.append(value)
                        print(f"  寄存器[{start_addr + i}] = 0x{value:04X} ({value})")
                    
                    print(f"{Fore.GREEN}✓ Modbus读取成功")
                    self.test_results['passed'] += 1
                    self.test_results['total'] += 1
                    return True
        
        print(f"{Fore.RED}✗ Modbus读取失败")
        self.test_results['failed'] += 1
        self.test_results['total'] += 1
        return False
    
    def test_modbus_write(self, slave_addr=0x01, reg_addr=0, value=0x1234):
        """Modbus写单个寄存器测试
        
        Args:
            slave_addr: 从站地址
            reg_addr: 寄存器地址
            value: 写入值
            
        Returns:
            bool: 测试是否通过
        """
        print(f"\n{Fore.CYAN}=== Modbus写寄存器测试 ===")
        print(f"从站地址: 0x{slave_addr:02X}")
        print(f"寄存器地址: {reg_addr}")
        print(f"写入值: 0x{value:04X} ({value})")
        
        # 构建Modbus RTU请求帧
        request = bytearray([
            slave_addr,
            0x06,  # 写单个寄存器
            (reg_addr >> 8) & 0xFF,
            reg_addr & 0xFF,
            (value >> 8) & 0xFF,
            value & 0xFF
        ])
        
        # 计算CRC16
        crc = self.modbus_crc16(request)
        request.extend([crc & 0xFF, (crc >> 8) & 0xFF])
        
        # 发送请求
        self.ser.reset_input_buffer()
        self.ser.write(request)
        
        # 等待响应
        time.sleep(0.1)
        
        # 响应应该是请求的回显
        response = self.ser.read(8)
        
        if response == request:
            print(f"{Fore.GREEN}✓ Modbus写入成功")
            self.test_results['passed'] += 1
            self.test_results['total'] += 1
            return True
        else:
            print(f"{Fore.RED}✗ Modbus写入失败")
            self.test_results['failed'] += 1
            self.test_results['total'] += 1
            return False
    
    def test_stress(self, duration=10, min_size=10, max_size=256):
        """压力测试
        
        Args:
            duration: 测试时长（秒）
            min_size: 最小包大小
            max_size: 最大包大小
            
        Returns:
            bool: 测试是否通过
        """
        import random
        
        print(f"\n{Fore.CYAN}=== 压力测试 ===")
        print(f"测试时长: {duration} 秒")
        print(f"包大小: {min_size}-{max_size} 字节")
        
        start_time = time.time()
        packet_count = 0
        success_count = 0
        total_bytes = 0
        
        while time.time() - start_time < duration:
            # 生成随机大小的数据
            size = random.randint(min_size, max_size)
            data = bytes([random.randint(0, 255) for _ in range(size)])
            
            # 发送并接收
            self.ser.reset_input_buffer()
            self.ser.write(data)
            time.sleep(0.01 + size * 0.001)  # 动态延时
            
            received = self.ser.read(size)
            packet_count += 1
            total_bytes += size
            
            if received == data:
                success_count += 1
            
            # 进度显示
            elapsed = time.time() - start_time
            if packet_count % 10 == 0:
                rate = (success_count / packet_count) * 100 if packet_count > 0 else 0
                print(f"  进度: {elapsed:.1f}s, 包: {packet_count}, 成功率: {rate:.1f}%", end='\r')
        
        print()  # 换行
        
        success_rate = (success_count / packet_count) * 100 if packet_count > 0 else 0
        throughput = total_bytes / duration
        
        print(f"\n{Fore.YELLOW}压力测试结果:")
        print(f"  总包数: {packet_count}")
        print(f"  成功包: {success_count}")
        print(f"  成功率: {success_rate:.2f}%")
        print(f"  吞吐量: {throughput:.1f} 字节/秒")
        
        self.test_results['total'] += packet_count
        self.test_results['passed'] += success_count
        self.test_results['failed'] += (packet_count - success_count)
        
        return success_rate >= 90
    
    def modbus_crc16(self, data):
        """计算Modbus CRC16
        
        Args:
            data: 数据字节
            
        Returns:
            int: CRC16值
        """
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def run_all_tests(self):
        """运行所有测试"""
        print(f"\n{Fore.MAGENTA}{'='*50}")
        print(f"{Fore.MAGENTA}      开始完整测试套件")
        print(f"{Fore.MAGENTA}{'='*50}")
        
        results = []
        
        # 1. 环回测试
        results.append(('环回测试', self.test_loopback()))
        
        # 2. 模式测试
        results.append(('模式测试(0x55)', self.test_pattern(0x55)))
        results.append(('模式测试(0xAA)', self.test_pattern(0xAA)))
        
        # 3. Modbus测试
        results.append(('Modbus读测试', self.test_modbus_read()))
        results.append(('Modbus写测试', self.test_modbus_write()))
        
        # 4. 压力测试
        results.append(('压力测试', self.test_stress(duration=5)))
        
        # 显示总结
        print(f"\n{Fore.MAGENTA}{'='*50}")
        print(f"{Fore.MAGENTA}      测试总结")
        print(f"{Fore.MAGENTA}{'='*50}")
        
        for name, passed in results:
            status = f"{Fore.GREEN}✓ 通过" if passed else f"{Fore.RED}✗ 失败"
            print(f"  {name:20s}: {status}")
        
        print(f"\n{Fore.YELLOW}统计信息:")
        print(f"  总测试数: {self.test_results['total']}")
        print(f"  通过: {self.test_results['passed']}")
        print(f"  失败: {self.test_results['failed']}")
        
        if self.test_results['total'] > 0:
            rate = (self.test_results['passed'] / self.test_results['total']) * 100
            print(f"  成功率: {rate:.2f}%")
            
            if rate >= 95:
                print(f"\n{Fore.GREEN}★ 测试套件通过 ★")
            elif rate >= 80:
                print(f"\n{Fore.YELLOW}⚠ 测试套件部分通过")
            else:
                print(f"\n{Fore.RED}✗ 测试套件失败")
        
        return all(passed for _, passed in results)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='STM32 UART测试工具')
    parser.add_argument('--port', '-p', required=True, help='串口号 (如 COM3 或 /dev/ttyUSB0)')
    parser.add_argument('--baudrate', '-b', type=int, default=9600, help='波特率 (默认: 9600)')
    parser.add_argument('--test', '-t', default='all', 
                       choices=['all', 'loopback', 'pattern', 'modbus', 'stress'],
                       help='测试类型 (默认: all)')
    parser.add_argument('--timeout', type=float, default=1.0, help='超时时间(秒) (默认: 1.0)')
    
    args = parser.parse_args()
    
    # 创建测试器
    tester = UARTTester(args.port, args.baudrate, args.timeout)
    
    # 执行测试
    try:
        if args.test == 'all':
            tester.run_all_tests()
        elif args.test == 'loopback':
            tester.test_loopback()
        elif args.test == 'pattern':
            tester.test_pattern(0x55)
            tester.test_pattern(0xAA)
        elif args.test == 'modbus':
            tester.test_modbus_read()
            tester.test_modbus_write()
        elif args.test == 'stress':
            tester.test_stress()
        
    except KeyboardInterrupt:
        print(f"\n{Fore.YELLOW}测试被用户中断")
    except Exception as e:
        print(f"\n{Fore.RED}测试出错: {e}")
    

if __name__ == '__main__':
    main()

