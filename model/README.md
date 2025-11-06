# Model: Person Detection

This module adds a simple, ready-to-run person detector for videos and webcams using Ultralytics YOLOv8.

- Framework: Ultralytics YOLOv8 (pretrained on COCO)
- Classes: Filters to the `person` class only
- Outputs: Annotated MP4 video and optional JSON summary of per-frame counts

## Folder layout

- `detector.py` — `PersonDetector` class for image/video detection
- `cli.py` — Command-line interface to run detection easily
- `requirements.txt` — Python dependencies
- `runs/` — Output folder (created at runtime) for annotated videos

## Quickstart

1. Create and activate a Python environment (recommended) and install deps:

```bash
python3 -m venv .venv
source .venv/bin/activate  # macOS/Linux
python -m pip install -U pip
pip install -r model/requirements.txt
```

Note: The first run will download the YOLOv8 weights automatically.

2. Run on a video file:

```bash
python -m model.cli --source /path/to/video.mp4 --conf 0.3
```

The annotated result will be saved under `model/runs/person_<timestamp>.mp4`.

3. Run on a webcam (index 0):

```bash
python -m model.cli --source 0 --show
```

Press `q` to stop the live preview.

4. Optional: Save per-frame counts to JSON:

```bash
python -m model.cli --source /path/to/video.mp4 --save-json model/runs/summary.json
```

## Programmatic usage

```python
import cv2
from model import PersonDetector

img = cv2.imread("frame.jpg")
detector = PersonDetector(conf=0.35)
annotated, detections = detector.detect_on_image(img)
print(f"found {len(detections)} person(s)")
cv2.imwrite("annotated.jpg", annotated)
```

## Notes

- If you're on Apple Silicon, YOLOv8 can use the Metal backend by setting `--device mps`.
- If you have a CUDA GPU, pass `--device 0` (or another GPU index) to use it.
- Output FPS will match the input FPS if available; otherwise defaults to 30.
