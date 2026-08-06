"""Vision-language model client abstractions.

The project intentionally separates bbox generation from final image cropping.
Only bbox coordinates come from a model; final character images are always
direct crops from the original image in cropper.py.
"""

from __future__ import annotations

import base64
import json
import mimetypes
import os
import re
import struct
import zlib
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Any

from .json_utils import read_json
from .provider_presets import resolve_provider_config


class VLMClientError(RuntimeError):
    """Base exception for model-client failures."""


class ExtractJSONError(VLMClientError):
    """Raised when a model response does not contain parseable JSON."""


def _decode_json_candidate(candidate: str) -> Any:
    try:
        return json.loads(candidate)
    except json.JSONDecodeError as exc:
        raise ExtractJSONError(f"Invalid JSON: {exc.msg} at char {exc.pos}") from exc


def extract_json_from_text(text: str) -> Any:
    """Extract a JSON object from raw model text, including fenced responses."""
    if not isinstance(text, str) or not text.strip():
        raise ExtractJSONError("Model response is empty or not text")

    stripped = text.strip()

    try:
        return json.loads(stripped)
    except json.JSONDecodeError:
        pass

    fence_pattern = re.compile(r"```(?:json|JSON)?\s*(.*?)```", re.DOTALL)
    for match in fence_pattern.finditer(stripped):
        candidate = match.group(1).strip()
        try:
            return _decode_json_candidate(candidate)
        except ExtractJSONError:
            continue

    decoder = json.JSONDecoder()
    for index, char in enumerate(stripped):
        if char not in "{[":
            continue
        try:
            data, _ = decoder.raw_decode(stripped[index:])
            return data
        except json.JSONDecodeError:
            continue

    raise ExtractJSONError("No valid JSON object or array found in model response")


class VLMClient(ABC):
    """Abstract VLM client interface used by the agent."""

    @abstractmethod
    def segment(
        self,
        image_path: str,
        target_text: str,
        prompt: str,
        image_width: int,
        image_height: int,
    ) -> dict:
        """Return raw model segmentation data."""


class MockVLMClient(VLMClient):
    """Development client that reads bbox data from a JSON file."""

    def __init__(self, model_json_path: str | Path):
        self.model_json_path = Path(model_json_path)

    def segment(
        self,
        image_path: str,
        target_text: str,
        prompt: str,
        image_width: int,
        image_height: int,
    ) -> dict:
        if not self.model_json_path.exists():
            raise FileNotFoundError(f"Mock model JSON not found: {self.model_json_path}")

        raw_data = read_json(self.model_json_path)

        if isinstance(raw_data, str):
            data = extract_json_from_text(raw_data)
        elif isinstance(raw_data, dict) and isinstance(raw_data.get("model_output"), str):
            data = extract_json_from_text(raw_data["model_output"])
        elif isinstance(raw_data, dict) and isinstance(raw_data.get("text"), str):
            data = extract_json_from_text(raw_data["text"])
        elif isinstance(raw_data, list):
            data = {"segments": raw_data}
        elif isinstance(raw_data, dict):
            data = dict(raw_data)
        else:
            raise VLMClientError(
                "Mock model JSON must be a dict, list, or string containing model JSON"
            )

        if not isinstance(data, dict):
            raise VLMClientError("Model JSON must decode to a JSON object")

        data.setdefault("image_width", image_width)
        data.setdefault("image_height", image_height)
        data.setdefault("target_text", target_text)
        data.setdefault("char_count", len(target_text))
        data.setdefault("notes", "Loaded by MockVLMClient")
        return data


class RealVLMClient(VLMClient):
    """OpenAI-compatible vision API client.

    Many VLM providers expose a /chat/completions endpoint compatible with the
    OpenAI message format. Configure this client with an API key, base URL, and
    model name. The model only returns bbox JSON; final crops are still taken
    directly from the original image by cropper.py.
    """

    def __init__(
        self,
        api_key: str | None = None,
        provider_id: str | None = None,
        base_url: str | None = None,
        model: str | None = None,
        workspace_id: str | None = None,
        timeout: int = 60,
    ):
        provider_config = resolve_provider_config(provider_id, model, base_url, workspace_id)
        provider_env_key = str(provider_config["api_key_env"])
        self.provider_id = str(provider_config["provider_id"])
        self.provider_name = str(provider_config["display_name"])
        self.api_key = api_key or os.environ.get("VLM_API_KEY", "") or os.environ.get(provider_env_key, "")
        self.base_url = str(provider_config["base_url"])
        self.model = str(provider_config["model"])
        self.workspace_id = str(provider_config.get("workspace_id") or "")
        self.supports_image = bool(provider_config["supports_image"])
        self.provider_note = str(provider_config["note"])
        self.timeout = int(timeout)
        self.connect_timeout = min(10, max(3, self.timeout))

    def _validate_config(self) -> None:
        if not self.supports_image:
            raise VLMClientError(
                f"{self.provider_name} 当前不支持本项目所需的图片输入。"
                f"{self.provider_note}"
            )
        if not self.api_key:
            raise VLMClientError(
                "Missing API key. Fill API Key in the GUI, set VLM_API_KEY, "
                "or set the provider-specific API key environment variable."
            )
        if not self.base_url:
            raise VLMClientError(
                "Missing API base URL. Check provider presets or set advanced "
                "override VLM_API_BASE_URL."
            )
        if not self.model:
            raise VLMClientError(
                "Missing model name. Fill Model in the GUI or set VLM_API_MODEL."
            )

    def _chat_completions_url(self) -> str:
        base_url = self.base_url.rstrip("/")
        if base_url.endswith("/chat/completions"):
            return base_url
        return f"{base_url}/chat/completions"

    @staticmethod
    def _tiny_png_data_url() -> str:
        # 16x16 white PNG. Alibaba Model Studio rejects 1x1 test images.
        width = 16
        height = 16

        def chunk(chunk_type: bytes, data: bytes) -> bytes:
            return (
                struct.pack(">I", len(data))
                + chunk_type
                + data
                + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
            )

        raw_rows = b"".join(b"\x00" + (b"\xff\xff\xff" * width) for _ in range(height))
        png_bytes = (
            b"\x89PNG\r\n\x1a\n"
            + chunk(
                b"IHDR",
                struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0),
            )
            + chunk(b"IDAT", zlib.compress(raw_rows))
            + chunk(b"IEND", b"")
        )
        png_base64 = base64.b64encode(png_bytes).decode("ascii")
        return f"data:image/png;base64,{png_base64}"

    @staticmethod
    def _image_data_url(image_path: str) -> str:
        path = Path(image_path)
        if not path.exists():
            raise FileNotFoundError(f"Image not found: {path}")
        mime_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        encoded = base64.b64encode(path.read_bytes()).decode("ascii")
        return f"data:{mime_type};base64,{encoded}"

    @staticmethod
    def _message_content_to_text(content: Any) -> str:
        if isinstance(content, str):
            return content
        if isinstance(content, list):
            parts: list[str] = []
            for item in content:
                if isinstance(item, dict):
                    text = item.get("text")
                    if isinstance(text, str):
                        parts.append(text)
                elif isinstance(item, str):
                    parts.append(item)
            return "\n".join(parts)
        return str(content)

    def _post_chat_completions(self, payload: dict) -> dict:
        try:
            import requests
        except ModuleNotFoundError as exc:
            raise VLMClientError(
                "Missing dependency: requests. Run `pip install -r requirements.txt`."
            ) from exc

        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        url = self._chat_completions_url()
        try:
            response = requests.post(
                url,
                headers=headers,
                json=payload,
                timeout=(self.connect_timeout, self.timeout),
            )
        except requests.exceptions.ConnectTimeout as exc:
            raise VLMClientError(
                f"API 连接超时。\n"
                f"厂商：{self.provider_name}\n"
                f"模型：{self.model}\n"
                f"接口：{url}\n"
                f"连接超时：{self.connect_timeout} 秒\n\n"
                "含义：程序还没能和 API 服务器建立稳定连接。"
                "这通常是网络、代理、防火墙、DNS 或接口地址问题，不是模型在思考。\n"
                "建议：\n"
                "1. 先点“测试连接”。\n"
                "2. 检查网络/代理是否能访问该厂商 API。\n"
                "3. 千问如使用百炼业务空间，请确认业务空间 ID 是否正确。"
            ) from exc
        except requests.exceptions.ReadTimeout as exc:
            raise VLMClientError(
                f"API 读取超时。\n"
                f"厂商：{self.provider_name}\n"
                f"模型：{self.model}\n"
                f"接口：{url}\n"
                f"读取超时：{self.timeout} 秒\n\n"
                "含义：连接阶段已经通过，请求也已发出，但平台在限定时间内没有返回完整响应。"
                "这更像是模型处理/排队/图片较大/平台响应慢，而不是“根本没连上”。\n"
                "如果“测试连接”是绿色，基本可以确认 Key、接口和模型是可达的。\n"
                "建议：\n"
                "1. 把 GUI 里的 API 超时改成 180 或 300 秒后重试。\n"
                "2. 换一个更快/更轻量的模型。\n"
                "3. 选更小的图片区域或压缩输入图片后再让模型给 bbox。"
            ) from exc
        except requests.exceptions.RequestException as exc:
            raise VLMClientError(
                f"API 网络请求失败。\n"
                f"厂商：{self.provider_name}\n"
                f"模型：{self.model}\n"
                f"接口：{url}\n"
                f"错误：{exc}"
            ) from exc
        if response.status_code >= 400:
            raise VLMClientError(self._format_http_error(response.status_code, response.text))
        try:
            return response.json()
        except ValueError as exc:
            raise VLMClientError(
                f"VLM API response is not JSON: {response.text[:1200]}"
            ) from exc

    def _format_http_error(self, status_code: int, response_text: str) -> str:
        provider = self.provider_name
        model = self.model
        message = response_text[:1200]
        error_code = ""
        error_message = ""
        try:
            body = json.loads(response_text)
            error = body.get("error", body) if isinstance(body, dict) else {}
            if isinstance(error, dict):
                error_code = str(error.get("code") or "")
                error_message = str(error.get("message") or "")
                message = error_message or message
        except ValueError:
            pass

        if status_code == 401:
            return (
                f"API 鉴权失败 HTTP 401。\n"
                f"厂商：{provider}\n模型：{model}\n\n"
                "请检查 API Key 是否属于当前选择的厂商，是否复制完整，是否已过期。"
            )

        if status_code == 403 or error_code == "access_denied":
            suggestions = [
                "API Key 不是当前厂商的 Key，或填错/复制不完整。",
                "账号没有开通该平台的模型服务权限。",
                f"当前模型 {model} 没有权限访问，换一个模型试试。",
                "账号欠费、额度不足，或服务被平台限制。",
            ]
            if self.provider_id == "qwen":
                suggestions = [
                    "请确认填写的是阿里云百炼 DashScope API Key，不是阿里云 AccessKey，也不是其他厂商 Key。",
                    "确认阿里云百炼/模型服务已经开通，并且账号有调用通义千问 VL 模型权限。",
                    f"当前模型 {model} 可能无权限，建议先试 qwen-vl-plus-latest。",
                    "检查百炼控制台是否欠费、额度不足、服务未开通或被限制。",
                ]
            return (
                f"API 访问被拒绝 HTTP 403。\n"
                f"厂商：{provider}\n模型：{model}\n"
                f"平台返回：{message}\n\n"
                "这通常不是图片或程序问题，而是 API 权限问题。请检查：\n"
                + "\n".join(f"{idx}. {item}" for idx, item in enumerate(suggestions, start=1))
            )

        return (
            f"VLM API 请求失败 HTTP {status_code}。\n"
            f"厂商：{provider}\n模型：{model}\n"
            f"平台返回：{message}"
        )

    def test_connection(self) -> dict:
        """Send a tiny image request to verify API key, model, and endpoint."""
        self._validate_config()
        payload = {
            "model": self.model,
            "temperature": 0,
            "max_tokens": 32,
            "messages": [
                {
                    "role": "system",
                    "content": "Reply with OK only.",
                },
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "text",
                            "text": "Connection test. If you can read this request, reply OK.",
                        },
                        {
                            "type": "image_url",
                            "image_url": {"url": self._tiny_png_data_url()},
                        },
                    ],
                },
            ],
        }
        response_data = self._post_chat_completions(payload)
        try:
            message = response_data["choices"][0]["message"]
            content = self._message_content_to_text(message.get("content", ""))
        except (KeyError, IndexError, TypeError):
            content = ""
        return {
            "ok": True,
            "provider_id": self.provider_id,
            "provider_name": self.provider_name,
            "base_url": self.base_url,
            "model": self.model,
            "message": content.strip() or "OK",
        }

    def segment(
        self,
        image_path: str,
        target_text: str,
        prompt: str,
        image_width: int,
        image_height: int,
    ) -> dict:
        self._validate_config()
        data_url = self._image_data_url(image_path)
        prompt_with_size = (
            f"{prompt}\n\n"
            f"实际原图尺寸：image_width={image_width}, image_height={image_height}。\n"
            "请再次确认输出 JSON 中的 image_width/image_height 与上述尺寸一致。"
        )
        payload = {
            "model": self.model,
            "temperature": 0,
            "max_tokens": 4096,
            "messages": [
                {
                    "role": "system",
                    "content": "You output only valid JSON. Do not include Markdown fences.",
                },
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": prompt_with_size},
                        {"type": "image_url", "image_url": {"url": data_url}},
                    ],
                },
            ],
        }

        response_data = self._post_chat_completions(payload)
        try:
            message = response_data["choices"][0]["message"]
            model_text = self._message_content_to_text(message.get("content", ""))
        except (KeyError, IndexError, TypeError) as exc:
            raise VLMClientError(
                f"Unexpected VLM API response shape: {json.dumps(response_data, ensure_ascii=False)[:1200]}"
            ) from exc

        parsed = extract_json_from_text(model_text)
        if isinstance(parsed, list):
            parsed = {"segments": parsed}
        if not isinstance(parsed, dict):
            raise VLMClientError("VLM API output JSON must be an object or segments list")

        parsed.setdefault("image_width", image_width)
        parsed.setdefault("image_height", image_height)
        parsed.setdefault("target_text", target_text)
        parsed.setdefault("char_count", len(target_text))
        parsed.setdefault("notes", f"Generated by RealVLMClient model={self.model}")
        parsed["_api"] = {
            "client": "RealVLMClient",
            "provider_id": self.provider_id,
            "provider_name": self.provider_name,
            "base_url": self.base_url,
            "model": self.model,
            "workspace_id": self.workspace_id,
        }
        return parsed
