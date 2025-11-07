/* modbus_rtu_slave.c */
#include "modbus_rtu_slave.h"

/* ����Ĺ������� stm32f1xx_it.c �ṩ�� IRQHandler����Ѵ˺���Ϊ 0�������ظ����� */
#ifndef MB_PROVIDE_IRQ_HANDLERS
#define MB_PROVIDE_IRQ_HANDLERS 0
#endif

/* ------ �ڲ�ǰ������ ------ */
static void ModbusRTU_Exception(ModbusRTU_Slave *mb, uint8_t funcCode, uint8_t exceptionCode);
static void ModbusRTU_ReadHoldingRegisters(ModbusRTU_Slave *mb);
static void ModbusRTU_ReadInputRegisters(ModbusRTU_Slave *mb);
static void ModbusRTU_WriteSingleRegister(ModbusRTU_Slave *mb);
static void ModbusRTU_WriteMultipleRegisters(ModbusRTU_Slave *mb);
static void ModbusRTU_ProcessFrame(ModbusRTU_Slave *mb);

/* ------ ����ʱ�󶨵Ļʵ�������� HAL �ص��� ------ */
// static ModbusRTU_Slave *s_active_mb = NULL; // 未使用，因为 MB_PROVIDE_IRQ_HANDLERS = 0
/* ����״̬ & ͳһ�������� */
static volatile uint8_t s_txInProgress = 0;

static void MB_RestartRx(ModbusRTU_Slave *mb){
    mb->rxComplete = 0;
    mb->rxCount = 0;
    mb->frameReceiving = 0;
    HAL_UART_Receive_DMA(mb->huart, mb->rxBuffer, MB_RTU_FRAME_MAX_SIZE);
}

/* ------ CRC16 �� ------ */
static const uint16_t crcTable[256] = {
    0x0000,0xC0C1,0xC181,0x0140,0xC301,0x03C0,0x0280,0xC241,0xC601,0x06C0,0x0780,0xC741,0x0500,0xC5C1,0xC481,0x0440,
    0xCC01,0x0CC0,0x0D80,0xCD41,0x0F00,0xCFC1,0xCE81,0x0E40,0x0A00,0xCAC1,0xCB81,0x0B40,0xC901,0x09C0,0x0880,0xC841,
    0xD801,0x18C0,0x1980,0xD941,0x1B00,0xDBC1,0xDA81,0x1A40,0x1E00,0xDEC1,0xDF81,0x1F40,0xDD01,0x1DC0,0x1C80,0xDC41,
    0x1400,0xD4C1,0xD581,0x1540,0xD701,0x17C0,0x1680,0xD641,0xD201,0x12C0,0x1380,0xD341,0x1100,0xD1C1,0xD081,0x1040,
    0xF001,0x30C0,0x3180,0xF141,0x3300,0xF3C1,0xF281,0x3240,0x3600,0xF6C1,0xF781,0x3740,0xF501,0x35C0,0x3480,0xF441,
    0x3C00,0xFCC1,0xFD81,0x3D40,0xFF01,0x3FC0,0x3E80,0xFE41,0xFA01,0x3AC0,0x3B80,0xFB41,0x3900,0xF9C1,0xF881,0x3840,
    0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,0xEE01,0x2EC0,0x2F80,0xEF41,0x2D00,0xEDC1,0xEC81,0x2C40,
    0xE401,0x24C0,0x2580,0xE541,0x2700,0xE7C1,0xE681,0x2640,0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,
    0xA001,0x60C0,0x6180,0xA141,0x6300,0xA3C1,0xA281,0x6240,0x6600,0xA6C1,0xA781,0x6740,0xA501,0x65C0,0x6480,0xA441,
    0x6C00,0xACC1,0xAD81,0x6D40,0xAF01,0x6FC0,0x6E80,0xAE41,0xAA01,0x6AC0,0x6B80,0xAB41,0x6900,0xA9C1,0xA881,0x6840,
    0x7800,0xB8C1,0xB981,0x7940,0xBB01,0x7BC0,0x7A80,0xBA41,0xBE01,0x7EC0,0x7F80,0xBF41,0x7D00,0xBDC1,0xBC81,0x7C40,
    0xB401,0x74C0,0x7580,0xB541,0x7700,0xB7C1,0xB681,0x7640,0x7200,0xB2C1,0xB381,0x7340,0xB101,0x71C0,0x7080,0xB041,
    0x5000,0x90C1,0x9181,0x5140,0x9301,0x53C0,0x5280,0x9241,0x9601,0x56C0,0x5780,0x9741,0x5500,0x95C1,0x9481,0x5440,
    0x9C01,0x5CC0,0x5D80,0x9D41,0x5F00,0x9FC1,0x9E81,0x5E40,0x5A00,0x9AC1,0x9B81,0x5B40,0x9901,0x59C0,0x5880,0x9841,
    0x8801,0x48C0,0x4980,0x8941,0x4E00,0x8EC1,0x8F81,0x4F40,0x8D01,0x4DC0,0x4C80,0x8C41,0x4400,0x84C1,0x8581,0x4540,
    0x8701,0x47C0,0x4680,0x8641,0x8201,0x42C0,0x4380,0x8341,0x4100,0x81C1,0x8081,0x4040
};

uint16_t ModbusRTU_CRC16(uint8_t *buffer, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crcTable[(crc ^ buffer[i]) & 0xFF];
    }
    return crc;
}

/* ---------- RS485 方向控制 (支持多串口) ---------- */
static inline void RS485_TxEnable(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        #ifdef MB_USART1_RS485_DE_Pin
        HAL_GPIO_WritePin(MB_USART1_RS485_DE_GPIO_Port, MB_USART1_RS485_DE_Pin, GPIO_PIN_SET);
        #endif
    } else if (huart->Instance == USART2) {
        #ifdef MB_USART2_RS485_DE_Pin
        HAL_GPIO_WritePin(MB_USART2_RS485_DE_GPIO_Port, MB_USART2_RS485_DE_Pin, GPIO_PIN_SET);
        #endif
    }
}
static inline void RS485_RxEnable(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        #ifdef MB_USART1_RS485_DE_Pin
        HAL_GPIO_WritePin(MB_USART1_RS485_DE_GPIO_Port, MB_USART1_RS485_DE_Pin, GPIO_PIN_RESET);
        #endif
    } else if (huart->Instance == USART2) {
        #ifdef MB_USART2_RS485_DE_Pin
        HAL_GPIO_WritePin(MB_USART2_RS485_DE_GPIO_Port, MB_USART2_RS485_DE_Pin, GPIO_PIN_RESET);
        #endif
    }
}

/* ---------- ��ʼ�� ---------- */
void ModbusRTU_Init(ModbusRTU_Slave *mb, UART_HandleTypeDef *huart, uint8_t slaveAddr)
{
    // s_active_mb   = mb; // 注释掉，因为变量未定义
    mb->huart     = huart;
    mb->slaveAddr = slaveAddr;
    mb->rxCount = 0;
    mb->rxComplete = 0;
    mb->txCount = 0;
    mb->frameReceiving = 0;
    mb->lastReceiveTime = 0;

    memset(mb->holdingRegs, 0, sizeof(mb->holdingRegs));
    memset(mb->inputRegs,   0, sizeof(mb->inputRegs));
    memset(mb->coils,       0, sizeof(mb->coils));
    memset(mb->discreteInputs, 0, sizeof(mb->discreteInputs));

    s_txInProgress = 0;
    RS485_RxEnable(mb->huart);
    HAL_UART_Receive_DMA(mb->huart, mb->rxBuffer, MB_RTU_FRAME_MAX_SIZE);
}

/* ---------- �����ּĴ��� 0x03������ʽ�� ---------- */
static void ModbusRTU_ReadHoldingRegisters(ModbusRTU_Slave *mb)
{
    if (mb->rxCount < 8) return;
    uint16_t startAddr = (mb->rxBuffer[2] << 8) | mb->rxBuffer[3];
    uint16_t quantity  = (mb->rxBuffer[4] << 8) | mb->rxBuffer[5];
    uint16_t endAddr   = startAddr + quantity - 1;

    if (quantity < 1 || quantity > 125) {
        ModbusRTU_Exception(mb, MB_FUNC_READ_HOLDING_REGISTERS, MB_EX_ILLEGAL_DATA_VALUE);
        return;
    }
    if (endAddr >= MB_HOLDING_REGS_SIZE) {
        ModbusRTU_Exception(mb, MB_FUNC_READ_HOLDING_REGISTERS, MB_EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    /* --- ���գ��ڶ��ٽ����ڰ�Ҫ�����������忽������ --- */
    uint16_t snapCnt = quantity;
    uint16_t snapBuf[125]; /* Modbus ������� 125 �Ĵ��� */
    {
        uint32_t pm = MB_CriticalEnter();
        for (uint16_t i = 0; i < snapCnt; i++) {
            snapBuf[i] = mb->holdingRegs[startAddr + i];
        }
        MB_CriticalExit(pm);
    }

    /* --- �ÿ������ --- */
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MB_FUNC_READ_HOLDING_REGISTERS;
    mb->txBuffer[2] = (uint8_t)(snapCnt * 2);

    for (uint16_t i = 0; i < snapCnt; i++) {
        uint16_t v = snapBuf[i];
        mb->txBuffer[3 + 2*i] = (uint8_t)(v >> 8);
        mb->txBuffer[4 + 2*i] = (uint8_t)(v & 0xFF);
    }

    mb->txCount = 3 + snapCnt * 2;
    uint16_t crc = ModbusRTU_CRC16(mb->txBuffer, mb->txCount);
    mb->txBuffer[mb->txCount++] = (uint8_t)(crc & 0xFF);       /* LSB */
    mb->txBuffer[mb->txCount++] = (uint8_t)((crc >> 8) & 0xFF);/* MSB */
}

/* ---------- ������Ĵ��� 0x04������ʽ�� ---------- */
static void ModbusRTU_ReadInputRegisters(ModbusRTU_Slave *mb)
{
    if (mb->rxCount < 8) return;
    uint16_t startAddr = (mb->rxBuffer[2] << 8) | mb->rxBuffer[3];
    uint16_t quantity  = (mb->rxBuffer[4] << 8) | mb->rxBuffer[5];
    uint16_t endAddr   = startAddr + quantity - 1;

    if (quantity < 1 || quantity > 125) {
        ModbusRTU_Exception(mb, MB_FUNC_READ_INPUT_REGISTERS, MB_EX_ILLEGAL_DATA_VALUE);
        return;
    }
    if (endAddr >= MB_INPUT_REGS_SIZE) {
        ModbusRTU_Exception(mb, MB_FUNC_READ_INPUT_REGISTERS, MB_EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    /* --- ���գ��ڶ��ٽ����ڰ�Ҫ�����������忽������ --- */
    uint16_t snapCnt = quantity;
    uint16_t snapBuf[125];
    {
        uint32_t pm = MB_CriticalEnter();
        for (uint16_t i = 0; i < snapCnt; i++) {
            snapBuf[i] = mb->inputRegs[startAddr + i];
        }
        MB_CriticalExit(pm);
    }

    /* --- �ÿ������ --- */
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MB_FUNC_READ_INPUT_REGISTERS;
    mb->txBuffer[2] = (uint8_t)(snapCnt * 2);

    for (uint16_t i = 0; i < snapCnt; i++) {
        uint16_t v = snapBuf[i];
        mb->txBuffer[3 + 2*i] = (uint8_t)(v >> 8);
        mb->txBuffer[4 + 2*i] = (uint8_t)(v & 0xFF);
    }

    mb->txCount = 3 + snapCnt * 2;
    uint16_t crc = ModbusRTU_CRC16(mb->txBuffer, mb->txCount);
    mb->txBuffer[mb->txCount++] = (uint8_t)(crc & 0xFF);
    mb->txBuffer[mb->txCount++] = (uint8_t)((crc >> 8) & 0xFF);
}

/* ---------- д���Ĵ��� 0x06��д����ٽ����� ---------- */
static void ModbusRTU_WriteSingleRegister(ModbusRTU_Slave *mb)
{
    if (mb->rxCount < 8) return;
    uint16_t addr  = (mb->rxBuffer[2] << 8) | mb->rxBuffer[3];
    uint16_t value = (mb->rxBuffer[4] << 8) | mb->rxBuffer[5];

    if (addr >= MB_HOLDING_REGS_SIZE) {
        ModbusRTU_Exception(mb, MB_FUNC_WRITE_SINGLE_REGISTER, MB_EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    ModbusRTU_PreWriteCallback(addr, value);
    {
        uint32_t pm = MB_CriticalEnter();
        mb->holdingRegs[addr] = value;
        MB_CriticalExit(pm);
    }
    ModbusRTU_PostWriteCallback(addr, value);

    /* �������󣨵�ַ+������+��ַ+ֵ+CRC�� */
    memcpy(mb->txBuffer, mb->rxBuffer, 6);
    uint16_t crc = ModbusRTU_CRC16(mb->txBuffer, 6);
    mb->txBuffer[6] = (uint8_t)(crc & 0xFF);
    mb->txBuffer[7] = (uint8_t)((crc >> 8) & 0xFF);
    mb->txCount = 8;
}

/* ---------- д��Ĵ��� 0x10��д����ٽ����� ---------- */
static void ModbusRTU_WriteMultipleRegisters(ModbusRTU_Slave *mb)
{
    if (mb->rxCount < 9) return;
    uint16_t startAddr = (mb->rxBuffer[2] << 8) | mb->rxBuffer[3];
    uint16_t quantity  = (mb->rxBuffer[4] << 8) | mb->rxBuffer[5];
    uint8_t  byteCount =  mb->rxBuffer[6];
    uint16_t endAddr   = startAddr + quantity - 1;

    if (quantity < 1 || quantity > 123 || byteCount != quantity * 2) {
        ModbusRTU_Exception(mb, MB_FUNC_WRITE_MULTIPLE_REGISTERS, MB_EX_ILLEGAL_DATA_VALUE);
        return;
    }
    if (endAddr >= MB_HOLDING_REGS_SIZE) {
        ModbusRTU_Exception(mb, MB_FUNC_WRITE_MULTIPLE_REGISTERS, MB_EX_ILLEGAL_DATA_ADDRESS);
        return;
    }
    if (mb->rxCount < (uint16_t)(7 + byteCount + 2)) {
        ModbusRTU_Exception(mb, MB_FUNC_WRITE_MULTIPLE_REGISTERS, MB_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t v = (mb->rxBuffer[7 + 2*i] << 8) | mb->rxBuffer[8 + 2*i];
        ModbusRTU_PreWriteCallback(startAddr + i, v);
        {
            uint32_t pm = MB_CriticalEnter();
            mb->holdingRegs[startAddr + i] = v;
            MB_CriticalExit(pm);
        }
        ModbusRTU_PostWriteCallback(startAddr + i, v);
    }

    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = MB_FUNC_WRITE_MULTIPLE_REGISTERS;
    mb->txBuffer[2] = (uint8_t)(startAddr >> 8);
    mb->txBuffer[3] = (uint8_t)(startAddr & 0xFF);
    mb->txBuffer[4] = (uint8_t)(quantity >> 8);
    mb->txBuffer[5] = (uint8_t)(quantity & 0xFF);

    mb->txCount = 6;
    uint16_t crc = ModbusRTU_CRC16(mb->txBuffer, mb->txCount);
    mb->txBuffer[mb->txCount++] = (uint8_t)(crc & 0xFF);
    mb->txBuffer[mb->txCount++] = (uint8_t)((crc >> 8) & 0xFF);
}

/* ---------- �쳣��Ӧ ---------- */
static void ModbusRTU_Exception(ModbusRTU_Slave *mb, uint8_t funcCode, uint8_t exceptionCode)
{
    mb->txBuffer[0] = mb->slaveAddr;
    mb->txBuffer[1] = funcCode | 0x80;
    mb->txBuffer[2] = exceptionCode;
    mb->txCount = 3;
    uint16_t crc = ModbusRTU_CRC16(mb->txBuffer, mb->txCount);
    mb->txBuffer[mb->txCount++] = (uint8_t)(crc & 0xFF);
    mb->txBuffer[mb->txCount++] = (uint8_t)((crc >> 8) & 0xFF);
}

/* ---------- ����һ֡ ---------- */
static void ModbusRTU_ProcessFrame(ModbusRTU_Slave *mb)
{
    if (mb->rxCount < 4) return;

    /* ��ַƥ���㲥 */
    uint8_t addr = mb->rxBuffer[0];
    uint8_t isBroadcast = (addr == 0);
    if (!isBroadcast && addr != mb->slaveAddr) return;

    /* CRC У�� */
    uint16_t crcRx = (uint16_t)((mb->rxBuffer[mb->rxCount - 1] << 8) | mb->rxBuffer[mb->rxCount - 2]);
    uint16_t crcClc = ModbusRTU_CRC16(mb->rxBuffer, mb->rxCount - 2);
    if (crcRx != crcClc) return;

    uint8_t fc = mb->rxBuffer[1];
    switch (fc) {
        case MB_FUNC_READ_HOLDING_REGISTERS:   ModbusRTU_ReadHoldingRegisters(mb);  break;
        case MB_FUNC_READ_INPUT_REGISTERS:     ModbusRTU_ReadInputRegisters(mb);    break;
        case MB_FUNC_WRITE_SINGLE_REGISTER:    ModbusRTU_WriteSingleRegister(mb);   break;
        case MB_FUNC_WRITE_MULTIPLE_REGISTERS: ModbusRTU_WriteMultipleRegisters(mb);break;
        default:
            ModbusRTU_Exception(mb, fc, MB_EX_ILLEGAL_FUNCTION);
            break;
    }

    /* �㲥���ذ������лذ����� DMA ���� Tx ��ɻص��ؿ����գ����򵱳��ؿ����� */
    if (mb->txCount > 0 && !isBroadcast) {
        RS485_TxEnable(mb->huart);
        s_txInProgress = 1;
        HAL_UART_Transmit_DMA(mb->huart, mb->txBuffer, mb->txCount);
    } else {
        MB_RestartRx(mb);
    }
}

/* ---------- ������ ---------- */
void ModbusRTU_Process(ModbusRTU_Slave *mb)
{
    if (mb->rxComplete) {
        ModbusRTU_ProcessFrame(mb);
        /* ��û�н��뷢�����̣��㲥/�쳣/У��ʧ�ܣ��������ؿ����� */
        if (!s_txInProgress) {
            MB_RestartRx(mb);
        }
    }
}

/* ---------- ��ʱ�����ף���ѡ�� ---------- */
void ModbusRTU_TimerISR(ModbusRTU_Slave *mb)
{
    if (mb->frameReceiving) {
        uint32_t now = HAL_GetTick();
        if ((now - mb->lastReceiveTime) >= 4U) { /* �� 9600bps Լ 3.5T */
            mb->frameReceiving = 0;
            HAL_UART_DMAStop(mb->huart);
            mb->rxCount = MB_RTU_FRAME_MAX_SIZE - __HAL_DMA_GET_COUNTER(mb->huart->hdmarx);
            mb->rxComplete = 1;
        }
    }
}

/* ---------- IDLE �жϻص���ǿ�ƽ�֡�� ---------- */
void ModbusRTU_UartRxCallback(ModbusRTU_Slave *mb)
{
    HAL_UART_DMAStop(mb->huart);
    mb->rxCount = MB_RTU_FRAME_MAX_SIZE - __HAL_DMA_GET_COUNTER(mb->huart->hdmarx);
    mb->rxComplete = 1;
    mb->frameReceiving = 0;
    mb->lastReceiveTime = HAL_GetTick();
}

/* ---------- ��������û��ص� ---------- */
__weak void ModbusRTU_PreWriteCallback(uint16_t addr, uint16_t value)
{
    (void)addr; (void)value;
}
__weak void ModbusRTU_PostWriteCallback(uint16_t addr, uint16_t value)
{
    (void)addr; (void)value;
}

/* ---------- ��ѡ�����ļ�ֱ���ṩ�жϴ������������� stm32f1xx_it.c�� ---------- */
#if MB_PROVIDE_IRQ_HANDLERS
/* ��Щ�ⲿ����� main.c �ṩ */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;
extern TIM_HandleTypeDef  htim2;

void USART1_IRQHandler(void)
{
    /* �ȴ��� IDLE���ٽ��� HAL */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        if (s_active_mb) ModbusRTU_UartRxCallback(s_active_mb);
    }
    HAL_UART_IRQHandler(&huart1);
}

void DMA1_Channel4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_tx); }
void DMA1_Channel5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_rx); }

void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&htim2); }

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && s_active_mb) {
        ModbusRTU_TimerISR(s_active_mb);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (s_active_mb && huart == s_active_mb->huart) {
        RS485_RxEnable();
        s_txInProgress = 0;
        MB_RestartRx(s_active_mb);
        s_active_mb->txCount = 0;
    }
}
#endif /* MB_PROVIDE_IRQ_HANDLERS */
