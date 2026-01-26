import { useState, useEffect } from "react";
import UseBleJsonReceiver from "./UseBleJsonReceiver";

type Command =
  | {
      Command: "CHANGE_NAMES";
      Value: [string, string];
    }
  | {
      Command: "TIME";
      Value: "START" | "STOP" | "RESET";
    }
  | {
      Command: "SCORE";
      Value: [string, "INCREMENT" | "DECREMENT"];
    };

interface CommandHandler {
  onChangeNames?: (homeTeam: string, awayTeam: string) => void;
  onTimeCommand?: (action: "START" | "STOP" | "RESET") => void;
  onScoreCommand?: (
    teamName: string,
    action: "INCREMENT" | "DECREMENT",
  ) => void;
}

export const useCommandReceiver = (handlers: CommandHandler) => {
  const receiver = UseBleJsonReceiver();
  const [lastCommand, setLastCommand] = useState<Command | null>(null);

  useEffect(() => {
    // Subscribe op data veranderingen
    const unsubscribe = receiver.subscribe((data, deviceName) => {
      try {
        // Parse command uit data
        const command = JSON.parse(
          typeof data.value === "string" ? data.value : JSON.stringify(data),
        ) as Command;

        // Log command ontvangst
        console.log(`Command ontvangen van ${deviceName}:`, command);

        // Zet command als laatst ontvangen
        setLastCommand(command);

        // Verwerk command op basis van type
        if (command.Command === "CHANGE_NAMES") {
          const [homeTeam, awayTeam] = command.Value;
          handlers.onChangeNames?.(homeTeam, awayTeam);
        } else if (command.Command === "TIME") {
          handlers.onTimeCommand?.(command.Value);
        } else if (command.Command === "SCORE") {
          const [teamName, action] = command.Value;
          handlers.onScoreCommand?.(teamName, action);
        }
      } catch (error) {
        console.error("Error parsing command:", error);
      }
    });

    return unsubscribe;
  }, [receiver, handlers]);

  return { lastCommand };
};
