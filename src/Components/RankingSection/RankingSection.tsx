import "./RankingSection.css";
import React from "react";

import RankTable from "./RankTable";

function RankingSection() {
  return (
    <div className="section">
      <h2>Ranking</h2>
      <RankTable
        headers={["Place", "Team", "G", "+", "-", "pt"]}
        data={[
          [1, "PKC", 10, 20, 5, 30],
          [2, "ODO", 8, 15, 7, 24],
          [3, "DVO", 12, 25, 10, 35],
          [4, "Fortuna", 9, 18, 8, 26],
          [5, "LDODK", 11, 22, 6, 32],
          [6, "Deetos", 7, 14, 9, 21],
          [7, "Blauw-Wit", 10, 19, 11, 28],
          [8, "KZ", 6, 12, 12, 18],
          [9, "Tachos", 5, 10, 15, 15],
          [10, "Feyenoord", 4, 8, 18, 12],
          [11, "VZV", 3, 6, 20, 9],
          [12, "Oostingh", 2, 4, 22, 6],
          [13, "Nieuwerkerk", 1, 2, 25, 3],
          [14, "Schelluinen", 0, 0, 30, 0],
          [15, "Groen Geel", 0, 0, 35, 0],
          [16, "Waddinxveen", 0, 0, 40, 0],
          [17, "Houten", 0, 0, 45, 0],
          [18, "Vriendenschaar", 0, 0, 50, 0],
        ]}
      />
    </div>
  );
}

export default RankingSection;
