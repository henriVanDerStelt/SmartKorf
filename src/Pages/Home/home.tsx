import React, { useState } from "react";
import "./home.css";
import { Link } from "react-router-dom";
import Popup from "../../Components/Popup/popup";

function Home() {
  const [showPopup, setShowPopup] = useState(false);
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
          {/* 
          <Link to="/login" className="btn-filled">
            Login
          </Link> */}

          {showPopup && (
            <Popup
              title="Connect"
              message="Devices found:"
              onClose={() => setShowPopup(false)}
              buttons={[
                {
                  label: "Connect",
                  variant: "primary",
                  onClick: () => {},
                  closes: true,
                },
              ]}
            >
              <div className="popup-device-list">
                <select
                  id="device-select"
                  className="popup-select"
                  defaultValue=""
                >
                  <option value="" disabled>
                    Select a device
                  </option>
                  <option value="device-a">PKC_SmartKorf_1</option>
                  <option value="device-b">PKC_SmartKorf_2</option>
                  <option value="device-c">PKC_SmartKorf_3</option>
                </select>
              </div>
            </Popup>
          )}
        </div>
      </div>
    </div>
  );
}

export default Home;
