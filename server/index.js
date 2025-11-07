// index.js
import express from "express";
import { initializeApp } from "firebase/app";
import { getDatabase, ref, set } from "firebase/database";
import { spawnWorker, sendFrameToWorker } from "./python_worker.js";
import { db } from "./firebase.js";

// YOLO Python worker 
const worker = await spawnWorker({
    conf: "0.3",  // confidence threshold for detections
    device: "mps" // Depends on machine that is running the YOLOv5 backend ("cpu", "0", "mps")
});

const app = express();
const PORT = process.env.PORT || 8080;

// Health
app.get("/", (_req, res) => res.json({ ok: true }));

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

            // Run YOLO on this single image
            const count = await sendFrameToWorker(worker, jpeg);

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
