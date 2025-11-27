import React from "react";
import "./home.css";
import { Link } from "react-router-dom";

function Home() {
  return (
    <div className="home-container">
      <div className="blob blob-1"></div>
      <div className="blob blob-2"></div>

      <div className="home-content">
        <h1>Welkom bij SmartKorf</h1>
        <p>
          Een moderne en slimme manier om je sportdata te koppelen en te
          visualiseren.
        </p>

        <div className="home-buttons">
          <Link to="/connect" className="btn-outline">
            Connect
          </Link>

          <Link to="/login" className="btn-filled">
            Login
          </Link>
        </div>
      </div>
    </div>
  );
}

export default Home;
