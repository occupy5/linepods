#!/usr/bin/env python3
"""Static contracts for the first-run and provisioning experience."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def main() -> None:
    app = read("main/main.c")
    logic = read("main/linepods_logic.c")
    network = read("main/linepods_network.c")
    ui = read("main/linepods_ui.c")
    client = read("main/linepods_client.c")

    assert 'ui_welcome();' in app
    assert 'No saved configuration; waiting for user to start setup' in app
    assert 'logic->view == LINEPODS_VIEW_WELCOME' in logic
    assert 'LINEPODS_INPUT_OK_CLICK' in logic
    assert 'LINEPODS_ACTION_START_SETUP' in logic

    assert '那一天我二十一岁' in ui
    assert '那一年我二十一岁' not in ui
    assert '在我一生的黄金时代' in ui
    assert '2  打开 192.168.4.1' in ui
    assert '确定键 · 同步划线' in ui
    assert '摘录已装进口袋' in ui
    assert '还差一步，没有连接成功' in ui

    for text in (
        '打开微信读书 App',
        '我 → 设置 → 微信读书技能 → 获取 API Key',
        '验证并同步划线',
        '配置已提交',
        '更新 Wi-Fi',
        'API Key 会保持不变',
        '更新 API Key',
        'Wi-Fi 会保持不变',
    ):
        assert text in network

    assert 'linepods_config_t config = s_provision_base;' in network
    assert 's_provision_mode != LINEPODS_PROVISION_API_KEY' in network
    assert 's_provision_mode != LINEPODS_PROVISION_WIFI' in network

    assert 'background:#fff' in network
    assert 'background:#e9e8e1' not in network
    assert 'background:#f3f1ea' not in network

    assert 'API Key 无效或已过期' in client
    assert '请求有些频繁' in client
    assert '微信读书服务暂时不可用' in client
    assert '#define LINEPODS_SKILL_VERSION "1.0.5"' in client
    assert '"1.0.4"' not in client
    print("Linepods onboarding contracts: PASS")


if __name__ == "__main__":
    main()
