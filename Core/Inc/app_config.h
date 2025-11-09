#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 运行模式选择（单一事实来源）
 * 0 = Modbus双串口模式（旧版本）
 * 1 = USART2 (PA2/PA3) 回环测试 - 使用huart2
 * 2 = USART2 (PA2/PA3) 调试模式
 * 3 = USART2 (PA2/PA3) 简单测试
 * 4 = USART1 (PA9/PA10) 回环测试 - 使用huart1
 * 10 = 模块化Modbus双串口模式（新版本）
 * 
 * 注意：
 * - USART1对应代码中的huart1，引脚为PA9/PA10
 * - USART2对应代码中的huart2，引脚为PA2/PA3
 * - 不要被"串口1""串口2"的命名混淆，以STM32的USART编号为准
 */
#ifndef RUN_MODE_ECHO_TEST
#define RUN_MODE_ECHO_TEST 10  /* 使用模块化Modbus双串口模式 */
#endif

#endif /* APP_CONFIG_H */
