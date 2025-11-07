/**
 * @file app_modbus.c
 * @brief 应用层Modbus接口的实现
 * @details
 * 该文件演示如何：
 * 1. 建立Modbus的寄存器映射区域
 * 2. 实现应用层回调函数，完成数据读写操作
 * 3. 初始化并连接ModbusRTU协议栈实例
 * 4. 集成继电器控制功能到Modbus寄存器
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#include "app_modbus.h"
#include "modbus_slave.h"
#include "modbus_hal.h"
#include "relay.h"

// 声明g_modbus_instance1和g_modbus_instance2在main.c中定义
extern ModbusInstance_t g_modbus_instance1;
extern ModbusInstance_t g_modbus_instance2;

//=============================================================================
// 1. 应用数据存储 (Application Data Storage)
//=============================================================================

/**
 * @brief Modbus寄存器地址定义
 * @details 定义各功能模块在Modbus寄存器中的地址分配
 */
#define MODBUS_REG_RELAY_CONTROL    0   /**< 继电器控制寄存器地址 */
#define MODBUS_REG_RELAY_STATUS     1   /**< 继电器状态寄存器地址 */
#define MODBUS_REG_SYSTEM_INFO      2   /**< 系统信息寄存器起始地址 */

/**
 * @brief Modbus保持寄存器映射区域
 * @details 寄存器功能分配：
 *          - 寄存器0: 继电器控制 (bit0-bit4对应继电器1-5)
 *          - 寄存器1: 继电器状态反馈 (只读)
 *          - 寄存器2-63: 系统扩展区域
 */
static uint16_t holding_registers[MODBUS_HOLDING_REG_COUNT];

//=============================================================================
// 2. 应用回调函数实现 (Application Callback Implementation)
//=============================================================================

/**
 * @brief 应用层实现的回调函数
 * @details
 * 当协议栈需要应用数据的时候，协议栈会在验证通过一条合法的命令后，
 * 调用此函数来执行实际的数据读写操作。
 * @note 此函数可能由不同Modbus实例并发调用，需要为不同通信实现不同逻辑。
 *       这里用ModbusInstance_t结构体中的一个用户数据指针(pUserData)，
 *       用于回调函数中区分不同的设备。
 */
int App_RegisterCallback(uint8_t u8FuncCode, uint16_t u16Addr, uint16_t u16Count, uint16_t* pData)
{
    // 检查请求的地址范围是否合法
    if ((u16Addr + u16Count) > MODBUS_HOLDING_REG_COUNT)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    switch (u8FuncCode)
    {
#if (MODBUS_SUPPORT_FC03 == 1)
        case 0x03: // 读保持寄存器
            // 将应用数据到协议栈提供的缓冲区
            for (int i = 0; i < u16Count; i++)
            {
                uint16_t regAddr = u16Addr + i;
                
                // 根据寄存器地址返回相应数据
                if (regAddr == MODBUS_REG_RELAY_STATUS)
                {
                    // 返回继电器实时状态
                    pData[i] = (uint16_t)relayGetAllStates();
                }
                else
                {
                    // 返回寄存器缓存值
                    pData[i] = holding_registers[regAddr];
                }
            }
            break;
#endif
            
#if (MODBUS_SUPPORT_FC06 == 1)
        case 0x06: // 写单个寄存器
            // 处理继电器控制寄存器
            if (u16Addr == MODBUS_REG_RELAY_CONTROL)
            {
                // 控制继电器状态
                if (relaySetAllStates((uint8_t)(*pData)) == HAL_OK)
                {
                    holding_registers[u16Addr] = *pData;
                }
                else
                {
                    return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
                }
            }
            else
            {
                // 普通寄存器写入
                holding_registers[u16Addr] = *pData;
            }
            break;
#endif
            
#if (MODBUS_SUPPORT_FC10 == 1)
        case 0x10: // 写多个寄存器
            // 处理多个寄存器写入
            for (int i = 0; i < u16Count; i++)
            {
                uint16_t regAddr = u16Addr + i;
                
                // 处理继电器控制寄存器
                if (regAddr == MODBUS_REG_RELAY_CONTROL)
                {
                    if (relaySetAllStates((uint8_t)pData[i]) == HAL_OK)
                    {
                        holding_registers[regAddr] = pData[i];
                    }
                    else
                    {
                        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
                    }
                }
                else
                {
                    // 普通寄存器写入
                    holding_registers[regAddr] = pData[i];
                }
            }
            break;
#endif
        default:
            // 这里不会到达，因为协议栈已经过滤了不支持的功能码
            return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }

    return MODBUS_OK; // 操作成功
}

//=============================================================================
// 3. 应用初始化函数 (Application Initialization Function)
//=============================================================================

void App_Modbus_Init(void)
{
    /* 初始化保持寄存器为默认值 */
    for(int i = 0; i < MODBUS_HOLDING_REG_COUNT; i++)
    {
        holding_registers[i] = 0;
    }
    
    // 同步继电器状态到寄存器
    holding_registers[MODBUS_REG_RELAY_CONTROL] = relayGetAllStates();
    holding_registers[MODBUS_REG_RELAY_STATUS] = relayGetAllStates();

    /* === 初始化Modbus实例1 (USART1) === */
    // Modbus_Init在main.c中调用，这里只注册回调函数和硬件
    Modbus_RegisterCallback(&g_modbus_instance1, App_RegisterCallback);
    Modbus_HAL_Init(&g_modbus_instance1); // 启动第一次DMA接收

    /* === 初始化Modbus实例2 (USART2) === */
    // Modbus_Init在main.c中调用，这里只注册回调函数和硬件
    Modbus_RegisterCallback(&g_modbus_instance2, App_RegisterCallback);
    Modbus_HAL_Init(&g_modbus_instance2); // 启动第一次DMA接收
}
