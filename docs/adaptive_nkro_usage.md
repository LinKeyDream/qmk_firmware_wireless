# Adaptive NKRO 使用报告

## 功能概述

本分支加入 Keychron 风格的 Adaptive NKRO（自适应全键无冲）报告分配逻辑。启用后，键盘不会在普通报告和 NKRO 报告之间二选一，而是并行维护两种报告：

- 前 6 个普通键保留在标准 USB 键盘报告的 `keys[6]` 中。
- 第 7 个及之后的普通键写入 NKRO 位图。
- 修饰键仍写入标准报告的 `mods` 字段。
- 标准报告和 NKRO 报告分别通过 dirty 标记发送，互不覆盖另一种报告的状态。

NKRO 报告使用现有的 `REPORT_ID_NKRO`，位图容量由 `NKRO_REPORT_BITS` 决定，本仓库当前为 30 字节。

## 启用范围

核心代码只有在同时满足 `NKRO_ENABLE` 和 `APDAPTIVE_NKRO_ENABLE` 时进入自适应路径。普通 QMK NKRO 构建不改变原有行为。

Keymagichorse 的共享构建规则在 `BLUETOOTH_DRIVER=bhq` 时加入 `APDAPTIVE_NKRO_ENABLE`，因此 BHQ BLE 键盘自动启用该功能；USB-only 或非 BHQ 构建不会因为这次修改被全局打开。

## 传输行为

- USB Report Protocol：支持标准报告和 NKRO 报告并行发送。
- USB Boot Protocol：`host_can_send_nkro()` 返回 false，只发送标准 6KRO 报告；超过 6 个同时按下的键无法进入 NKRO 位图，仍遵循 6KRO 的传输限制。
- Bluetooth：是否发送 NKRO 由蓝牙驱动的 `bluetooth_can_send_nkro()` 决定。BHQ 驱动支持时使用自适应分配，否则退回标准报告路径。

清空按键时会同时清空标准报告和 NKRO 位图，并重置内部计数器。重复添加或删除同一个键不会重复计数。`has_anykey()`、`is_key_pressed()` 和 `get_first_key()` 会考虑两种存储区域。

## 构建与测试

代表性 BHQ BLE 固件目标：

```sh
make keymagichorse/hl6095:ble
```

该目标应使用 `NKRO_ENABLE`，并从 `keyboards/keymagichorse/kb_common/kb_common.mk` 获得 `-DAPDAPTIVE_NKRO_ENABLE`。

专用单元测试：

```sh
make test:adaptive_nkro
```

测试覆盖前六键、溢出键、溢出键释放、清空复用、Boot Protocol fallback，以及标准报告和 NKRO 报告的独立发送。

本工作区当前无法实际运行上述测试和固件编译：QMK Makefile 在启动时调用 `qmk hello`，但环境中未安装或未加入 PATH 的 `qmk` 命令，因此命令在编译前退出。补齐 QMK CLI/依赖后应重新执行两条命令。

## 已知限制

1. Boot Protocol 不支持 NKRO；在该协议下第 7 个及之后的同时按下键不会被缓存到未来的 Report Protocol 切换中。
2. NKRO 位图可表达的键码范围仍受 `NKRO_REPORT_BITS` 限制，超出范围的键会被忽略并输出调试信息。
3. 报告是否真正能够通过蓝牙发送，取决于具体蓝牙传输驱动的 NKRO 能力声明。
