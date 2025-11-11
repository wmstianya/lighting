/**
 * @file modbus_rtu_core.c
 * @brief Modbus RTU协议核心层实现
 * @details 提供与硬件无关的Modbus RTU协议实现
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "modbus_rtu_core.h"
#include <string.h>

/* ==================== CRC16查找表 ==================== */
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
static uint16_t modbusCrc16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;
    
    for (i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16Table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc;
}

static uint8_t modbusCheckCrc(uint8_t *frame, uint16_t len)
{
    uint16_t crcCalc;
    uint16_t crcRecv;
    
    if (len < 4) return 0;
    
    crcCalc = modbusCrc16(frame, len - 2);
    crcRecv = frame[len - 2] | (frame[len - 1] << 8);
    
    return (crcCalc == crcRecv) ? 1 : 0;
}

static void modbusAddCrc(uint8_t *frame, uint16_t len)
{
    uint16_t crc = modbusCrc16(frame, len);
    frame[len] = crc & 0xFF;
    frame[len + 1] = crc >> 8;
}

/* ==================== 位操作辅助函数 ==================== */
static uint8_t modbusGetBit(uint8_t *data, uint16_t bitAddr)
{
    uint16_t byteIndex = bitAddr / 8;
    uint8_t bitIndex = bitAddr % 8;
    return (data[byteIndex] >> bitIndex) & 0x01;
}

static void modbusSetBit(uint8_t *data, uint16_t bitAddr, uint8_t value)
{
    uint16_t byteIndex = bitAddr / 8;
    uint8_t bitIndex = bitAddr % 8;
    if (value) {
        data[byteIndex] |= (1 << bitIndex);
    } else {
        data[byteIndex] &= ~(1 << bitIndex);
    }
}

/* ==================== 异常响应 ==================== */
static void modbusSendException(ModbusRTU_t *mb, uint8_t funcCode, uint8_t exCode)
{
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = funcCode | 0x80;
    mb->txBuffer[2] = exCode;
    modbusAddCrc(mb->txBuffer, 3);
    
    /* 切换RS485到发送模式 */
    if (mb->hw.setRS485Dir) {
        mb->hw.setRS485Dir(mb->hw.portContext, 1);
    }
    
    /* 发送数据 */
    if (mb->hw.sendData) {
        mb->hw.sendData(mb->hw.portContext, mb->txBuffer, 5);
    }
    
    mb->stats.txFrameCount++;
    mb->state = MODBUS_STATE_SENDING;
}

/* ==================== 功能码处理函数 ==================== */
static uint16_t modbusHandleReadCoils(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t coilCount = (frame[4] << 8) | frame[5];
    uint8_t byteCount;
    uint16_t i;
    
    if (!mb->coils || startAddr >= mb->coilCount || 
        (startAddr + coilCount) > mb->coilCount ||
        coilCount == 0 || coilCount > 2000) {
        modbusSendException(mb, MODBUS_FC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MODBUS_FC_READ_COILS;
    byteCount = (coilCount + 7) / 8;
    mb->txBuffer[2] = byteCount;
    
    for (i = 0; i < byteCount; i++) {
        mb->txBuffer[3 + i] = 0;
    }
    
    for (i = 0; i < coilCount; i++) {
        if (modbusGetBit(mb->coils, startAddr + i)) {
            uint8_t byteIndex = i / 8;
            uint8_t bitIndex = i % 8;
            mb->txBuffer[3 + byteIndex] |= (1 << bitIndex);
        }
    }
    
    modbusAddCrc(mb->txBuffer, 3 + byteCount);
    return 3 + byteCount + 2;
}

static uint16_t modbusHandleReadHoldingRegs(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint16_t i;
    uint8_t byteCount;
    
    if (!mb->holdingRegs || startAddr >= mb->holdingRegCount || 
        (startAddr + regCount) > mb->holdingRegCount ||
        regCount == 0 || regCount > 125) {
        modbusSendException(mb, MODBUS_FC_READ_HOLDING_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MODBUS_FC_READ_HOLDING_REGS;
    byteCount = regCount * 2;
    mb->txBuffer[2] = byteCount;
    
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = mb->holdingRegs[startAddr + i];
        mb->txBuffer[3 + i * 2] = regValue >> 8;
        mb->txBuffer[3 + i * 2 + 1] = regValue & 0xFF;
    }
    
    modbusAddCrc(mb->txBuffer, 3 + byteCount);
    return 3 + byteCount + 2;
}

static uint16_t modbusHandleReadInputRegs(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint16_t i;
    uint8_t byteCount;
    
    if (!mb->inputRegs || startAddr >= mb->inputRegCount || 
        (startAddr + regCount) > mb->inputRegCount ||
        regCount == 0 || regCount > 125) {
        modbusSendException(mb, MODBUS_FC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MODBUS_FC_READ_INPUT_REGS;
    byteCount = regCount * 2;
    mb->txBuffer[2] = byteCount;
    
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = mb->inputRegs[startAddr + i];
        mb->txBuffer[3 + i * 2] = regValue >> 8;
        mb->txBuffer[3 + i * 2 + 1] = regValue & 0xFF;
    }
    
    modbusAddCrc(mb->txBuffer, 3 + byteCount);
    return 3 + byteCount + 2;
}

static uint16_t modbusHandleWriteSingleCoil(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t coilAddr = (frame[2] << 8) | frame[3];
    uint16_t coilValue = (frame[4] << 8) | frame[5];
    
    if (!mb->coils || coilAddr >= mb->coilCount) {
        modbusSendException(mb, MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    if (coilValue != 0x0000 && coilValue != 0xFF00) {
        modbusSendException(mb, MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return 0;
    }
    
    modbusSetBit(mb->coils, coilAddr, coilValue == 0xFF00 ? 1 : 0);
    
    /* 用户回调 */
    if (mb->onCoilChanged) {
        mb->onCoilChanged(coilAddr, coilValue == 0xFF00 ? 1 : 0);
    }
    
    memcpy(mb->txBuffer, frame, 6);
    modbusAddCrc(mb->txBuffer, 6);
    return 8;
}

static uint16_t modbusHandleWriteSingleReg(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t regAddr = (frame[2] << 8) | frame[3];
    uint16_t regValue = (frame[4] << 8) | frame[5];
    
    if (!mb->holdingRegs || regAddr >= mb->holdingRegCount) {
        modbusSendException(mb, MODBUS_FC_WRITE_SINGLE_REG, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    mb->holdingRegs[regAddr] = regValue;
    
    /* 用户回调 */
    if (mb->onRegChanged) {
        mb->onRegChanged(regAddr, regValue);
    }
    
    memcpy(mb->txBuffer, frame, 6);
    modbusAddCrc(mb->txBuffer, 6);
    return 8;
}

static uint16_t modbusHandleWriteMultipleRegs(ModbusRTU_t *mb, uint8_t *frame)
{
    uint16_t startAddr = (frame[2] << 8) | frame[3];
    uint16_t regCount = (frame[4] << 8) | frame[5];
    uint8_t byteCount = frame[6];
    uint16_t i;
    
    if (!mb->holdingRegs || startAddr >= mb->holdingRegCount || 
        (startAddr + regCount) > mb->holdingRegCount ||
        regCount == 0 || regCount > 123 ||
        byteCount != (regCount * 2)) {
        modbusSendException(mb, MODBUS_FC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
        return 0;
    }
    
    for (i = 0; i < regCount; i++) {
        uint16_t regValue = (frame[7 + i * 2] << 8) | frame[7 + i * 2 + 1];
        mb->holdingRegs[startAddr + i] = regValue;
        
        /* 用户回调 */
        if (mb->onRegChanged) {
            mb->onRegChanged(startAddr + i, regValue);
        }
    }
    
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MODBUS_FC_WRITE_MULTIPLE_REGS;
    mb->txBuffer[2] = frame[2];
    mb->txBuffer[3] = frame[3];
    mb->txBuffer[4] = frame[4];
    mb->txBuffer[5] = frame[5];
    modbusAddCrc(mb->txBuffer, 6);
    return 8;
}

/* ==================== 帧处理 ==================== */
static void modbusProcessFrame(ModbusRTU_t *mb)
{
    uint16_t txLen = 0;
    
    if (mb->rxLen < MODBUS_MIN_FRAME_SIZE) {
        mb->stats.errorCount++;
        return;
    }
    
    if (!modbusCheckCrc(mb->rxBuffer, mb->rxLen)) {
        mb->stats.crcErrorCount++;
        return;
    }
    
    if (mb->rxBuffer[0] != mb->slaveAddr && mb->rxBuffer[0] != 0) {
        return;
    }
    
    mb->stats.rxFrameCount++;
    
    /* LED指示 */
		/***
    if (mb->hw.ledIndicate) {
        mb->hw.ledIndicate(mb->hw.portContext, 1);
    }
    ***/
    switch (mb->rxBuffer[1]) {
        case MODBUS_FC_READ_COILS:
            txLen = modbusHandleReadCoils(mb, mb->rxBuffer);
            break;
            
        case MODBUS_FC_READ_HOLDING_REGS:
            txLen = modbusHandleReadHoldingRegs(mb, mb->rxBuffer);
            break;
            
        case MODBUS_FC_READ_INPUT_REGS:
            txLen = modbusHandleReadInputRegs(mb, mb->rxBuffer);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
            txLen = modbusHandleWriteSingleCoil(mb, mb->rxBuffer);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_REG:
            txLen = modbusHandleWriteSingleReg(mb, mb->rxBuffer);
            break;
            
        case MODBUS_FC_WRITE_MULTIPLE_REGS:
            txLen = modbusHandleWriteMultipleRegs(mb, mb->rxBuffer);
            break;
            
        default:
            modbusSendException(mb, mb->rxBuffer[1], MODBUS_EX_ILLEGAL_FUNCTION);
            return;
    }
    
    if (txLen > 0 && mb->rxBuffer[0] != 0) {
        /* 切换RS485到发送模式 */
        if (mb->hw.setRS485Dir) {
            mb->hw.setRS485Dir(mb->hw.portContext, 1);
        }
        
        /* 发送数据 */
        if (mb->hw.sendData) {
            mb->hw.sendData(mb->hw.portContext, mb->txBuffer, txLen);
        }
        
        mb->stats.txFrameCount++;
        mb->state = MODBUS_STATE_SENDING;
    }
}

/* ==================== API实现 ==================== */
void ModbusRTU_Init(ModbusRTU_t *mb)
{
    if (!mb) return;
    
    memset(mb->rxBuffer, 0, MODBUS_BUFFER_SIZE);
    memset(mb->txBuffer, 0, MODBUS_BUFFER_SIZE);
    
    mb->rxLen = 0;
    mb->frameReady = 0;
    mb->state = MODBUS_STATE_IDLE;
    mb->lastRxTime = 0;
    
    memset(&mb->stats, 0, sizeof(ModbusStats_t));
}

void ModbusRTU_SetSlaveAddr(ModbusRTU_t *mb, uint8_t addr)
{
    if (mb) mb->slaveAddr = addr;
}

void ModbusRTU_SetHoldingRegs(ModbusRTU_t *mb, uint16_t *regs, uint16_t count)
{
    if (mb) {
        mb->holdingRegs = regs;
        mb->holdingRegCount = count;
    }
}

void ModbusRTU_SetInputRegs(ModbusRTU_t *mb, uint16_t *regs, uint16_t count)
{
    if (mb) {
        mb->inputRegs = regs;
        mb->inputRegCount = count;
    }
}

void ModbusRTU_SetCoils(ModbusRTU_t *mb, uint8_t *coils, uint16_t count)
{
    if (mb) {
        mb->coils = coils;
        mb->coilCount = count;
    }
}

void ModbusRTU_SetDiscreteInputs(ModbusRTU_t *mb, uint8_t *inputs, uint16_t count)
{
    if (mb) {
        mb->discreteInputs = inputs;
        mb->discreteCount = count;
    }
}

void ModbusRTU_SetHardware(ModbusRTU_t *mb, ModbusHardware_t *hw)
{
    if (mb && hw) {
        mb->hw = *hw;
    }
}

void ModbusRTU_Process(ModbusRTU_t *mb)
{
    if (!mb) return;
    
    if (mb->frameReady) {
        mb->frameReady = 0;
        modbusProcessFrame(mb);
        
        if (mb->state != MODBUS_STATE_SENDING) {
            memset(mb->rxBuffer, 0, MODBUS_BUFFER_SIZE);
            mb->rxLen = 0;
            mb->state = MODBUS_STATE_IDLE;
            
            /* LED指示 */
            if (mb->hw.ledIndicate) {
                mb->hw.ledIndicate(mb->hw.portContext, 0);
            }
        }
    }
    
    /* 超时检测 */
    if (mb->state == MODBUS_STATE_RECEIVING) {
        if (mb->hw.getSysTick) {
            uint32_t currentTick = mb->hw.getSysTick();
            if ((currentTick - mb->lastRxTime) > MODBUS_FRAME_TIMEOUT) {
                mb->rxLen = 0;
                mb->frameReady = 0;
                mb->state = MODBUS_STATE_IDLE;
            }
        }
    }
}

void ModbusRTU_RxCallback(ModbusRTU_t *mb, uint16_t rxLen)
{
    if (!mb) return;
    
    mb->rxLen = rxLen;
    
    if (rxLen > 0) {
        mb->frameReady = 1;
        if (mb->hw.getSysTick) {
            mb->lastRxTime = mb->hw.getSysTick();
        }
        mb->state = MODBUS_STATE_PROCESSING;
    }
}

void ModbusRTU_TxCallback(ModbusRTU_t *mb)
{
    if (!mb) return;
    
    /* 切换RS485回接收模式 */
    if (mb->hw.setRS485Dir) {
        mb->hw.setRS485Dir(mb->hw.portContext, 0);
    }
    
    memset(mb->rxBuffer, 0, MODBUS_BUFFER_SIZE);
    mb->rxLen = 0;
    mb->frameReady = 0;
    mb->state = MODBUS_STATE_IDLE;
    
    /* LED指示 */
    if (mb->hw.ledIndicate) {
        mb->hw.ledIndicate(mb->hw.portContext, 0);
    }
}

/* 数据访问函数 */
uint16_t ModbusRTU_ReadHoldingReg(ModbusRTU_t *mb, uint16_t addr)
{
    if (mb && mb->holdingRegs && addr < mb->holdingRegCount) {
        return mb->holdingRegs[addr];
    }
    return 0;
}

void ModbusRTU_WriteHoldingReg(ModbusRTU_t *mb, uint16_t addr, uint16_t value)
{
    if (mb && mb->holdingRegs && addr < mb->holdingRegCount) {
        mb->holdingRegs[addr] = value;
    }
}

uint16_t ModbusRTU_ReadInputReg(ModbusRTU_t *mb, uint16_t addr)
{
    if (mb && mb->inputRegs && addr < mb->inputRegCount) {
        return mb->inputRegs[addr];
    }
    return 0;
}

uint8_t ModbusRTU_ReadCoil(ModbusRTU_t *mb, uint16_t addr)
{
    if (mb && mb->coils && addr < mb->coilCount) {
        return modbusGetBit(mb->coils, addr);
    }
    return 0;
}

void ModbusRTU_WriteCoil(ModbusRTU_t *mb, uint16_t addr, uint8_t value)
{
    if (mb && mb->coils && addr < mb->coilCount) {
        modbusSetBit(mb->coils, addr, value);
    }
}

ModbusStats_t* ModbusRTU_GetStats(ModbusRTU_t *mb)
{
    return mb ? &mb->stats : NULL;
}
