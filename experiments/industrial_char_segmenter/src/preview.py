"""Preview image generation with visual bbox overlays."""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np

from .image_io import imread_unchanged, imwrite_image
from .json_utils import ensure_dir


def _to_uint8_for_preview(image: np.ndarray) -> np.ndarray:
    """Convert arbitrary image depth to uint8 for preview drawing only."""
    if image.dtype == np.uint8:
        return image.copy()
    normalized = cv2.normalize(image, None, 0, 255, cv2.NORM_MINMAX)
    return normalized.astype(np.uint8)


def _preview_canvas(image: np.ndarray) -> np.ndarray:
    canvas = _to_uint8_for_preview(image)
    if canvas.ndim == 2:
        return cv2.cvtColor(canvas, cv2.COLOR_GRAY2BGR)
    if canvas.ndim == 3 and canvas.shape[2] == 4:
        return cv2.cvtColor(canvas, cv2.COLOR_BGRA2BGR)
    if canvas.ndim == 3 and canvas.shape[2] == 3:
        return canvas.copy()
    raise ValueError(f"Unsupported image shape for preview: {canvas.shape}")


def _ascii_label(text: str) -> str:
    return "".join(ch if 32 <= ord(ch) <= 126 else "?" for ch in text)


def draw_preview(image_path: str, segments: list[dict] | dict, output_path: str) -> str:
    """Draw red bbox overlays on a copy of the source image."""
    image = imread_unchanged(image_path)

    if isinstance(segments, dict):
        segment_list = segments.get("segments", [])
    else:
        segment_list = segments
    if not isinstance(segment_list, list):
        raise ValueError("segments must be a list or a dict containing a segments list")

    canvas = _preview_canvas(image)
    for segment in segment_list:
        x1, y1, x2, y2 = [int(v) for v in segment["bbox"]]
        label = _ascii_label(f"{segment.get('index', '?')}:{segment.get('char', '?')}")
        cv2.rectangle(canvas, (x1, y1), (x2 - 1, y2 - 1), (0, 0, 255), 2)
        label_y = max(12, y1 - 4)
        cv2.putText(
            canvas,
            label,
            (x1, label_y),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.45,
            (0, 0, 255),
            1,
            cv2.LINE_AA,
        )

    preview_path = Path(output_path)
    ensure_dir(preview_path.parent)
    imwrite_image(preview_path, canvas)
    return str(preview_path)
