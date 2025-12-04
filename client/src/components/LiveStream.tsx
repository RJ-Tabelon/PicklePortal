// Live streams are embedded via iframe

export default function LiveStream() {
  return (
    <section id="stream" className="stream-section">
      <div className="stream-header">
        <div>
          <h2 className="stream-title">Live Court Feed</h2>
          <p className="stream-sub">Stream Each Court in Real Time</p>
        </div>

        <div className="live-pill">
          <span className="live-dot" />
          Live
        </div>
      </div>

      <div className='stream-grid'>
        <figure className='stream-card'>
          <figcaption className='stream-tag'>Court 1</figcaption>
          <div className='stream-frame'>
            {/* Streams the livestream of court 1 */}
            <img
              src="http://172.20.10.12:81/stream"
              alt="Court 1 Stream"
              style={{
                width: "100%",
                height: "auto",
                display: "block",
                objectFit: "contain",
                background: "#2f3b2f", // matches your green theme
                border: "2px solid #000",
              }}
              onError={(e) => {
                e.target.style.display = "none"; // hide broken stream
              }}
              onLoad={(e) => {
                e.target.style.display = "block";
              }}
            />
          </div>
        </figure>

        <figure className='stream-card mb-[60px]'>
          <figcaption className='stream-tag'>Court 2</figcaption>
          <div className='stream-frame'>
            {/* Streams the livestream of court 2 */}
              <img
              src="http://172.20.10.12:81/stream2"
              alt="Court 1 Stream"
              style={{
                width: "100%",
                height: "auto",
                display: "block",
                objectFit: "contain",
                background: "#2f3b2f", // matches your green theme
                border: "2px solid #000",
              }}
              onError={(e) => {
                e.target.style.display = "none"; // hide broken stream
              }}
              onLoad={(e) => {
                e.target.style.display = "block";
              }}
            />
          </div>
        </figure>
      </div>
    </section>
  );
}
