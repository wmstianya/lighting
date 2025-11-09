/**
 * @file modbus_rtu_core.h
 * @brief Modbus RTU协议核心层头文件
 * @details 提供与硬件无关的Modbus RTU协议实现
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#ifndef __MODBUS_RTU_CORE_H
#define __MODBUS_RTU_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ==================== Modbus配置 ==================== */
#define MODBUS_BUFFER_SIZE      256          /* 缓冲区大小 */
#define MODBUS_FRAME_TIMEOUT    5            /* 帧超时(ms) */
#define MODBUS_MIN_FRAME_SIZE   4            /* 最小帧长度 */

/* Modbus功能码定义 */
#define MODBUS_FC_READ_COILS             0x01   /* 读线圈 */
#define MODBUS_FC_READ_DISCRETE_INPUTS   0x02   /* 读离散输入 */
#define MODBUS_FC_READ_HOLDING_REGS      0x03   /* 读保持寄存器 */
#define MODBUS_FC_READ_INPUT_REGS        0x04   /* 读输入寄存器 */
#define MODBUS_FC_WRITE_SINGLE_COIL      0x05   /* 写单个线圈 */
#define MODBUS_FC_WRITE_SINGLE_REG       0x06   /* 写单个寄存器 */
#define MODBUS_FC_WRITE_MULTIPLE_COILS   0x0F   /* 写多个线圈 */
#define MODBUS_FC_WRITE_MULTIPLE_REGS    0x10   /* 写多个寄存器 */

/* Modbus异常码定义 */
#define MODBUS_EX_ILLEGAL_FUNCTION      0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDRESS  0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE    0x03
#define MODBUS_EX_SLAVE_DEVICE_FAILURE  0x04

/* ==================== 数据结构 ==================== */

/* Modbus状态机 */
typedef enum {
    MODBUS_STATE_IDLE,          /* 空闲等待 */
    MODBUS_STATE_RECEIVING,     /* 接收中 */
    MODBUS_STATE_PROCESSING,    /* 处理中 */
    MODBUS_STATE_SENDING        /* 发送中 */
} ModbusState_t;

/* Modbus统计信息 */
typedef struct {
    uint32_t rxFrameCount;      /* 接收帧计数 */
    uint32_t txFrameCount;      /* 发送帧计数 */
    uint32_t errorCount;        /* 错误计数 */
    uint32_t crcErrorCount;     /* CRC错误计数 */
} ModbusStats_t;

/* 硬件接口函数指针 */
typedef struct {
    /* 发送数据函数 */
    void (*sendData)(void *port, uint8_t *data, uint16_t len);
    /* RS485方向控制 (1=发送, 0=接收) */
    void (*setRS485Dir)(void *port, uint8_t txMode);
    /* LED指示 */
    void (*ledIndicate)(void *port, uint8_t state);
    /* 获取系统时间(ms) */
    uint32_t (*getSysTick)(void);
    /* 硬件端口上下文 */
    void *portContext;
} ModbusHardware_t;

/* Modbus RTU实例结构体 */
typedef struct {
    /* 配置参数 */
    uint8_t slaveAddr;              /* 从站地址 */
    
    /* 数据存储 */
    uint16_t *holdingRegs;          /* 保持寄存器 */
    uint16_t *inputRegs;            /* 输入寄存器 */
    uint8_t *coils;                 /* 线圈 */
    uint8_t *discreteInputs;        /* 离散输入 */
    
    /* 数据大小 */
    uint16_t holdingRegCount;       /* 保持寄存器数量 */
    uint16_t inputRegCount;         /* 输入寄存器数量 */
    uint16_t coilCount;             /* 线圈数量 */
    uint16_t discreteCount;         /* 离散输入数量 */
    
    /* 通信缓冲区 */
    uint8_t rxBuffer[MODBUS_BUFFER_SIZE];
    uint8_t txBuffer[MODBUS_BUFFER_SIZE];
    
    /* 状态管理 */
    volatile uint16_t rxLen;
    volatile uint8_t frameReady;
    volatile ModbusState_t state;
    volatile uint32_t lastRxTime;
    
    /* 统计信息 */
    ModbusStats_t stats;
    
    /* 硬件接口 */
    ModbusHardware_t hw;
    
    /* 用户回调（可选） */
    void (*onCoilChanged)(uint16_t addr, uint8_t value);
    void (*onRegChanged)(uint16_t addr, uint16_t value);
    
} ModbusRTU_t;

/* ==================== API函数 ==================== */

/* 初始化和配置 */
void ModbusRTU_Init(ModbusRTU_t *mb);
void ModbusRTU_SetSlaveAddr(ModbusRTU_t *mb, uint8_t addr);

/* 数据存储配置 */
void ModbusRTU_SetHoldingRegs(ModbusRTU_t *mb, uint16_t *regs, uint16_t count);
void ModbusRTU_SetInputRegs(ModbusRTU_t *mb, uint16_t *regs, uint16_t count);
void ModbusRTU_SetCoils(ModbusRTU_t *mb, uint8_t *coils, uint16_t count);
void ModbusRTU_SetDiscreteInputs(ModbusRTU_t *mb, uint8_t *inputs, uint16_t count);

/* 硬件接口配置 */
void ModbusRTU_SetHardware(ModbusRTU_t *mb, ModbusHardware_t *hw);

/* 运行时处理 */
void ModbusRTU_Process(ModbusRTU_t *mb);
void ModbusRTU_RxCallback(ModbusRTU_t *mb, uint16_t rxLen);
void ModbusRTU_TxCallback(ModbusRTU_t *mb);

/* 数据访问 */
uint16_t ModbusRTU_ReadHoldingReg(ModbusRTU_t *mb, uint16_t addr);
void ModbusRTU_WriteHoldingReg(ModbusRTU_t *mb, uint16_t addr, uint16_t value);
uint16_t ModbusRTU_ReadInputReg(ModbusRTU_t *mb, uint16_t addr);
uint8_t ModbusRTU_ReadCoil(ModbusRTU_t *mb, uint16_t addr);
void ModbusRTU_WriteCoil(ModbusRTU_t *mb, uint16_t addr, uint8_t value);

/* 统计信息 */
ModbusStats_t* ModbusRTU_GetStats(ModbusRTU_t *mb);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_RTU_CORE_H */
