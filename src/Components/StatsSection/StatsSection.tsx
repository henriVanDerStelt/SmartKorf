import "./StatsSection.css";
import React from "react";

import Stat from "./Stat";
import SecondaryStat from "./SecondaryStat";

function StatsSection() {
  return (
    <div className="section">
      <div className="stats-header">
        <h2>PKC</h2>
        <h3>1</h3>
      </div>
      <div className="stats-minor-section games-played">
        <Stat title="Games Played" value={12} />
      </div>
      <div className="stats-minor-section games-won">
        <Stat title="Wins" value={10} percentage={83} />
        <SecondaryStat title="Home" value={6} />
        <SecondaryStat title="Away" value={4} />
        <Stat title="Losses" value={2} percentage={16} />
        <SecondaryStat title="Home" value={1} />
        <SecondaryStat title="Away" value={1} />
        <Stat title="Draws" value={0} percentage={0} />
      </div>
      <div className="stats-minor-section points-stats">
        <Stat title="Shots (on target)" value={1782} percentage={70} />
        <Stat title="Goals +" value={1301} />
        <Stat title="Goals -" value={481} />
        <Stat title="Goal difference" value={820} />
      </div>
    </div>
  );
}

export default StatsSection;
