from __future__ import annotations

import os
import time
from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional, Tuple, Union

import cv2
import numpy as np
from ultralytics import YOLO


@dataclass
class Detection:
    """A single person detection."""
    xyxy: Tuple[int, int, int, int]
    confidence: float
    class_id: int
    class_name: str


class PersonDetector:
    """Detects people in images and videos using YOLOv8.

    - Uses the pretrained COCO weights (person class id 0)
    - Provides helper methods for images and videos
    """

    def __init__(
        self,
        model_name: str = "yolov8n.pt",
        conf: float = 0.3,
        device: Optional[Union[str, int]] = None,
    ) -> None:
        """Create a detector.

        Args:
            model_name: YOLOv8 weights name or path (e.g., 'yolov8n.pt').
            conf: Minimum confidence threshold.
            device: Device to run on (e.g., 'cpu', 'mps', 0 for CUDA GPU id). None lets YOLO choose.
        """
        self.model = YOLO(model_name)
        self.conf = conf
        self.device = device
        # COCO person class is 0
        self.person_class_id = 0

    def detect_on_image(self, image_bgr: np.ndarray) -> Tuple[np.ndarray, List[Detection]]:
        """Run person detection on a single BGR image (OpenCV format).

        Returns an annotated image and a list of detections.
        """
        results = self.model.predict(
            source=image_bgr,
            conf=self.conf,
            classes=[self.person_class_id],
            device=self.device,
            verbose=False,
        )
        # YOLO returns a list of results; for a single image, take the first
        res = results[0]
        annotated = res.plot()  # draws boxes

        detections: List[Detection] = []
        names = res.names
        if res.boxes is not None and len(res.boxes) > 0:
            xyxy = res.boxes.xyxy.cpu().numpy().astype(int)
            confs = res.boxes.conf.cpu().numpy()
            clss = res.boxes.cls.cpu().numpy().astype(int)
            for box, conf, cls_id in zip(xyxy, confs, clss):
                detections.append(
                    Detection(
                        xyxy=(int(box[0]), int(box[1]), int(box[2]), int(box[3])),
                        confidence=float(conf),
                        class_id=int(cls_id),
                        class_name=names.get(int(cls_id), str(int(cls_id))),
                    )
                )

        return annotated, detections

    def detect_on_video(
        self,
        source: Union[str, int],
        out_path: str,
        show: bool = False,
        save_json: Optional[str] = None,
        progress_every: int = 30,
    ) -> Dict[str, Any]:
        """Run person detection on a video file or webcam.

        Args:
            source: Path to a video file or integer index for webcam (e.g., 0).
            out_path: Path to save the annotated video (e.g., 'runs/person.mp4').
            show: If True, opens a preview window while processing (press 'q' to quit).
            save_json: If set, saves per-frame counts and basic metadata to this JSON path.
            progress_every: Frame interval for printing progress.

        Returns:
            Summary dict with keys: frames, duration_sec, fps_out, output, person_counts
        """
        # Attempt to read FPS using OpenCV for accurate output timing
        input_fps = 30.0
        frame_count_est = None
        if isinstance(source, (str, int)):
            cap_meta = cv2.VideoCapture(source)
            if cap_meta.isOpened():
                fps_val = cap_meta.get(cv2.CAP_PROP_FPS)
                if fps_val and fps_val > 0:
                    input_fps = float(fps_val)
                count_val = cap_meta.get(cv2.CAP_PROP_FRAME_COUNT)
                if count_val and count_val > 0:
                    frame_count_est = int(count_val)
            cap_meta.release()

        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)

        writer = None
        frame_index = 0
        person_counts: List[int] = []
        t0 = time.time()

        try:
            for res in self.model.predict(
                source=source,
                stream=True,
                conf=self.conf,
                classes=[self.person_class_id],
                device=self.device,
                verbose=False,
            ):
                frame = res.plot()  # annotated BGR frame
                h, w = frame.shape[:2]
                if writer is None:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    writer = cv2.VideoWriter(out_path, fourcc, input_fps, (w, h))

                # Count people in this frame
                count = 0
                if res.boxes is not None and len(res.boxes) > 0:
                    clss = res.boxes.cls.cpu().numpy().astype(int)
                    # since we already filtered classes, this is effectively len
                    count = int((clss == self.person_class_id).sum())
                person_counts.append(count)

                writer.write(frame)

                if show:
                    cv2.imshow("Person Detection", frame)
                    # Press 'q' to quit early
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

                frame_index += 1
                if progress_every and frame_index % progress_every == 0:
                    if frame_count_est:
                        print(f"Processed {frame_index}/{frame_count_est} frames…")
                    else:
                        print(f"Processed {frame_index} frames…")
        finally:
            if writer is not None:
                writer.release()
            if show:
                try:
                    cv2.destroyAllWindows()
                except Exception:
                    pass

        duration = time.time() - t0
        summary: Dict[str, Any] = {
            "frames": frame_index,
            "duration_sec": round(duration, 3),
            "fps_out": input_fps,
            "output": os.path.abspath(out_path),
            "person_counts": person_counts,
        }

        if save_json:
            try:
                import json

                with open(save_json, "w", encoding="utf-8") as f:
                    json.dump(
                        {
                            **summary,
                            "source": str(source),
                            "conf": self.conf,
                            "model": str(self.model.model if hasattr(self.model, 'model') else 'yolov8'),
                        },
                        f,
                        indent=2,
                    )
                summary["json"] = os.path.abspath(save_json)
            except Exception as e:
                print(f"Warning: failed to save JSON: {e}")

        return summary
