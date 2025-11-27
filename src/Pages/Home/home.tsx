import React from "react";
import "./home.css";

function Home() {
  return (
    <div className="home-container">
      <h1>Welcome to SmartKorf</h1>
      <p>
        SmartKorf is your ultimate companion for korfball enthusiasts. Track
        your performance, analyze game statistics, and connect with other
        players in the community.
      </p>
      <h2>Features</h2>
      <ul>
        <li>Real-time performance tracking</li>
        <li>Comprehensive game statistics</li>
        <li>Interactive community features</li>
      </ul>
      <h2>Get Started</h2>
      <p>
        Sign up today to start improving your korfball skills with SmartKorf!
      </p>
    </div>
  );
}

export default Home;
