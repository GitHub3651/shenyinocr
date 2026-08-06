"""Create a small mock example and run the full segmentation pipeline."""

from __future__ import annotations

import sys
from pathlib import Path

import cv2
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from src.agent import run_char_segmentation_agent  # noqa: E402
from src.json_utils import ensure_dir, write_json  # noqa: E402


def build_demo_files() -> tuple[Path, Path, str]:
    target_text = "ABC123"
    input_dir = ensure_dir(PROJECT_ROOT / "data" / "input")
    image_path = input_dir / "example.bmp"
    model_json_path = input_dir / "example_segments.json"

    image = np.full((80, 260, 3), 255, dtype=np.uint8)
    font = cv2.FONT_HERSHEY_SIMPLEX
    x = 18
    y = 52
    segments = []

    for index, char in enumerate(target_text, start=1):
        (text_w, text_h), baseline = cv2.getTextSize(char, font, 1.2, 2)
        cv2.putText(image, char, (x, y), font, 1.2, (0, 0, 0), 2, cv2.LINE_AA)
        bbox = [x - 2, y - text_h - 2, x + text_w + 2, y + baseline + 2]
        segments.append(
            {
                "index": index,
                "char": char,
                "bbox": bbox,
                "confidence": 1.0,
            }
        )
        x += text_w + 12

    cv2.imwrite(str(image_path), image)
    write_json(
        model_json_path,
        {
            "image_width": int(image.shape[1]),
            "image_height": int(image.shape[0]),
            "target_text": target_text,
            "char_count": len(target_text),
            "segments": segments,
            "notes": "Synthetic mock example.",
        },
    )
    return image_path, model_json_path, target_text


def main() -> int:
    image_path, model_json_path, target_text = build_demo_files()
    output_dir = PROJECT_ROOT / "data" / "output" / "example_run"
    report = run_char_segmentation_agent(
        image_path=str(image_path),
        target_text=target_text,
        output_dir=str(output_dir),
        model_json_path=str(model_json_path),
    )
    print(f"status: {report['status']}")
    print(f"output: {output_dir}")
    return 0 if report["status"] == "success" else 1


if __name__ == "__main__":
    raise SystemExit(main())
