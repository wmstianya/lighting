#ifndef USART1_ECHO_TEST_H
#define USART1_ECHO_TEST_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void usart1EchoTestInit(void);
void usart1EchoHandleIdle(void);
void usart1EchoProcess(void);
void usart1EchoTxCallback(UART_HandleTypeDef *huart);
void usart1EchoGetDiagnostics(uint32_t *idle, uint32_t *process, uint32_t *txCplt);
void usart1EchoTestRun(void);

#ifdef __cplusplus
}
#endif

#endif /* USART1_ECHO_TEST_H */


