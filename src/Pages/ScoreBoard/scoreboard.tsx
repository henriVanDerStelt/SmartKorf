import React, { useState, useEffect } from "react";
import "./scoreboard.css";
import ScoreCounter from "../../Components/ScoreCounter/scorecounter";
import Timer from "../../Components/Timer/timer";
import { useEspData } from "../../Contexts/EspDataContext";
import { useCommandReceiver } from "../../Hooks/useCommandReceiver";
import { supabase } from "../../supabaseClient";

const TIMER_DURATION = 30; // time in minutes

function ScoreBoard() {
  const { devices } = useEspData();
  const [homeTeamName, setHomeTeamName] = useState("Home");
  const [awayTeamName, setAwayTeamName] = useState("Away");
  const [timeLeft, setTimeLeft] = useState(TIMER_DURATION * 60 * 100);
  const [isTimerRunning, setIsTimerRunning] = useState(false);
  const [currentGameId, setCurrentGameId] = useState<string | null>(null);
  const [homeScore, setHomeScore] = useState(0);
  const [awayScore, setAwayScore] = useState(0);

  // Sync met CentralUnit data
  useEffect(() => {
    devices.forEach((device) => {
      if (device.name === "CentralUnit") {
        // Use structured data fields directly from device object
        console.log("[CentralUnit] device received:", { 
          time: device.time, 
          score: device.score, 
          accuracy: device.accuracy, 
          goalAttempt: device.goalAttempt 
        });

        // Sync Score using structured field
        if (device.score && Array.isArray(device.score)) {
          const [centralHome, centralAway] = device.score;
          if (homeScore !== centralHome) {
            console.log(`Syncing home score: ${homeScore} -> ${centralHome}`);
            setHomeScore(centralHome);
          }
          if (awayScore !== centralAway) {
            console.log(`Syncing away score: ${awayScore} -> ${centralAway}`);
            setAwayScore(centralAway);
          }
        }

        // Sync Time using structured field (convert seconds to centiseconds)
        if (device.time !== undefined) {
          const centralTime = device.time * 100; // Convert to centiseconds
          const timeDiff = Math.abs(timeLeft - centralTime);
          // Only sync if difference is more than 1 second (100 centiseconds)
          if (timeDiff > 100) {
            console.log(`Syncing time: ${timeLeft} -> ${centralTime}`);
            setTimeLeft(centralTime);
          }
        }

        // Future: Use accuracy and goalAttempt when needed
        // if (device.accuracy) { ... }
        // if (device.goalAttempt) { ... }
      }
    });
  }, [devices, homeScore, awayScore, timeLeft]);

  // Get data from specific devices (assuming first device is home, second is away)
  const homeDevice = Array.from(devices.values())[0];
  const awayDevice = Array.from(devices.values())[1];

  // Use synced scores or fallback to device data
  const homeData =
    homeScore > 0 ? homeScore : homeDevice ? parseInt(homeDevice.data) || 0 : 0;
  const awayData =
    awayScore > 0 ? awayScore : awayDevice ? parseInt(awayDevice.data) || 0 : 0;

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

  // Verwerk inkomende commands
  useCommandReceiver({
    onChangeNames: (homeTeam, awayTeam) => {
      setHomeTeamName(homeTeam);
      setAwayTeamName(awayTeam);
      updateTeamName("home", homeTeam);
      updateTeamName("away", awayTeam);
    },
    onScoreCommand: (teamName, action) => {
      console.log(`Score command received: ${teamName} - ${action}`);
      // Score updates worden automatisch verwerkt via BLE in ScoreCounter
    },
    onTimeCommand: (action) => {
      console.log("Time command received:", action);
      if (action === "START") {
        setIsTimerRunning(true);
      } else if (action === "STOP") {
        setIsTimerRunning(false);
      } else if (action === "RESET") {
        setIsTimerRunning(false);
        setTimeLeft(TIMER_DURATION * 60 * 100);
      }
    },
  });

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
        <Timer
          timeLeft={timeLeft}
          setTimeLeft={setTimeLeft}
          isRunning={isTimerRunning}
          setIsRunning={setIsTimerRunning}
          currentGameId={currentGameId}
          setCurrentGameId={setCurrentGameId}
        />
      </div>
    </div>
  );
}

export default ScoreBoard;
