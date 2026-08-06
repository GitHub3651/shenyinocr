"""OpenCV image IO helpers that support Windows non-ASCII paths."""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np


def imread_unchanged(path: str | Path) -> np.ndarray:
    """Read an image with OpenCV IMREAD_UNCHANGED, including Chinese paths."""
    image_path = Path(path)
    if not image_path.exists():
        raise FileNotFoundError(f"Image file does not exist: {image_path}")
    data = np.fromfile(str(image_path), dtype=np.uint8)
    if data.size == 0:
        raise ValueError(f"Image file is empty: {image_path}")
    image = cv2.imdecode(data, cv2.IMREAD_UNCHANGED)
    if image is None:
        raise ValueError(
            f"OpenCV could not decode image file: {image_path}. "
            "Check that the file is a valid bmp/png/jpg/tif image."
        )
    return image


def imwrite_image(path: str | Path, image: np.ndarray) -> None:
    """Write an image with OpenCV encoder, including Chinese output paths."""
    output_path = Path(path)
    suffix = output_path.suffix or ".png"
    ok, encoded = cv2.imencode(suffix, image)
    if not ok:
        raise OSError(f"OpenCV failed to encode image as {suffix}: {output_path}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    encoded.tofile(str(output_path))
