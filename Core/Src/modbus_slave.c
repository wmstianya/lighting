/**
 * @file modbus_slave.c
 * @brief Modbus RTU�ӻ�Э��ջ�ĺ����߼�ʵ��
 * @details
 * ���ļ�ʵ������Ӳ���޹ص�Э�鴦�����ܣ�����״̬��������֡У�顢
 * ���������������Ӧ�ûص��Լ�������Ӧ��
 */

#include "modbus_slave.h"
#include "modbus_hal.h"
#include <string.h> // for memcpy

//=============================================================================
// 1. �ڲ���������ԭ�� (Internal Helper Function Prototypes)
//=============================================================================

/**
 * @brief ����Modbus RTU֡��CRC-16У����
 * @param puchMsg ָ������֡��ָ��
 * @param usDataLen ����֡�ĳ���
 * @return uint16_t �������CRC-16ֵ
 */
static uint16_t prvCRC16(const uint8_t *puchMsg, uint16_t usDataLen);

/**
 * @brief ����һ֡�����ġ��ѽ��յ�����
 * @param pInstance ָ��ǰModbusʵ��
 */
static void prvProcessFrame(ModbusInstance_t *pInstance);

/**
 * @brief ������׼������һ��Modbus�쳣��Ӧ
 * @param pInstance ָ��ǰModbusʵ��
 * @param u8FuncCode ԭʼ����Ĺ�����
 * @param u8ExceptionCode Modbus�쳣��
 */
static void prvBuildExceptionResponse(ModbusInstance_t *pInstance, uint8_t u8FuncCode, uint8_t u8ExceptionCode);


//=============================================================================
// 2. ����API����ʵ�� (Public API Implementations)
//=============================================================================

void Modbus_Init(ModbusInstance_t *pInstance,
                 uint8_t u8SlaveAddr,
                 UART_HandleTypeDef* huart,
                 DMA_HandleTypeDef* hdma_rx,
                 DMA_HandleTypeDef* hdma_tx,
                 GPIO_TypeDef* de_re_port,
                 uint16_t de_re_pin)
{
    if (pInstance == NULL)
    {
        return;
    }

    pInstance->u8SlaveAddress = u8SlaveAddr;
    pInstance->huart = huart;
    pInstance->hdma_rx = hdma_rx;
    pInstance->hdma_tx = hdma_tx;
    pInstance->de_re_port = de_re_port;
    pInstance->de_re_pin = de_re_pin;

    pInstance->eState = STATE_IDLE;
    pInstance->u16RxLen = 0;
    pInstance->pfnAppCallback = NULL;
    
    // 清空接收和发送缓冲区
    for(int i = 0; i < MODBUS_BUFFER_SIZE; i++)
    {
        pInstance->au8RxBuffer[i] = 0;
        pInstance->au8TxBuffer[i] = 0;
    }
    
    // 确保RS485处于接收模式
    if (pInstance->de_re_port != NULL)
    {
        HAL_GPIO_WritePin(pInstance->de_re_port, pInstance->de_re_pin, GPIO_PIN_RESET);
    }
}

void Modbus_RegisterCallback(ModbusInstance_t *pInstance, pfnModbusCallback pfnCallback)
{
    if (pInstance!= NULL)
    {
        pInstance->pfnAppCallback = pfnCallback;
    }
}

void Modbus_SetSlaveAddress(ModbusInstance_t *pInstance, uint8_t u8Addr)
{
    if (pInstance!= NULL)
    {
        pInstance->u8SlaveAddress = u8Addr;
    }
}

void Modbus_Poll(ModbusInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return;
    }

    // ״̬������
    switch (pInstance->eState)
    {
        case STATE_IDLE:
            // ����״̬���ȴ�IDLE�жϽ�״̬�л���STATE_FRAME_RECEIVED
            break;

        case STATE_FRAME_RECEIVED:
            // �յ���֡�������л�״̬����ʼ����
            pInstance->eState = STATE_PROCESSING;
            prvProcessFrame(pInstance);
            break;

        case STATE_PROCESSING:
        case STATE_BUILDING_RESPONSE:
        case STATE_TRANSMITTING:
            // ��Щ��˲ʱ״̬��ȴ�Ӳ����ɵ�״̬����Poll���������账��
            break;

        default:
            // �쳣״̬����λ
            pInstance->eState = STATE_IDLE;
            break;
    }
}


//=============================================================================
// 3. �ڲ���������ʵ�� (Internal Helper Implementations)
//=============================================================================

static void prvProcessFrame(ModbusInstance_t *pInstance)
{
    // 1. ��С����У�� (��ַ + ������ + CRC�� + CRC��)
    if (pInstance->u16RxLen < 4)
    {
        pInstance->eState = STATE_IDLE;
        return;
    }

    uint8_t u8SlaveAddr = pInstance->au8RxBuffer[0];
    uint8_t u8FuncCode = pInstance->au8RxBuffer[1];

    // 2. ��ַУ��
    if ((u8SlaveAddr!= pInstance->u8SlaveAddress) && (u8SlaveAddr!= MODBUS_BROADCAST_ADDRESS))
    {
        pInstance->eState = STATE_IDLE;
        return;
    }

    // 3. CRC校验
    uint16_t u16CalculatedCRC = prvCRC16(pInstance->au8RxBuffer, pInstance->u16RxLen - 2);
    // Modbus RTU: CRC低字节在前，高字节在后
    uint16_t u16ReceivedCRC = (pInstance->au8RxBuffer[pInstance->u16RxLen - 2]) | (pInstance->au8RxBuffer[pInstance->u16RxLen - 1] << 8);

    if (u16CalculatedCRC!= u16ReceivedCRC)
    {
        pInstance->eState = STATE_IDLE;
        return;
    }

    // 4. �����봦����Ӧ�ûص�
    pInstance->eState = STATE_BUILDING_RESPONSE;
    int callback_result = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    uint16_t u16StartAddr, u16RegCount, u16RegValue;
    uint16_t u16TxLen = 0;

    switch (u8FuncCode)
    {
#if (MODBUS_SUPPORT_FC03 == 1)
        case 0x03: // �����ּĴ���
            if (pInstance->u16RxLen == 8)
            {
                u16StartAddr = (pInstance->au8RxBuffer[2] << 8) | pInstance->au8RxBuffer[3];
                u16RegCount = (pInstance->au8RxBuffer[4] << 8) | pInstance->au8RxBuffer[5];
                
                // �������ļĴ��������Ƿ��ں�����Χ��
                if (u16RegCount > 0 && u16RegCount <= 125)
                {
                    // ׼��һ����ʱ���������ص��������
                    uint16_t temp_data_buffer[125];
                    if (pInstance->pfnAppCallback!= NULL)
                    {
                        callback_result = pInstance->pfnAppCallback(u8FuncCode, u16StartAddr, u16RegCount, temp_data_buffer);
                    }

                    if (callback_result == MODBUS_OK)
                    {
                        pInstance->au8TxBuffer[0] = pInstance->u8SlaveAddress;
                        pInstance->au8TxBuffer[1] = u8FuncCode;
                        pInstance->au8TxBuffer[2] = (uint8_t)(u16RegCount * 2);
                        for (int i = 0; i < u16RegCount; i++)
                        {
                            pInstance->au8TxBuffer[3 + i * 2] = (uint8_t)(temp_data_buffer[i] >> 8);
                            pInstance->au8TxBuffer[4 + i * 2] = (uint8_t)(temp_data_buffer[i] & 0xFF);
                        }
                        u16TxLen = 3 + u16RegCount * 2;
                    }
                }
                else
                {
                    callback_result = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE; // �������Ϸ�
                }
            }
            break;
#endif
#if (MODBUS_SUPPORT_FC06 == 1)
        case 0x06: // д�����Ĵ���
            if (pInstance->u16RxLen == 8)
            {
                u16StartAddr = (pInstance->au8RxBuffer[2] << 8) | pInstance->au8RxBuffer[3];
                u16RegValue = (pInstance->au8RxBuffer[4] << 8) | pInstance->au8RxBuffer[5];
                if (pInstance->pfnAppCallback!= NULL)
                {
                    callback_result = pInstance->pfnAppCallback(u8FuncCode, u16StartAddr, 1, &u16RegValue);
                }

                if (callback_result == MODBUS_OK)
                {
                    // �ɹ���ԭ����������֡��Ϊ��Ӧ
                    memcpy(pInstance->au8TxBuffer, pInstance->au8RxBuffer, 8);
                    u16TxLen = 8;
                }
            }
            break;
#endif
#if (MODBUS_SUPPORT_FC10 == 1)
        case 0x10: // д����Ĵ���
            if (pInstance->u16RxLen >= 9)
            {
                u16StartAddr = (pInstance->au8RxBuffer[2] << 8) | pInstance->au8RxBuffer[3];
                u16RegCount = (pInstance->au8RxBuffer[4] << 8) | pInstance->au8RxBuffer[5];
                uint8_t u8ByteCount = pInstance->au8RxBuffer[6];

                if ((u8ByteCount == u16RegCount * 2) && (u16RegCount > 0 && u16RegCount <= 123))
                {
                    if (pInstance->pfnAppCallback!= NULL)
                    {
                        callback_result = pInstance->pfnAppCallback(u8FuncCode, u16StartAddr, u16RegCount, (uint16_t*)&pInstance->au8RxBuffer[7]);
                    }

                    if (callback_result == MODBUS_OK)
                    {
                        pInstance->au8TxBuffer[0] = pInstance->u8SlaveAddress;
                        pInstance->au8TxBuffer[1] = u8FuncCode;
                        memcpy(&pInstance->au8TxBuffer[2], &pInstance->au8RxBuffer[2], 4); // ������ʼ��ַ������
                        u16TxLen = 6;
                    }
                }
                else
                {
                    callback_result = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
                }
            }
            break;
#endif
        default:
            callback_result = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            break;
    }

    // 5. ������Ӧ�����㲥
    if (u8SlaveAddr!= MODBUS_BROADCAST_ADDRESS)
    {
        if (callback_result!= MODBUS_OK)
        {
            prvBuildExceptionResponse(pInstance, u8FuncCode, callback_result);
        }
        else
        {
            uint16_t u16ResponseCRC = prvCRC16(pInstance->au8TxBuffer, u16TxLen);
            pInstance->au8TxBuffer[u16TxLen] = (uint8_t)(u16ResponseCRC & 0xFF);
            pInstance->au8TxBuffer[u16TxLen + 1] = (uint8_t)(u16ResponseCRC >> 8);
            
            pInstance->eState = STATE_TRANSMITTING;
            Modbus_HAL_Transmit(pInstance, u16TxLen + 2);
        }
    }
    else // ���ڹ㲥��ַ��ִֻ��д����������Ӧ 
    {
        pInstance->eState = STATE_IDLE;
    }
}

static void prvBuildExceptionResponse(ModbusInstance_t *pInstance, uint8_t u8FuncCode, uint8_t u8ExceptionCode)
{
    pInstance->au8TxBuffer[0] = pInstance->u8SlaveAddress;
    pInstance->au8TxBuffer[1] = u8FuncCode | 0x80; // ���������λ��1
    pInstance->au8TxBuffer[2] = u8ExceptionCode;

    uint16_t u16CRC = prvCRC16(pInstance->au8TxBuffer, 3);
    pInstance->au8TxBuffer[3] = (uint8_t)(u16CRC & 0xFF);
    pInstance->au8TxBuffer[4] = (uint8_t)(u16CRC >> 8);

    pInstance->eState = STATE_TRANSMITTING;
    Modbus_HAL_Transmit(pInstance, 5);
}

static uint16_t prvCRC16(const uint8_t *puchMsg, uint16_t usDataLen)
{
    static const uint8_t auchCRCHi[] = {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
        0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
        0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
        0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
        0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
        0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
        0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
        0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
        0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
        0x40
    };
    static const uint8_t auchCRCLo[] = {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
        0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
        0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
        0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
        0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
        0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
        0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
        0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
        0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
        0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
        0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
        0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
        0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
        0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
        0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
        0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
        0x40
    };
    uint8_t uchCRCHi = 0xFF;
    uint8_t uchCRCLo = 0xFF;
    uint16_t uIndex;

    while (usDataLen--)
    {
        uIndex = uchCRCLo ^ *puchMsg++;
        uchCRCLo = uchCRCHi ^ auchCRCHi[uIndex];
        uchCRCHi = auchCRCLo[uIndex];
    }
    return (uchCRCHi << 8 | uchCRCLo);
}