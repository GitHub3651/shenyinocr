"""Main industrial character segmentation workflow."""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

from .cropper import crop_original_chars
from .image_io import imread_unchanged
from .json_utils import ensure_dir, write_json
from .preview import draw_preview
from .validator import validate_segments
from .vlm_client import MockVLMClient, RealVLMClient, VLMClient


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _load_prompt(target_text: str) -> str:
    prompt_path = _project_root() / "prompts" / "segmenter_prompt.txt"
    if not prompt_path.exists():
        raise FileNotFoundError(f"Prompt template not found: {prompt_path}")
    template = prompt_path.read_text(encoding="utf-8")
    return template.replace("{target_text}", target_text)


def _read_image_size(image_path: str) -> tuple[int, int]:
    image = imread_unchanged(image_path)
    height, width = image.shape[:2]
    return width, height


def _make_client(
    model_json_path: str | None,
    api_key: str | None,
    api_provider: str | None,
    api_base_url: str | None,
    api_model: str | None,
    api_workspace_id: str | None,
    api_timeout: int,
) -> VLMClient:
    if model_json_path:
        return MockVLMClient(model_json_path)
    return RealVLMClient(
        api_key=api_key,
        provider_id=api_provider,
        base_url=api_base_url,
        model=api_model,
        workspace_id=api_workspace_id,
        timeout=api_timeout,
    )


def _paths(output_dir: str) -> dict[str, Path]:
    root = ensure_dir(output_dir)
    return {
        "root": root,
        "segments_model": root / "segments_model.json",
        "segments_final": root / "segments_final.json",
        "preview": root / "preview_boxes.png",
        "run_report": root / "run_report.json",
    }


def run_char_segmentation_agent(
    image_path: str,
    target_text: str,
    output_dir: str,
    model_json_path: str | None = None,
    max_retry: int = 2,
    api_key: str | None = None,
    api_provider: str | None = None,
    api_base_url: str | None = None,
    api_model: str | None = None,
    api_workspace_id: str | None = None,
    api_timeout: int = 60,
    progress_callback: Callable[[str, int, str], None] | None = None,
) -> dict[str, Any]:
    """Run the complete bbox validation, preview, and direct-crop workflow."""
    def progress(stage: str, percent: int, message: str) -> None:
        if progress_callback:
            progress_callback(stage, percent, message)

    max_retry = max(0, int(max_retry))
    progress("prepare", 5, "准备输出目录")
    paths = _paths(output_dir)
    started_at = _utc_now()

    run_report: dict[str, Any] = {
        "status": "running",
        "started_at": started_at,
        "finished_at": None,
        "image_path": str(image_path),
        "target_text": target_text,
        "target_length": len(target_text),
        "output_dir": str(paths["root"]),
        "model_json_path": str(model_json_path) if model_json_path else None,
        "api_mode": "mock_json" if model_json_path else "real_api",
        "api_provider": api_provider,
        "api_base_url": api_base_url,
        "api_model": api_model,
        "api_workspace_id": api_workspace_id,
        "attempts": [],
        "errors": [],
        "warnings": [],
        "outputs": {},
    }

    model_data: dict[str, Any] | None = None
    final_data: dict[str, Any] | None = None
    validation_report: dict[str, Any] | None = None

    try:
        if not target_text:
            raise ValueError("target_text must not be empty")

        progress("read_image", 12, "读取原图尺寸")
        image_width, image_height = _read_image_size(image_path)
        run_report["image_width"] = image_width
        run_report["image_height"] = image_height

        progress("load_prompt", 20, "加载分割提示词")
        prompt = _load_prompt(target_text)
        client = _make_client(
            model_json_path=model_json_path,
            api_key=api_key,
            api_provider=api_provider,
            api_base_url=api_base_url,
            api_model=api_model,
            api_workspace_id=api_workspace_id,
            api_timeout=api_timeout,
        )
        total_attempts = max_retry + 1

        for attempt in range(1, total_attempts + 1):
            if total_attempts == 1:
                attempt_text = "建立 API 连接并发送图片，等待模型返回 bbox"
            elif attempt == 1:
                attempt_text = (
                    f"建立 API 连接并发送图片，等待模型返回 bbox"
                    f"（首次调用，最多重试 {max_retry} 次）"
                )
            else:
                attempt_text = (
                    f"建立 API 连接并发送图片，等待模型返回 bbox"
                    f"（第 {attempt - 1} 次重试，"
                    f"剩余 {total_attempts - attempt} 次）"
                )
            progress(
                "call_model",
                30,
                attempt_text,
            )
            attempt_record: dict[str, Any] = {
                "attempt": attempt,
                "client": client.__class__.__name__,
                "started_at": _utc_now(),
            }
            try:
                model_data = client.segment(
                    image_path=image_path,
                    target_text=target_text,
                    prompt=prompt,
                    image_width=image_width,
                    image_height=image_height,
                )
                write_json(paths["segments_model"], model_data)

                progress("validate", 60, "校验和修正 bbox")
                final_data, validation_report = validate_segments(
                    model_data,
                    target_text=target_text,
                    image_width=image_width,
                    image_height=image_height,
                )

                attempt_record["validation"] = validation_report
                attempt_record["finished_at"] = _utc_now()
                run_report["attempts"].append(attempt_record)

                if validation_report["warnings"]:
                    run_report["warnings"].extend(validation_report["warnings"])

                if not validation_report["errors"]:
                    break

                # A JSON file is deterministic; retrying it would produce the same result.
                if model_json_path:
                    break

            except Exception as exc:
                attempt_record["error"] = f"{type(exc).__name__}: {exc}"
                attempt_record["finished_at"] = _utc_now()
                run_report["attempts"].append(attempt_record)
                if model_json_path:
                    raise
                if attempt == total_attempts:
                    raise

        if model_data is None:
            raise RuntimeError("No model segmentation data was produced")
        if final_data is None or validation_report is None:
            raise RuntimeError("Validation did not produce final segment data")
        if validation_report["errors"]:
            raise ValueError(
                "Validation failed: " + "; ".join(validation_report["errors"])
            )

        progress("preview", 75, "生成画框预览图")
        preview_path = draw_preview(
            image_path=image_path,
            segments=final_data["segments"],
            output_path=str(paths["preview"]),
        )

        progress("crop", 85, "从原图直接裁剪字符")
        crop_info = crop_original_chars(
            image_path=image_path,
            segments_data=final_data,
            output_dir=str(paths["root"]),
        )
        final_data["crop_output_dir"] = crop_info["output_dir"]
        final_data["crop_file_count"] = crop_info["file_count"]
        progress("write", 95, "写入最终 JSON 和运行报告")
        write_json(paths["segments_final"], final_data)

        run_report.update(
            {
                "status": "success",
                "finished_at": _utc_now(),
                "validation": validation_report,
                "crop_info": crop_info,
                "outputs": {
                    "chars_original_crop": crop_info["output_dir"],
                    "segments_model": str(paths["segments_model"]),
                    "segments_final": str(paths["segments_final"]),
                    "preview_boxes": preview_path,
                    "run_report": str(paths["run_report"]),
                },
            }
        )
        write_json(paths["run_report"], run_report)
        progress("done", 100, "完成")
        return run_report

    except Exception as exc:
        progress("failed", 100, "失败")
        run_report.update(
            {
                "status": "failed",
                "finished_at": _utc_now(),
            }
        )
        run_report["errors"].append(f"{type(exc).__name__}: {exc}")
        if validation_report:
            run_report["validation"] = validation_report
        write_json(paths["run_report"], run_report)
        return run_report
