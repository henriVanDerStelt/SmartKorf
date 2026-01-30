import { useEspData } from "../Contexts/EspDataContext";

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
    }
  | {
      Command: "HALFTIME";
      Value: boolean;
    };

const sendCommand = (
  command: Command,
  sendMessage: (message: string) => Promise<void>,
) => {
  const jsonCommand = JSON.stringify(command);
  console.log("Sending command:", jsonCommand);
  sendMessage(jsonCommand);
};

export const useChangeNames = () => {
  const { sendCommand: sendEspCommand } = useEspData();

  const changeNames = (homeTeam: string, awayTeam: string) => {
    const command: Command = {
      Command: "CHANGE_NAMES",
      Value: [homeTeam, awayTeam],
    };
    sendCommand(command, sendEspCommand);
  };

  return { changeNames };
};

export const useTime = () => {
  const { sendCommand: sendEspCommand } = useEspData();

  const start = () => {
    const command: Command = {
      Command: "TIME",
      Value: "START",
    };
    sendCommand(command, sendEspCommand);
  };

  const stop = () => {
    const command: Command = {
      Command: "TIME",
      Value: "STOP",
    };
    sendCommand(command, sendEspCommand);
  };

  const reset = () => {
    const command: Command = {
      Command: "TIME",
      Value: "RESET",
    };
    sendCommand(command, sendEspCommand);
  };

  return { start, stop, reset };
};

export const useScore = () => {
  const { sendCommand: sendEspCommand } = useEspData();

  const updateScore = (teamName: string, action: "INCREMENT" | "DECREMENT") => {
    const command: Command = {
      Command: "SCORE",
      Value: [teamName, action],
    };
    sendCommand(command, sendEspCommand);
  };

  const increment = (teamName: string) => {
    updateScore(teamName, "INCREMENT");
  };

  const decrement = (teamName: string) => {
    updateScore(teamName, "DECREMENT");
  };

  return { updateScore, increment, decrement };
};

export const useHalfTime = () => {
  const { sendCommand: sendEspCommand } = useEspData();

  const setHalfTime = (boolean: boolean) => {
    const command: Command = {
      Command: "HALFTIME",
      Value: boolean,
    };
    sendCommand(command, sendEspCommand);
  };

  return { setHalfTime };
};
