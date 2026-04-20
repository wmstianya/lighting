/**
 * @file lamp_manager.c
 * @brief 灯管场景引擎 + 模式仲裁器实现
 * @author Lighting Ultra Team
 * @date 2026-04-20
 */

#include "lamp_manager.h"
#include "relay.h"
#include "led.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ==================== 内部状态 ==================== */

static LampMode_e s_mode             = LAMP_MODE_MANUAL;
static uint8_t    s_activeScene      = SCENE_NONE;
static uint8_t    s_lastCommittedBits = 0x00;
static uint8_t    s_sceneMasks[REG_HOLD_SCENE_MASK_COUNT] = {
    SCENE_MASK_ALL_ON,
    SCENE_MASK_HALF_ECO,
    SCENE_MASK_MAIN_ONLY,
    SCENE_MASK_EMERGENCY,
    SCENE_MASK_ALL_OFF
};

static uint32_t s_overrideDeadlineMs = 0;
static bool     s_overrideActive     = false;
static uint16_t s_rejectCount        = 0;

/* ==================== 内部辅助 ==================== */

/**
 * @brief 把位掩码应用到硬件 (DO + LED 指示同步)
 */
static void commitBitsToHardware(uint8_t mask)
{
    uint8_t trimmed = (uint8_t)(mask & 0x1F);
    relaySetAllStates(trimmed);
    s_lastCommittedBits = trimmed;

    /* LED1~LED4 镜像 DO1~DO4（低电平点亮由 led.c 内部处理） */
    for (uint8_t i = 0; i < LED_CHANNEL_COUNT && i < COIL_LAMP_COUNT; i++) {
        LedState_e st = (trimmed & (1u << i)) ? LED_STATE_ON : LED_STATE_OFF;
        ledSetState((LedChannel_e)i, st);
    }
}

/**
 * @brief 判断给定场景 ID 是否合法（1~SCENE_ID_MAX）
 */
static bool isValidSceneId(uint8_t sceneId)
{
    return (sceneId >= SCENE_ALL_ON && sceneId <= SCENE_ID_MAX);
}

/**
 * @brief 根据 sceneId 查位掩码
 */
static uint8_t sceneIdToMask(uint8_t sceneId)
{
    if (!isValidSceneId(sceneId)) {
        return 0x00;
    }
    return s_sceneMasks[sceneId - 1];
}

/**
 * @brief 仲裁核心：决定某个来源在当前模式下是否允许修改 DO
 */
static bool arbitrateAccept(LampSource_e src)
{
    if (s_mode == LAMP_MODE_EMERGENCY) {
        /* 应急模式下只允许 LOCAL 退出（改模式），拒绝所有其他请求 */
        return (src == LAMP_SRC_LOCAL);
    }

    if (s_mode == LAMP_MODE_MANUAL) {
        /* 手动模式下拒绝 SCHEDULE */
        return (src != LAMP_SRC_SCHEDULE);
    }

    /* LAMP_MODE_SMART */
    if (src == LAMP_SRC_SCHEDULE || src == LAMP_SRC_LOCAL) {
        return true;
    }
    /* MODBUS 在智能模式下：仅当临时覆盖启用且未到期时接受 */
    if (src == LAMP_SRC_MODBUS && s_overrideActive) {
        return true;
    }
    return false;
}

/* ==================== API 实现 ==================== */

void lampMgrInit(void)
{
    s_mode              = LAMP_MODE_MANUAL;
    s_activeScene       = SCENE_NONE;
    s_overrideDeadlineMs = 0;
    s_overrideActive    = false;
    s_rejectCount       = 0;

    /* 恢复默认场景掩码 */
    s_sceneMasks[0] = SCENE_MASK_ALL_ON;
    s_sceneMasks[1] = SCENE_MASK_HALF_ECO;
    s_sceneMasks[2] = SCENE_MASK_MAIN_ONLY;
    s_sceneMasks[3] = SCENE_MASK_EMERGENCY;
    s_sceneMasks[4] = SCENE_MASK_ALL_OFF;

    /* 上电默认：全关，可见状态安全 */
    commitBitsToHardware(0x00);
}

void lampMgrProcess(void)
{
    /* 临时覆盖倒计时检测 */
    if (s_overrideActive) {
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - s_overrideDeadlineMs) >= 0) {
            s_overrideActive = false;
            /* 覆盖结束，若仍在智能模式，场景由下次 schedule tick 决定，此处不动 */
        }
    }

    /* 应急模式持续维持全开（防御性：即使硬件位被外力改动也会拉回） */
    if (s_mode == LAMP_MODE_EMERGENCY) {
        if (relayGetAllStates() != SCENE_MASK_ALL_ON) {
            commitBitsToHardware(SCENE_MASK_ALL_ON);
        }
    }
}

void lampMgrSetMode(LampMode_e mode)
{
    if (mode == s_mode) {
        return;
    }
    s_mode = mode;

    switch (mode) {
        case LAMP_MODE_EMERGENCY:
            /* 切到应急：立即全开 */
            s_activeScene = SCENE_EMERGENCY;
            commitBitsToHardware(SCENE_MASK_ALL_ON);
            break;

        case LAMP_MODE_SMART:
            /* 切到智能：保持当前输出，等调度器命中时再切 */
            s_overrideActive = false;
            break;

        case LAMP_MODE_MANUAL:
        default:
            /* 切到手动：保持当前输出，等主机下一步操作 */
            s_overrideActive = false;
            break;
    }
}

LampMode_e lampMgrGetMode(void)
{
    return s_mode;
}

bool lampMgrRequestScene(uint8_t sceneId, LampSource_e src)
{
    if (!arbitrateAccept(src)) {
        s_rejectCount++;
        return false;
    }
    if (!isValidSceneId(sceneId)) {
        s_rejectCount++;
        return false;
    }

    s_activeScene = sceneId;
    commitBitsToHardware(sceneIdToMask(sceneId));
    return true;
}

bool lampMgrRequestLampBits(uint8_t mask, LampSource_e src)
{
    if (!arbitrateAccept(src)) {
        s_rejectCount++;
        return false;
    }

    s_activeScene = SCENE_NONE;  /* 自定义位图，不属于任何预设场景 */
    commitBitsToHardware(mask);
    return true;
}

void lampMgrSetSceneMask(uint8_t sceneId, uint8_t mask)
{
    if (!isValidSceneId(sceneId)) {
        return;
    }
    s_sceneMasks[sceneId - 1] = (uint8_t)(mask & 0x1F);

    /* 如果正在显示这个场景，立即刷新硬件 */
    if (s_activeScene == sceneId && s_mode != LAMP_MODE_EMERGENCY) {
        commitBitsToHardware(s_sceneMasks[sceneId - 1]);
    }
}

uint8_t lampMgrGetSceneMask(uint8_t sceneId)
{
    if (!isValidSceneId(sceneId)) {
        return 0x00;
    }
    return s_sceneMasks[sceneId - 1];
}

uint8_t lampMgrGetActiveScene(void)
{
    return s_activeScene;
}

uint8_t lampMgrGetActualBits(void)
{
    return relayGetAllStates();
}

void lampMgrSetOverrideSeconds(uint16_t seconds)
{
    if (s_mode != LAMP_MODE_SMART) {
        return;
    }
    if (seconds == 0) {
        s_overrideActive = false;
        return;
    }
    s_overrideDeadlineMs = HAL_GetTick() + (uint32_t)seconds * 1000u;
    s_overrideActive = true;
}

void lampMgrGetStatus(LampMgrStatus_t *outStatus)
{
    if (!outStatus) {
        return;
    }
    outStatus->activeMode   = s_mode;
    outStatus->activeScene  = s_activeScene;
    outStatus->actualBits   = s_lastCommittedBits;
    outStatus->rejectCount  = s_rejectCount;
    outStatus->overrideActive = s_overrideActive;

    if (s_overrideActive) {
        uint32_t now = HAL_GetTick();
        int32_t remain = (int32_t)(s_overrideDeadlineMs - now);
        outStatus->overrideLeftMs = (remain > 0) ? (uint16_t)(remain > 0xFFFF ? 0xFFFF : remain) : 0;
    } else {
        outStatus->overrideLeftMs = 0;
    }
}
