import React from "react";
import "./scorecounter.css";
import { useState } from "react";

import useEspData from "../../Hooks/EspData/EspData";

function ScoreCounter() {
  const [score, setScore] = useState(0);
  const espData = useEspData();

  React.useEffect(() => {
    setScore(espData.home);
  }, [espData]);

  const incrementScore = () => {
    setScore(score + 1);
  };

  const decrementScore = () => {
    setScore(score - 1 >= 0 ? score - 1 : 0);
  };

  return (
    <div className="score-counter">
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
