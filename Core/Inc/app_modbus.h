/* modbus_rtu_slave.h */
#ifndef __MODBUS_RTU_SLAVE_H
#define __MODBUS_RTU_SLAVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* ---------- 可选：485 方向控制 ----------
   如需 485，请在 main.c 的 GPIO 初始化里配置此引脚为推挽输出。
   不用 485 可不定义这两个宏，库内部会自动空操作。 */
// #define MB_RS485_DE_GPIO_Port  GPIOA
// #define MB_RS485_DE_Pin        GPIO_PIN_8

/* ---------- 功能码 ---------- */
#define MB_FUNC_READ_COILS                  0x01
#define MB_FUNC_READ_DISCRETE_INPUTS        0x02
#define MB_FUNC_READ_HOLDING_REGISTERS      0x03
#define MB_FUNC_READ_INPUT_REGISTERS        0x04
#define MB_FUNC_WRITE_SINGLE_COIL           0x05
#define MB_FUNC_WRITE_SINGLE_REGISTER       0x06
#define MB_FUNC_WRITE_MULTIPLE_COILS        0x0F
#define MB_FUNC_WRITE_MULTIPLE_REGISTERS    0x10

/* ---------- 异常码 ---------- */
#define MB_EX_ILLEGAL_FUNCTION              0x01
#define MB_EX_ILLEGAL_DATA_ADDRESS          0x02
#define MB_EX_ILLEGAL_DATA_VALUE            0x03
#define MB_EX_SLAVE_DEVICE_FAILURE          0x04

/* ---------- 配置 ---------- */
#define MB_RTU_FRAME_MAX_SIZE               256U
#define MB_HOLDING_REGS_SIZE                100U
#define MB_INPUT_REGS_SIZE                  100U
#define MB_COILS_SIZE                       100U
#define MB_DISCRETE_INPUTS_SIZE             100U

typedef struct {
    uint8_t  slaveAddr;                 /* 从站地址 */
    UART_HandleTypeDef *huart;          /* UART 句柄 */

    /* 接收缓冲区 */
    uint8_t  rxBuffer[MB_RTU_FRAME_MAX_SIZE];
    uint16_t rxCount;
    uint8_t  rxComplete;
    uint8_t  frameReceiving;
    uint32_t lastReceiveTime;

    /* 发送缓冲区 */
    uint8_t  txBuffer[MB_RTU_FRAME_MAX_SIZE];
    uint16_t txCount;

    /* 数据区 */
    uint16_t holdingRegs[MB_HOLDING_REGS_SIZE];
    uint16_t inputRegs[MB_INPUT_REGS_SIZE];
    uint8_t  coils[MB_COILS_SIZE];
    uint8_t  discreteInputs[MB_DISCRETE_INPUTS_SIZE];
} ModbusRTU_Slave;

/* --------- 轻量临界区（关总中断，极短时间） --------- */
static inline uint32_t MB_CriticalEnter(void){
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}
static inline void MB_CriticalExit(uint32_t primask){
    __set_PRIMASK(primask);
}

/* 安全访问助手（可选，供应用侧使用） */
static inline uint16_t MB_SafeReadHolding(ModbusRTU_Slave *mb, uint16_t addr){
    uint32_t pm = MB_CriticalEnter();
    uint16_t v = (addr < MB_HOLDING_REGS_SIZE) ? mb->holdingRegs[addr] : 0;
    MB_CriticalExit(pm);
    return v;
}
static inline void MB_SafeWriteHolding(ModbusRTU_Slave *mb, uint16_t addr, uint16_t val){
    if (addr < MB_HOLDING_REGS_SIZE){
        uint32_t pm = MB_CriticalEnter();
        mb->holdingRegs[addr] = val;
        MB_CriticalExit(pm);
    }
}

/* API */
void     ModbusRTU_Init(ModbusRTU_Slave *mb, UART_HandleTypeDef *huart, uint8_t slaveAddr);
void     ModbusRTU_Process(ModbusRTU_Slave *mb);
void     ModbusRTU_TimerISR(ModbusRTU_Slave *mb);     /* 兜底超时用，可选 */
void     ModbusRTU_UartRxCallback(ModbusRTU_Slave *mb);
uint16_t ModbusRTU_CRC16(uint8_t *buffer, uint16_t length);

/* 用户回调（弱定义，可重载） */
void ModbusRTU_PreWriteCallback(uint16_t addr, uint16_t value);
void ModbusRTU_PostWriteCallback(uint16_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif
#endif /* __MODBUS_RTU_SLAVE_H */
