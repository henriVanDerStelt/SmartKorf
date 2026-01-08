import React, { useState, useEffect, useRef } from "react";
import "./scorecounter.css";
import { supabase } from "../../supabaseClient";

interface ScoreCounterProps {
  data: number | string; // Changed to accept both number and string
  team: "home" | "away";
}

function ScoreCounter({ data, team }: ScoreCounterProps) {
  const [score, setScore] = useState(0);
  const prevScoreRef = useRef<number>(0);

  useEffect(() => {
    // Convert data to number if it's a string
    let numericData: number;

    if (typeof data === "string") {
      numericData = parseInt(data);
      if (isNaN(numericData)) {
        numericData = 0;
      }
    } else {
      numericData = data;
    }

    // Detect score increase from ESP data
    const prev = prevScoreRef.current;

    // Only update if the new score is different
    if (numericData !== score) {
      setScore(numericData);

      // Check if this is a score increase (not a reset)
      if (numericData > prev && numericData > 0) {
        const gameId = localStorage.getItem("currentGameId");
        if (gameId) {
          const is_home = team === "home";

          // Insert goal and update game score
          (async () => {
            try {
              // First insert the goal
              const { data: goalData, error: goalErr } = await supabase
                .from("goals")
                .insert({
                  game_id: gameId,
                  is_home,
                  scored_at: new Date().toISOString(),
                })
                .select();

              if (goalErr) {
                console.error("Error inserting goal:", goalErr);
              } else {
                console.log(`Goal inserted for ${team} team`);
              }

              // Then update the game score
              const column = is_home ? "home_score" : "away_score";
              const { data: gameData, error: gameErr } = await supabase
                .from("games")
                .update({ [column]: numericData })
                .eq("id", gameId)
                .select();

              if (gameErr) {
                console.error("Error updating game score:", gameErr);
              } else {
                console.log(`${team} score updated to ${numericData}`);
              }
            } catch (e) {
              console.error("Error in score update process:", e);
            }
          })();
        }
      }
    }

    prevScoreRef.current = numericData;
  }, [data, team, score]);

  const incrementScore = async () => {
    const newScore = score + 1;
    setScore(newScore);
    await updateScoreInDatabase(newScore);
  };

  const decrementScore = async () => {
    const newScore = score > 0 ? score - 1 : 0;
    setScore(newScore);
    await updateScoreInDatabase(newScore);
  };

  const updateScoreInDatabase = async (newScore: number) => {
    try {
      const gameId = localStorage.getItem("currentGameId");
      if (gameId) {
        const column = team === "home" ? "home_score" : "away_score";
        const is_home = team === "home";

        // Only insert goal if manually incrementing (not from BLE)
        // Since BLE updates already handle goal insertion in the useEffect
        if (newScore > score) {
          const { error: goalErr } = await supabase.from("goals").insert({
            game_id: gameId,
            is_home,
            scored_at: new Date().toISOString(),
          });

          if (goalErr) {
            console.error("Error inserting goal:", goalErr);
          } else {
            console.log(`Manual goal inserted for ${team}`);
          }
        }

        // Update game score
        const { error } = await supabase
          .from("games")
          .update({ [column]: newScore })
          .eq("id", gameId);

        if (error) {
          console.error("Error updating score:", error);
        } else {
          console.log(`${team} score updated to ${newScore}`);
        }
      }
    } catch (e) {
      console.error(e);
    }
  };

  // Format score display
  const displayScore = score.toString().padStart(2, "0");

  return (
    <div className={`score-counter ${team}`}>
      <h2 className="score">{displayScore}</h2>
      <div className="button-container">
        <button
          className="score-button decrement"
          onClick={decrementScore}
          aria-label={`Decrement ${team} score`}
        >
          -
        </button>
        <button
          className="score-button increment"
          onClick={incrementScore}
          aria-label={`Increment ${team} score`}
        >
          +
        </button>
      </div>
    </div>
  );
}

export default ScoreCounter;
