import React from "react";
import "./about.css";

function About() {
  return (
    <div className="about-container">
      <div className="about-content">
        <h1>About SmartKorf</h1>

        <section className="about-section">
          <h2>Het Project</h2>
          <p>
            SmartKorf is een innovatief platform dat real-time sportdata
            visualiseert voor korfbal. Met behulp van BLE (Bluetooth Low Energy)
            technologie verzamelen we live scores en statistieken die direct
            worden weergegeven op een interactief scorebord.
          </p>
        </section>

        <section className="about-section">
          <h2>Technologie</h2>
          <p>
            Het systeem maakt gebruik van ESP32 microcontrollers die fungeren
            als gateway voor BLE communicatie. De web applicatie is gebouwd met
            React en biedt een moderne, glassmorphism-gebaseerde interface voor
            optimale gebruikerservaring.
          </p>
        </section>

        <section className="about-section">
          <h2>Het Team</h2>
          <div className="team-grid">
            <div className="team-member">
              <h3>Toine</h3>
            </div>
            <div className="team-member">
              <h3>Aiden</h3>
            </div>
            <div className="team-member">
              <h3>Remco</h3>
            </div>
            <div className="team-member">
              <h3>Henri</h3>
            </div>
          </div>
        </section>

        <section className="about-section">
          <h2>Smart Things Minor</h2>
          <p>
            Dit project is ontwikkeld als onderdeel van de Smart Things minor,
            waarbij we ons richten op het combineren van hardware en software
            voor slimme oplossingen in de sport.
          </p>
        </section>
      </div>
    </div>
  );
}

export default About;
