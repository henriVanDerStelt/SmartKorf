import React from "react";
import "./navbar.css";
import { Link, useNavigate } from "react-router-dom";
import { UserButton, useUser } from "@clerk/clerk-react";
import { useEspData } from "../../Contexts/EspDataContext";

function NavBar() {
  const navigate = useNavigate();
  const { user, isLoaded } = useUser();
  const { devices } = useEspData();
  const hasConnectedDevices = devices.size > 0;

  return (
    <nav className="navbar-container">
      <h1 onClick={() => navigate("/")} style={{ cursor: "pointer" }}>
        SmartKorf
      </h1>

      <ul className="navbar-links">
        {hasConnectedDevices && (
          <li>
            <Link to="/ScoreBoard" className="scoreboard-link-live">
              ScoreBoard
            </Link>
          </li>
        )}
        <li>
          <Link to="/">Home</Link>
        </li>
        {user && (
          <li>
            <Link to="/Stats">Stats</Link>
          </li>
        )}
        <li>
          <Link to="/About">About</Link>
        </li>

        <li>
          {!isLoaded ? null : user ? (
            <div className="navbar-user">
              <UserButton afterSignOutUrl="/SmartKorf" />
            </div>
          ) : (
            <button
              onClick={() => navigate("/sign-in")}
              className="navbar-signin"
            >
              Sign in
            </button>
          )}
        </li>
      </ul>
    </nav>
  );
}

export default NavBar;
