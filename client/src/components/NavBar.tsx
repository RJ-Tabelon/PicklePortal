import { Link, useNavigate } from "react-router-dom";
import ball from "../assets/ball.png";

export default function NavBar() {
  const navigate = useNavigate();

  const scrollToId = (id: string) => {
    const el = document.getElementById(id);
    if (el) el.scrollIntoView({ behavior: "smooth", block: "start" });
  };

  const navigateAndScroll = (hashId?: string) => {
    if (window.location.pathname === "/" && !hashId) {
      navigate("/");
      setTimeout(() => window.scrollTo({ top: 0, behavior: "smooth" }), 50);
      return;
    }

    if (window.location.pathname === "/" && hashId) {
      scrollToId(hashId);
      return;
    }

    const target = hashId ? `/#${hashId}` : "/";
    navigate(target);
    if (hashId) {
      setTimeout(() => scrollToId(hashId), 100);
    } else {
      setTimeout(() => window.scrollTo({ top: 0, behavior: "smooth" }), 100);
    }
  };

  return (
    <header className="nav-wrap">
      <div className="nav-slab">
        <nav className="nav-pill">
          <a
            href="#occupancy"
            onClick={(e) => {
              e.preventDefault();
              navigateAndScroll("occupancy");
            }}
          >
            occupancy
          </a>

          <a
            href="#stream"
            onClick={(e) => {
              e.preventDefault();
              navigateAndScroll("stream");
            }}
          >
            stream
          </a>

          <button
            className="ball-btn"
            aria-label="home"
            onClick={() => navigateAndScroll()}
          >
            <img src={ball} alt="pickleball icon" />
          </button>

          <Link to="/about">about</Link>
          <Link to="/about#contact">contact</Link>

        </nav>
      </div>
    </header>
  );
}

