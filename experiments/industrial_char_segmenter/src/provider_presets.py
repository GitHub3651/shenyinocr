"""Preset VLM providers and model choices for the GUI/CLI.

The app sends images through OpenAI-compatible chat-completions payloads.
Provider base URLs are hidden from beginner users, but kept here so they are
easy to update when a vendor changes an endpoint or model name.
"""

from __future__ import annotations

import os
from copy import deepcopy
from typing import Any


DEFAULT_PROVIDER_ID = "qwen"

PROVIDER_PRESETS: dict[str, dict[str, Any]] = {
    "qwen": {
        "display_name": "千问 / 阿里云百炼",
        "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "workspace_base_url_template": "https://{workspace_id}.cn-beijing.maas.aliyuncs.com/compatible-mode/v1",
        "api_key_env": "DASHSCOPE_API_KEY",
        "workspace_id_env": "DASHSCOPE_WORKSPACE_ID",
        "supports_image": True,
        "note": (
            "推荐用于视觉 bbox 分割。使用阿里云百炼 DashScope API Key。"
            "若连接测试 403，可填写百炼业务空间 ID 使用官方推荐的新域名。"
        ),
        "models": [
            {
                "id": "qwen3.7-plus",
                "label": "qwen3.7-plus（官方图像理解入口，推荐）",
            },
            {
                "id": "qwen3.5-omni-plus",
                "label": "qwen3.5-omni-plus（全模态）",
            },
            {
                "id": "qwen3.5-omni-plus-realtime",
                "label": "qwen3.5-omni-plus-realtime",
            },
            {
                "id": "qwen3.6-flash",
                "label": "qwen3.6-flash（低成本，需确认图片能力）",
            },
            {
                "id": "qwen3.7-max",
                "label": "qwen3.7-max（高能力，需确认图片能力/权限）",
            },
        ],
    },
    "zhipu": {
        "display_name": "智谱 GLM",
        "base_url": "https://open.bigmodel.cn/api/paas/v4",
        "api_key_env": "ZHIPUAI_API_KEY",
        "supports_image": True,
        "note": "使用智谱开放平台 API Key。请选择带 V 的视觉模型。",
        "models": [
            {
                "id": "glm-4.5v",
                "label": "glm-4.5v（推荐）",
            },
            {
                "id": "glm-4v-plus",
                "label": "glm-4v-plus",
            },
            {
                "id": "glm-4v-flash",
                "label": "glm-4v-flash",
            },
        ],
    },
    "kimi": {
        "display_name": "Kimi / Moonshot",
        "base_url": "https://api.moonshot.cn/v1",
        "api_key_env": "MOONSHOT_API_KEY",
        "supports_image": True,
        "note": "使用 Moonshot/Kimi API Key。请选择 vision-preview 视觉模型。",
        "models": [
            {
                "id": "moonshot-v1-8k-vision-preview",
                "label": "moonshot-v1-8k-vision-preview（推荐）",
            },
            {
                "id": "moonshot-v1-32k-vision-preview",
                "label": "moonshot-v1-32k-vision-preview",
            },
            {
                "id": "moonshot-v1-128k-vision-preview",
                "label": "moonshot-v1-128k-vision-preview",
            },
        ],
    },
    "deepseek": {
        "display_name": "DeepSeek（官方接口暂不支持图片）",
        "base_url": "https://api.deepseek.com/v1",
        "api_key_env": "DEEPSEEK_API_KEY",
        "supports_image": False,
        "note": (
            "DeepSeek 官方 API 当前主要是文本模型，不能直接完成本项目的图片 bbox 分割。"
            "如需图片分割，请选千问、智谱或 Kimi。"
        ),
        "models": [
            {
                "id": "deepseek-chat",
                "label": "deepseek-chat（文本，不适合本任务）",
            },
            {
                "id": "deepseek-reasoner",
                "label": "deepseek-reasoner（文本，不适合本任务）",
            },
        ],
    },
}


def provider_ids() -> list[str]:
    return list(PROVIDER_PRESETS.keys())


def get_provider(provider_id: str | None) -> dict[str, Any]:
    provider_key = (provider_id or os.environ.get("VLM_PROVIDER") or DEFAULT_PROVIDER_ID).lower()
    if provider_key not in PROVIDER_PRESETS:
        supported = ", ".join(provider_ids())
        raise ValueError(f"Unsupported provider {provider_key!r}. Supported: {supported}")
    return deepcopy(PROVIDER_PRESETS[provider_key])


def get_provider_display_name(provider_id: str) -> str:
    return str(get_provider(provider_id)["display_name"])


def get_provider_id_by_display_name(display_name: str) -> str:
    for provider_id, preset in PROVIDER_PRESETS.items():
        if preset["display_name"] == display_name:
            return provider_id
    raise ValueError(f"Unknown provider display name: {display_name}")


def get_models(provider_id: str | None) -> list[dict[str, str]]:
    return list(get_provider(provider_id)["models"])


def default_model(provider_id: str | None) -> str:
    models = get_models(provider_id)
    if not models:
        raise ValueError(f"Provider {provider_id!r} has no configured models")
    return models[0]["id"]


def resolve_provider_config(
    provider_id: str | None,
    model: str | None,
    base_url: str | None,
    workspace_id: str | None = None,
) -> dict[str, Any]:
    provider = get_provider(provider_id)
    selected_model = model or os.environ.get("VLM_API_MODEL") or provider["models"][0]["id"]
    selected_workspace_id = (
        workspace_id
        or os.environ.get("VLM_API_WORKSPACE_ID")
        or os.environ.get(str(provider.get("workspace_id_env", "")), "")
    )
    selected_base_url = base_url or os.environ.get("VLM_API_BASE_URL")
    if not selected_base_url and selected_workspace_id and provider.get("workspace_base_url_template"):
        selected_base_url = str(provider["workspace_base_url_template"]).format(
            workspace_id=selected_workspace_id
        )
    if not selected_base_url:
        selected_base_url = provider["base_url"]
    return {
        "provider_id": (provider_id or os.environ.get("VLM_PROVIDER") or DEFAULT_PROVIDER_ID).lower(),
        "display_name": provider["display_name"],
        "base_url": selected_base_url,
        "model": selected_model,
        "workspace_id": selected_workspace_id,
        "api_key_env": provider["api_key_env"],
        "supports_image": bool(provider["supports_image"]),
        "note": provider["note"],
    }
