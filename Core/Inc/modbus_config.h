/**
 * @file modbus_config.h
 * @brief Modbus RTU�ӻ�Э��ջ��ȫ�������ļ�
 * @details
 * ���ļ�������Э��ջ�������û������ò��������绺������С��֧�ֵĹ����롢
 * �Ĵ��������ȡ�ͨ���޸Ĵ��ļ����������ɵؽ�Э��ջ���䵽��ͬ��Ӧ�ó�����
 * ��Դ�����У�������Ķ�����Э����롣
 */

#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

//=============================================================================
// 1. ��Դ���� (Resource Configuration)
//=============================================================================

/**
 * @brief Modbus RTU֡����󻺳�����С (�ֽ�)
 * @note Modbus RTU�淶��������֡����Ϊ256�ֽڡ���ֱֵ��Ӱ��ÿ��Modbusʵ��
 *       �����RAM��С�������ڴ漫�����޵��豸���������ȷ����վ����ĳ���
 *       ���ᳬ���ض�ֵ�������ʵ���С�˺ꡣ
 */
#define MODBUS_BUFFER_SIZE 128

/**
 * @brief Ӧ�ò㱣�ּĴ���������
 * @note �������ʾ��Ӧ�ò�(app_modbus.c)�Ķ��壬Э��ջ�����������˺ꡣ
 *       �û�Ӧ����ʵ����Ŀ����������������ģ�͵Ĵ�С��
 */
#define MODBUS_HOLDING_REG_COUNT 64


//=============================================================================
// 2. ���ܲü� (Feature Flags)
//=============================================================================

/**
 * @brief �Ƿ�֧�ֹ����� 0x03 (Read Holding Registers)
 * @note ��Ϊ1�����ã���Ϊ0�Խ��á����ò�֧�ֵĹ�������Լ�СFlashռ�á�
 */
#define MODBUS_SUPPORT_FC03 1

/**
 * @brief �Ƿ�֧�ֹ����� 0x06 (Write Single Register)
 * @note ��Ϊ1�����ã���Ϊ0�Խ��á�
 */
#define MODBUS_SUPPORT_FC06 1

/**
 * @brief �Ƿ�֧�ֹ����� 0x10 (Write Multiple Registers)
 * @note ��Ϊ1�����ã���Ϊ0�Խ��á�
 */
#define MODBUS_SUPPORT_FC10 1


//=============================================================================
// 3. Э�鳣�����쳣�� (Protocol Constants & Exception Codes)
//=============================================================================

/**
 * @brief Modbus�㲥��ַ
 */
#define MODBUS_BROADCAST_ADDRESS 0

/**
 * @brief Ӧ�ò�ص����������룺�����ɹ�
 */
#define MODBUS_OK 0

/**
 * @brief Modbus�쳣�룺�Ƿ����� (ILLEGAL FUNCTION)
 * @details �����յ�Э��ջ��֧�ֵĹ�����ʱ���� ��
 */
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01

/**
 * @brief Modbus�쳣�룺�Ƿ����ݵ�ַ (ILLEGAL DATA ADDRESS)
 * @details ������ļĴ�����ַ��Χ����Ӧ�ò㶨�����Ч��Χʱ���� ��
 */
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02

/**
 * @brief Modbus�쳣�룺�Ƿ�����ֵ (ILLEGAL DATA VALUE)
 * @details ��д����Ĵ���(0x10)�����У���������ֽڼ���ֵ��Ĵ���������ƥ��ʱ���� ��
 */
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03


#endif // MODBUS_CONFIG_H
