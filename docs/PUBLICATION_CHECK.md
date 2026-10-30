# 公开前检查

检查日期：2026-10-30；当前源代码版本：O-Radio 0.3.12。
检查针对原开发目录进行；本发布副本从检查后的白名单复制，以独立新仓库发布，不携带原 .git、设备镜像、备份或日志。

## 扫描结果

- 扫描当前候选源码/文档的常见密钥模式，没有发现命中。
- 扫描本地全部 refs 可达历史：2199 个提交，3106 个 blob，约 386.6MB 未压缩内容。
- 历史文本未命中本次模式：GitHub token、AWS access ID、私钥块、Slack token、OpenAI 风格 key、URL 内嵌账号密码、字面量密码赋值。
- 7 个二进制 blob 未纳入文本模式判定。上游提交作者署名和历史保持原样；本次没有重写历史。
- 这是启发式检查，不是所有形式秘密均不存在的保证；运行时 NVS/Flash 与设备配置不应进入公开仓库。

## 公开源码范围

- merchant_s3/src、include、web、tests、tools、fonts，以及 PlatformIO 配置和分区表。
- 根目录 README、LICENSE、.gitignore，公开说明与来源/构建环境资料。
- 中文字体已附 SIL OFL；保留 GPL-3.0 及 PE5PVB 上游归属说明。
- 后台密码在运行时随机生成并保存在设备 NVS，不是源码中的固定值；网页 password 表单和固件 wifiPass 字段属于程序逻辑，不是写死的凭据。

## 不公开的本地资料

.gitignore 已排除：
- 整个 firmware_analysis/，含原厂镜像、整片备份、恢复资料、设备标识和研究笔记。
- 整个 merchant_s3/releases/，含本地 BIN/ELF、启动日志及测试产物；固件可另行用 GitHub Releases 发布。
- docs/cleanup-manifest.json、docs/CLEANUP.md，本地清理记录含设备备份文件名。
- .pio/、构建/生成缓存，以及 .aws/、.codex/、.agents/、.env 和常见私钥文件。

排除不等于删除：私人资料仍保留本地。当前已跟踪文件没有命中上述私人目录。
.gitignore 对已经提交过的秘密不构成历史清理；本次检查未发现本机自定义固件、备份或研究目录存在于历史提交中。

## 本仓库范围

本仓库是 73 个白名单源码/文档文件的干净快照，不包含原开发目录的 Git 历史。
原目录和设备恢复资料仍由设备所有者单独保留。
仓库来源为 PE5PVB/TEF6686_ESP32，保留 GPL-3.0、字体许可及 docs/ATTRIBUTION.md。
后续提交仍须检查暂存文件，避免新增运行时配置、密码、固件备份或日志。
