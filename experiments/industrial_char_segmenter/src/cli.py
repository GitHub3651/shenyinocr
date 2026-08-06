"""Command-line entry point for the industrial character segmenter."""

from __future__ import annotations

import argparse
import json
import sys

from .provider_presets import DEFAULT_PROVIDER_ID, provider_ids


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Segment industrial characters by bbox and save each character as "
            "a direct crop from the original image."
        )
    )
    parser.add_argument("--image", required=True, help="Input industrial image path")
    parser.add_argument("--target", required=True, help="Known target string")
    parser.add_argument("--output", required=True, help="Output run directory")
    parser.add_argument(
        "--model-json",
        default=None,
        help="Optional test bbox JSON file. If provided, real API is not called.",
    )
    parser.add_argument(
        "--api-key",
        default=None,
        help="Vision API key. Can also be set with VLM_API_KEY.",
    )
    parser.add_argument(
        "--provider",
        choices=provider_ids(),
        default=DEFAULT_PROVIDER_ID,
        help=f"Model provider; default: {DEFAULT_PROVIDER_ID}",
    )
    parser.add_argument(
        "--api-base-url",
        default=None,
        help=(
            "OpenAI-compatible API base URL, for example https://api.openai.com/v1. "
            "Can also be set with VLM_API_BASE_URL."
        ),
    )
    parser.add_argument(
        "--api-model",
        default=None,
        help="Vision model name. Can also be set with VLM_API_MODEL.",
    )
    parser.add_argument(
        "--api-workspace-id",
        default=None,
        help="Optional Alibaba Model Studio workspace ID. Can also be set with VLM_API_WORKSPACE_ID.",
    )
    parser.add_argument(
        "--api-timeout",
        type=int,
        default=60,
        help="Vision API timeout in seconds; default: 60",
    )
    parser.add_argument(
        "--max-retry",
        type=int,
        default=2,
        help="Maximum retry count for real VLM calls; default: 2",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        from .agent import run_char_segmentation_agent
    except ModuleNotFoundError as exc:
        if exc.name == "cv2":
            print(
                "Missing dependency: cv2. Install dependencies with "
                "`pip install -r requirements.txt` from the project root.",
                file=sys.stderr,
            )
            return 2
        if exc.name == "requests":
            print(
                "Missing dependency: requests. Install dependencies with "
                "`pip install -r requirements.txt` from the project root.",
                file=sys.stderr,
            )
            return 2
        raise

    report = run_char_segmentation_agent(
        image_path=args.image,
        target_text=args.target,
        output_dir=args.output,
        model_json_path=args.model_json,
        max_retry=args.max_retry,
        api_key=args.api_key,
        api_provider=args.provider,
        api_base_url=args.api_base_url,
        api_model=args.api_model,
        api_workspace_id=args.api_workspace_id,
        api_timeout=args.api_timeout,
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report.get("status") == "success" else 1


if __name__ == "__main__":
    raise SystemExit(main())
