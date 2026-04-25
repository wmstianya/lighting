/**
 * @file modbus_app.c
 * @brief Modbus RTU 应用层（路由到 lamp_manager + schedule）
 * @details
 *   - 双 UART Modbus 实例管理（UART1/UART2）
 *   - 线圈/寄存器回调路由到 lamp_manager（模式仲裁）和 schedule（调度器）
 *   - 输入寄存器和离散输入每秒更新一次传感器与运行时状态
 *
 * @author Lighting Ultra Team
 * @date 2025-11-08
 * @modified 2026-04-20 重构：接入场景引擎 + 时间调度
 */

#include "modbus_app.h"
#include "modbus_port.h"
#include "modbus_reg_map.h"
#include "lamp_manager.h"
#include "schedule.h"
#include "main.h"
#include "relay.h"
#include "led.h"
#include "beep.h"
#include "pressure_sensor.h"
#include "water_level.h"
#include "config_manager.h"
#include "error_handler.h"

/* ==================== Modbus 实例 ==================== */
ModbusRTU_t modbusUart1;
ModbusRTU_t modbusUart2;

/* ==================== 数据存储 ==================== */
/* 保持寄存器大小必须 >= REG_HOLD_COUNT（256），输入寄存器 >= REG_INPUT_COUNT（64） */
static uint16_t uart1_holdingRegs[REG_HOLD_COUNT];
static uint16_t uart1_inputRegs[REG_INPUT_COUNT];
static uint8_t  uart1_coils[(COIL_COUNT + 7) / 8];
static uint8_t  uart1_discreteInputs[(DI_COUNT + 7) / 8];

static uint16_t uart2_holdingRegs[REG_HOLD_COUNT];
static uint16_t uart2_inputRegs[REG_INPUT_COUNT];
static uint8_t  uart2_coils[(COIL_COUNT + 7) / 8];
static uint8_t  uart2_discreteInputs[(DI_COUNT + 7) / 8];

/* ==================== 内部辅助 ==================== */

/**
 * @brief 读 UART1 线圈虚拟寄存器中 COIL_LAMP 区的 5 位，组成位掩码
 */
static uint8_t coilsToLampMask(const uint8_t *coils)
{
    return (uint8_t)(coils[0] & 0x1F);
}

/**
 * @brief 用实际硬件 DO 位图反向刷新两个 UART 的虚拟线圈 0~4
 */
static void syncCoilsFromHardware(uint8_t actualBits)
{
    uint8_t masked = (uint8_t)(actualBits & 0x1F);
    uart1_coils[0] = (uint8_t)((uart1_coils[0] & ~0x1F) | masked);
#if ENABLE_DUAL_UART_SYNC
    uart2_coils[0] = (uint8_t)((uart2_coils[0] & ~0x1F) | masked);
#endif
}

/**
 * @brief 把 schedule 配置整段回读后写入调度器
 */
static void commitScheduleEntryFromRegs(const uint16_t *holdingRegs, uint8_t entryIdx)
{
    if (entryIdx >= REG_HOLD_SCHEDULE_COUNT) {
        return;
    }
    uint16_t base = REG_HOLD_SCHEDULE_BASE +
                    (uint16_t)entryIdx * REG_HOLD_SCHEDULE_ENTRY_WORDS;

    ScheduleEntry_t e;
    e.startHHMM = holdingRegs[base + REG_SCHED_OFFSET_START_HHMM];
    e.endHHMM   = holdingRegs[base + REG_SCHED_OFFSET_END_HHMM];
    e.sceneId   = (uint8_t)(holdingRegs[base + REG_SCHED_OFFSET_SCENE_ID] & 0xFF);
    e.dowMask   = (uint8_t)(holdingRegs[base + REG_SCHED_OFFSET_DOW_MASK] & 0xFF);
    scheduleSetEntry(entryIdx, &e);
}

/**
 * @brief 处理命令寄存器写入
 */
static void handleCmdWrite(uint16_t cmd)
{
    switch (cmd) {
        case REG_CMD_SAVE:
            (void)configSave();
            break;
        case REG_CMD_RESTORE_DEFAULT:
            (void)configResetToDefault();
            break;
        case REG_CMD_SELF_TEST:
            /* 依次短亮 5 个场景（阻塞时长由主机容忍度决定，这里走非阻塞路径） */
            /* 简化版：仅触发一次蜂鸣作为自检确认 */
            beepSetTime(500);
            break;
        default:
            break;
    }
}

/* ==================== Coil 回调（UART1 / UART2 通用） ==================== */

/**
 * @brief 线圈变化统一处理：根据地址分发
 */
static void dispatchCoilWrite(uint16_t addr, uint8_t value, uint8_t *coilsArr)
{
    /* COIL_LAMP 0~4：拉出完整掩码走 lamp_manager 仲裁 */
    if (addr < COIL_LAMP_COUNT) {
        uint8_t mask = coilsToLampMask(coilsArr);
        bool accepted = lampMgrRequestLampBits(mask, LAMP_SRC_MODBUS);
        if (!accepted) {
            /* 仲裁拒绝：把虚拟线圈回退到真实硬件状态 */
            syncCoilsFromHardware(lampMgrGetActualBits());
        } else {
            /* 接受后用实际 committed 位图回填（避免无效位残留） */
            syncCoilsFromHardware(lampMgrGetActualBits());
        }
        return;
    }

    if (addr == COIL_BEEP_ONCE && value) {
        beepSetTime(200);
        return;
    }

    if (addr == COIL_FACTORY_RESET && value) {
        (void)configResetToDefault();
        return;
    }
}

static void uart1_onCoilChanged(uint16_t addr, uint8_t value)
{
    dispatchCoilWrite(addr, value, uart1_coils);
}

static void uart2_onCoilChanged(uint16_t addr, uint8_t value)
{
    dispatchCoilWrite(addr, value, uart2_coils);
}

/* ==================== Holding 寄存器回调 ==================== */

static void dispatchRegWrite(uint16_t addr, uint16_t value, uint16_t *holdingRegs)
{
    /* 模式切换 */
    if (addr == REG_HOLD_MODE) {
        if (value <= LAMP_MODE_VAL_EMERGENCY) {
            lampMgrSetMode((LampMode_e)value);
            configSetLampMode((uint8_t)value);
        }
        return;
    }

    /* 位掩码直写 */
    if (addr == REG_HOLD_LAMP_BITS_RW) {
        (void)lampMgrRequestLampBits((uint8_t)(value & 0x1F), LAMP_SRC_MODBUS);
        /* 无论是否接受，virtual coils 由 ModbusApp_UpdateSensorData 下一轮同步 */
        return;
    }

    /* 场景请求 */
    if (addr == REG_HOLD_SCENE_REQ) {
        (void)lampMgrRequestScene((uint8_t)(value & 0xFF), LAMP_SRC_MODBUS);
        return;
    }

    /* 临时覆盖秒数 */
    if (addr == REG_HOLD_OVERRIDE_SEC) {
        lampMgrSetOverrideSeconds(value);
        return;
    }

    /* 场景掩码配置 */
    if (addr >= REG_HOLD_SCENE_MASK_BASE &&
        addr <  REG_HOLD_SCENE_MASK_BASE + REG_HOLD_SCENE_MASK_COUNT) {
        uint8_t sceneId = (uint8_t)(addr - REG_HOLD_SCENE_MASK_BASE + 1);
        lampMgrSetSceneMask(sceneId, (uint8_t)(value & 0x1F));
        configSetSceneMask(sceneId, (uint8_t)(value & 0x1F));
        return;
    }

    /* 软 RTC 授时：HHMM 单独处理，DOW 与 HHMM 任一写入都触发一次同步
     * HMI 以小端（低字节在前）发送时间值，做字节翻转还原正确 HHMM：
     *   例：HMI 发 8D 05 → 固件收到 0x8D05 → 翻转 → 0x058D = 1421 = 14:21 */
    if (addr == REG_HOLD_TIME_HHMM) {
        uint16_t hhmm = (uint16_t)(((value & 0xFF) << 8) | ((value >> 8) & 0xFF));
        uint8_t  dow  = (uint8_t)(holdingRegs[REG_HOLD_TIME_DOW] & 0xFF);
        scheduleSyncTime(hhmm, dow);
        return;
    }
    if (addr == REG_HOLD_TIME_DOW) {
        uint16_t rawHhmm = holdingRegs[REG_HOLD_TIME_HHMM];
        uint16_t hhmm    = (uint16_t)(((rawHhmm & 0xFF) << 8) | ((rawHhmm >> 8) & 0xFF));
        scheduleSyncTime(hhmm, (uint8_t)(value & 0xFF));
        return;
    }

    /* 调度表单字写入 → 整段重新提交 */
    if (addr >= REG_HOLD_SCHEDULE_BASE && addr <= REG_HOLD_SCHEDULE_END) {
        uint16_t offset = addr - REG_HOLD_SCHEDULE_BASE;
        uint8_t  entryIdx = (uint8_t)(offset / REG_HOLD_SCHEDULE_ENTRY_WORDS);
        commitScheduleEntryFromRegs(holdingRegs, entryIdx);

        uint16_t base = REG_HOLD_SCHEDULE_BASE +
                        (uint16_t)entryIdx * REG_HOLD_SCHEDULE_ENTRY_WORDS;
        configSetScheduleEntry(entryIdx,
            holdingRegs[base + REG_SCHED_OFFSET_START_HHMM],
            holdingRegs[base + REG_SCHED_OFFSET_END_HHMM],
            (uint8_t)(holdingRegs[base + REG_SCHED_OFFSET_SCENE_ID] & 0xFF),
            (uint8_t)(holdingRegs[base + REG_SCHED_OFFSET_DOW_MASK] & 0xFF));
        return;
    }

    /* 调度使能掩码 */
    if (addr == REG_HOLD_SCHEDULE_ENABLE) {
        scheduleSetEnableMask((uint8_t)(value & 0xFF));
        configSetScheduleEnable((uint8_t)(value & 0xFF));
        return;
    }

    /* 命令寄存器 */
    if (addr == REG_HOLD_CMD) {
        handleCmdWrite(value);
        /* 命令是"一次性触发"，读回为 0 */
        holdingRegs[REG_HOLD_CMD] = 0;
        return;
    }
}

static void uart1_onRegChanged(uint16_t addr, uint16_t value)
{
    dispatchRegWrite(addr, value, uart1_holdingRegs);
#if ENABLE_DUAL_UART_SYNC
    /* 受控配置区同步到 UART2（场景掩码 / 调度表 / 使能 / 模式） */
    if (addr <= REG_HOLD_SCHEDULE_ENABLE) {
        uart2_holdingRegs[addr] = uart1_holdingRegs[addr];
    }
#endif
}

static void uart2_onRegChanged(uint16_t addr, uint16_t value)
{
    dispatchRegWrite(addr, value, uart2_holdingRegs);
#if ENABLE_DUAL_UART_SYNC
    if (addr <= REG_HOLD_SCHEDULE_ENABLE) {
        uart1_holdingRegs[addr] = uart2_holdingRegs[addr];
    }
#endif
}

/* ==================== 静态寄存器布局初始化 ==================== */

/**
 * @brief 把当前 lamp_manager / schedule 运行时状态写回保持寄存器区
 *        使主机读回的配置值与内部状态一致
 */
static void seedConfigMirror(uint16_t *holdingRegs)
{
    holdingRegs[REG_HOLD_MODE]         = (uint16_t)lampMgrGetMode();
    holdingRegs[REG_HOLD_LAMP_BITS_RW] = lampMgrGetActualBits();
    holdingRegs[REG_HOLD_SCENE_REQ]    = lampMgrGetActiveScene();
    holdingRegs[REG_HOLD_OVERRIDE_SEC] = 0;

    for (uint8_t i = 0; i < REG_HOLD_SCENE_MASK_COUNT; i++) {
        holdingRegs[REG_HOLD_SCENE_MASK_BASE + i] = lampMgrGetSceneMask(i + 1);
    }

    holdingRegs[REG_HOLD_TIME_HHMM]        = scheduleGetCurHHMM();
    holdingRegs[REG_HOLD_TIME_DOW]         = scheduleGetCurDow();
    holdingRegs[REG_HOLD_SCHEDULE_ENABLE]  = scheduleGetEnableMask();

    for (uint8_t i = 0; i < REG_HOLD_SCHEDULE_COUNT; i++) {
        ScheduleEntry_t e;
        if (scheduleGetEntry(i, &e)) {
            uint16_t base = REG_HOLD_SCHEDULE_BASE +
                            (uint16_t)i * REG_HOLD_SCHEDULE_ENTRY_WORDS;
            holdingRegs[base + REG_SCHED_OFFSET_START_HHMM] = e.startHHMM;
            holdingRegs[base + REG_SCHED_OFFSET_END_HHMM]   = e.endHHMM;
            holdingRegs[base + REG_SCHED_OFFSET_SCENE_ID]   = e.sceneId;
            holdingRegs[base + REG_SCHED_OFFSET_DOW_MASK]   = e.dowMask;
        }
    }

    holdingRegs[REG_HOLD_SLAVE_ADDR] = 0;  /* 由 configGet() 填充下面 */
    holdingRegs[REG_HOLD_BAUD_INDEX] = 0;
    holdingRegs[REG_HOLD_CMD]        = 0;
}

/* ==================== 初始化 ==================== */

void ModbusApp_Init(void)
{
    const SystemConfig_t *config = configGet();

    /* lamp_manager 和 schedule 必须先 init（由 main.c 在此之前调用） */

    /* ========== UART1 ========== */
    ModbusRTU_SetSlaveAddr(&modbusUart1, config->modbus1SlaveAddr);
    ModbusRTU_SetHoldingRegs(&modbusUart1, uart1_holdingRegs, REG_HOLD_COUNT);
    ModbusRTU_SetInputRegs(&modbusUart1, uart1_inputRegs, REG_INPUT_COUNT);
    ModbusRTU_SetCoils(&modbusUart1, uart1_coils, COIL_COUNT);
    ModbusRTU_SetDiscreteInputs(&modbusUart1, uart1_discreteInputs, DI_COUNT);
    modbusUart1.onCoilChanged = uart1_onCoilChanged;
    modbusUart1.onRegChanged  = uart1_onRegChanged;

    seedConfigMirror(uart1_holdingRegs);
    uart1_holdingRegs[REG_HOLD_SLAVE_ADDR] = config->modbus1SlaveAddr;
    uart1_inputRegs[REG_INPUT_MAP_VERSION] = MODBUS_REG_MAP_VERSION;
    uart1_coils[0] = lampMgrGetActualBits() & 0x1F;

    ModbusPort_UART1_Init(&modbusUart1);
    ModbusRTU_Init(&modbusUart1);

    /* ========== UART2 ========== */
    ModbusRTU_SetSlaveAddr(&modbusUart2, config->modbus2SlaveAddr);
    ModbusRTU_SetHoldingRegs(&modbusUart2, uart2_holdingRegs, REG_HOLD_COUNT);
    ModbusRTU_SetInputRegs(&modbusUart2, uart2_inputRegs, REG_INPUT_COUNT);
    ModbusRTU_SetCoils(&modbusUart2, uart2_coils, COIL_COUNT);
    ModbusRTU_SetDiscreteInputs(&modbusUart2, uart2_discreteInputs, DI_COUNT);
    modbusUart2.onCoilChanged = uart2_onCoilChanged;
    modbusUart2.onRegChanged  = uart2_onRegChanged;

    seedConfigMirror(uart2_holdingRegs);
    uart2_holdingRegs[REG_HOLD_SLAVE_ADDR] = config->modbus2SlaveAddr;
    uart2_inputRegs[REG_INPUT_MAP_VERSION] = MODBUS_REG_MAP_VERSION;
    uart2_coils[0] = lampMgrGetActualBits() & 0x1F;

    ModbusPort_UART2_Init(&modbusUart2);
    ModbusRTU_Init(&modbusUart2);
}

/* ==================== 主循环处理 ==================== */

void ModbusApp_Process(void)
{
    ModbusRTU_Process(&modbusUart1);
    ModbusRTU_Process(&modbusUart2);
}

/* ==================== 传感器/状态更新（每秒调用） ==================== */

void ModbusApp_UpdateSensorData(void)
{
    static uint16_t heartbeat = 0;
    heartbeat++;

    /* ---- 传感器 ---- */
    PressureData_t pressureData = pressureSensorGetData();
    WaterLevelState_e waterLevel = waterLevelGetLevel();
    bool lowProbe, midProbe, highProbe;
    waterLevelGetProbeStates(&lowProbe, &midProbe, &highProbe);

    LampMgrStatus_t lampStatus;
    lampMgrGetStatus(&lampStatus);

    ScheduleStatus_t schedStatus;
    scheduleGetStatus(&schedStatus);

    uint8_t actualBits = lampMgrGetActualBits();

    /* ---- 填 UART1 输入寄存器 ---- */
    uart1_inputRegs[REG_INPUT_HEARTBEAT]         = heartbeat;
    uart1_inputRegs[REG_INPUT_PRESSURE_X1000]    = (uint16_t)(pressureData.pressureFiltered * 1000.0f);
    uart1_inputRegs[REG_INPUT_CURRENT_X100]      = (uint16_t)(pressureData.current * 100.0f);
    uart1_inputRegs[REG_INPUT_ADC_RAW]           = pressureData.adcRaw;
    uart1_inputRegs[REG_INPUT_PRESSURE_VALID]    = pressureData.isValid ? 1 : 0;

    switch (waterLevel) {
        case WATER_LEVEL_NONE:  uart1_inputRegs[REG_INPUT_WATER_LEVEL] = 0; break;
        case WATER_LEVEL_LOW:   uart1_inputRegs[REG_INPUT_WATER_LEVEL] = 1; break;
        case WATER_LEVEL_MID:   uart1_inputRegs[REG_INPUT_WATER_LEVEL] = 2; break;
        case WATER_LEVEL_HIGH:  uart1_inputRegs[REG_INPUT_WATER_LEVEL] = 3; break;
        case WATER_LEVEL_ERROR: /* fallthrough */
        default:                uart1_inputRegs[REG_INPUT_WATER_LEVEL] = 0xFF; break;
    }

    uart1_inputRegs[REG_INPUT_ACTIVE_SCENE]      = lampStatus.activeScene;
    uart1_inputRegs[REG_INPUT_ACTIVE_MODE]       = (uint16_t)lampStatus.activeMode;
    uart1_inputRegs[REG_INPUT_SCHEDULE_HIT]      = schedStatus.activeEntry;
    uart1_inputRegs[REG_INPUT_LAMP_BITS_ACTUAL]  = actualBits;
    uart1_inputRegs[REG_INPUT_OVERRIDE_LEFT_SEC] = (uint16_t)(lampStatus.overrideLeftMs / 1000u);
    uart1_inputRegs[REG_INPUT_TIME_SYNCED]       = schedStatus.timeSynced ? 1 : 0;

    uart1_inputRegs[REG_INPUT_ERR_LO]            = (uint16_t)(errorGetActiveMask() & 0xFFFF);
    uart1_inputRegs[REG_INPUT_ERR_HI]            = (uint16_t)((errorGetActiveMask() >> 16) & 0xFFFF);
    uart1_inputRegs[REG_INPUT_RX_FRAME_CNT]      = (uint16_t)(modbusUart1.stats.rxFrameCount & 0xFFFF);
    uart1_inputRegs[REG_INPUT_CRC_ERR_CNT]       = (uint16_t)(modbusUart1.stats.crcErrorCount & 0xFFFF);
    uart1_inputRegs[REG_INPUT_MAP_VERSION]       = MODBUS_REG_MAP_VERSION;

    /* UART2 镜像（仅修改传感器+状态区，其他与 UART1 同） */
    for (uint16_t i = 0; i <= REG_INPUT_MAP_VERSION; i++) {
        uart2_inputRegs[i] = uart1_inputRegs[i];
    }
    uart2_inputRegs[REG_INPUT_RX_FRAME_CNT] = (uint16_t)(modbusUart2.stats.rxFrameCount & 0xFFFF);
    uart2_inputRegs[REG_INPUT_CRC_ERR_CNT]  = (uint16_t)(modbusUart2.stats.crcErrorCount & 0xFFFF);

    /* ---- 填离散输入 ---- */
    uint8_t diPressure = 0;
    if (pressureData.isValid)               diPressure |= (1u << 0);
    if (pressureData.pressureFiltered > 1.0f) diPressure |= (1u << 1);
    if (pressureData.pressureFiltered > 1.4f) diPressure |= (1u << 2);

    uint8_t diProbes = 0;
    if (!lowProbe)  diProbes |= (1u << 0);
    if (!midProbe)  diProbes |= (1u << 1);
    if (!highProbe) diProbes |= (1u << 2);

    uint8_t diWaterCode = 0;
    switch (waterLevel) {
        case WATER_LEVEL_NONE:  diWaterCode = 0x00; break;
        case WATER_LEVEL_LOW:   diWaterCode = 0x01; break;
        case WATER_LEVEL_MID:   diWaterCode = 0x02; break;
        case WATER_LEVEL_HIGH:  diWaterCode = 0x03; break;
        case WATER_LEVEL_ERROR: /* fallthrough */
        default:                diWaterCode = 0xFF; break;
    }
    if (waterLevelIsStable()) {
        diWaterCode |= 0x80;
    }

    uint8_t diMode = 0;
    if (lampStatus.activeMode == LAMP_MODE_SMART)     diMode |= (1u << 0);
    if (schedStatus.activeEntry != 0xFF)              diMode |= (1u << 1);
    if (lampStatus.overrideActive)                    diMode |= (1u << 2);
    if (lampStatus.activeMode == LAMP_MODE_EMERGENCY) diMode |= (1u << 3);

    uart1_discreteInputs[DI_BYTE_PRESSURE]    = diPressure;
    uart1_discreteInputs[DI_BYTE_WATER_PROBE] = diProbes;
    uart1_discreteInputs[DI_BYTE_WATER_CODE]  = diWaterCode;
    uart1_discreteInputs[DI_BYTE_MODE_STATUS] = diMode;

    uart2_discreteInputs[DI_BYTE_PRESSURE]    = diPressure;
    uart2_discreteInputs[DI_BYTE_WATER_PROBE] = diProbes;
    uart2_discreteInputs[DI_BYTE_WATER_CODE]  = diWaterCode;
    uart2_discreteInputs[DI_BYTE_MODE_STATUS] = diMode;

    /* ---- 同步虚拟线圈到实际硬件状态 ---- */
    syncCoilsFromHardware(actualBits);

    /* ---- 同步几个关键保持寄存器的"镜像值"方便主机读回 ---- */
    uart1_holdingRegs[REG_HOLD_MODE]         = (uint16_t)lampStatus.activeMode;
    uart1_holdingRegs[REG_HOLD_LAMP_BITS_RW] = actualBits;
    uart1_holdingRegs[REG_HOLD_SCENE_REQ]    = lampStatus.activeScene;
    uart1_holdingRegs[REG_HOLD_TIME_HHMM]    = schedStatus.curHHMM;
    uart1_holdingRegs[REG_HOLD_TIME_DOW]     = schedStatus.curDow;

#if ENABLE_DUAL_UART_SYNC
    uart2_holdingRegs[REG_HOLD_MODE]         = uart1_holdingRegs[REG_HOLD_MODE];
    uart2_holdingRegs[REG_HOLD_LAMP_BITS_RW] = uart1_holdingRegs[REG_HOLD_LAMP_BITS_RW];
    uart2_holdingRegs[REG_HOLD_SCENE_REQ]    = uart1_holdingRegs[REG_HOLD_SCENE_REQ];
    uart2_holdingRegs[REG_HOLD_TIME_HHMM]    = uart1_holdingRegs[REG_HOLD_TIME_HHMM];
    uart2_holdingRegs[REG_HOLD_TIME_DOW]     = uart1_holdingRegs[REG_HOLD_TIME_DOW];
#endif
}

/* ==================== 实例 getter ==================== */

ModbusRTU_t* ModbusApp_GetUart1Instance(void)
{
    return &modbusUart1;
}

ModbusRTU_t* ModbusApp_GetUart2Instance(void)
{
    return &modbusUart2;
}
