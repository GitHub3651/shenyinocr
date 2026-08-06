"""Direct original-image character cropping.

This module is the critical safety boundary: output character images must come
only from image[y1:y2, x1:x2] on the original image loaded with IMREAD_UNCHANGED.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from .image_io import imread_unchanged, imwrite_image
from .json_utils import ensure_dir


def _safe_char_token(char: str) -> str:
    """Return a filesystem-safe token for one target character."""
    if len(char) == 1 and char.isascii() and char.isalnum():
        return char
    if not char:
        return "empty"
    return "_".join(f"u{ord(part):04X}" for part in char)


def crop_original_chars(
    image_path: str,
    segments_data: dict,
    output_dir: str,
) -> dict:
    """Crop and save each character directly from the original image.

    No grayscale conversion, thresholding, denoising, resizing, alpha change,
    rotation, normalization, morphology, enhancement, or redraw is applied.
    """
    image = imread_unchanged(image_path)

    output_root = ensure_dir(output_dir)
    chars_dir = ensure_dir(output_root / "chars_original_crop")

    segments = segments_data.get("segments")
    if not isinstance(segments, list):
        raise ValueError("segments_data must contain a list field named 'segments'")

    files: list[dict[str, Any]] = []
    for segment in segments:
        index = int(segment["index"])
        char = str(segment["char"])
        x1, y1, x2, y2 = [int(v) for v in segment["bbox"]]

        # Hard requirement: direct crop from the original image array.
        crop = image[y1:y2, x1:x2]
        if crop.size == 0:
            raise ValueError(f"Segment {index}: crop is empty for bbox {[x1, y1, x2, y2]}")

        filename = f"{index:03d}_{_safe_char_token(char)}.png"
        output_path = chars_dir / filename
        imwrite_image(output_path, crop)

        segment["filename"] = filename
        segment["relative_path"] = str(Path("chars_original_crop") / filename)
        files.append(
            {
                "index": index,
                "char": char,
                "bbox": [x1, y1, x2, y2],
                "filename": filename,
                "path": str(output_path),
            }
        )

    return {
        "output_dir": str(chars_dir),
        "file_count": len(files),
        "files": files,
    }
