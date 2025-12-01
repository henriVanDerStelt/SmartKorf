import React from "react";
import "./scoreboard.css";

import ScoreCounter from "../../Components/ScoreCounter/scorecounter";
import Timer from "../../Components/Timer/timer";

function ScoreBoard() {
  return (
    <div className="Scoreboard-page">
      <div className="scoreboard-container">
        <div className="score-container home">
          <input
            type="text"
            className="team-name-input"
            placeholder="Home Team"
          />
          <ScoreCounter />
        </div>
        <div className="score-container away">
          <input
            type="text"
            className="team-name-input"
            placeholder="Away Team"
          />
          <ScoreCounter />
        </div>
      </div>

      <div className="time-container">
        <Timer />
      </div>
    </div>
  );
}

export default ScoreBoard;
