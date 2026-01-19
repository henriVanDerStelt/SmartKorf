import "./Stat.css";
import React from "react";

function Stat({
  title,
  value,
  percentage,
}: {
  title: string;
  value: number;
  percentage?: number;
}) {
  return (
    <div className="stat">
      <h4>{title}</h4>
      <div className="stat-values">
        <h3>{value}</h3>
        {percentage !== undefined && <h5>{percentage}%</h5>}
      </div>
    </div>
  );
}

export default Stat;
