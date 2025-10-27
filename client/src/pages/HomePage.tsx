import pickle from "../assets/pickle.png";
import OccupancyMonitor from "../components/OccupancyMonitor";
import type { CourtInfo } from "../components/OccupancyMonitor";
import LiveStream from "../components/LiveStream";


const MOCK_COURTS: CourtInfo[] = [
  { name: "Court 1", players: 4, queue: 2, status: "occupied_with_queue" },
  { name: "Court 2", players: 4, queue: 0, status: "occupied_no_queue" },
];

export default function HomePage() {
  return (
    <>
      <section className="hero">
        <div className="hero-title">
          <img className="hero-logo" src={pickle} alt="pickleball paddles" />
          <h1 className="site-title">PicklePortal</h1>
        </div>

        <p className="hero-tagline">
          Plan your next pickleball match effortlessly.
        </p>

        <div className="divider"></div>
      </section>

      <OccupancyMonitor totalPlayers={26} courts={MOCK_COURTS} />
      <div className="divider"></div>
      <LiveStream />
    </>
  );
}
