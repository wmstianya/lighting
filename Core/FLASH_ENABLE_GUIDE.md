# Flash存储功能启用指南

## 一键开关

Flash存储功能由 **单个宏定义** 控制，只需修改一处即可：

### 📍 配置位置

**文件：** `Core/Inc/config_manager.h` (第26行)

```c
/* ==================== 功能开关 ==================== */
/**
 * @brief Flash功能总开关
 * @note 设置为1启用Flash读写，设置为0禁用Flash（使用默认配置）
 */
#define ENABLE_FLASH_STORAGE    0   /* 0=禁用Flash, 1=启用Flash */
```

## 🔧 如何启用Flash

### 方法1：修改宏定义（推荐）

在 `Core/Inc/config_manager.h` 中：

```c
#define ENABLE_FLASH_STORAGE    1   /* 改为1启用Flash */
```

保存后重新编译，Flash功能自动生效！

### 方法2：Keil全局定义

在Keil项目设置中：
1. 右键项目 → Options for Target
2. C/C++ → Preprocessor Symbols → Define
3. 添加：`ENABLE_FLASH_STORAGE=1`

## 📊 两种模式对比

| 功能 | Flash禁用 (0) | Flash启用 (1) |
|------|--------------|--------------|
| **配置来源** | 默认配置（内存） | Flash读取或默认 |
| **配置保存** | 不保存（重启丢失） | 保存到Flash（掉电保持） |
| **错误日志** | 仅内存统计 | Flash持久化62条 |
| **启动速度** | ✅ 快（<1ms） | 稍慢（读Flash ~1ms） |
| **Flash寿命** | ✅ 无损耗 | 每次保存消耗1次擦写 |
| **适用场景** | 调试、测试 | 生产部署 |

## ⚙️ 受影响的功能

启用/禁用Flash会自动影响以下模块：

### 配置管理 (config_manager.c)

**禁用时 (0)：**
```c
configManagerInit() → 使用默认配置
configSave() → 返回成功但不保存
```

**启用时 (1)：**
```c
configManagerInit() → 从Flash读取配置（有效则使用，无效则用默认）
configSave() → 保存到Flash（擦除+写入）
```

### 错误处理 (error_handler.c)

**禁用时 (0)：**
```c
errorHandlerInit() → 仅内存统计
errorReport() → 内存记录，不写Flash
errorLogRead() → 返回0条
```

**启用时 (1)：**
```c
errorHandlerInit() → 从Flash读取历史日志
errorReport() → 写入Flash日志
errorLogRead() → 读取Flash日志（最近62条）
```

## ⚠️ 启用Flash前的注意事项

### 1. 确认Flash地址正确

**STM32F103C8T6 (64KB)：**
```c
#define CONFIG_FLASH_BASE_ADDR      0x0800F800  ✓ 正确
#define ERROR_LOG_FLASH_BASE_ADDR   0x0800F000  ✓ 正确
```

**如果是其他型号，请修改地址：**

| 型号 | Flash | 配置区地址 | 日志区地址 |
|------|-------|-----------|-----------|
| C8T6 | 64KB | 0x0800F800 | 0x0800F000 |
| CBT6 | 128KB | 0x0801F800 | 0x0801F000 |
| RCT6 | 256KB | 0x0803F800 | 0x0803F000 |

### 2. 代码空间检查

启用Flash前，检查编译后的`.map`文件：

```
Total RO  Size (Code + RO Data)    xxxxx
```

**安全阈值：**
- C8T6: < 60KB （为配置和日志预留4KB）
- 超过60KB会覆盖配置区，导致系统异常！

### 3. Flash擦写寿命

STM32F103 Flash：**10,000次擦除周期**

**建议：**
- ❌ 不要频繁保存（如每秒保存）
- ✅ 仅在参数修改时手动保存
- ✅ 或延迟保存（如10秒后自动保存一次）

### 4. 看门狗兼容性

Flash擦除需要**20-40ms**，期间CPU挂起。

如果外部看门狗超时时间 < 100ms，可能会复位。

**解决方案：**
```c
// 在configSave()前暂停看门狗
// 保存完成后恢复看门狗
```

## 🚀 推荐使用场景

### 禁用Flash (0) - 适用于：
- ✅ 开发调试阶段
- ✅ 功能验证测试
- ✅ 频繁修改参数
- ✅ 快速启动需求

### 启用Flash (1) - 适用于：
- ✅ 生产部署
- ✅ 现场安装
- ✅ 需要掉电保存配置
- ✅ 需要错误日志追溯

## 📝 使用示例

### 场景1：开发测试（禁用Flash）

```c
// config_manager.h
#define ENABLE_FLASH_STORAGE    0   // 禁用

// 效果：
configSetModbus(0x05, 0x06);  // 修改配置（仅内存）
configSave();  // 返回成功但不保存
// 重启后恢复默认配置0x01, 0x02
```

### 场景2：生产部署（启用Flash）

```c
// config_manager.h
#define ENABLE_FLASH_STORAGE    1   // 启用

// 效果：
configSetModbus(0x05, 0x06);  // 修改配置
configSave();  // 保存到Flash（20-40ms）
// 重启后配置保持：0x05, 0x06 ✓
```

## 🔍 故障排查

### 问题：启用Flash后系统卡住

**检查清单：**
1. ✅ Flash地址是否正确？
   - C8T6: 0x0800F800 ✓
   - 不是: 0x0803F800 ❌

2. ✅ 代码大小是否超限？
   - 查看`.map`文件
   - Code Size < 60KB ✓

3. ✅ 是否在初始化时保存？
   - 不应该在 `configManagerInit()` 中自动保存
   - 应该手动调用 `configSave()`

4. ✅ 看门狗是否冲突？
   - Flash擦除20-40ms
   - 看门狗超时应 > 100ms

### 问题：Flash保存失败

**可能原因：**
- Flash已被写保护
- Flash损坏
- 地址错误

**诊断代码：**
```c
HAL_StatusTypeDef result = configSave();
if (result != HAL_OK) {
    ERROR_REPORT_ERROR(ERROR_FLASH_WRITE_FAILED, "Config Save Fail");
}
```

## ✅ 当前配置

- **Flash状态**：禁用 (ENABLE_FLASH_STORAGE = 0)
- **Flash地址**：已修正为C8T6正确地址
- **系统状态**：正常运行，使用默认配置
- **下一步**：测试稳定后可启用Flash

---

**快速修改：只需修改 `config_manager.h` 第26行的 `0` 改为 `1` 即可启用Flash！**

