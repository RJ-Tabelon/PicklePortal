# Reads base64 JPEG frames from stdin, runs model.PersonDetector, returns count
# Protocol:
#   IN:  {"id": <int>, "type": "frame", "image_b64": "<...>"}
#   OUT: {"id": <int>, "count": <int>}  OR {"id": <int>, "error": "<msg>"}
import sys, json, base64, os, argparse
import numpy as np
import cv2

# Allow `import model.detector` when server/ is not the project root
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, ROOT)

from model.detector import PersonDetector

def parse_args():
  ap = argparse.ArgumentParser()
  ap.add_argument("--conf", type=float, default=0.3)   # detection confidence threshold
  ap.add_argument("--device", default=None)            # "cpu", "0", "mps"
  return ap.parse_args()

def b64_to_bgr(b64):
  # Decode base64 -> bytes -> np array -> BGR image
  data = base64.b64decode(b64)
  arr = np.frombuffer(data, dtype=np.uint8)
  img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
  if img is None:
    # Return a error if JPEG was corrupt
    raise ValueError("Failed to decode JPEG")
  return img

def main():
  args = parse_args()
  # Construct the detector and keep it in memory for subsequent frames
  det = PersonDetector(conf=args.conf, device=args.device)

  # Read newline-delimited JSON messages from stdin
  for line in sys.stdin:
    line = line.strip()
    if not line:
      continue
    try:
      msg = json.loads(line)
      mid = msg.get("id")
      if msg.get("type") == "frame":
        # Decode the image and run detection
        bgr = b64_to_bgr(msg["image_b64"])
        # Get count from API, ignore everything else
        _, dets = det.detect_on_image(bgr)
        out = {"id": mid, "count": int(len(dets))}
      else:
        # If unknown message type reply with an error to unblock caller
        out = {"id": mid, "error": "unknown message type"}
    except Exception as e:
      # Any exception becomes an error packet with the same id
      out = {"id": msg.get("id", -1), "error": str(e)}

    # Emit exactly one JSON line per request
    sys.stdout.write(json.dumps(out) + "\n")
    sys.stdout.flush()

if __name__ == "__main__":
  main()
