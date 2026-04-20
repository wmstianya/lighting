# 压力传感器滤波算法使用指南

## 📚 概述

本项目为4-20mA压力传感器提供了**三种滤波算法**，可通过宏定义轻松切换：

1. **卡尔曼滤波** - 理论最优，适合高精度应用
2. **中值滤波 + IIR滤波混合** - 抗干扰能力最强，适合工业现场（推荐）
3. **仅IIR滤波** - 计算最简单，内存占用最小

---

## 🔧 如何切换滤波算法

### 修改配置文件

打开 `Core/Inc/pressure_sensor.h`，找到以下代码：

```c
/* 当前使用的滤波算法（修改此行切换算法） */
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_MEDIAN_IIR
```

修改为以下三个值之一：

```c
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_KALMAN          // 卡尔曼滤波
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_MEDIAN_IIR      // 中值+IIR混合（默认）
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_IIR_ONLY        // 仅IIR滤波
```

### 重新编译

修改后需要**重新编译**整个项目才能生效。

---

## 📊 算法详细对比

### 性能对比表

| 算法              | 内存占用 | 计算量 | 延迟 | 参数调整难度 | 噪声抑制 | 脉冲抑制 | 推荐场景 |
|-------------------|---------|--------|------|-------------|---------|---------|---------|
| **卡尔曼滤波**    | 20字节  | 中     | 中   | 困难        | 好      | 差      | 实验室、高精度 |
| **中值+IIR混合**  | 28字节  | 大     | 中   | 容易        | 很好    | 最好    | **工业现场（推荐）** |
| **仅IIR滤波**     | 8字节   | 最小   | 小   | 最容易      | 好      | 差      | 内存受限、简单应用 |

---

## 🎯 详细算法说明

### 1. 卡尔曼滤波（FILTER_TYPE_KALMAN）

#### 算法原理
卡尔曼滤波是一种递归贝叶斯估计算法，在高斯噪声假设下提供理论最优估计。

```
预测步骤：
  x_pred = x
  p_pred = p + Q

更新步骤：
  K = p_pred / (p_pred + R)
  x = x_pred + K * (measurement - x_pred)
  p = (1 - K) * p_pred
```

#### 参数配置
在 `pressure_sensor.h` 中修改：

```c
#define KALMAN_PROCESS_NOISE    0.005f   // Q值：过程噪声（0.001~0.01）
#define KALMAN_MEASURE_NOISE    0.5f     // R值：测量噪声（0.1~10）
#define KALMAN_ESTIMATE_ERROR   1.0f     // P0值：初始估计误差
```

**参数调整建议：**
- **Q值越小** → 滤波越平滑，但响应越慢
- **R值越小** → 越信任测量值，滤波效果越弱
- **Q/R比值** → 决定滤波强度和响应速度的平衡

#### 优势
- ✅ 理论上的最优估计（高斯噪声假设下）
- ✅ 具有预测能力
- ✅ 自适应性强

#### 劣势
- ❌ 参数调整困难，需要反复试验
- ❌ 对脉冲干扰敏感
- ❌ 假设噪声为高斯分布（实际可能不满足）

---

### 2. 中值滤波 + IIR滤波混合（FILTER_TYPE_MEDIAN_IIR）⭐推荐⭐

#### 算法原理
采用两级滤波架构：

**第1级：中值滤波**
- 使用5个样本的滑动窗口
- 对窗口内数据排序，取中值
- 有效去除脉冲噪声和电磁干扰

**第2级：IIR低通滤波**
- 对中值滤波后的数据进一步平滑
- 公式：`y[n] = α × x[n] + (1 - α) × y[n-1]`
- α为平滑系数（0~1）

#### 参数配置
在 `pressure_sensor.h` 中修改：

```c
#define IIR_ALPHA    0.3f    // IIR平滑系数（0.1~0.5）
```

**参数调整建议：**
- `α = 0.1` → 最平滑，延迟最大
- `α = 0.2` → 适合噪声大的场景
- `α = 0.3` → **平衡型（推荐）**
- `α = 0.5` → 快速响应，适合快变信号

#### 优势
- ✅ **抗干扰能力最强**（能去除脉冲噪声）
- ✅ 参数调整简单直观
- ✅ 适合工业现场复杂环境
- ✅ 对非高斯噪声效果好

#### 劣势
- ❌ 计算量较大（排序 + 滤波）
- ❌ 内存占用稍多（28字节）
- ❌ 延迟稍大（中值窗口 + IIR延迟）

---

### 3. 仅IIR滤波（FILTER_TYPE_IIR_ONLY）

#### 算法原理
一阶IIR低通滤波器，也称为指数移动平均（EMA）：

```
y[n] = α × x[n] + (1 - α) × y[n-1]
```

#### 参数配置
在 `pressure_sensor.h` 中修改：

```c
#define IIR_ALPHA    0.25f   // IIR平滑系数（0.1~0.5）
```

#### 优势
- ✅ **计算量最小**（仅2次乘法 + 1次加法）
- ✅ **内存占用最小**（仅8字节）
- ✅ 参数调整最简单
- ✅ 延迟小

#### 劣势
- ❌ 对脉冲干扰敏感
- ❌ 无法去除突发噪声
- ❌ 抗干扰能力弱

---

## 🛠️ 实际应用建议

### 场景1：工业现场（电磁干扰强）
**推荐：中值滤波 + IIR滤波混合**

```c
#define PRESSURE_FILTER_TYPE    FILTER_TYPE_MEDIAN_IIR
#define IIR_ALPHA               0.3f    // 平衡型参数
```

**理由：**
- 工业现场有大量电磁干扰（继电器、电机、变频器等）
- 中值滤波能有效去除脉冲噪声
- IIR滤波进一步平滑数据
- 抗干扰能力最强

---

### 场景2：实验室环境（干扰小、需要高精度）
**推荐：卡尔曼滤波**

```c
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_KALMAN
#define KALMAN_PROCESS_NOISE        0.002f  // 压力变化很慢
#define KALMAN_MEASURE_NOISE        0.3f    // ADC噪声较小
#define KALMAN_ESTIMATE_ERROR       1.0f
```

**理由：**
- 环境噪声小且符合高斯分布
- 需要理论最优估计
- 有时间调试参数

---

### 场景3：简单应用（内存受限）
**推荐：仅IIR滤波**

```c
#define PRESSURE_FILTER_TYPE    FILTER_TYPE_IIR_ONLY
#define IIR_ALPHA               0.3f    // 快速响应
```

**理由：**
- 内存占用最小（仅8字节）
- 计算量最小
- 对于4-20mA慢变信号已足够

---

### 场景4：需要快速响应
**推荐：仅IIR滤波（大α值）**

```c
#define PRESSURE_FILTER_TYPE    FILTER_TYPE_IIR_ONLY
#define IIR_ALPHA               0.5f    // 快速响应
```

**理由：**
- 延迟最小
- 能快速跟踪压力变化

---

## 📈 参数调整技巧

### 卡尔曼滤波参数调整

1. **先设置Q和R的比值**
   - `Q/R = 0.01` → 非常平滑，响应慢
   - `Q/R = 0.1` → 平衡型
   - `Q/R = 1.0` → 快速响应

2. **再调整绝对值**
   - 根据实际噪声水平调整R值
   - R值 = ADC噪声方差的估计值

3. **观察效果**
   - 如果滤波后仍有抖动 → 减小Q值
   - 如果响应太慢 → 增大Q值或减小R值

### IIR滤波参数调整

1. **从0.3开始**（推荐值）

2. **噪声大** → 减小α（如0.2或0.1）

3. **需要快速响应** → 增大α（如0.4或0.5）

4. **公式参考**
   ```
   延迟时间 ≈ (1/α - 1) × 采样周期
   
   例如：α=0.3, 采样周期=100ms
   延迟 ≈ (1/0.3 - 1) × 100ms = 233ms
   ```

---

## 🧪 测试验证

### 测试步骤

1. **设置滤波算法**
   ```c
   #define PRESSURE_FILTER_TYPE    FILTER_TYPE_MEDIAN_IIR
   ```

2. **编译并下载程序**

3. **监控数据**
   - 通过串口或Modbus读取滤波前后的数据
   - `pressureSensorGetPressureRaw()` - 原始值
   - `pressureSensorGetPressure()` - 滤波后的值

4. **评估指标**
   - **平滑度**：滤波后数据的波动幅度
   - **延迟**：施加压力变化后的响应时间
   - **抗干扰**：在继电器动作时的数据稳定性

---

## 💡 常见问题

### Q1: 切换滤波算法后需要重新编译吗？
**A:** 是的，必须重新编译整个项目。因为使用的是条件编译（`#if`），只有在编译时才会选择相应的代码。

### Q2: 三种算法可以同时启用吗？
**A:** 不可以。同一时间只能选择一种滤波算法。

### Q3: 如何知道哪种算法最适合我的应用？
**A:** 建议按以下顺序测试：
1. 先用 `FILTER_TYPE_MEDIAN_IIR`（通用性最好）
2. 如果内存不够，改用 `FILTER_TYPE_IIR_ONLY`
3. 如果需要极致精度，尝试 `FILTER_TYPE_KALMAN` 并仔细调参

### Q4: 滤波后数据仍有抖动怎么办？
**A:** 
- **卡尔曼滤波**：减小Q值（如从0.005改为0.002）
- **IIR滤波**：减小α值（如从0.3改为0.2）
- **中值+IIR**：减小IIR的α值

### Q5: 滤波后响应太慢怎么办？
**A:** 
- **卡尔曼滤波**：增大Q值或减小R值
- **IIR滤波**：增大α值（如从0.3改为0.5）
- **中值+IIR**：增大IIR的α值

---

## 📝 代码示例

### 示例1：使用中值+IIR混合滤波

```c
/* 在 pressure_sensor.h 中配置 */
#define PRESSURE_FILTER_TYPE    FILTER_TYPE_MEDIAN_IIR
#define IIR_ALPHA               0.3f

/* 在 main.c 中使用 */
pressureSensorInit(0.0f, 1.6f);  // 0-1.6 MPa

while (1) {
    pressureSensorProcess();  // 自动100ms采样一次
    
    float pressure = pressureSensorGetPressure();  // 获取滤波后的压力值
    
    // 使用pressure做控制逻辑...
}
```

### 示例2：使用卡尔曼滤波

```c
/* 在 pressure_sensor.h 中配置 */
#define PRESSURE_FILTER_TYPE        FILTER_TYPE_KALMAN
#define KALMAN_PROCESS_NOISE        0.005f
#define KALMAN_MEASURE_NOISE        0.5f
#define KALMAN_ESTIMATE_ERROR       1.0f

/* 使用方法同上 */
```

### 示例3：获取原始值和滤波值对比

```c
PressureData_t data = pressureSensorGetData();

printf("ADC Raw: %d\r\n", data.adcRaw);
printf("Voltage: %.3f V\r\n", data.voltage);
printf("Current: %.2f mA\r\n", data.current);
printf("Pressure Raw: %.3f MPa\r\n", data.pressureRaw);
printf("Pressure Filtered: %.3f MPa\r\n", data.pressureFiltered);
printf("Valid: %s\r\n", data.isValid ? "Yes" : "No");
```

---

## 📖 参考资料

### 相关文件
- `Core/Inc/filter.h` - 中值滤波和IIR滤波定义
- `Core/Src/filter.c` - 中值滤波和IIR滤波实现
- `Core/Inc/kalman.h` - 卡尔曼滤波定义
- `Core/Src/kalman.c` - 卡尔曼滤波实现
- `Core/Inc/pressure_sensor.h` - 压力传感器驱动（含滤波切换）
- `Core/Src/pressure_sensor.c` - 压力传感器驱动实现

### 算法复杂度
| 算法 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 卡尔曼滤波 | O(1) | O(1) |
| 中值滤波 | O(n²) 或 O(n log n) | O(n) |
| IIR滤波 | O(1) | O(1) |

注：中值滤波当前使用冒泡排序，时间复杂度O(n²)，但n=5很小，实际性能可接受。

---

## ✅ 总结

- **工业现场首选**：中值滤波 + IIR滤波混合（`FILTER_TYPE_MEDIAN_IIR`）
- **极简场景**：仅IIR滤波（`FILTER_TYPE_IIR_ONLY`）
- **高精度需求**：卡尔曼滤波（`FILTER_TYPE_KALMAN`）

只需修改一行宏定义，即可切换不同的滤波算法，无需改动其他代码！

---

**文档版本：** v1.0  
**最后更新：** 2025-11-13  
**作者：** Lighting Ultra Team

