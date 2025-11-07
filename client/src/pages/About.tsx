import { useState, useEffect, type FormEvent } from "react";
import { useLocation } from "react-router-dom";
import pickle from "../assets/pickle.png";
import court from '../assets/court.png';

export default function About() {
  const [message, setMessage] = useState("");
  const [senderName, setSenderName] = useState("");
  const [senderEmail, setSenderEmail] = useState("");
  const location = useLocation();

  const handleSubmit = (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const subject = encodeURIComponent("Contact from PicklePortal");
    const bodyText = `Name: ${senderName}\nEmail: ${senderEmail}\n\n${message}`;
    const body = encodeURIComponent(bodyText || "");
    window.location.href = `mailto:dolphinsquare03@gmail.com?subject=${subject}&body=${body}`;
  };

  useEffect(() => {
    if (location.hash === "#contact") {
      setTimeout(() => {
        const el = document.getElementById("contact");
        if (el) el.scrollIntoView({ behavior: "smooth", block: "start" });
      }, 80);
    }
  }, [location]);

  return (
    <>
      <section className='hero'>
        <div className='hero-title'>
          <div>
            <h1 className='site-title'>About</h1>
            <p className='hero-tagline'>
              PicklePortal helps you plan matches by showing live court status
              and a running player count.
            </p>
          </div>
        </div>
        <div className='divider' />
      </section>

      <section className='section about-section'>
        <div className='section-wrap'>
          <h2 className='section-title'>About</h2>
          <div className='about-page-accent' aria-hidden='true' />
          <p className='section-text'>
            Sensors at each court detect play and send events to a small
            backend. the site listens for updates and reflects availability
            instantly, using the same green theme and simple labels you see on
            the physical lights. it is meant to be quick to read on phones and
            kiosks at the courts.
          </p>
          <ul className='section-list'>
            <li>- Available, occupied, or occupied with queue</li>
            <li>- Live total number of players</li>
            <li>- Fast, simple interface for real time checks</li>
          </ul>
        </div>

        <div className='about-image-wrap'>
          <div className='about-image-frame'>
            <img
              src={court}
              alt='Pickleball court with sensors'
              className='about-image'
            />
          </div>
        </div>
      </section>

      <div className='divider' />

      <section id='contact' className='section'>
        <div className='section-wrap'>
          <h2 className='section-title'>Contact Us</h2>

          <p className='section-text contact-desc'>
            Feel free to contact us if you have any questions or something.
          </p>
          <br />
          <div className='card-grid contact-grid'>
            <form className='info-card contact-form' onSubmit={handleSubmit}>
              <div className='dot' aria-hidden='true' />
              <h3 className='card-title'>Contact Form</h3>

              <div className='contact-row'>
                <label htmlFor='contact-name' className='sr-only'>
                  Your name
                </label>
                <input
                  id='contact-name'
                  className='contact-input'
                  placeholder='Your name'
                  value={senderName}
                  onChange={e => setSenderName(e.target.value)}
                  required
                />

                <label htmlFor='contact-email' className='sr-only'>
                  Your email
                </label>
                <input
                  id='contact-email'
                  type='email'
                  className='contact-input'
                  placeholder='Your email'
                  value={senderEmail}
                  onChange={e => setSenderEmail(e.target.value)}
                  required
                />
              </div>

              <label htmlFor='contact-message' className='sr-only'>
                Message
              </label>
              <textarea
                id='contact-message'
                className='card-text contact-textarea'
                placeholder='Write your message here...'
                value={message}
                onChange={e => setMessage(e.target.value)}
                rows={8}
                required
              />

              <div className='contact-actions'>
                <button type='submit' className='send-btn'>
                  Send
                </button>
              </div>
            </form>
          </div>
        </div>
      </section>

      <div className='divider' />
    </>
  );
}
