import React, { useState, useEffect } from "react";
import "./scorecounter.css";
import useEspData from "../../Hooks/EspData/EspData";

function ScoreCounter() {
  const { data, isConnected, connectToESP32 } = useEspData();
  const [score, setScore] = useState(0);

  useEffect(() => {
    setScore(data.home);
  }, [data]);

  const incrementScore = () => setScore(score + 1);
  const decrementScore = () => setScore(score > 0 ? score - 1 : 0);

  return (
    <div className="score-counter">
      <button
        onClick={connectToESP32}
        disabled={isConnected}
        style={{ marginBottom: 10 }}
      >
        {isConnected ? "Connected" : "Connect ESP32"}
      </button>

      <h2 className="score">{score.toString().padStart(2, "0")}</h2>
      <div className="button-container">
        <button className="score-button decrement" onClick={decrementScore}>
          -
        </button>
        <button className="score-button increment" onClick={incrementScore}>
          +
        </button>
      </div>
    </div>
  );
}

export default ScoreCounter;
