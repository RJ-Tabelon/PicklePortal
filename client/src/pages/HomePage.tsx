import { useState } from "react";
import pickle from "../assets/pickle.png";
import OccupancyMonitor from "../components/OccupancyMonitor";
import type { CourtInfo } from "../components/OccupancyMonitor";
import LiveStream from "../components/LiveStream";
import { ref, onValue, update, db } from "../firebase_example";

let listenerAttached = false;

export default function HomePage() {
  const defaultCourts: CourtInfo[] = [
    { name: "Court 1", players: 0, queue: 0, status: "available", lightColor: "green" },
    { name: "Court 2", players: 0, queue: 0, status: "available", lightColor: "green" },
  ];

  const [courts, setCourts] = useState<CourtInfo[]>(defaultCourts);
  const [totalPlayers, setTotalPlayers] = useState(
    defaultCourts.reduce((s, c) => s + c.players, 0)
  );

  if (!listenerAttached) {
    listenerAttached = true;

    const rootRef = ref(db);
  type Snap = { exists: () => boolean; val: () => Record<string, unknown> | null };
  onValue(rootRef, (snapshot: Snap) => {
      if (!snapshot.exists()){
        return;
      } 

  const data = snapshot.val();
  const obj = (data ?? {}) as Record<string, unknown>;
  const c1 = obj["court1"] as Record<string, unknown> | undefined;
  const c2 = obj["court2"] as Record<string, unknown> | undefined;

  const c1Occupancy = c1 && typeof c1["occupancyCount"] === "number" ? (c1["occupancyCount"] as number) : 0;
  const c1Queue = c1 && typeof c1["queueSize"] === "number" ? (c1["queueSize"] as number) : 0;

  const c2Occupancy = c2 && typeof c2["occupancyCount"] === "number" ? (c2["occupancyCount"] as number) : 0;
  const c2Queue = c2 && typeof c2["queueSize"] === "number" ? (c2["queueSize"] as number) : 0;

      let c1Color = "gray";
      let c1Status = "available";

      if (c1Occupancy == 0) {
        c1Color = "green";
        c1Status = "available";
      } else if (c1Occupancy > 0 && c1Queue === 0) {
        c1Color = "yellow";
        c1Status = "occupied_no_queue";
      } else if (c1Occupancy > 0 && c1Queue > 0) {
        c1Color = "red";
        c1Status = "occupied_with_queue";
      }
      update(ref(db, "court1"), {
        lightColor: c1Color, 
        status: c1Status 
      });

      let c2Color = "gray";
      let c2Status = "available";
      if (c2Occupancy == 0) {
        c2Color = "green";
        c2Status = "available";
      } else if (c2Occupancy > 0 && c2Queue === 0) {
        c2Color = "yellow";
        c2Status = "occupied_no_queue";
      } else if (c2Occupancy > 0 && c2Queue > 0) {
        c2Color = "red";
        c2Status = "occupied_with_queue";
      }

      update(ref(db, "court2"), {
        lightColor: c2Color,
        status: c2Status
      });

      const updatedCourts: CourtInfo[] = [
        {
          name: "Court 1",
          players: c1Occupancy,
            queue: c1Queue,
          status: c1Status as CourtInfo["status"],
          lightColor: c1Color,
        },
        {
          name: "Court 2",
          players: c2Occupancy,
            queue: c2Queue,
          status: c2Status as CourtInfo["status"],
          lightColor: c2Color,
        },
      ];

  setCourts(updatedCourts);
  setTotalPlayers(c1Occupancy + c2Occupancy);
    });
  }

  return (
    <>
      <section className="hero">
        <div className="hero-title">
          <img className="hero-logo" src={pickle} alt="pickleball paddles" />
          <h1 className="site-title">PicklePortal</h1>
        </div>
        <p className="hero-tagline">Plan your next pickleball match effortlessly.</p>
        <div className="divider"></div>
      </section>

      <OccupancyMonitor totalPlayers={totalPlayers} courts={courts} />
      <div className="divider"></div>
      <LiveStream />
    </>
  );
}
