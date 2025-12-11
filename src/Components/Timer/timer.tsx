import React, { useState, useEffect, useRef } from "react";
import "./timer.css";

function Timer() {
  const [timeLeft, setTimeLeft] = useState(30 * 60 * 100); // 30 minuten in centiseconden (1/100 sec)
  const [isRunning, setIsRunning] = useState(false);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const startTimeRef = useRef<number | null>(null);
  const pausedTimeRef = useRef<number>(30 * 60 * 100);

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
  }, [isRunning]);

  const formatTime = (centiseconds: number) => {
    const totalSeconds = Math.floor(centiseconds / 100);
    const mins = Math.floor(totalSeconds / 60);
    const secs = totalSeconds % 60;
    const cs = centiseconds % 100;
    return `${mins.toString().padStart(2, "0")}:${secs
      .toString()
      .padStart(2, "0")}.${cs.toString().padStart(2, "0")}`;
  };

  const handleStart = () => {
    if (timeLeft > 0) {
      setIsRunning(true);
    }
  };

  const handleStop = () => {
    setIsRunning(false);
  };

  const handleToggle = () => {
    if (timeLeft > 0) {
      setIsRunning(!isRunning);
    }
  };

  const handleReset = () => {
    setIsRunning(false);
    setTimeLeft(30 * 60 * 100);
  };

  return (
    <div className="timer-container">
      <h2 className={`time ${timeLeft === 0 ? "time-expired" : ""}`}>
        {formatTime(timeLeft)}
      </h2>
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
