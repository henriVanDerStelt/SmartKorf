import React, { useEffect, useState } from "react";
import "./home.css";
import { Link, useNavigate, useLocation } from "react-router-dom";
import Popup from "../../Components/Popup/popup";
import { useEspData } from "../../Contexts/EspDataContext";

function Home() {
  const [showPopup, setShowPopup] = useState(false);
  const { devices, isConnected, connectToGateway } = useEspData();
  const navigate = useNavigate();
  const location = useLocation();
  const [hasNavigated, setHasNavigated] = useState(false);

  useEffect(() => {
    // Alleen navigeren als we op de home pagina zijn EN nog niet genavigeerd hebben
    if (isConnected && location.pathname === "/" && !hasNavigated) {
      setHasNavigated(true);
      navigate("/ScoreBoard");
    }
  }, [isConnected, navigate, location.pathname, hasNavigated]);

  return (
    <div className="home-container">
      <div className="blob blob-1"></div>
      <div className="blob blob-2"></div>

      <div className="home-content">
        <h1>Welkom bij SmartKorf</h1>
        <p className="smart-korf-text">
          Een moderne en slimme manier om je sportdata te koppelen en te
          visualiseren.
        </p>

        <div className="home-buttons">
          <button className="btn-outline" onClick={() => setShowPopup(true)}>
            Connect
          </button>

          {showPopup && (
            <Popup
              title="Connect"
              message="Devices found:"
              onClose={() => setShowPopup(false)}
            >
              <div className="popup-device-list">
                <button
                  onClick={connectToGateway}
                  disabled={isConnected}
                  className="btn-filled"
                >
                  {isConnected ? "Connected" : "Connect to Gateway"}
                </button>
                {isConnected && (
                  <div className="connected-devices">
                    <p>Connected devices:</p>
                    <ul>
                      {Array.from(devices.entries()).map(([name, device]) => (
                        <li key={name}>
                          {device.name} - RSSI: {device.rssi}dBm
                        </li>
                      ))}
                    </ul>
                  </div>
                )}
              </div>
            </Popup>
          )}
        </div>
      </div>
    </div>
  );
}

export default Home;
