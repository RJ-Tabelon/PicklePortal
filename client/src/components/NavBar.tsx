import ball from "../assets/ball.png";

export default function NavBar() {
  const goTop = () => window.scrollTo({ top: 0, behavior: "smooth" });

  return (
    <header className="nav-wrap">
      <div className="nav-slab">
        <nav className="nav-pill">
          <a href="#occupancy">occupancy</a>
          <a href="#about">about</a>

          <button className="ball-btn" aria-label="home" onClick={goTop}>
            <img src={ball} alt="pickleball icon" />
          </button>

          <a href="#contact">contact</a>
          <a href="#stream">stream</a>
        </nav>
      </div>
    </header>
  );
}

