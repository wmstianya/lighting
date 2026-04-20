/**
 * @file schedule.h
 * @brief 软 RTC + 时间段调度器（用于智能模式）
 * @details
 *   - 软 RTC：基于 HAL_GetTick()，分钟精度推进；主机通过 Modbus 授时
 *   - 8 段调度表：每段指定 [startHHMM, endHHMM, sceneId, dowMask]
 *   - 每秒 tick：命中使能段时调用 lampMgrRequestScene(sceneId, LAMP_SRC_SCHEDULE)
 *
 * @author Lighting Ultra Team
 * @date 2026-04-20
 */

#ifndef __SCHEDULE_H
#define __SCHEDULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "modbus_reg_map.h"

/* ==================== 类型定义 ==================== */

/**
 * @brief 单段调度条目
 */
typedef struct {
    uint16_t startHHMM;   /**< 起始时间：HH*100 + MM，例如 1800 = 18:00 */
    uint16_t endHHMM;     /**< 结束时间；若 end < start 表示跨零点 */
    uint8_t  sceneId;     /**< 目标场景 ID (1~5) */
    uint8_t  dowMask;     /**< 星期位掩码，bit7=忽略星期 */
} ScheduleEntry_t;

/**
 * @brief 调度器运行时状态（供输入寄存器镜像）
 */
typedef struct {
    uint16_t curHHMM;       /**< 当前时间 */
    uint8_t  curDow;        /**< 当前星期 */
    uint8_t  activeEntry;   /**< 命中段序号（0xFF=未命中） */
    bool     timeSynced;    /**< 是否已被主机授时 */
} ScheduleStatus_t;

/* ==================== API ==================== */

/**
 * @brief 初始化调度器（默认关闭所有段，时间未同步）
 */
void scheduleInit(void);

/**
 * @brief 主循环周期处理
 * @note  内部自管 1s / 1min 节拍，主循环随意调用频率不敏感
 *        - 每秒推进"应否切换"检查
 *        - 每分钟推进软 RTC
 */
void scheduleProcess(void);

/**
 * @brief 主机授时（写入 REG_HOLD_TIME_HHMM 时调用）
 * @param hhmm  HH*100 + MM
 * @param dow   星期 0~6
 */
void scheduleSyncTime(uint16_t hhmm, uint8_t dow);

/**
 * @brief 配置一段调度条目
 * @param index  0 ~ REG_HOLD_SCHEDULE_COUNT-1
 * @param entry  条目内容
 */
void scheduleSetEntry(uint8_t index, const ScheduleEntry_t *entry);

/**
 * @brief 读取某段条目
 * @param index  0 ~ REG_HOLD_SCHEDULE_COUNT-1
 * @param out    输出指针
 * @return true=成功
 */
bool scheduleGetEntry(uint8_t index, ScheduleEntry_t *out);

/**
 * @brief 设置段使能位掩码（bit0~7 对应段 0~7）
 */
void scheduleSetEnableMask(uint8_t mask);

/**
 * @brief 获取段使能位掩码
 */
uint8_t scheduleGetEnableMask(void);

/**
 * @brief 获取当前软 RTC 时间
 */
uint16_t scheduleGetCurHHMM(void);

/**
 * @brief 获取当前星期
 */
uint8_t scheduleGetCurDow(void);

/**
 * @brief 时间是否已被主机授时
 */
bool scheduleIsTimeSynced(void);

/**
 * @brief 获取完整状态快照
 */
void scheduleGetStatus(ScheduleStatus_t *outStatus);

#ifdef __cplusplus
}
#endif

#endif /* __SCHEDULE_H */
