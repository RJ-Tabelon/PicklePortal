// Live streams are embedded via iframe

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
            {/* Streams the livestream of court 1 */}
            <iframe
              width="560"
              height="315"
              src="https://www.youtube.com/embed/LCwzqDMXbCk?autoplay=1&mute=1"
              title="YouTube live stream"
              frameBorder="0"
              allow="autoplay; encrypted-media"
              allowFullScreen
            ></iframe>
          </div>
        </figure>

        <figure className="stream-card">
          <figcaption className="stream-tag">Court 2</figcaption>
          <div className="stream-frame">
            {/* Streams the livestream of court 2 */}
            <iframe
              width="560"
              height="315"
              src="https://www.youtube.com/embed/-yaAA2wVHRA?autoplay=1&mute=1"
              title="YouTube live stream"
              frameBorder="0"
              allow="autoplay; encrypted-media"
              allowFullScreen
            ></iframe>
          </div>
        </figure>
      </div>
    </section>
  );
}
