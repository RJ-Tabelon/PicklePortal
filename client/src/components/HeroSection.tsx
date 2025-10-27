import React from "react";
import paddles from "../assets/pickle.png"; 

const HeroSection: React.FC = () => {
  return (
    <section className="hero">
      <div className="brand">
        <img className="brand-icon" src={paddles} alt="pickle paddles logo" />
        <h1 className="brand-title">
          <span className="brand-title-top">PicklePortal</span>
          <span aria-hidden className="brand-title-back">PicklePortal</span>
        </h1>
      </div>

      <p className="tagline">Plan your next pickleball match effortlessly.</p>

      <div className="double-rule" aria-hidden />
    </section>
  );
};

export default HeroSection;
