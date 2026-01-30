import React, { useState, useEffect, useRef } from "react";
import "./timer.css";
import { supabase } from "../../supabaseClient";
import { useUser } from "@clerk/clerk-react";
import { useTime, useHalfTime } from "../../Hooks/Commands";

export const TIMER_DURATION = 0.1; // time in minutes

interface TimerProps {
  timeLeft: number;
  setTimeLeft: (time: number) => void;
  isRunning: boolean;
  setIsRunning: (running: boolean) => void;
  currentGameId: string | null;
  setCurrentGameId: (id: string | null) => void;
  onResetScores?: () => void;
}

interface HalfIndication {
  half: string;
}

function Timer({
  timeLeft,
  setTimeLeft,
  isRunning,
  setIsRunning,
  currentGameId,
  setCurrentGameId,
  onResetScores,
}: TimerProps) {
  const { user } = useUser();
  const { start: sendStart, stop: sendStop, reset: sendReset } = useTime();
  const { setHalfTime: sendHalfTime } = useHalfTime();
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const startTimeRef = useRef<number | null>(null);
  const pausedTimeRef = useRef<number>(TIMER_DURATION * 60 * 100);
  const [half, setHalf] = useState<"1st" | "2nd">("1st");

  useEffect(() => {
    if (isRunning && timeLeft > 0) {
      startTimeRef.current = Date.now();
      pausedTimeRef.current = timeLeft;

      intervalRef.current = setInterval(() => {
        const elapsed = Math.floor((Date.now() - startTimeRef.current!) / 10);
        const newTime = pausedTimeRef.current - elapsed;

        if (newTime <= 0) {
          setTimeLeft(0);
          setIsRunning(false);
          if (intervalRef.current) {
            clearInterval(intervalRef.current);
          }
          // Only finish game if it's the end of 2nd half
          if (half === "2nd" && currentGameId) {
            finishGame();
          }
        } else {
          setTimeLeft(newTime);
        }
      }, 10);
    } else {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    }

    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [isRunning, currentGameId, half]);

  const formatTime = (centiseconds: number) => {
    const totalSeconds = Math.floor(centiseconds / 100);
    const mins = Math.floor(totalSeconds / 60);
    const secs = totalSeconds % 60;
    const cs = centiseconds % 100;
    return `${mins.toString().padStart(2, "0")}:${secs
      .toString()
      .padStart(2, "0")}.${cs.toString().padStart(2, "0")}`;
  };

  const handleStop = () => {
    setIsRunning(false);
  };

  const handleToggle = () => {
    if (timeLeft > 0) {
      if (!isRunning) {
        handleStart();
      } else {
        setIsRunning(false);
        sendStop();
      }
    }
  };

  const handleReset = () => {
    setIsRunning(false);
    setTimeLeft(TIMER_DURATION * 60 * 100);
    // If we're in 1st half and timer is at 0, switch to 2nd half
    if (half === "1st" && timeLeft === 0) {
      sendHalfTime(true);
      setHalf("2nd");
    }
    // If we're in 2nd half, reset scores and go back to 1st half
    else if (half === "2nd") {
      if (onResetScores) {
        sendHalfTime(false);
        onResetScores();
      }
      setHalf("1st");
    }
    sendReset();
  };

  const toggleHalf = () => {
    setHalf(half === "1st" ? "2nd" : "1st");
  };

  const finishGame = async () => {
    try {
      const { error } = await supabase
        .from("games")
        .update({
          end_time: new Date().toISOString(),
        })
        .eq("id", currentGameId);

      if (error) {
        console.error("Error finishing game:", error);
      } else {
        console.log("Game finished with end_time");
        setCurrentGameId(null);
        try {
          localStorage.removeItem("currentGameId");
        } catch (_) {}
      }
    } catch (err) {
      console.error(err);
    }
  };

  const handleStart = async () => {
    if (timeLeft > 0) {
      // Check of er al een game is
      if (!currentGameId) {
        try {
          const { data, error } = await supabase
            .from("games")
            .insert({
              start_time: new Date().toISOString(),
              home: "Home",
              away: "Away",
              user: user?.id || null,
            })
            .select()
            .single();

          if (error) {
            console.error("Error creating game:", error);
          } else {
            setCurrentGameId(data.id);
            try {
              localStorage.setItem("currentGameId", data.id);
            } catch (_) {}
            console.log("New game created:", data);
          }
        } catch (err) {
          console.error(err);
        }
      }

      setIsRunning(true);
      sendStart();
    }
  };

  return (
    <div className="timer-container">
      <div className="half-and-time">
        <div
          className="half-indicator"
          onClick={toggleHalf}
          style={{ cursor: "pointer" }}
        >
          <h3>{half}</h3>
        </div>
        <h2 className={`time ${timeLeft === 0 ? "time-expired" : ""}`}>
          {formatTime(timeLeft)}
        </h2>
      </div>
      <div className="timer-buttons">
        <button
          className="timer-button start"
          onClick={handleToggle}
          disabled={timeLeft === 0}
        >
          {isRunning ? "Stop" : "Start"}
        </button>
        <button className="timer-button reset" onClick={handleReset}>
          Reset
        </button>
      </div>
    </div>
  );
}

export default Timer;
