import React from "react";
import "./footer.css";
import { Link } from "react-router-dom";

function Footer() {
  return (
    <footer className="footer-container">
      <div className="footer-content">
        <div className="footer-section">
          <h3>SmartKorf</h3>
          <p>Moderne sportdata visualisatie voor korfbal</p>
        </div>

        <div className="footer-section">
          <h4>Links</h4>
          <ul>
            <li>
              <Link to="/">Home</Link>
            </li>
            <li>
              <Link to="/Stats">Statistics</Link>
            </li>
            <li>
              <Link to="/About">About</Link>
            </li>
          </ul>
        </div>

        <div className="footer-section">
          <h4>Team</h4>
          <ul>
            <li>Toine</li>
            <li>Aiden</li>
            <li>Remco</li>
            <li>Henri</li>
          </ul>
        </div>
      </div>

      <div className="footer-bottom">
        <p>&copy; {new Date().getFullYear()} SmartKorf. All rights reserved.</p>
      </div>
    </footer>
  );
}

export default Footer;
