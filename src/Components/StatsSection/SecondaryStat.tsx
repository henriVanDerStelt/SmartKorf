import "./SecondaryStat.css";
import React from "react";

function SecondaryStat({ title, value }: { title: string; value: number }) {
  return (
    <div className="secondary-stat">
      <h5>&emsp;&emsp;{title}</h5>
      <div className="secondary-stat-values">
        <h6>{value}</h6>
      </div>
    </div>
  );
}

export default SecondaryStat;
