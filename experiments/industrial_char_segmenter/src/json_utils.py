"""Small JSON and directory helpers used by the segmentation pipeline."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def ensure_dir(path: str | Path) -> Path:
    """Create a directory if it does not exist and return it as a Path."""
    directory = Path(path)
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def read_json(path: str | Path) -> Any:
    """Read a UTF-8 JSON file."""
    json_path = Path(path)
    with json_path.open("r", encoding="utf-8") as file:
        return json.load(file)


def write_json(path: str | Path, data: Any) -> Path:
    """Write UTF-8 JSON with stable indentation and readable non-ASCII text."""
    json_path = Path(path)
    if json_path.parent:
        ensure_dir(json_path.parent)
    with json_path.open("w", encoding="utf-8") as file:
        json.dump(data, file, ensure_ascii=False, indent=2)
        file.write("\n")
    return json_path
