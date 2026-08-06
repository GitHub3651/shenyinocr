"""Geometry helpers for bbox validation and reading-order checks."""

from __future__ import annotations

from statistics import median
from typing import Iterable, Sequence

BBox = list[int]


def _to_number(value: object) -> float:
    if isinstance(value, bool):
        raise ValueError("bool is not a valid bbox coordinate")
    return float(value)  # type: ignore[arg-type]


def clamp_bbox(bbox: Sequence[object], width: int, height: int) -> BBox:
    """Round and clamp [x1, y1, x2, y2] to image bounds."""
    if len(bbox) != 4:
        raise ValueError("bbox must contain exactly four coordinates")
    x1, y1, x2, y2 = [int(round(_to_number(v))) for v in bbox]
    return [
        max(0, min(width, x1)),
        max(0, min(height, y1)),
        max(0, min(width, x2)),
        max(0, min(height, y2)),
    ]


def bbox_width(bbox: Sequence[int]) -> int:
    return int(bbox[2]) - int(bbox[0])


def bbox_height(bbox: Sequence[int]) -> int:
    return int(bbox[3]) - int(bbox[1])


def bbox_area(bbox: Sequence[int]) -> int:
    return max(0, bbox_width(bbox)) * max(0, bbox_height(bbox))


def bbox_iou(b1: Sequence[int], b2: Sequence[int]) -> float:
    """Return intersection-over-union for two bboxes."""
    ix1 = max(int(b1[0]), int(b2[0]))
    iy1 = max(int(b1[1]), int(b2[1]))
    ix2 = min(int(b1[2]), int(b2[2]))
    iy2 = min(int(b1[3]), int(b2[3]))
    inter = bbox_area([ix1, iy1, ix2, iy2])
    union = bbox_area(b1) + bbox_area(b2) - inter
    if union <= 0:
        return 0.0
    return inter / union


def overlap_ratio_over_smaller(b1: Sequence[int], b2: Sequence[int]) -> float:
    """Return intersection divided by the smaller non-zero bbox area."""
    ix1 = max(int(b1[0]), int(b2[0]))
    iy1 = max(int(b1[1]), int(b2[1]))
    ix2 = min(int(b1[2]), int(b2[2]))
    iy2 = min(int(b1[3]), int(b2[3]))
    inter = bbox_area([ix1, iy1, ix2, iy2])
    smaller = min(bbox_area(b1), bbox_area(b2))
    if smaller <= 0:
        return 0.0
    return inter / smaller


def has_large_overlap(
    b1: Sequence[int],
    b2: Sequence[int],
    iou_threshold: float = 0.35,
    smaller_area_threshold: float = 0.55,
) -> bool:
    """Detect severe overlap between neighboring character boxes."""
    return (
        bbox_iou(b1, b2) >= iou_threshold
        or overlap_ratio_over_smaller(b1, b2) >= smaller_area_threshold
    )


def _center_y(segment: dict) -> float:
    bbox = segment["bbox"]
    return (bbox[1] + bbox[3]) / 2.0


def _center_x(segment: dict) -> float:
    bbox = segment["bbox"]
    return (bbox[0] + bbox[2]) / 2.0


def sort_segments_reading_order(segments: Iterable[dict]) -> list[dict]:
    """Sort bboxes top-to-bottom, and left-to-right within each text row."""
    items = list(segments)
    if len(items) <= 1:
        return items

    heights = [max(1, bbox_height(item["bbox"])) for item in items]
    row_threshold = max(3.0, float(median(heights)) * 0.65)

    rows: list[dict] = []
    for segment in sorted(items, key=lambda s: (_center_y(s), _center_x(s))):
        y = _center_y(segment)
        best_row = None
        best_distance = None
        for row in rows:
            distance = abs(y - row["center_y"])
            if distance <= row_threshold and (
                best_distance is None or distance < best_distance
            ):
                best_row = row
                best_distance = distance
        if best_row is None:
            rows.append({"center_y": y, "segments": [segment]})
        else:
            best_row["segments"].append(segment)
            count = len(best_row["segments"])
            best_row["center_y"] = (
                best_row["center_y"] * (count - 1) + y
            ) / count

    ordered: list[dict] = []
    for row in sorted(rows, key=lambda r: r["center_y"]):
        ordered.extend(sorted(row["segments"], key=lambda s: (s["bbox"][0], _center_x(s))))
    return ordered
