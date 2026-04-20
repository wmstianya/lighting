/**
 * @file modbus_reg_map.h
 * @brief Modbus 寄存器地址映射表（单一真值源）
 * @details 灯管控制 + 场景引擎 + 时间调度的寄存器布局
 * @author Lighting Ultra Team
 * @date 2026-04-20
 * @version 0x0100
 *
 * 配套文档：Core/Doc/ModbusRegisterMap.md
 */

#ifndef __MODBUS_REG_MAP_H
#define __MODBUS_REG_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==================== 寄存器表版本 ==================== */
#define MODBUS_REG_MAP_VERSION          0x0100u   /**< v1.0 */

/* ==================== 保持寄存器 (FC 03/06/10) ==================== */
/* ---- 模式与直接控制区 (0x0000~0x000F) ---- */
#define REG_HOLD_MODE                   0x0000u   /**< 工作模式：0=手动, 1=智能, 2=应急 */
#define REG_HOLD_LAMP_BITS_RW           0x0001u   /**< 手动模式下直写 bit0~4 控灯 */
#define REG_HOLD_SCENE_REQ              0x0002u   /**< 场景请求 1~5，写即切换 */
#define REG_HOLD_OVERRIDE_SEC           0x0003u   /**< 智能模式临时手动接管秒数（0=不允许） */

/* ---- 场景定义区 (0x0010~0x0014) ---- */
#define REG_HOLD_SCENE_MASK_BASE        0x0010u   /**< 场景 1~5 的 DO 位掩码 */
#define REG_HOLD_SCENE_MASK_COUNT       5u

/* ---- 时间与调度区 (0x0020~0x0050) ---- */
#define REG_HOLD_TIME_HHMM              0x0020u   /**< 软 RTC 时间 (HH*100+MM)，写=授时 */
#define REG_HOLD_TIME_DOW               0x0021u   /**< 星期 0~6（0=周日） */

#define REG_HOLD_SCHEDULE_BASE          0x0030u   /**< 调度表基址：每段 4 字 */
#define REG_HOLD_SCHEDULE_ENTRY_WORDS   4u        /**< 每段占用 4 个寄存器 */
#define REG_HOLD_SCHEDULE_COUNT         8u        /**< 最多 8 段 */
#define REG_HOLD_SCHEDULE_END           (REG_HOLD_SCHEDULE_BASE + \
                                         REG_HOLD_SCHEDULE_COUNT * \
                                         REG_HOLD_SCHEDULE_ENTRY_WORDS - 1u)
#define REG_HOLD_SCHEDULE_ENABLE        0x0050u   /**< 调度使能位掩码 bit0~7 */

/* 单段调度内部偏移 */
#define REG_SCHED_OFFSET_START_HHMM     0u
#define REG_SCHED_OFFSET_END_HHMM       1u
#define REG_SCHED_OFFSET_SCENE_ID       2u
#define REG_SCHED_OFFSET_DOW_MASK       3u        /**< bit0=周日, bit6=周六, bit7=忽略星期 */

/* ---- 通信配置区 (0x0060~0x006F) ---- */
#define REG_HOLD_SLAVE_ADDR             0x0060u   /**< 从站地址（写入持久化） */
#define REG_HOLD_BAUD_INDEX             0x0061u   /**< 波特率档位 */

/* ---- 命令寄存器 (0x00F0) ---- */
#define REG_HOLD_CMD                    0x00F0u   /**< 1=保存配置, 2=恢复默认, 3=场景自检 */
#define REG_CMD_SAVE                    1u
#define REG_CMD_RESTORE_DEFAULT         2u
#define REG_CMD_SELF_TEST               3u

/* 保持寄存器总数（必须 >= 所有上述地址 + 1） */
#define REG_HOLD_COUNT                  256u

/* ==================== 输入寄存器 (FC 04) ==================== */
/* ---- 心跳与传感器 (0x0000~0x000F) ---- */
#define REG_INPUT_HEARTBEAT             0x0000u   /**< 秒级心跳计数 */
#define REG_INPUT_PRESSURE_X1000        0x0001u   /**< 压力 ×1000 (0.001 MPa 单位) */
#define REG_INPUT_CURRENT_X100          0x0002u   /**< 电流 ×100 (0.01 mA 单位) */
#define REG_INPUT_ADC_RAW               0x0003u   /**< 压力 ADC 原始值 */
#define REG_INPUT_PRESSURE_VALID        0x0004u   /**< 压力数据有效标志 */
#define REG_INPUT_WATER_LEVEL           0x0005u   /**< 水位等级：0~3, 0xFF=错误 */

/* ---- 灯管与模式生效状态 (0x0006~0x000F) ---- */
#define REG_INPUT_ACTIVE_SCENE          0x0006u   /**< 当前生效场景（0=无，1~5） */
#define REG_INPUT_ACTIVE_MODE           0x0007u   /**< 当前生效模式（镜像 REG_HOLD_MODE） */
#define REG_INPUT_SCHEDULE_HIT          0x0008u   /**< 命中的调度段序号（0xFF=未命中） */
#define REG_INPUT_LAMP_BITS_ACTUAL      0x0009u   /**< DO 实际位图（从硬件回读） */
#define REG_INPUT_OVERRIDE_LEFT_SEC     0x000Au   /**< 临时手动覆盖剩余秒数 */
#define REG_INPUT_TIME_SYNCED           0x000Bu   /**< 软 RTC 是否已被主机授时 */

/* ---- 错误与诊断 (0x0010~0x001F) ---- */
#define REG_INPUT_ERR_LO                0x0010u
#define REG_INPUT_ERR_HI                0x0011u
#define REG_INPUT_RX_FRAME_CNT          0x0012u
#define REG_INPUT_CRC_ERR_CNT           0x0013u

/* ---- 版本信息 ---- */
#define REG_INPUT_MAP_VERSION           0x001Fu   /**< 寄存器表版本号 */

#define REG_INPUT_COUNT                 64u

/* ==================== 线圈 (FC 01/05/0F) ==================== */
#define COIL_LAMP_BASE                  0u        /**< DO1~DO5 直写 */
#define COIL_LAMP_COUNT                 5u
#define COIL_BEEP_ONCE                  5u        /**< 写 1 触发一次鸣叫 */
#define COIL_FACTORY_RESET              6u        /**< 写 1 恢复出厂 */
#define COIL_COUNT                      80u       /**< 预留总数（80 bits = 10 bytes） */

/* ==================== 离散输入 (FC 02) ==================== */
/* 按字节组织：byte0=压力状态, byte1=水位探针, byte2=水位编码, byte3=模式状态 */
#define DI_BYTE_PRESSURE                0u
#define DI_BYTE_WATER_PROBE             1u
#define DI_BYTE_WATER_CODE              2u
#define DI_BYTE_MODE_STATUS             3u

/* byte0: 压力状态位 */
#define DI_BIT_PRESSURE_VALID           ((DI_BYTE_PRESSURE * 8u) + 0u)
#define DI_BIT_PRESSURE_OVER_1P0        ((DI_BYTE_PRESSURE * 8u) + 1u)
#define DI_BIT_PRESSURE_OVER_1P4        ((DI_BYTE_PRESSURE * 8u) + 2u)

/* byte3: 模式生效状态 */
#define DI_BIT_SMART_ACTIVE             ((DI_BYTE_MODE_STATUS * 8u) + 0u)
#define DI_BIT_SCHEDULE_HIT             ((DI_BYTE_MODE_STATUS * 8u) + 1u)
#define DI_BIT_OVERRIDE_ACTIVE          ((DI_BYTE_MODE_STATUS * 8u) + 2u)
#define DI_BIT_EMERGENCY_ACTIVE         ((DI_BYTE_MODE_STATUS * 8u) + 3u)

#define DI_COUNT                        40u       /**< 5 bytes = 40 bits */

/* ==================== 场景定义 ==================== */
typedef enum {
    SCENE_NONE          = 0,   /**< 未定义/无效 */
    SCENE_ALL_ON        = 1,   /**< 全开 0b11111 */
    SCENE_HALF_ECO      = 2,   /**< 半开节能 0b10101 */
    SCENE_MAIN_ONLY     = 3,   /**< 仅主灯 0b00001 */
    SCENE_EMERGENCY     = 4,   /**< 应急仅备用 0b11000 */
    SCENE_ALL_OFF       = 5,   /**< 全关 0b00000 */
    SCENE_ID_MAX        = SCENE_ALL_OFF
} SceneId_e;

#define SCENE_MASK_ALL_ON               0x1Fu     /* bit0~4 */
#define SCENE_MASK_HALF_ECO             0x15u
#define SCENE_MASK_MAIN_ONLY            0x01u
#define SCENE_MASK_EMERGENCY            0x18u
#define SCENE_MASK_ALL_OFF              0x00u

/* ==================== 工作模式 ==================== */
#define LAMP_MODE_VAL_MANUAL            0u
#define LAMP_MODE_VAL_SMART             1u
#define LAMP_MODE_VAL_EMERGENCY         2u

/* ==================== 调度 DOW 掩码 ==================== */
#define DOW_BIT_SUN                     (1u << 0)
#define DOW_BIT_MON                     (1u << 1)
#define DOW_BIT_TUE                     (1u << 2)
#define DOW_BIT_WED                     (1u << 3)
#define DOW_BIT_THU                     (1u << 4)
#define DOW_BIT_FRI                     (1u << 5)
#define DOW_BIT_SAT                     (1u << 6)
#define DOW_BIT_IGNORE                  (1u << 7)   /**< 置位=忽略星期（每天生效） */
#define DOW_MASK_ALL_DAYS               (DOW_BIT_IGNORE)
#define DOW_MASK_WORKDAYS               (DOW_BIT_MON | DOW_BIT_TUE | DOW_BIT_WED | \
                                         DOW_BIT_THU | DOW_BIT_FRI)
#define DOW_MASK_WEEKEND                (DOW_BIT_SUN | DOW_BIT_SAT)

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_REG_MAP_H */
