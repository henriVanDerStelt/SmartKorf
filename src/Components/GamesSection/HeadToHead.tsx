import "./HeadToHead.css";
import React from "react";

function HeadToHead() {
  return (
    <div className="head-to-head-section">
      <h4>Head to Head</h4>
      <div className="head-to-head-records">
        <div className="head-to-head-record">
          <p>Wins</p>
          <h5>
            2 <span>/</span> 7
          </h5>
        </div>
        <div className="head-to-head-record">
          <p>Goals</p>
          <h5>
            123 <span>/</span> 287
          </h5>
        </div>
      </div>
    </div>
  );
}

export default HeadToHead;
