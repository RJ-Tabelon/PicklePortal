// index.js
import express from "express";
import { initializeApp } from "firebase/app";
import { getDatabase, ref, set } from "firebase/database";
import { spawnWorker, sendFrameToWorker } from "./python_worker.js";
import { db } from "./firebase.js";
import cors from "cors";

// Simple in-memory cache of last annotated frame per court
const lastImages = new Map(); // courtId -> Buffer (JPEG)


// YOLO Python worker 
const worker = await spawnWorker({
    conf: "0.3",  // confidence threshold for detections
    device: "mps" // Depends on machine that is running the YOLOv5 backend ("cpu", "0", "mps")
});

const app = express();
const PORT = process.env.PORT || 8080;

app.use(cors());

// Health
app.get("/", (_req, res) => res.json({ ok: true }));

// Preview JPEG: returns the latest annotated frame as image/jpeg
app.get("/preview/:courtId.jpg", (req, res) => {
    const courtId = req.params.courtId || "court1";
    const buf = lastImages.get(courtId);
    if (!buf) return res.status(404).send("No image yet");
    res.set("Content-Type", "image/jpeg");
    res.set("Cache-Control", "no-store");
    res.send(buf);
});

// Simple HTML page that auto-refreshes the JPEG every 1s
app.get("/preview/:courtId", (req, res) => {
    const courtId = req.params.courtId || "court1";
    res.set("Cache-Control", "no-store");
    res.send(`<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>Preview - ${courtId}</title>
    <style>
      body { margin: 0; background:#111; color:#eee; font:14px/1.4 system-ui, sans-serif; }
      .wrap { padding:12px; }
      img { max-width: 100%; height: auto; display:block; margin: 0 auto; }
      .row { display:flex; justify-content:space-between; align-items:center; margin-bottom:8px; }
      button { padding:6px 10px; }
    </style>
  </head>
  <body>
    <div class="wrap">
      <div class="row">
        <div>court: <b>${courtId}</b></div>
        <div><button id="pause">pause</button></div>
      </div>
      <img id="img" src="/preview/${courtId}.jpg?ts=${Date.now()}" />
    </div>
    <script>
      const img = document.getElementById('img');
      const btn = document.getElementById('pause');
      let paused = false;
      let timer = setInterval(() => {
        if (!paused) img.src = '/preview/${courtId}.jpg?ts=' + Date.now();
      }, 1000);
      btn.onclick = () => { paused = !paused; btn.textContent = paused ? 'resume' : 'pause'; };
    </script>
  </body>
</html>`);
});


// Snapshot upload: POST a single JPEG body
app.post(
    "/snapshot/:courtId",
    // Raw parser so the request body is delivered as a Buffer
    express.raw({ type: "image/jpeg", limit: "5mb" }),
    async (req, res) => {
        const courtId = req.params.courtId || "court1";
        try {
            const jpeg = req.body; // Buffer
            // Validate we actually received a payload.
            if (!Buffer.isBuffer(jpeg) || jpeg.length === 0) {
                return res.status(400).send("Empty JPEG body");
            }

            const { count, annotated_b64 } = await sendFrameToWorker(worker, jpeg);

            // Cache latest annotated preview in memory for this court
            if (annotated_b64) {
                lastImages.set(courtId, Buffer.from(annotated_b64, "base64"));
            }

            // Update Firebase DB
            await set(ref(db, `${courtId}/occupancyCount`), count);


            // No response body needed, 204 indicates success with no content.
            res.sendStatus(204);
        } catch (e) {
            // Surface an error for server logs and return 500.
            console.error("[snapshot] error:", e);
            res.status(500).send("error");
        }
    }
);

// Start HTTP server
app.listen(PORT, () => {
    console.log(`Server listening on http://0.0.0.0:${PORT}`);
});
