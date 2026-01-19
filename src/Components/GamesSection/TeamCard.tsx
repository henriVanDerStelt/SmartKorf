import "./TeamCard.css";
import React from "react";

import AngryFace from "../../Assets/Images/angry_face.png";

interface TeamCardProps {
  id: number;
  teamName: string;
  ranking?: number;
  image: string;
  color?: string;
}

function TeamCard({ id, teamName, ranking, image, color }: TeamCardProps) {
  const gradientDirection = id === 1 ? "to top left" : "to top right";
  const backgroundPosition = id === 1 ? "right center" : "left center";
  const imageTransform = id === 1 ? "scaleX(-1)" : "none";

  const gradientStyle = color
    ? {
        backgroundImage: `linear-gradient(${gradientDirection}, ${color}88, #ffffff22)`,
      }
    : {};

  const imageLayerStyle = {
    backgroundImage: `url(${image})`,
    backgroundPosition: backgroundPosition,
    backgroundSize: "cover",
    backgroundRepeat: "no-repeat",
    transform: imageTransform,
  };

  return (
    <div className="team-card" style={gradientStyle}>
      <div className="team-card-image" style={imageLayerStyle}></div>
      <div className="team-card-content">
        <h3>{teamName}</h3>
        {ranking !== undefined && <h4>{ranking}</h4>}
      </div>
    </div>
  );
}

export default TeamCard;
