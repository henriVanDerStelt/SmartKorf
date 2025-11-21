import React from "react";
import './Navbar.css';

function NavBar() {
    return (
        <nav className="navbar">
            <h2 className="navbar-logo">SmartKorf</h2>
            <ul className="navbar-links">
                <li><a href="/">Home</a></li>
                <li><a href="/about">About</a></li>
            </ul>
        </nav>
    );
}

export default NavBar;