import "./RankTable.css";
import React from "react";

interface RankTableProps {
  headers: string[];
  data: (string | number)[][];
}

function RankTable(props: RankTableProps) {
  return (
    <table className="rank-table">
      <thead>
        <tr>
          {props.headers.map((header, index) => (
            <th key={index}>{header}</th>
          ))}
        </tr>
      </thead>
      <tbody>
        {props.data.map((row, rowIndex) => (
          <tr key={rowIndex}>
            {row.map((cell, cellIndex) => (
              <td key={cellIndex}>{cell}</td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
}

export default RankTable;
