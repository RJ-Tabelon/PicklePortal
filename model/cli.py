from __future__ import annotations

import argparse
import os
import sys
from datetime import datetime

from .detector import PersonDetector


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Detect people in a video or webcam stream and save an annotated video.",
    )
    parser.add_argument(
        "--source",
        required=True,
        help="Path to a video file or webcam index (e.g., 0)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output video path (default: model/runs/person_<timestamp>.mp4)",
    )
    parser.add_argument(
        "--conf",
        type=float,
        default=0.3,
        help="Confidence threshold (default: 0.3)",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Preview the annotated frames while processing (press 'q' to quit)",
    )
    parser.add_argument(
        "--save-json",
        default=None,
        help="Optional JSON path to save per-frame counts and summary",
    )
    parser.add_argument(
        "--device",
        default=None,
        help="Device to run on: 'cpu', 'mps', CUDA id like 0, etc. (default: auto)",
    )

    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)

    # Parse source (support integer webcam indices)
    source: str | int
    if args.source.isdigit():
        source = int(args.source)
    else:
        source = args.source
        if not os.path.exists(source):
            print(f"Error: source path does not exist: {source}")
            return 2

    runs_dir = os.path.join("model", "runs")
    os.makedirs(runs_dir, exist_ok=True)
    if args.output:
        out_path = args.output
    else:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = os.path.join(runs_dir, f"person_{timestamp}.mp4")

    detector = PersonDetector(conf=args.conf, device=args.device)
    print(f"Running detection on {source} -> {out_path} (conf={args.conf})")
    summary = detector.detect_on_video(
        source=source,
        out_path=out_path,
        show=args.show,
        save_json=args.save_json,
    )

    print("\nSummary:")
    for k, v in summary.items():
        if k == "person_counts":
            print(f"  {k}: [len={len(v)}]")
        else:
            print(f"  {k}: {v}")

    print("\nDone.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
