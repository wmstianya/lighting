/**
 * @file modbus_hal.h
 * @brief Modbus RTU�ӻ�Э��ջ��Ӳ�������ӿڶ���
 * @details
 * ���ļ�������һ���׼���ĺ����ӿڣ�Э��ջ�����߼�ͨ��������Щ������
 * �����ײ�Ӳ������UART, DMA, GPIO����Ҫ��Э��ջ��ֲ���µ�MCUƽ̨��
 * ������ֻ��Ϊ��ƽ̨ʵ������ļ��ж�������к������ɡ�
 */

#ifndef MODBUS_HAL_H
#define MODBUS_HAL_H

#include "modbus_slave.h"

/* ����ʹ��һ��ͨ�õ�״̬ö�٣�����STM32 HAL���HAL_StatusTypeDef */
/* ���ƽ̨��ͬ�����Զ����Լ���״̬���� */

//=============================================================================
// HAL API����ԭ�� (HAL API Prototypes)
//=============================================================================

/**
 * @brief ��ʼ��һ��Modbusͨ�������Ӳ����Դ
 * @note ʵ�ʵ�Ӳ����ʼ������ʱ�ӡ����Ÿ��ã�ͨ����ƽ̨�������ɹ���
 *       ����STM32CubeMX����main��������ɡ��˺�����Ҫ��������Э��ջ
 *       ��������ĳ�ʼ�����������һ������DMA���ա�
 * @param pInstance ָ�����ʼ����Modbusʵ��������
 * @return HAL_StatusTypeDef �����ɹ���ʧ�ܵ�״̬
 */
HAL_StatusTypeDef Modbus_HAL_Init(ModbusInstance_t *pInstance);

/**
 * @brief ����ָ��ͨ����DMA����
 * @param pInstance ָ��Ŀ��Modbusʵ��������
 * @return HAL_StatusTypeDef �����ɹ���ʧ�ܵ�״̬
 */
HAL_StatusTypeDef Modbus_HAL_StartReception(ModbusInstance_t *pInstance);

/**
 * @brief ָֹͣ��ͨ����DMA����
 * @note ����IDLE�ж������ھ�ȷ������ճ��ȡ�
 * @param pInstance ָ��Ŀ��Modbusʵ��������
 * @return HAL_StatusTypeDef �����ɹ���ʧ�ܵ�״̬
 */
HAL_StatusTypeDef Modbus_HAL_StopReception(ModbusInstance_t *pInstance);

/**
 * @brief ʹ��DMA����ָ�����ȵ�����
 * @param pInstance ָ��Ŀ��Modbusʵ��������
 * @param u16Length ��Ҫ���͵��ֽ���
 * @return HAL_StatusTypeDef �����ɹ���ʧ�ܵ�״̬
 */
HAL_StatusTypeDef Modbus_HAL_Transmit(ModbusInstance_t *pInstance, uint16_t u16Length);

/**
 * @brief ��ָ��ͨ����RS485�շ�������Ϊ����ģʽ
 * @param pInstance ָ��Ŀ��Modbusʵ��������
 */
void Modbus_HAL_SetDirTx(ModbusInstance_t *pInstance);

/**
 * @brief ��ָ��ͨ����RS485�շ�������Ϊ����ģʽ
 * @param pInstance ָ��Ŀ��Modbusʵ��������
 */
void Modbus_HAL_SetDirRx(ModbusInstance_t *pInstance);


#endif // MODBUS_HAL_H
