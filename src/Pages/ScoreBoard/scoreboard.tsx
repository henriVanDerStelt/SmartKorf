import React, { useState, useEffect } from "react";
import "./scoreboard.css";
import ScoreCounter from "../../Components/ScoreCounter/scorecounter";
import Timer from "../../Components/Timer/timer";
import { useEspData } from "../../Contexts/EspDataContext";
import { supabase } from "../../supabaseClient";

function ScoreBoard() {
  const { devices } = useEspData();
  const [homeTeamName, setHomeTeamName] = useState("Home");
  const [awayTeamName, setAwayTeamName] = useState("Away");

  // Get data from specific devices (assuming first device is home, second is away)
  const homeDevice = Array.from(devices.values())[0];
  const awayDevice = Array.from(devices.values())[1];

  // Parse the data from devices
  const homeData = homeDevice ? parseInt(homeDevice.data) || 0 : 0;
  const awayData = awayDevice ? parseInt(awayDevice.data) || 0 : 0;

  const updateTeamName = async (team: "home" | "away", name: string) => {
    try {
      const gameId = localStorage.getItem("currentGameId");
      if (gameId) {
        const { error } = await supabase
          .from("games")
          .update({ [team]: name })
          .eq("id", gameId);

        if (error) {
          console.error("Error updating team name:", error);
        } else {
          console.log(`${team} team name updated to: ${name}`);
        }
      }
    } catch (e) {
      console.error(e);
    }
  };

  const handleHomeNameChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const name = e.target.value;
    setHomeTeamName(name);
    updateTeamName("home", name);
  };

  const handleAwayNameChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const name = e.target.value;
    setAwayTeamName(name);
    updateTeamName("away", name);
  };

  return (
    <div className="Scoreboard-page">
      <div className="scoreboard-container">
        <div className="score-container home">
          <input
            type="text"
            className="team-name-input"
            placeholder="Home Team"
            value={homeTeamName}
            onChange={handleHomeNameChange}
          />
          <ScoreCounter data={homeData} team="home" />
          {homeDevice && (
            <div className="device-info">
              <small>
                {homeDevice.name} (RSSI: {homeDevice.rssi}dBm)
              </small>
            </div>
          )}
        </div>
        <div className="score-container away">
          <input
            type="text"
            className="team-name-input"
            placeholder="Away Team"
            value={awayTeamName}
            onChange={handleAwayNameChange}
          />
          <ScoreCounter data={awayData} team="away" />
          {awayDevice && (
            <div className="device-info">
              <small>
                {awayDevice.name} (RSSI: {awayDevice.rssi}dBm)
              </small>
            </div>
          )}
        </div>
      </div>

      <div className="time-container">
        <Timer />
      </div>
    </div>
  );
}

export default ScoreBoard;
