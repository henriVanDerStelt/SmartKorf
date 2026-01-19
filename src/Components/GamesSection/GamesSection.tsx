import "./GamesSection.css";
import React from "react";

import TeamCard from "./TeamCard";
import AngryFace from "../../Assets/Images/angry_face.png";
import HeadToHead from "./HeadToHead";
import RecentGame from "./RecentGame";
import { R } from "@clerk/clerk-react/dist/useAuth-fq1pQd_y";

function GamesSection() {
  return (
    <div className="section">
      <h2>Games</h2>
      <div className="next-game">
        <div className="next-game-title">
          <h4>Next game:</h4>
          <h3>09-01-2026, 13:00</h3>
        </div>
        <div className="teams">
          <TeamCard
            id={1}
            teamName="Deetos"
            ranking={6}
            image={AngryFace}
            color="#D1060C"
          />
          <TeamCard
            id={2}
            teamName="PKC"
            ranking={1}
            image={AngryFace}
            color="#027B45"
          />
        </div>
        <HeadToHead />
      </div>
      <div className="recent-games">
        <h4>Recent results:</h4>
        <RecentGame
          date="26-06-25"
          teamA="PKC"
          teamB="Deetos"
          scoreA={24}
          scoreB={19}
        />
        <RecentGame
          date="26-06-25"
          teamA="Deetos"
          teamB="PKC"
          scoreA={24}
          scoreB={19}
        />
      </div>
    </div>
  );
}

export default GamesSection;
