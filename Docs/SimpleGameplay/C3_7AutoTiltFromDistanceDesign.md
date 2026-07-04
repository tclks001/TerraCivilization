# TerraCivilization SimpleGameplay C3.7 自动倾角设计稿

> 本稿从 [C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md) 的 $8 "暂不处理" 拆出。
>
> C3.7 把 C3 手动相机中独立维护的 `TiltDeg` 改为从 `Distance` 自动线性插值派生。

---

## 1. 动机

在 C3 手动相机中，`TiltDeg` 是仅有的"没有输入驱动"的内部真值：

| 输入 | 影响的状态 |
| --- | --- |
| W/S/A/D | `FocusUnitDir` + `YawAroundFocusDeg` |
| Q/E (C3.6) | `YawAroundFocusDeg` |
| 滚轮 | `DistanceToFocusCM` |
| **无** | `TiltDeg`（仅 Sync 时初始化一次，之后冻结） |

这导致玩家拉近到贴地距离时 Tilt 仍停留在 C2 残留的 55°，构图别扭看不到地平线；拉远时也缺少俯瞰视野。

C3.7 让 `TiltDeg = f(Distance)` 自动插值，玩家滚轮缩放即可自然获得"拉近 = 平视细节 / 拉远 = 俯瞰全局"的体验。

---

## 2. 插值公式

线性插值：

```text
t = clamp((Distance - MinDistance) / (MaxDistance - MinDistance), 0, 1)
TiltDeg = lerp(TiltAtMinDistance, TiltAtMaxDistance, t)
```

```text
                     TiltDeg
                        ↑
            TiltAtMax ──┼────────────●  (最大距离 = 俯瞰)
                        │          ／
                        │        ／
                        │      ／  ← 线性
                        │    ／
                        │  ／
            TiltAtMin ──┼●──────────────→ Distance
                        │
                    MinDistance      MaxDistance
```

---

## 3. 新增配置参数（4 个 UPROPERTY + 1 个初始距离）

全部暴露在 `APlanetTessellatedMesh` 上：

| 参数 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| `C3AutoTiltMinDistanceCM` | float | 2500.0 | 触发最小倾角的距离（cm） |
| `C3AutoTiltMaxDistanceCM` | float | 30000.0 | 触发最大倾角的距离（cm） |
| `C3AutoTiltAtMinDistanceDeg` | float | 10.0 | 最小距离时的倾角（度，接近平视） |
| `C3AutoTiltAtMaxDistanceDeg` | float | 85.0 | 最大距离时的倾角（度，接近垂直俯瞰） |
| `C3InitialDistanceToFocusCM` | float | 8000.0 | C3 首次进入手动模式前的 fallback 初始距离（cm） |

端点不取 0°/90° 的原因：
- 0° → 相机贴地平视，几乎看不到地面；
- 90° → cos(Tilt) 归零，极点退化加重；且与 C2 的 55° 斜俯视差异过大。

---

## 4. 对现有架构的影响

### 4.1 消除的字段

**Controller 侧**（`APlanetInteractionController`）：

- 删除成员 `float C3TiltDeg` —— Tilt 不再是独立持久状态。

### 4.2 修改的函数

**Controller 侧**（`APlanetInteractionController::UpdateC3FocusCameraControl_`）：

- 删除 `C3TiltDeg = FMath::Clamp(C3TiltDeg, 5.0f, 85.0f);`
- 新增每帧计算：从 `C3DistanceToFocusCM` 按 §2 公式线性插值得到本帧 `TiltDeg`
- 将计算所得 `TiltDeg` 作为局部变量传入 `ApplyFocusCameraState`

**Controller 侧**（`APlanetInteractionController::InitializeC3FocusCameraState_`）：

- `SyncFocusCameraStateFromView` 仍然会返回 TiltDeg（签名不改，C4 以后可能需要），但 C3 的 Init 不再保存到持久状态。当前 C3 也不保存。
- `C3InitialDistanceToFocusCM` 替代原先的硬编码 `8000.0f` 作为 fallback 默认距离。

### 4.3 不修改的部分

- `ApplyFocusCameraState`：签名不变，仍接收 `TiltDeg` 参数，不关心 Tilt 是谁算的。
- `OffsetFocusCameraStateOnTangent`：不涉及 Tilt，完全不动。
- Q/E、WSAD、滚轮的输入映射：不动。
- C3.5 每帧 Apply：自动 Tilt 每帧算出来后立刻落到相机，零额外成本。

---

## 5. 验收

1. **拉到最近距离**：倾角接近 `C3AutoTiltAtMinDistanceDeg`（默认 10°），构图接近平视。
2. **拉到最远距离**：倾角接近 `C3AutoTiltAtMaxDistanceDeg`（默认 85°），构图接近俯瞰。
3. **滚轮缩放中间档位**：倾角随距离线性变化，无跳变。
4. **缩放过程中 WSAD 仍正常工作**：Tilt 改变不影响焦点位置和 Yaw 平行运输。
5. **Q/E 旋转后缩放**：Yaw 和 Tilt 互不影响。
6. **C3 原有 §7 验收条款仍成立**：因为只是把 Tilt 的来源从"冻结常量"换成了"距离函数"，不改变 WSAD/QE/滚轮的行为语义。

---

## 6. 暂不处理

- 非线性缓入缓出曲线。当前线性插值足够，未来若需要可用编辑器曲线资产替换。
- 自动镜头期间暂停自动 Tilt。此问题留给 C4 智能选中聚焦阶段处理。
