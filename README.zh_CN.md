<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Linepods

**随时回顾微信读书划线。**

Linepods 把 FoloToy AI Passport 变成一个随身划线口袋。完成一次同步后，
不用打开手机，也能在设备上重新遇见读过的句子。

## 功能亮点

- 更友好的首次使用流程：用户主动开始配置前，设备保持在欢迎页。
- 通过 Linepods 临时 Wi-Fi 热点，在手机上完成本地配置。
- 使用你自己的微信读书 API Key 同步划线。
- 划线持久缓存在设备中，可离线浏览卡片并分页阅读详情。
- 针对 240 x 320 屏幕设计并验证的中文排版。
- 清晰的同步成功与错误恢复页，以及电量、时间和低功耗行为。

## 界面

| 同步页 | 划线卡片 | 详情页 |
| --- | --- | --- |
| <img src="assets/images/linepods-sync-poster.png" alt="Linepods 同步页" width="240"> | <img src="assets/images/linepods-card-poster.png" alt="Linepods 划线卡片" width="240"> | <img src="assets/images/linepods-detail-poster.png" alt="Linepods 详情页" width="240"> |

## 首次使用

1. 将已校验的合并固件从 `0x0` 偏移地址刷入设备。
2. 在欢迎页按确定键，开始同步。
3. 手机连接设备屏幕上的 `Linepods-XXXX` 临时热点，再打开
   `192.168.4.1`。
4. 填写 2.4 GHz Wi-Fi 和微信读书 API Key。API Key 可在微信读书 App 的
   “我 → 设置 → 微信读书技能 → 获取 API Key”中找到。
5. 提交后回到设备，查看验证和首次同步结果。

只有验证和首次同步成功后，设备才会保存配置。请勿将 API Key 或
Wi-Fi 密码提交到仓库。

## 构建

Linepods 面向 ESP32-C3、8 MB Flash、无 PSRAM 的 AI Passport，使用 ESP-IDF 5.5.3。

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

验证通过的完整固件位于 `build/FoloToy-AI-Passport-full.bin`，需从 `0x0`
刷入；合并固件可能重置已有 NVS 配置。

## 版本

当前版本：**Linepods v0.1.0**（`v0.1.0-linepods`）。

Linepods 运行在开源的 FoloToy AI Passport 硬件平台上，并遵循本仓库的
[MIT License](LICENSE)。
