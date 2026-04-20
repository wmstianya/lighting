/**
 * @file lamp_manager.h
 * @brief 灯管场景引擎 + 模式仲裁器
 * @details
 *   - 统一灯管控制入口：所有 DO 改变都必须经过 lampMgrRequestXxx()
 *   - 模式仲裁：手动 / 智能 / 应急
 *   - 场景引擎：5 个可配置场景（bit mask → DO1~DO5）
 *   - 临时覆盖：智能模式下允许主机短时手动接管 N 秒
 *
 * @author Lighting Ultra Team
 * @date 2026-04-20
 */

#ifndef __LAMP_MANAGER_H
#define __LAMP_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "modbus_reg_map.h"

/* ==================== 类型定义 ==================== */

/**
 * @brief 灯管工作模式
 */
typedef enum {
    LAMP_MODE_MANUAL    = LAMP_MODE_VAL_MANUAL,     /**< 主机直控 */
    LAMP_MODE_SMART     = LAMP_MODE_VAL_SMART,      /**< 时间调度自动 */
    LAMP_MODE_EMERGENCY = LAMP_MODE_VAL_EMERGENCY   /**< 强制全开 */
} LampMode_e;

/**
 * @brief 请求来源（用于模式仲裁）
 */
typedef enum {
    LAMP_SRC_MODBUS   = 0,   /**< 主机通过 Modbus 下发 */
    LAMP_SRC_SCHEDULE = 1,   /**< 本地调度器触发 */
    LAMP_SRC_LOCAL    = 2    /**< 本地逻辑（应急、自检等） */
} LampSource_e;

/**
 * @brief 仲裁运行时状态（供 modbus_app 填输入寄存器）
 */
typedef struct {
    LampMode_e activeMode;       /**< 当前生效模式 */
    uint8_t    activeScene;      /**< 当前场景（0=无，1~5） */
    uint8_t    actualBits;       /**< 最近一次 commit 到硬件的 DO 位图 */
    uint16_t   overrideLeftMs;   /**< 临时覆盖剩余毫秒数 */
    uint16_t   rejectCount;      /**< 被仲裁拒绝的请求总数（诊断用） */
    bool       overrideActive;   /**< 临时覆盖进行中 */
} LampMgrStatus_t;

/* ==================== API ==================== */

/**
 * @brief 初始化场景引擎（装载默认场景掩码 + 默认模式=手动）
 * @note  必须在 relayInit() 之后、主循环之前调用
 */
void lampMgrInit(void);

/**
 * @brief 主循环周期处理：覆盖倒计时、应急状态维持
 * @note  建议每 1ms 调用一次（从 main.c 主循环）
 */
void lampMgrProcess(void);

/**
 * @brief 设置工作模式
 * @param mode 目标模式
 * @note  应急模式会立即把硬件 DO 置为 SCENE_EMERGENCY
 */
void lampMgrSetMode(LampMode_e mode);

/**
 * @brief 获取当前模式
 */
LampMode_e lampMgrGetMode(void);

/**
 * @brief 请求切换到指定场景
 * @param sceneId 场景 ID（1~SCENE_ID_MAX）
 * @param src     请求来源（决定是否通过仲裁）
 * @return true=接受并已切换，false=被仲裁拒绝
 */
bool lampMgrRequestScene(uint8_t sceneId, LampSource_e src);

/**
 * @brief 请求按位掩码控制灯管
 * @param mask bit0~bit4 = DO1~DO5
 * @param src  请求来源
 * @return true=接受，false=拒绝
 * @note  接受后 activeScene 被置为 SCENE_NONE（自定义位图状态）
 */
bool lampMgrRequestLampBits(uint8_t mask, LampSource_e src);

/**
 * @brief 重定义某个场景的 DO 位掩码（配置接口）
 * @param sceneId 1~SCENE_ID_MAX
 * @param mask    bit0~4
 */
void lampMgrSetSceneMask(uint8_t sceneId, uint8_t mask);

/**
 * @brief 读取场景掩码（用于输入寄存器镜像）
 */
uint8_t lampMgrGetSceneMask(uint8_t sceneId);

/**
 * @brief 当前生效场景
 */
uint8_t lampMgrGetActiveScene(void);

/**
 * @brief 从 relay 驱动回读实际 DO 位图
 */
uint8_t lampMgrGetActualBits(void);

/**
 * @brief 设置临时覆盖时长（秒）
 * @note  仅在智能模式下有意义；手动模式忽略
 */
void lampMgrSetOverrideSeconds(uint16_t seconds);

/**
 * @brief 获取完整状态快照（诊断用）
 */
void lampMgrGetStatus(LampMgrStatus_t *outStatus);

#ifdef __cplusplus
}
#endif

#endif /* __LAMP_MANAGER_H */
