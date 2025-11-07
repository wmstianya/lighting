#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 运行模式选择（单一事实来源）
 * 0 = Modbus双串口
 * 1 = 串口2(USART2) 回环测试
 * 2 = 串口2 调试模式
 * 3 = 串口2 简单测试
 * 4 = 串口1(USART1) 回环测试
 */
#ifndef RUN_MODE_ECHO_TEST
#define RUN_MODE_ECHO_TEST 4
#endif

#endif /* APP_CONFIG_H */


