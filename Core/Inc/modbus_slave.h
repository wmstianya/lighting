/**
 * @file modbus_slave.h
 * @brief Modbus RTU�ӻ�Э��ջ�Ĺ����ӿ��ļ�
 * @details
 * ���ļ�������Э��ջ�ĺ������ݽṹ (ModbusInstance_t)��״̬����
 * Ӧ�ò�ص�����ָ�������Լ����й��ⲿ���õĹ���API��
 */

#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>
#include "modbus_config.h"

/* ���ʹ��STM32 HAL�⣬��������ͷ�ļ��Ի�ȡӲ��������Ͷ��� */
/* �û��ɸ����Լ���ƽ̨�޸Ĵ˲��� */
#ifdef USE_HAL_DRIVER
#include "stm32f1xx_hal.h"
#endif

//=============================================================================
// 1. ����ö�������Ͷ��� (Core Enums & Typedefs)
//=============================================================================

/**
 * @brief ModbusЭ��ջ״̬��ö��
 * @details ������Э��ջ�ڴ���һ֡Modbus����ʱ���ܾ����ĸ���״̬��
 *          ����Modbus_Poll()�����ڲ��߼��ĺ��� ��
 */
typedef enum
{
    STATE_IDLE,              /**< ���У��ȴ���֡ */
    STATE_FRAME_RECEIVED,    /**< ֡������ɣ���IDLE�ж���λ�����ȴ����� */
    STATE_PROCESSING,        /**< ���ڴ���֡��У�顢������ */
    STATE_BUILDING_RESPONSE, /**< ���ڹ�����Ӧ֡ */
    STATE_TRANSMITTING       /**< ����ͨ��DMA������Ӧ */
} ModbusState_e;

/**
 * @brief Ӧ�ò�ص�����ָ������
 * @details
 * ����ʵ��Э��ջ��Ӧ�ò����ݽ���Ĺؼ������Ʒ�תģʽ����
 * Ӧ�ò����ʵ��һ�������͵ĺ��������ڳ�ʼ��ʱע�ᵽЭ��ջʵ���С�
 * ��Э��ջ��Ҫ��д�Ĵ���ʱ������ô˺�����
 *
 * @param u8FuncCode ������ (���� 0x03, 0x06, 0x10)��
 * @param u16Addr    �������ʼ�Ĵ�����ַ��
 * @param u16Count   ����ļĴ�������������д�����Ĵ���(0x06)����ֵΪ1��
 * @param pData      һ��ָ�����ݻ�������ָ�롣
 *                   - ���ڶ�����(0x03)��Ӧ�ò��轫��ȡ��������䵽�˻�������
 *                   - ����д����(0x06, 0x10)���˻�����������վ���͹��������ݣ�
 *                     Ӧ�ò��轫��д���Լ�����������
 *                     ע�⣺����0x06��pDataָ����ǵ���16λ�Ĵ���ֵ��
 *
 * @return int ����0 (MODBUS_OK) ��ʾ�ɹ������ط����Modbus�쳣��
 *             (���� MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS) ��ʾ��������
 *             Э��ջ�������������쳣��Ӧ��
 */
typedef int (*pfnModbusCallback)(uint8_t u8FuncCode, uint16_t u16Addr, uint16_t u16Count, uint16_t* pData);


//=============================================================================
// 2. Modbusʵ�������Ľṹ�� (Context Structure)
//=============================================================================

/**
 * @brief Modbusʵ�������Ľṹ��
 * @details
 * ����ʵ�ֿ�����Ͷ�ʵ��֧�ֵĺ��� ���ýṹ���װ�˵���Modbusͨ��
 * �����ȫ��״̬��Ϣ��ʹ������Э��ջ��������ͨ�������˽ṹ��ָ����������
 * ���������κ�ȫ�ֻ�̬������
 */
typedef struct
{
    /* === Ӳ����ؾ�� (��Ӧ�ò��ڳ�ʼ��ʱ����) === */
#ifdef USE_HAL_DRIVER
    UART_HandleTypeDef* huart;      /**< ָ��HAL���UART��� */
    DMA_HandleTypeDef*  hdma_rx;    /**< ָ��HAL���DMA����ͨ����� */
    DMA_HandleTypeDef*  hdma_tx;    /**< ָ��HAL���DMA����ͨ����� */
    GPIO_TypeDef*       de_re_port; /**< RS485����������ŵ�GPIO�˿� */
    uint16_t            de_re_pin;  /**< RS485����������ŵ�GPIO Pin */
#else
    void*               hw_handle1; /* �����Ӳ���������������ƽ̨ */
    void*               hw_handle2;
#endif

    /* === Э��״̬������ === */
    volatile ModbusState_e eState;         /**< Э��״̬���ĵ�ǰ״̬ (volatile��ֹ�������Ż�) */
    uint8_t                u8SlaveAddress; /**< ��ʵ�����õĴӻ���ַ (1-247) */

    /* === ���ݻ����� === */
    uint8_t  au8RxBuffer[MODBUS_BUFFER_SIZE]; /**< DMA���ջ����� */
    uint8_t  au8TxBuffer[MODBUS_BUFFER_SIZE]; /**< ��Ӧ֡�����뷢�ͻ����� */
    uint16_t u16RxLen;                        /**< ������յ���֡�ĳ��� */

    /* === Ӧ�ò�ӿ� === */
    pfnModbusCallback pfnAppCallback; /**< ָ��Ӧ�ò����ݴ����ص�������ָ�� */

} ModbusInstance_t;


//=============================================================================
// 3. ����API����ԭ�� (Public API Prototypes)
//=============================================================================

/**
 * @brief ��ʼ��һ��Modbusʵ���ṹ��
 * @param pInstance ָ��Ҫ��ʼ����Modbusʵ��
 * @param u8SlaveAddr ��ʼ�Ĵӻ���ַ
 * @param huart ������UART���
 * @param hdma_rx ������DMA���վ��
 * @param hdma_tx ������DMA���;��
 * @param de_re_port RS485�������GPIO�˿�
 * @param de_re_pin RS485�������GPIO Pin
 */
void Modbus_Init(ModbusInstance_t *pInstance,
                 uint8_t u8SlaveAddr,
                 UART_HandleTypeDef* huart,
                 DMA_HandleTypeDef* hdma_rx,
                 DMA_HandleTypeDef* hdma_tx,
                 GPIO_TypeDef* de_re_port,
                 uint16_t de_re_pin);

/**
 * @brief ΪModbusʵ��ע��Ӧ�ò�ص�����
 * @param pInstance ָ��Ŀ��Modbusʵ��
 * @param pfnCallback Ӧ�ò�ʵ�ֵĻص�������ָ��
 */
void Modbus_RegisterCallback(ModbusInstance_t *pInstance, pfnModbusCallback pfnCallback);

/**
 * @brief ������ʱ���û����Modbusʵ���Ĵӻ���ַ
 * @param pInstance ָ��Ŀ��Modbusʵ��
 * @param u8Addr �µĴӻ���ַ (1-247)
 */
void Modbus_SetSlaveAddress(ModbusInstance_t *pInstance, uint8_t u8Addr);

/**
 * @brief Э��ջ�ĺ�����ѯ����
 * @details
 * �˺�����������ѭ���б������Եء�Ƶ���ص��á����ڲ�ʵ����һ��״̬����
 * ������������Э��Ĵ������̣�����֡У�顢���������ûص���������Ӧ�ȡ�
 * �˺���Ϊ��������ơ�
 * @param pInstance ָ��Ҫ������Modbusʵ��
 */
void Modbus_Poll(ModbusInstance_t *pInstance);


#endif // MODBUS_SLAVE_H
