import "./RecentGame.css";
import React from "react";

interface RecentGameProps {
  date?: string;
  teamA?: string;
  teamB?: string;
  scoreA?: number;
  scoreB?: number;
}

function RecentGame(props: RecentGameProps) {
  const formatScore = (score?: number) => {
    return score !== undefined ? String(score).padStart(2, "0") : "00";
  };

  const scoreA = props.scoreA ?? 0;
  const scoreB = props.scoreB ?? 0;
  const teamAWins = scoreA > scoreB;
  const teamBWins = scoreB > scoreA;

  return (
    <div className="recent-game-card">
      <div className="recent-game-date">
        <h5>{props.date}</h5>
      </div>
      <div className="recent-game-teams">
        <h4 className={teamAWins ? "winning-team" : ""}>{props.teamA}</h4>
        <div className="recent-game-score">
          <span className="score1">{formatScore(props.scoreA)}</span>-
          <span className="score1">{formatScore(props.scoreB)}</span>
        </div>
        <h4 className={teamBWins ? "winning-team" : ""}>{props.teamB}</h4>
      </div>
    </div>
  );
}

export default RecentGame;
