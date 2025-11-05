import React from "react";

type CourtStatus = "available" | "occupied_no_queue" | "occupied_with_queue" | "occupied";

export type CourtInfo = {
  name: string;
  players: number;
  queue: number;
  status: CourtStatus;
  lightColor: string;
};

export default function OccupancyMonitor({
  totalPlayers,
  courts,
}: {
  totalPlayers: number;
  courts: CourtInfo[];
}) {
  return (
    <section id="occupancy" className="occ-section">

      <div className="occ-head">
        <div>
          <h2 className="occ-title">Court Occupancy Monitor</h2>
          <p className="occ-sub">Track Real-Time Court Availability with IoT Sensors</p>
        </div>

        <div className="live-pill">
          <span className="live-dot" />
          Live
        </div>
      </div>

      <div className="metric-wrap">
        <div className="metric-card">
          <div className="metric-title">Total Number of Players</div>
          <div className="metric-value">{totalPlayers}</div>
        </div>
      </div>

      <div className="legend">
        <Legend color="#27c93f" label="Court is Available" />
        <Legend color="#f6c350" label="Court Occupied Without Active Queue" />
        <Legend color="#e44d4d" label="Court Occupied With Active Queue" />
      </div>

      <div className="court-grid">
        {courts.map((c) => (
          <CourtCard key={c.name} court={c} />
        ))}
      </div>

    </section>
  );
}

function Legend({ color, label }: { color: string; label: string }) {
  return (
    <div className="legend-item">
      <span className="legend-dot" style={{ background: color }} />
      <span>{label}</span>
    </div>
  );
}

function CourtCard({ court }: { court: CourtInfo }) {
  const statusDot =
    court.status === "available"
      ? "#27c93f"
      : court.status === "occupied_no_queue"
      ? "#f6c350"
      : "#e44d4d";

  return (
    <div className="court-card">
      <div className="court-badge">
        {court.name} Status
        <span className="badge-dot" style={{ background: statusDot }} />
      </div>

      <div className="nums">{court.players}</div>
      <div className="label">Players in COURT</div>
      <div className="nums sm">{court.queue}</div>
      <div className="label">Players in QUEUE</div>
    </div>
  );
}