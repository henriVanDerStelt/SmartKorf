import React from "react";
import "./footer.css";

function Footer() {
  return (
    <footer className="footer-container">
      <p>&copy; {new Date().getFullYear()} SmartKorf. All rights reserved.</p>
    </footer>
  );
}

export default Footer;
