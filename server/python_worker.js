// Spawns the Python YOLO worker and provides a simple IPC
import { spawn } from "node:child_process";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));

export async function spawnWorker(opts = {}) {
    // Resolve path to worker.py so it runs regardless of CWD.
    const workerPath = resolve(__dirname, "py", "worker.py");
    const args = ["-u", workerPath];
    if (opts.conf) args.push("--conf", String(opts.conf));
    if (opts.device) args.push("--device", String(opts.device));

    // Spawn python process with pipes:
    const child = spawn("python3", args, { stdio: ["pipe", "pipe", "inherit"] });
    child.on("spawn", () => console.log("[worker] python started"));
    child.on("exit", (code) => console.log("[worker] python exited", code));

    // Response demux
    let buf = "";
    const pending = new Map();
    let nextId = 1;

    child.stdout.on("data", (chunk) => {
        buf += chunk.toString();
        let nl;
        // Process complete lines, keep remainder in buf
        while ((nl = buf.indexOf("\n")) >= 0) {
            const line = buf.slice(0, nl);
            buf = buf.slice(nl + 1);
            if (!line.trim()) continue;
            try {
                const msg = JSON.parse(line);
                const p = pending.get(msg.id);
                if (p) {
                    pending.delete(msg.id);
                    if (msg.error) p.reject(new Error(msg.error));
                    else p.resolve(msg);
                }
            } catch (e) {
                // If worker prints anything non-JSON, it lands here
                console.error("[worker] bad json:", e);
            }
        }
    });

    // Send a message to worker
    function send(type, payload) {
        const id = nextId++;
        const packet = { id, type, ...payload };
        child.stdin.write(JSON.stringify(packet) + "\n");
        return new Promise((resolve, reject) => {
            pending.set(id, { resolve, reject });
            // Timeout to avoid hanging forever if worker stalls
            setTimeout(() => {
                if (pending.has(id)) {
                    pending.delete(id);
                    reject(new Error("worker timeout"));
                }
            }, 15000);
        });
    }

    // Attach the send function on the child handle
    child._send = send;
    return child;
}

// Helper for image inference:
export function sendFrameToWorker(child, jpegBuffer) {
    const base64 = jpegBuffer.toString("base64");
    return child._send("frame", { image_b64: base64 })
        .then((resp) => ({
            count: resp.count,
            annotated_b64: resp.annotated_b64 || null
        }));
}
