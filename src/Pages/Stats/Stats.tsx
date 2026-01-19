import "./Stats.css";
import React from "react";

import StatsSection from "../../Components/StatsSection/StatsSection";
import GamesSection from "../../Components/GamesSection/GamesSection";
import RankingSection from "../../Components/RankingSection/RankingSection";

function Stats() {
  return (
    <div className="stats-page">
      <h1>Stats</h1>
      <div className="stats-sections">
        <div className="stats-section stats">
          <StatsSection />
        </div>
        <div className="stats-section games">
          <GamesSection />
        </div>
        <div className="stats-section ranking">
          <RankingSection />
        </div>
      </div>
    </div>
  );
}

export default Stats;
