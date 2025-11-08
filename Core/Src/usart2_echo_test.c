/**
 * @file usart2_echo_test.c
 * @brief USART2 Modbus RTU从站实现
 * @details 完整的Modbus RTU协议实现，支持功能码03/06/10h
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>

/* 外部变量引用 */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* ==================== Modbus RTU配置 ==================== */
#define MODBUS_SLAVE_ADDRESS    0x01        /* 从站地址 */
#define MODBUS_BUFFER_SIZE      256          /* 缓冲区大小 */
#define MODBUS_REG_COUNT        100          /* 保持寄存器数量 */
#define MODBUS_INPUT_REG_COUNT  50           /* 输入寄存器数量 */
#define MODBUS_COIL_COUNT       80           /* 线圈数量(可控制80个开关) */
#define MODBUS_DISCRETE_COUNT   40           /* 离散输入数量 */
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

/* RS485控制引脚（PA4） */
#define RS485_DE_PORT   GPIOA
#define RS485_DE_PIN    GPIO_PIN_4

/* LED诊断（PB1） */
#define LED_PORT        GPIOB
#define LED_PIN         GPIO_PIN_1

/* ==================== Modbus数据结构 ==================== */
/* Modbus状态机 */
typedef enum {
    MODBUS_STATE_IDLE,          /* 空闲等待 */
    MODBUS_STATE_RECEIVING,     /* 接收中 */
    MODBUS_STATE_PROCESSING,    /* 处理中 */
    MODBUS_STATE_SENDING        /* 发送中 */
} ModbusState;

/* Modbus统计信息 */
typedef struct {
    uint32_t rxFrameCount;      /* 接收帧计数 */
    uint32_t txFrameCount;      /* 发送帧计数 */
    uint32_t errorCount;        /* 错误计数 */
    uint32_t crcErrorCount;     /* CRC错误计数 */
} ModbusStats;

/* ==================== 全局变量 ==================== */
static uint8_t modbusRxBuffer[MODBUS_BUFFER_SIZE];
static uint8_t modbusTxBuffer[MODBUS_BUFFER_SIZE];

/* Modbus数据存储 */
static uint16_t modbusHoldingRegs[MODBUS_REG_COUNT];      /* 保持寄存器(可读写) */
static uint16_t modbusInputRegs[MODBUS_INPUT_REG_COUNT];  /* 输入寄存器(只读) */
static uint8_t modbusCoils[(MODBUS_COIL_COUNT + 7) / 8];  /* 线圈(位存储) */
static uint8_t modbusDiscreteInputs[(MODBUS_DISCRETE_COUNT + 7) / 8]; /* 离散输入(位存储) */

static volatile uint16_t modbusRxLen = 0;
static volatile uint8_t modbusFrameReady = 0;
static volatile ModbusState modbusState = MODBUS_STATE_IDLE;
static volatile uint32_t modbusLastRxTime = 0;

static ModbusStats modbusStats = {0};

/* CRC16查找表 */
static const uint16_t crc16Table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* ==================== CRC计算函数 ==================== */
/**
 * @brief 计算Modbus CRC16
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC16值
 */
static uint16_t modbusCrc16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;
    
    for (i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16Table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc;
}

/**
 * @brief 检查CRC是否正确
 * @param frame 帧数据
 * @param len 帧长度(包含CRC)
 * @return 1=正确, 0=错误
 */
static uint8_t modbusCheckCrc(uint8_t *frame, uint16_t len)
{
    uint16_t crcCalc;
    uint16_t crcRecv;
    
    if (len < 4) return 0;
    
    /* 计算CRC (不包括CRC字节) */
    crcCalc = modbusCrc16(frame, len - 2);
    
    /* 提取接收的CRC (小端) */
    crcRecv = frame[len - 2] | (frame[len - 1] << 8);
    
    return (crcCalc == crcRecv) ? 1 : 0;
}

/**
 * @brief 添加CRC到帧尾
 * @param frame 帧数据
 * @param len 数据长度(不含CRC)
 */
static void modbusAddCrc(uint8_t *frame, uint16_t len)
{
    uint16_t crc = modbusCrc16(frame, len);
    frame[len] = crc & 0xFF;        /* CRC低字节 */
    frame[len + 1] = crc >> 8;       /* CRC高字节 */
}

/* ==================== Modbus异常响应 ==================== */
/**
 * @brief 发送异常响应
 * @param funcCode 功能码
 * @param exCode 异常码
 */
static void modbusSendException(uint8_t funcCode, uint8_t exCode)
{
    modbusTxBuffer[0] = MODBUS_SLAVE_ADDRESS;
    modbusTxBuffer[1] = funcCode | 0x80;  /* 功能码最高位置1 */
    modbusTxBuffer[2] = exCode;
    modbusAddCrc(modbusTxBuffer, 3);
    
    /* 切换RS485到发送模式 */
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
    for(volatile uint32_t i = 0; i < 100; i++);
    
    /* DMA发送 */
    HAL_UART_Transmit_DMA(&huart2, modbusTxBuffer, 5);
    modbusStats.txFrameCount++;
    modbusState = MODBUS_STATE_SENDING;
}

/* ==================== 位操作辅助函数 ==================== */
/**
 * @brief 获取线圈状态
 * @param coilAddr 线圈地址
 * @return 线圈状态(0或1)
 */
static uint8_t modbusGetCoil(uint16_t coilAddr)
{
    if (coilAddr >= MODBUS_COIL_COUNT) return 0;
    uint16_t byteIndex = coilAddr / 8;
    uint8_t bitIndex = coilAddr % 8;
    return (modbusCoils[byteIndex] >> bitIndex) & 0x01;
}

/**
 * @brief 设置线圈状态
 * @param coilAddr 线圈地址
 * @param value 线圈状态(0或1)
 */
static void modbusSetCoil(uint16_t coilAddr, uint8_t value)
{
    if (coilAddr >= MODBUS_COIL_COUNT) return;
    uint16_t byteIndex = coilAddr / 8;
    uint8_t bitIndex = coilAddr % 8;
    if (value) {
        modbusCoils[byteIndex] |= (1 << bitIndex);
    } else {
        modbusCoils[byteIndex] &= ~(1 << bitIndex);
    }
}

/**
 * @brief 获取离散输入状态
 * @param inputAddr 输入地址
 * @return 输入状态(0或1)
 */
static uint8_t modbusGetDiscreteInput(uint16_t inputAddr)
{
    if (inputAddr >= MODBUS_DISCRETE_COUNT) return 0;
    uint16_t byteIndex = inputAddr / 8;
    uint8_t bitIndex = inputAddr % 8;
    return (modbusDiscreteInputs[byteIndex] >> bitIndex) & 0x01;
}

/* ==================== Modbus功能码处理 ==================== */
/**
 * @brief 处理功能码01 - 读线圈
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleReadCoils(uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t coilCount = (frame[4] << 8) | frame[5];
    uint8_t byteCount;
    uint16_t i;
    
    /* 检查地址范围 */
    if (startAddr >= MODBUS_COIL_COUNT || 
        (startAddr + coilCount) > MODBUS_COIL_COUNT ||
        coilCount == 0 || coilCount > 2000) {
        modbusSendException(MODBUS_FC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 构建响应 */
    modbusTxBuffer[0] = MODBUS_SLAVE_ADDRESS;
    modbusTxBuffer[1] = MODBUS_FC_READ_COILS;
    byteCount = (coilCount + 7) / 8;  /* 计算需要的字节数 */
    modbusTxBuffer[2] = byteCount;
    
    /* 清空数据区 */
    for (i = 0; i < byteCount; i++) {
        modbusTxBuffer[3 + i] = 0;
    }
    
    /* 填充线圈状态 */
    for (i = 0; i < coilCount; i++) {
        if (modbusGetCoil(startAddr + i)) {
            uint8_t byteIndex = i / 8;
            uint8_t bitIndex = i % 8;
            modbusTxBuffer[3 + byteIndex] |= (1 << bitIndex);
        }
    }
    
    /* 添加CRC */
    modbusAddCrc(modbusTxBuffer, 3 + byteCount);
    
    return 3 + byteCount + 2;  /* 头部3字节 + 数据 + CRC2字节 */
}

/**
 * @brief 处理功能码03 - 读保持寄存器
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleReadHoldingRegs(uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint16_t i;
    uint8_t byteCount;
    
    /* 检查地址范围 */
    if (startAddr >= MODBUS_REG_COUNT || 
        (startAddr + regCount) > MODBUS_REG_COUNT ||
        regCount == 0 || regCount > 125) {
        modbusSendException(MODBUS_FC_READ_HOLDING_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 构建响应 */
    modbusTxBuffer[0] = MODBUS_SLAVE_ADDRESS;
    modbusTxBuffer[1] = MODBUS_FC_READ_HOLDING_REGS;
    byteCount = regCount * 2;
    modbusTxBuffer[2] = byteCount;
    
    /* 复制寄存器数据 */
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = modbusHoldingRegs[startAddr + i];
        modbusTxBuffer[3 + i * 2] = regValue >> 8;      /* 高字节 */
        modbusTxBuffer[3 + i * 2 + 1] = regValue & 0xFF; /* 低字节 */
    }
    
    /* 添加CRC */
    modbusAddCrc(modbusTxBuffer, 3 + byteCount);
    
    return 3 + byteCount + 2;  /* 头部3字节 + 数据 + CRC2字节 */
}

/**
 * @brief 处理功能码04 - 读输入寄存器
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleReadInputRegs(uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint16_t i;
    uint8_t byteCount;
    
    /* 检查地址范围 */
    if (startAddr >= MODBUS_INPUT_REG_COUNT || 
        (startAddr + regCount) > MODBUS_INPUT_REG_COUNT ||
        regCount == 0 || regCount > 125) {
        modbusSendException(MODBUS_FC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 构建响应 */
    modbusTxBuffer[0] = MODBUS_SLAVE_ADDRESS;
    modbusTxBuffer[1] = MODBUS_FC_READ_INPUT_REGS;
    byteCount = regCount * 2;
    modbusTxBuffer[2] = byteCount;
    
    /* 复制寄存器数据 */
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = modbusInputRegs[startAddr + i];
        modbusTxBuffer[3 + i * 2] = regValue >> 8;      /* 高字节 */
        modbusTxBuffer[3 + i * 2 + 1] = regValue & 0xFF; /* 低字节 */
    }
    
    /* 添加CRC */
    modbusAddCrc(modbusTxBuffer, 3 + byteCount);
    
    return 3 + byteCount + 2;  /* 头部3字节 + 数据 + CRC2字节 */
}

/**
 * @brief 处理功能码05 - 写单个线圈
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleWriteSingleCoil(uint8_t *frame)
{
    uint16_t coilAddr = (frame[2] << 8) | frame[3];
    uint16_t coilValue = (frame[4] << 8) | frame[5];
    
    /* 检查地址 */
    if (coilAddr >= MODBUS_COIL_COUNT) {
        modbusSendException(MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 检查值（0x0000=OFF, 0xFF00=ON） */
    if (coilValue != 0x0000 && coilValue != 0xFF00) {
        modbusSendException(MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return 0;
    }
    
    /* 设置线圈 */
    modbusSetCoil(coilAddr, coilValue == 0xFF00 ? 1 : 0);
    
    /* 响应(回显请求) */
    memcpy(modbusTxBuffer, frame, 6);
    modbusAddCrc(modbusTxBuffer, 6);
    
    return 8;  /* 6字节数据 + 2字节CRC */
}

/**
 * @brief 处理功能码06 - 写单个寄存器
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleWriteSingleReg(uint8_t *frame)
{
    uint16_t regAddr = (frame[2] << 8) | frame[3];
    uint16_t regValue = (frame[4] << 8) | frame[5];
    
    /* 检查地址 */
    if (regAddr >= MODBUS_REG_COUNT) {
        modbusSendException(MODBUS_FC_WRITE_SINGLE_REG, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 写入寄存器 */
    modbusHoldingRegs[regAddr] = regValue;
    
    /* 响应(回显请求) */
    memcpy(modbusTxBuffer, frame, 6);
    modbusAddCrc(modbusTxBuffer, 6);
    
    return 8;  /* 6字节数据 + 2字节CRC */
}

/**
 * @brief 处理功能码10h - 写多个寄存器
 * @param frame 接收帧
 * @return 响应长度
 */
static uint16_t modbusHandleWriteMultipleRegs(uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint8_t byteCount = frame[6];
    uint16_t i;
    
    /* 检查参数 */
    if (startAddr >= MODBUS_REG_COUNT || 
        (startAddr + regCount) > MODBUS_REG_COUNT ||
        regCount == 0 || regCount > 123 ||
        byteCount != (regCount * 2)) {
        modbusSendException(MODBUS_FC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    /* 写入寄存器 */
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = (frame[7 + i * 2] << 8) | frame[7 + i * 2 + 1];
        modbusHoldingRegs[startAddr + i] = regValue;
    }
    
    /* 构建响应 */
    modbusTxBuffer[0] = MODBUS_SLAVE_ADDRESS;
    modbusTxBuffer[1] = MODBUS_FC_WRITE_MULTIPLE_REGS;
    modbusTxBuffer[2] = frame[2];  /* 起始地址高字节 */
    modbusTxBuffer[3] = frame[3];  /* 起始地址低字节 */
    modbusTxBuffer[4] = frame[4];  /* 寄存器数量高字节 */
    modbusTxBuffer[5] = frame[5];  /* 寄存器数量低字节 */
    modbusAddCrc(modbusTxBuffer, 6);
    
    return 8;  /* 6字节数据 + 2字节CRC */
}

/* ==================== Modbus帧处理 ==================== */
/**
 * @brief 处理接收到的Modbus帧
 */
static void modbusProcessFrame(void)
{
    uint16_t txLen = 0;
    
    /* 检查最小帧长度 */
    if (modbusRxLen < MODBUS_MIN_FRAME_SIZE) {
        modbusStats.errorCount++;
        return;
    }
    
    /* 检查CRC */
    if (!modbusCheckCrc(modbusRxBuffer, modbusRxLen)) {
        modbusStats.crcErrorCount++;
        return;
    }
    
    /* 检查从站地址 */
    if (modbusRxBuffer[0] != MODBUS_SLAVE_ADDRESS && modbusRxBuffer[0] != 0) {
        return;  /* 不是本站地址也不是广播 */
    }
    
    /* 统计 */
    modbusStats.rxFrameCount++;
    
    /* 处理功能码 */
    switch (modbusRxBuffer[1]) {
        case MODBUS_FC_READ_COILS:
            txLen = modbusHandleReadCoils(modbusRxBuffer);
            break;
            
        case MODBUS_FC_READ_HOLDING_REGS:
            txLen = modbusHandleReadHoldingRegs(modbusRxBuffer);
            break;
            
        case MODBUS_FC_READ_INPUT_REGS:
            txLen = modbusHandleReadInputRegs(modbusRxBuffer);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
            txLen = modbusHandleWriteSingleCoil(modbusRxBuffer);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_REG:
            txLen = modbusHandleWriteSingleReg(modbusRxBuffer);
            break;
            
        case MODBUS_FC_WRITE_MULTIPLE_REGS:
            txLen = modbusHandleWriteMultipleRegs(modbusRxBuffer);
            break;
            
        default:
            modbusSendException(modbusRxBuffer[1], MODBUS_EX_ILLEGAL_FUNCTION);
            return;
    }
    
    /* 发送响应 */
    if (txLen > 0 && modbusRxBuffer[0] != 0) {  /* 广播不响应 */
    /* 切换RS485到发送模式 */
        HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
        for(volatile uint32_t i = 0; i < 100; i++);
        
        /* DMA发送 */
        HAL_UART_Transmit_DMA(&huart2, modbusTxBuffer, txLen);
        modbusStats.txFrameCount++;
        modbusState = MODBUS_STATE_SENDING;
    }
}

/* ==================== 初始化和主循环 ==================== */
/**
 * @brief 初始化Modbus RTU
 */
void modbusRtuInit(void)
{
    uint16_t i;
    
    /* 清空缓冲区 */
    memset(modbusRxBuffer, 0, MODBUS_BUFFER_SIZE);
    memset(modbusTxBuffer, 0, MODBUS_BUFFER_SIZE);
    
    /* 初始化保持寄存器(测试数据) */
    for (i = 0; i < MODBUS_REG_COUNT; i++) {
        modbusHoldingRegs[i] = i + 1000;  /* 1000-1099 */
    }
    
    /* 初始化输入寄存器(模拟传感器数据) */
    for (i = 0; i < MODBUS_INPUT_REG_COUNT; i++) {
        modbusInputRegs[i] = i + 2000;  /* 2000-2049 */
    }
    
    /* 初始化线圈(前8个设为ON，其余OFF) */
    memset(modbusCoils, 0, sizeof(modbusCoils));
    for (i = 0; i < 8 && i < MODBUS_COIL_COUNT; i++) {
        modbusSetCoil(i, 1);  /* 前8个线圈ON */
    }
    
    /* 初始化离散输入(模拟开关状态) */
    memset(modbusDiscreteInputs, 0, sizeof(modbusDiscreteInputs));
    modbusDiscreteInputs[0] = 0xAA;  /* 10101010 - 测试模式 */
    
    /* 复位状态 */
    modbusRxLen = 0;
    modbusFrameReady = 0;
    modbusState = MODBUS_STATE_IDLE;
    modbusLastRxTime = 0;
    
    /* 清空统计 */
    memset(&modbusStats, 0, sizeof(modbusStats));
    
    /* RS485设置为接收模式 */
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    /* 清除IDLE标志并启用IDLE中断 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    HAL_UART_Receive_DMA(&huart2, modbusRxBuffer, MODBUS_BUFFER_SIZE);
    
    /* LED闪3次表示初始化完成 */
    for (i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }
}

/**
 * @brief IDLE中断处理（Modbus帧接收）
 * @note 在stm32f1xx_it.c的USART2_IRQHandler中调用
 */
void modbusHandleIdle(void)
{
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 清除可能的ORE：读SR再读DR（F1系列需如此操作） */
    volatile uint32_t sr = huart2.Instance->SR; (void)sr;
    volatile uint32_t dr = huart2.Instance->DR; (void)dr;
    
    /* 计算接收字节数 */
    modbusRxLen = MODBUS_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    if (modbusRxLen > 0) {
        modbusFrameReady = 1;
        modbusLastRxTime = HAL_GetTick();
        modbusState = MODBUS_STATE_PROCESSING;
        /* LED快闪表示接收到数据（不能在中断中使用HAL_Delay） */
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    } else {
        /* 没有数据，重启DMA接收 */
        HAL_UART_Receive_DMA(&huart2, modbusRxBuffer, MODBUS_BUFFER_SIZE);
    }
}

/**
 * @brief DMA发送完成回调
 * @note 在HAL_UART_TxCpltCallback中被调用
 */
void modbusTxCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        /* 等待发送移位寄存器完全空 (TC=1) */
        uint32_t startTick = HAL_GetTick();
        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
            if ((HAL_GetTick() - startTick) > 2U) { /* 2ms超时 */
                break;
            }
        }
        
        /* 切换RS485回接收模式 */
        HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
        
        /* 清空接收缓冲区 */
        memset(modbusRxBuffer, 0, MODBUS_BUFFER_SIZE);
        modbusRxLen = 0;
        modbusFrameReady = 0;
        
        /* 重启DMA接收 */
        HAL_UART_Receive_DMA(&huart2, modbusRxBuffer, MODBUS_BUFFER_SIZE);
        
        /* 恢复空闲状态 */
        modbusState = MODBUS_STATE_IDLE;
        
        /* LED恢复 */
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    }
}

/**
 * @brief Modbus主处理函数
 * @note 在主循环中调用
 */
void modbusRtuProcess(void)
{
    static uint32_t lastLedBlink = 0;
    static uint32_t debugCounter = 0;
    
    /* LED心跳指示(500ms快速闪烁表示运行) */
    if (HAL_GetTick() - lastLedBlink > 500) {
        lastLedBlink = HAL_GetTick();
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        debugCounter++;
    }
    
    /* 处理接收到的帧 */
    if (modbusFrameReady) {
        modbusFrameReady = 0;
        
        /* 调试：简单回环测试（先测试基本通信） */
        #if 0  /* 禁用简单回环测试，启用Modbus处理 */
        {
            /* LED快闪表示收到数据 */
            for(int i = 0; i < 2; i++) {
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
                HAL_Delay(50);
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
                HAL_Delay(50);
            }
            
            /* 简单回环：直接返回接收到的数据 */
            memcpy(modbusTxBuffer, modbusRxBuffer, modbusRxLen);
    
    /* 切换RS485到发送模式 */
            HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
            for(volatile uint32_t i = 0; i < 100; i++);
            
            /* DMA发送 */
            HAL_UART_Transmit_DMA(&huart2, modbusTxBuffer, modbusRxLen);
            modbusState = MODBUS_STATE_SENDING;
        }
        #else
        /* 正常Modbus处理 */
        
        /* LED快速指示收到数据（不延时） */
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        
        modbusProcessFrame();
        
        /* 恢复LED */
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        #endif
        
        /* 如果没有发送响应，重启接收 */
        if (modbusState != MODBUS_STATE_SENDING) {
            memset(modbusRxBuffer, 0, MODBUS_BUFFER_SIZE);
            modbusRxLen = 0;
            HAL_UART_Receive_DMA(&huart2, modbusRxBuffer, MODBUS_BUFFER_SIZE);
            modbusState = MODBUS_STATE_IDLE;
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        }
    }
    
    /* 超时检测 */
    if (modbusState == MODBUS_STATE_RECEIVING) {
        if ((HAL_GetTick() - modbusLastRxTime) > MODBUS_FRAME_TIMEOUT) {
            /* 接收超时，重启 */
            HAL_UART_DMAStop(&huart2);
            memset(modbusRxBuffer, 0, MODBUS_BUFFER_SIZE);
            modbusRxLen = 0;
            modbusFrameReady = 0;
            HAL_UART_Receive_DMA(&huart2, modbusRxBuffer, MODBUS_BUFFER_SIZE);
            modbusState = MODBUS_STATE_IDLE;
        }
    }
}

/**
 * @brief 运行Modbus RTU从站
 * @note 完整的主循环
 */
void modbusRtuRun(void)
{
    /* 初始化 */
    modbusRtuInit();
    
    /* 主循环 */
    while (1) {
        modbusRtuProcess();
        HAL_Delay(1);
    }
}

/* ==================== 寄存器操作接口 ==================== */
/**
 * @brief 读取保持寄存器
 * @param addr 寄存器地址
 * @return 寄存器值
 */
uint16_t modbusReadReg(uint16_t addr)
{
    if (addr < MODBUS_REG_COUNT) {
        return modbusHoldingRegs[addr];
    }
    return 0;
}

/**
 * @brief 写入保持寄存器
 * @param addr 寄存器地址
 * @param value 寄存器值
 */
void modbusWriteReg(uint16_t addr, uint16_t value)
{
    if (addr < MODBUS_REG_COUNT) {
        modbusHoldingRegs[addr] = value;
    }
}

/**
 * @brief 读取输入寄存器
 * @param addr 寄存器地址
 * @return 寄存器值
 */
uint16_t modbusReadInputReg(uint16_t addr)
{
    if (addr < MODBUS_INPUT_REG_COUNT) {
        return modbusInputRegs[addr];
    }
    return 0;
}

/**
 * @brief 读取线圈状态（对外接口）
 * @param addr 线圈地址
 * @return 线圈状态
 */
uint8_t modbusReadCoil(uint16_t addr)
{
    return modbusGetCoil(addr);
}

/**
 * @brief 写入线圈状态（对外接口）
 * @param addr 线圈地址
 * @param value 线圈值
 */
void modbusWriteCoil(uint16_t addr, uint8_t value)
{
    modbusSetCoil(addr, value);
}

/**
 * @brief 获取Modbus统计信息
 * @return 统计信息指针
 */
ModbusStats* modbusGetStats(void)
{
    return &modbusStats;
}

/* ==================== 兼容性接口(保留原有函数名) ==================== */
/* 以下函数保留原有名称，实际调用Modbus功能 */

void usart2EchoTestInit(void)
{
    modbusRtuInit();
}

void usart2EchoHandleIdle(void)
{
    modbusHandleIdle();
}

void usart2EchoProcess(void)
{
    modbusRtuProcess();
}

void usart2EchoTxCallback(UART_HandleTypeDef *huart)
{
    modbusTxCallback(huart);
}

void usart2EchoTestRun(void)
{
    modbusRtuRun();
}

