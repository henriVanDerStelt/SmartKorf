import React, { useState, ReactNode } from "react";
import "./Tooltip.css";

interface TooltipProps {
  text: ReactNode;
  children: ReactNode;
  position?: "top" | "bottom" | "left" | "right";
}

const Tooltip: React.FC<TooltipProps> = ({
  text,
  children,
  position = "top",
}) => {
  const [visible, setVisible] = useState(false);

  return (
    <div
      className="tooltip-container"
      onMouseEnter={() => setVisible(true)}
      onMouseLeave={() => setVisible(false)}
    >
      {children}
      {visible && (
        <div className={`tooltip-box tooltip-${position}`}>{text}</div>
      )}
    </div>
  );
};

export default Tooltip;
