# WS73 / Hi3873S Linux 驱动

Ai-WS1-CBS（Hi3873S）SDIO 驱动，适配 Linux 6.18，支持 Wi-Fi、蓝牙和星闪。
CBS 使用 `firmware/us/`；`firmware/e/` 对应 Ai-WS1-CBE / Hi3873E，不能混用。

| 模块 | 功能 |
| --- | --- |
| `plat_soc.ko` | 平台、电源、SDIO 和固件下载，其余模块均依赖它 |
| `wifi_soc.ko` | Wi-Fi，使用 cfg80211 / nl80211 |
| `ble_soc.ko` | Bluetooth HCI，可配合 BlueZ 使用 |
| `sle_soc.ko` | 星闪字符设备 `/dev/hwsle`，需要匹配的用户空间协议栈 |

## 构建

使用与目标内核一致的源码、配置、架构和工具链，在仓库根目录执行：

```sh
export KERNEL_DIR=/path/to/linux
export CROSS_COMPILE=arm-linux-gnueabi-
export TARGET_ARCH=arm

make wifi           # 同时构建平台模块并生成 INI
# make wifi ble sle # 按需增加蓝牙、星闪
```

产物位于 `output/bin/`：所选模块的 `.ko` 文件及 `ws73_cfg.ini`。
默认配置见 `build/config/ws73_default.config`，可通过 `make menuconfig` 调整。
`make all` 还会构建默认启用的蓝牙、星闪和调试工具。

增量构建可使用 `make platform_fast`、`make wifi_fast`；它们不重新生成配置。
`wifi_fast` 需要当前平台模块对应的 `driver/platform/Module.symvers`。
只更新 INI 时执行 `make ini`。

内核需启用模块、cfg80211、MMC/SDIO 及板载 MMC 控制器支持；按所选功能配置
WEXT、RFKILL、SHA256、Bluetooth（BR/EDR、LE）和电源时序支持。
`regulatory.db` 可安装到目标系统或嵌入内核。

## 部署

以下示例安装 Wi-Fi 所需模块、生成的配置及 CBS 固件：

```sh
TARGET_ROOT=/path/to/target-rootfs
KREL=$(make -s -C "$KERNEL_DIR" ARCH="$TARGET_ARCH" \
    CROSS_COMPILE="$CROSS_COMPILE" kernelrelease)

mkdir -p "$TARGET_ROOT/lib/modules/$KREL/extra" "$TARGET_ROOT/etc/ws73"
cp output/bin/plat_soc.ko output/bin/wifi_soc.ko \
    "$TARGET_ROOT/lib/modules/$KREL/extra/"
cp output/bin/ws73_cfg.ini "$TARGET_ROOT/etc/ws73_cfg.ini"
cp firmware/us/*.bin "$TARGET_ROOT/etc/ws73/"
depmod -b "$TARGET_ROOT" "$KREL"
```

蓝牙、星闪按需额外安装 `ble_soc.ko`、`sle_soc.ko`。固件文件应来自同一 SDK
版本；其中 `btc_cali.bin` 用于蓝牙/星闪校准，`wow.bin` 用于 WoW。

目标设备上加载：

```sh
modprobe plat_soc
modprobe wifi_soc
# modprobe ble_soc
# modprobe sle_soc
```

Wi-Fi 使用 `wpa_supplicant` 和 DHCP 客户端联网；蓝牙使用 BlueZ，无需 UART
HCI attach。星闪通过 `/dev/hwsle` 收发，不能用 BlueZ 管理。

## 硬件与配置

SDIO 控制器需配置 4 bit 数据宽度、SDIO IRQ、正确的引脚复用和供电。
CBS-Kit 建议使用稳定的 3.3 V 电源。电源时序、卡检测和休眠唤醒按实际硬件设置。

运行配置为 `/etc/ws73_cfg.ini`：

- `power_gpio_idx`、`wkup_gpio_idx` 填写实际 Linux GPIO 编号；设为 `-1`
  表示驱动不控制该引脚。P_on 是模块信号名，不是 GPIO 编号。
- `[HOST_PLAT]` 默认不设置 `mac_addr`：优先使用 eFuse 地址，无有效地址时
  随机生成。需要固定地址时，在每台设备上单独配置唯一地址，不要写入公共镜像。
- Wi-Fi、蓝牙和星闪共存时，按硬件设置 `[DEVICE_BT]` 中的共存、休眠和
  校准参数；没有明确需求时保留默认值。

枚举异常先检查供电、引脚及 MMC 配置；模块加载失败先检查内核版本、配置和
`Module.symvers` 是否匹配。
