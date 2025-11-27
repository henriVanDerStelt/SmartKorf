import React from "react";
import "./navbar.css";

function NavBar() {
  return (
    <nav className="navbar-container">
      <h1>SmartKorf</h1>
      <ul className="navbar-links">
        <li>
          <a href="/">Home</a>
        </li>
        <li>
          <a href="/About">About</a>
        </li>
      </ul>
    </nav>
  );
}

export default NavBar;
