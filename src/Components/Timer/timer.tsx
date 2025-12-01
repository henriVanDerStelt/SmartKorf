import React from "react";
import "./timer.css";

function Timer() {
  return (
    <div className="timer-container">
      <h2 className="time">00:00:00</h2>
      <div className="timer-buttons">
        <button className="timer-button start">Start</button>
        <button className="timer-button stop">Stop</button>
        <button className="timer-button reset">Reset</button>
      </div>
    </div>
  );
}

export default Timer;
