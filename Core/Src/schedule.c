/**
 * @file schedule.c
 * @brief 软 RTC + 时间段调度器实现
 * @author Lighting Ultra Team
 * @date 2026-04-20
 *
 * 时序：
 *   - s_secondTickLast：最近一次 1s 处理的 HAL_GetTick 时刻
 *   - s_minuteTickLast：最近一次 1min 推进的 HAL_GetTick 时刻
 *   - 每 1000ms 做一次"当前时刻应落在哪个段"检查
 *   - 每 60_000ms curHHMM +1 分钟，processMinuteRollover 处理进位
 */

#include "schedule.h"
#include "lamp_manager.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ==================== 内部状态 ==================== */

static ScheduleEntry_t s_entries[REG_HOLD_SCHEDULE_COUNT];
static uint8_t         s_enableMask     = 0;
static uint16_t        s_curHHMM        = 0;
static uint8_t         s_curDow         = 0;
static uint8_t         s_activeEntry    = 0xFF;
static bool            s_timeSynced     = false;
static uint32_t        s_secondTickLast = 0;
static uint32_t        s_minuteTickLast = 0;

/* ==================== 内部辅助 ==================== */

/**
 * @brief 判断给定 HHMM 是否落在 [startHHMM, endHHMM] 区间内
 *        支持跨零点：endHHMM < startHHMM 时视为 [start,2359] U [0,end]
 */
static bool isTimeInRange(uint16_t hhmm, uint16_t startHHMM, uint16_t endHHMM)
{
    if (startHHMM <= endHHMM) {
        return (hhmm >= startHHMM && hhmm <= endHHMM);
    }
    /* 跨零点 */
    return (hhmm >= startHHMM) || (hhmm <= endHHMM);
}

/**
 * @brief 判断给定星期是否被 dowMask 选中
 */
static bool isDowMatch(uint8_t dow, uint8_t dowMask)
{
    if (dowMask & DOW_BIT_IGNORE) {
        return true;  /* 忽略星期 = 每天生效 */
    }
    if (dow > 6) {
        return false;
    }
    return (dowMask & (1u << dow)) != 0;
}

/**
 * @brief 扫描使能段，返回第一个命中的段序号，未命中返回 0xFF
 * @note  段索引从低到高扫描，前面的段优先级高
 */
static uint8_t findHitEntry(void)
{
    for (uint8_t i = 0; i < REG_HOLD_SCHEDULE_COUNT; i++) {
        if ((s_enableMask & (1u << i)) == 0) {
            continue;
        }
        const ScheduleEntry_t *e = &s_entries[i];
        if (e->sceneId == SCENE_NONE || e->sceneId > SCENE_ID_MAX) {
            continue;
        }
        if (!isDowMatch(s_curDow, e->dowMask)) {
            continue;
        }
        if (isTimeInRange(s_curHHMM, e->startHHMM, e->endHHMM)) {
            return i;
        }
    }
    return 0xFF;
}

/**
 * @brief 软 RTC 分钟进位处理
 */
static void advanceOneMinute(void)
{
    uint16_t hh = s_curHHMM / 100u;
    uint16_t mm = s_curHHMM % 100u;

    mm++;
    if (mm >= 60u) {
        mm = 0;
        hh++;
        if (hh >= 24u) {
            hh = 0;
            s_curDow = (uint8_t)((s_curDow + 1u) % 7u);
        }
    }
    s_curHHMM = (uint16_t)(hh * 100u + mm);
}

/**
 * @brief 执行每秒一次的调度命中检查
 */
static void runSecondTick(void)
{
    if (!s_timeSynced) {
        /* 未授时前不做任何场景切换，避免 00:00 默认值触发误动作 */
        s_activeEntry = 0xFF;
        return;
    }

    uint8_t hit = findHitEntry();
    if (hit != s_activeEntry) {
        s_activeEntry = hit;
        if (hit != 0xFF) {
            (void)lampMgrRequestScene(s_entries[hit].sceneId, LAMP_SRC_SCHEDULE);
        }
        /* 未命中时保持上一场景不变，避免频繁切换 */
    }
}

/* ==================== API 实现 ==================== */

void scheduleInit(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    s_enableMask     = 0;
    s_curHHMM        = 0;
    s_curDow         = 0;
    s_activeEntry    = 0xFF;
    s_timeSynced     = false;
    s_secondTickLast = HAL_GetTick();
    s_minuteTickLast = s_secondTickLast;
}

void scheduleProcess(void)
{
    uint32_t now = HAL_GetTick();

    /* 分钟推进 */
    if ((uint32_t)(now - s_minuteTickLast) >= 60000u) {
        s_minuteTickLast += 60000u;
        if (s_timeSynced) {
            advanceOneMinute();
        }
    }

    /* 每秒检查命中 */
    if ((uint32_t)(now - s_secondTickLast) >= 1000u) {
        s_secondTickLast += 1000u;
        runSecondTick();
    }
}

void scheduleSyncTime(uint16_t hhmm, uint8_t dow)
{
    uint16_t hh = hhmm / 100u;
    uint16_t mm = hhmm % 100u;
    if (hh >= 24u || mm >= 60u || dow > 6u) {
        return;   /* 非法时间忽略 */
    }
    s_curHHMM        = hhmm;
    s_curDow         = dow;
    s_timeSynced     = true;
    s_minuteTickLast = HAL_GetTick();  /* 重置分钟计时起点，避免立刻 +1min */
    s_activeEntry    = 0xFF;           /* 强制下次 tick 重新判命中 */
}

void scheduleSetEntry(uint8_t index, const ScheduleEntry_t *entry)
{
    if (index >= REG_HOLD_SCHEDULE_COUNT || !entry) {
        return;
    }
    s_entries[index] = *entry;
    s_activeEntry = 0xFF;  /* 配置变更，强制重判 */
}

bool scheduleGetEntry(uint8_t index, ScheduleEntry_t *out)
{
    if (index >= REG_HOLD_SCHEDULE_COUNT || !out) {
        return false;
    }
    *out = s_entries[index];
    return true;
}

void scheduleSetEnableMask(uint8_t mask)
{
    s_enableMask = mask;
    s_activeEntry = 0xFF;
}

uint8_t scheduleGetEnableMask(void)
{
    return s_enableMask;
}

uint16_t scheduleGetCurHHMM(void)
{
    return s_curHHMM;
}

uint8_t scheduleGetCurDow(void)
{
    return s_curDow;
}

bool scheduleIsTimeSynced(void)
{
    return s_timeSynced;
}

void scheduleGetStatus(ScheduleStatus_t *outStatus)
{
    if (!outStatus) {
        return;
    }
    outStatus->curHHMM     = s_curHHMM;
    outStatus->curDow      = s_curDow;
    outStatus->activeEntry = s_activeEntry;
    outStatus->timeSynced  = s_timeSynced;
}
