# O-Radio 0.3.12

STS-6686N3（ESP32-S3、16MB Flash、8MB OPI PSRAM、TEF6686、ST7789）专用固件。
支持网络电台、FM 搜台/收藏、中文机身界面、网页管理、闹钟、深睡与七套配色。
当前版本已通过 USB 写入并确认启动，主机测试 14 项通过。配色预览按键和实际半小时自动深睡仍需使用中验证。

## 机身操作

| 操作 | 作用 |
|---|---|
| 转 A | 网络切台 / FM 在已保存电台中切换 |
| 单击 A | 网络播放/停止 / FM 静音 / 搜台时取消 |
| 双击 A | 进入闹钟设置 |
| 长按 A 1.5 秒 | FM 自动搜台 |
| 转 B | 调音量，顺时针增大 |
| 单击 B | 网络/FM 切换 |
| 双击 B | 显示/关闭设备信息（地址、用户名、密码） |
| 长按 B 3 秒并松开 | 配色预览 |

闹钟页：转 A 选项、转 B 调整；单击 A 试听/停止，单击 B 保存，双击 A 取消。
重复预设为仅一次、每天、工作日、周末；网页自定义星期在未改此项时保留。
配色页：转 B 预览，单击 B 保存，单击 A 恢复原配色。预览不调台、不改音量，确认前不写闪存。
浅色：浅紫、薄荷绿、蜜桃；深色：午夜蓝、暮紫、深林绿、暖墨琥珀。选择会保存。

## 网页

双击 B 查看局域网地址和密码；用户名 `admin`。热点 `O-NetRadio-xxxx` 的地址为 `http://192.168.4.1`，密码同后台。

- Wi-Fi 配网、模式切换、音量、播放、FM 搜台/收藏/手动调谐。
- 最多 100 个网络电台；导入 JSON、M3U、PLS 或每行 URL，支持追加去重、排序、搜索和导出。
- 填写可直接播放的 HTTP/HTTPS 音频流地址，而非电台官网页面。具体编码、HLS 源可用性取决于解码库和网络。
- 闹钟电台、时分、重复星期和音量设置；北京时间通过 NTP 校时。
- 控制 POST 需 HTTP Basic Auth 与 `X-Radio-Control: 1`；机身设置页打开时 POST 返回 409，GET 状态可继续查看。
- 网页 OTA 会写另一个 OTA 分区，可能覆盖保留的原厂 app1；本机日常刷入采用 app0 定址写入。

## 熄屏与深睡

- 30 秒无操作：仅熄屏，继续播放；第一次操作只亮屏。
- 连续 30 分钟无操作且未播放：自动深睡；网页也有手动深睡入口。
- 网络 wanted=true（含静音、缓冲和断线重连）仍视为播放会话；主动停止后开始计时。FM 静音或音量零视为停止。
- 搜台、更新、机身设置页和闹钟唤醒等待期间不自动休眠。
- 深睡停止播放、关闭 Wi-Fi；按 B 唤醒。已启用闹钟时规划定时唤醒，通常提前至多 60 秒联网；联网慢可在到点后十分钟内补响。
- 原厂电池 ADC：GPIO3、20 次平均、raw×1.79mV。电量是估算值，不判断充电状态；整机深睡电流尚未测量。

闹钟定时唤醒已由机主在 0.3.8 确认有效，后续版本沿用。半小时自动深睡与新增配色选择页仍需本机实际验证。

## 构建

从仓库根目录：

```bash
python3 merchant_s3/tools/embed_web.py
pio run -d merchant_s3
```

构建输出 `merchant_s3/.pio/build/sts_6686n3/firmware.bin`。网页和中文字体嵌入应用，无需 uploadfs。
`.pio/` 是可再生成的依赖和编译缓存，清理后第一次构建会重新安装依赖，需要网络。

当前构建环境：espressif32 8.0.0、Arduino-ESP32 3.3.12、TFT_eSPI 2.5.43、ArduinoJson 7.4.2。
ESP32-audioI2S 固定提交 `8b4bbbdef5b993acc30139046d3e5abb4dd354dc`。完整编译环境见 `../docs/build-environment.txt`。
音频库 HTTPS 行为沿用该提交的 setInsecure，不校验证书链。

中文字体已生成在 include/cjk_font.h；仅重新生成时需要 Pillow 和 Noto Sans CJK：

```bash
python3 merchant_s3/tools/build_cjk_font.py --font /path/to/NotoSansCJK-Regular.ttc
```

## 测试与固件校验

```bash
python3 merchant_s3/tools/test_host.py
node merchant_s3/tests/web/run_tests.cjs
python3 merchant_s3/tools/check_image.py merchant_s3/releases/O-Radio-0.3.12-app.bin
```

主机测试需要 g++；导入器测试需要 Node.js。测试不是实际 GPIO、屏幕、扬声器和睡眠电流测试。

## 刷入与恢复

只写 app0，不使用默认 pio upload，不擦整片、不刷文件系统：

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write-flash \
  0x10000 merchant_s3/releases/O-Radio-0.3.12-app.bin
```

应用 `.bin` 绝不能写入 0x0。固定分区表在 partitions.csv：app0 为 0x10000/0x700000，原厂 app1 为 0x710000/0x700000。
本机恢复需使用设备所有者单独保存的完整备份和恢复说明；原厂整片备份包含私人配置，不在公开仓库提供。
NVS 使用 o-radio、FM 使用 o-fm；电台文件为 SPIFFS 的 /o_net.json，挂载失败不自动格式化。

## 维护要点

- 编译只使用本目录 src/include/web，不依赖已清理的原上游 src/。
- 音频 I2S 是 32-bit MSB/LSBJ、bit_shift=false、48kHz。PCM16→32 桥接不可误改为 Philips I2S，否则会重现刺耳噪声。
- FM 时 TEF 为时钟主机，ESP I2S 必须释放总线；网络时恢复 ESP 主机。GPIO45 的真实外部电路功能仍未证实。
- 硬件引脚：include/board_sts_6686n3.h；原厂逆向研究笔记只保留本地，不随源码提交。
- 本地构建产物及 releases/ 中的应用、ELF 和日志不随源码提交；GitHub 固件下载可通过 Releases 单独提供。
- 校验/刷入示例中的 releases/O-Radio-0.3.12-app.bin 是本地发布产物；从源码开始请先构建，再校验或刷入 .pio/build/sts_6686n3/firmware.bin。
- 上游来源和许可见 ../docs/ATTRIBUTION.md 与根目录 LICENSE。
