import React from "react";

import court from "../assets/court.png";   

export default function LiveStream() {
  return (
    <section id="stream" className="stream-section">
      <div className="stream-header">
        <div>
          <h2 className="stream-title">Live Court Feed</h2>
          <p className="stream-sub">Stream Each Court in Real Time</p>
        </div>

        <span className="live-badge">
          <span className="dot" /> Live
        </span>
      </div>

      <div className="stream-grid">
        <figure className="stream-card">
          <figcaption className="stream-tag">Court 1</figcaption>
          <div className="stream-frame">
            <img src={court} alt="Court 1 live" />
          </div>
        </figure>

        <figure className="stream-card">
          <figcaption className="stream-tag">Court 2</figcaption>
          <div className="stream-frame">
            <img src={court} alt="Court 2 live" />
          </div>
        </figure>
      </div>
    </section>
  );
}
