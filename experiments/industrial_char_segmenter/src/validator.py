"""Validation and light normalization for VLM character bboxes."""

from __future__ import annotations

from copy import deepcopy
from statistics import median
from typing import Any

from .geometry_utils import (
    bbox_height,
    bbox_width,
    clamp_bbox,
    has_large_overlap,
    overlap_ratio_over_smaller,
)


def _new_report(expected_count: int, image_width: int, image_height: int) -> dict:
    return {
        "ok": False,
        "errors": [],
        "warnings": [],
        "autocorrections": [],
        "expected_count": expected_count,
        "valid_count": 0,
        "image_width": image_width,
        "image_height": image_height,
    }


def _add_error(report: dict, message: str) -> None:
    report["errors"].append(message)


def _add_warning(report: dict, message: str) -> None:
    report["warnings"].append(message)


def _add_fix(report: dict, message: str) -> None:
    report["autocorrections"].append(message)


def _is_numeric_bbox(raw_bbox: Any) -> bool:
    if not isinstance(raw_bbox, (list, tuple)) or len(raw_bbox) != 4:
        return False
    for value in raw_bbox:
        if isinstance(value, bool):
            return False
        try:
            float(value)
        except (TypeError, ValueError):
            return False
    return True


def _parse_optional_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _check_reading_order(segments: list[dict], report: dict) -> None:
    if len(segments) <= 1:
        return

    heights = [max(1, bbox_height(segment["bbox"])) for segment in segments]
    row_threshold = max(4.0, float(median(heights)) * 0.70)

    for idx, (prev, current) in enumerate(zip(segments, segments[1:]), start=2):
        prev_box = prev["bbox"]
        current_box = current["bbox"]
        prev_center_y = (prev_box[1] + prev_box[3]) / 2.0
        current_center_y = (current_box[1] + current_box[3]) / 2.0

        if current_center_y < prev_center_y - row_threshold:
            _add_warning(
                report,
                f"Segment {idx} appears above previous segment; reading order may be wrong",
            )
        elif abs(current_center_y - prev_center_y) <= row_threshold:
            if current_box[0] + 2 < prev_box[0]:
                _add_warning(
                    report,
                    f"Segment {idx} appears left of previous segment in the same row",
                )


def validate_segments(
    data: dict,
    target_text: str,
    image_width: int,
    image_height: int,
) -> tuple[dict, dict]:
    """Validate model segments and return normalized data plus a report.

    Light issues such as float coordinates, minor out-of-bounds coordinates,
    missing index, or wrong char labels are corrected in the returned data.
    Severe issues such as missing segments, count mismatch, invalid bboxes,
    and zero-area crops are reported as errors.
    """
    expected_count = len(target_text)
    report = _new_report(expected_count, image_width, image_height)

    valid_data = {
        "image_width": image_width,
        "image_height": image_height,
        "target_text": target_text,
        "char_count": expected_count,
        "segments": [],
        "notes": "",
    }

    if not isinstance(data, dict):
        _add_error(report, "Model data must be a JSON object")
        return valid_data, report

    valid_data["notes"] = str(data.get("notes", ""))
    raw_segments = data.get("segments")
    if raw_segments is None:
        _add_error(report, "Missing required field: segments")
        return valid_data, report
    if not isinstance(raw_segments, list):
        _add_error(report, "Field segments must be a list")
        return valid_data, report

    if len(raw_segments) != expected_count:
        _add_error(
            report,
            f"Segment count mismatch: got {len(raw_segments)}, expected {expected_count}",
        )

    source_width = _parse_optional_int(data.get("image_width"))
    source_height = _parse_optional_int(data.get("image_height"))
    if data.get("image_width") is not None and source_width is None:
        _add_warning(report, f"Model image_width is not an integer: {data.get('image_width')!r}")
    if data.get("image_height") is not None and source_height is None:
        _add_warning(report, f"Model image_height is not an integer: {data.get('image_height')!r}")
    if source_width is not None and source_width != image_width:
        _add_warning(
            report,
            f"Model image_width={source_width} differs from actual width={image_width}",
        )
    if source_height is not None and source_height != image_height:
        _add_warning(
            report,
            f"Model image_height={source_height} differs from actual height={image_height}",
        )

    normalized_segments: list[dict] = []
    for zero_based, raw_segment in enumerate(raw_segments):
        position = zero_based + 1
        expected_char = target_text[zero_based] if zero_based < expected_count else None

        if not isinstance(raw_segment, dict):
            _add_error(report, f"Segment {position} must be a JSON object")
            continue

        normalized = deepcopy(raw_segment)
        raw_index = raw_segment.get("index")
        try:
            parsed_index = int(raw_index)
        except (TypeError, ValueError):
            parsed_index = position
            _add_fix(report, f"Segment {position}: missing/invalid index corrected")

        if parsed_index != position:
            _add_fix(
                report,
                f"Segment {position}: index corrected from {parsed_index} to {position}",
            )
        normalized["index"] = position

        if expected_char is not None:
            raw_char = raw_segment.get("char")
            if raw_char != expected_char:
                _add_fix(
                    report,
                    f"Segment {position}: char corrected from {raw_char!r} to {expected_char!r}",
                )
            normalized["char"] = expected_char

        raw_bbox = raw_segment.get("bbox")
        if not _is_numeric_bbox(raw_bbox):
            _add_error(
                report,
                f"Segment {position}: bbox must be a numeric array of length 4",
            )
            continue

        rounded_bbox = [int(round(float(v))) for v in raw_bbox]
        if rounded_bbox != list(raw_bbox):
            _add_fix(
                report,
                f"Segment {position}: float/string bbox coordinates rounded to integers",
            )

        clamped_bbox = clamp_bbox(rounded_bbox, image_width, image_height)
        if clamped_bbox != rounded_bbox:
            _add_fix(
                report,
                f"Segment {position}: bbox clamped from {rounded_bbox} to {clamped_bbox}",
            )

        x1, y1, x2, y2 = clamped_bbox
        if x2 <= x1:
            _add_error(
                report,
                f"Segment {position}: invalid bbox x2 <= x1 after normalization: {clamped_bbox}",
            )
            continue
        if y2 <= y1:
            _add_error(
                report,
                f"Segment {position}: invalid bbox y2 <= y1 after normalization: {clamped_bbox}",
            )
            continue

        box_w = bbox_width(clamped_bbox)
        box_h = bbox_height(clamped_bbox)
        if box_w < 2 or box_h < 2:
            _add_error(
                report,
                f"Segment {position}: bbox too small to crop reliably: {clamped_bbox}",
            )
            continue
        if box_w < 4 or box_h < 4:
            _add_warning(
                report,
                f"Segment {position}: bbox is very small: width={box_w}, height={box_h}",
            )

        if x1 == 0 or y1 == 0 or x2 == image_width or y2 == image_height:
            _add_warning(
                report,
                f"Segment {position}: bbox touches image boundary; check cut-edge risk",
            )

        normalized["bbox"] = clamped_bbox
        normalized_segments.append(normalized)

    for idx, (prev, current) in enumerate(
        zip(normalized_segments, normalized_segments[1:]),
        start=2,
    ):
        prev_box = prev["bbox"]
        current_box = current["bbox"]
        if has_large_overlap(prev_box, current_box):
            ratio = overlap_ratio_over_smaller(prev_box, current_box)
            _add_warning(
                report,
                f"Segments {idx - 1} and {idx} overlap heavily "
                f"(intersection/smaller={ratio:.3f})",
            )

    _check_reading_order(normalized_segments, report)

    valid_data["segments"] = normalized_segments
    report["valid_count"] = len(normalized_segments)
    report["ok"] = not report["errors"]
    return valid_data, report
