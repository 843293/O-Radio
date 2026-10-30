# 来源与许可

本项目从 PE5PVB/TEF6686_ESP32 仓库开展开发：
https://github.com/PE5PVB/TEF6686_ESP32

当前 STS-6686N3 固件位于 merchant_s3/。TEF 补丁/LUT 和基础初始化资料沿用上游项目来源（原 Tuner_Patch_Lithio_V102_p224.h、Tuner_Interface.cpp）；保留根目录 GPL-3.0 LICENSE。原始上游工程可从本仓库原有 Git 历史或上游仓库取得，不再复制一套无关硬件构建目标。

依赖及各自许可以依赖项目为准：
- TFT_eSPI：https://github.com/Bodmer/TFT_eSPI
- ESP32-audioI2S：https://github.com/schreibfaul1/ESP32-audioI2S
- ArduinoJson：https://github.com/bblanchon/ArduinoJson
- Arduino-ESP32 / ESP-IDF：Espressif 官方项目。
- 中文位图来自 Noto Sans CJK SC，字体许可保留于 merchant_s3/fonts/OFL.txt。

依赖版本和音频库提交在 merchant_s3/platformio.ini 固定；构建环境记录见 build-environment.txt。
