import React from "react";
import "./navbar.css";
import { Link, useNavigate } from "react-router-dom";

function NavBar() {
  const navigate = useNavigate();

  return (
    <nav className="navbar-container">
      <h1 onClick={() => navigate("/")} style={{ cursor: "pointer" }}>
        SmartKorf
      </h1>
      <ul className="navbar-links">
        <li>
          <Link to="/">Home</Link>
        </li>
        <li>
          <Link to="/About">About</Link>
        </li>
      </ul>
    </nav>
  );
}

export default NavBar;
