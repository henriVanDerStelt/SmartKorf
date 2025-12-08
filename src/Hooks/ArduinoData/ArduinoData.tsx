import { useState, useEffect } from "react";

export default function useArduinoData() {
  const [data, setData] = useState({ home: 0, away: 0 });

  useEffect(() => {
    const interval = setInterval(async () => {
      try {
        const res = await fetch("http://192.168.4.1");
        const json = await res.json();
        setData(json);
        console.log("Fetched Arduino data:", json);
      } catch (err) {
        console.error(err);
      }
    }, 1000);

    return () => clearInterval(interval);
  }, []);

  return data;
}
