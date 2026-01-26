
import { useCallback } from "react";
import { useEspData } from "../Contexts/EspDataContext";

export interface CentralUnitData {
  From: string;
  Time: number;
  Score: number[];
  Accuracy: number[];
  GoalAttempt: number[];
}

export interface ParsedDeviceData {
  deviceName: string;
  time: number;
  scores: number[];
  accuracies: number[];
  attempts: number[];
  totalScore: number;
  averageScore: number;
  averageAccuracy: number;
}

export function useCentralUnitData() {
  const { devices } = useEspData();

  // Parse CentralUnit data from JSON string
  const parseDeviceData = useCallback((deviceName: string): ParsedDeviceData | null => {
    const device = devices.get(deviceName);
    if (!device || !device.data) return null;

    try {
      const data = JSON.parse(device.data) as CentralUnitData;
      
      // Calculate derived values
      const totalScore = data.Score.reduce((sum, score) => sum + score, 0);
      const averageScore = data.Score.length > 0 ? totalScore / data.Score.length : 0;
      const averageAccuracy = data.Accuracy.length > 0 
        ? data.Accuracy.reduce((sum, acc) => sum + acc, 0) / data.Accuracy.length 
        : 0;

      return {
        deviceName: data.From,
        time: data.Time,
        scores: data.Score,
        accuracies: data.Accuracy,
        attempts: data.GoalAttempt,
        totalScore,
        averageScore,
        averageAccuracy,
      };
    } catch (error) {
      console.error(`Error parsing data for ${deviceName}:`, error);
      return null;
    }
  }, [devices]);

  // Get all CentralUnit devices
  const getAllCentralUnitDevices = useCallback((): ParsedDeviceData[] => {
    const result: ParsedDeviceData[] = [];
    
    devices.forEach((device, deviceName) => {
      try {
        // Try to parse as CentralUnit data
        const parsed = JSON.parse(device.data);
        if (parsed.From && parsed.Score) {
          const deviceData = parseDeviceData(deviceName);
          if (deviceData) {
            result.push(deviceData);
          }
        }
      } catch {
        // Not CentralUnit format, skip
      }
    });
    
    return result;
  }, [devices, parseDeviceData]);

  // Subscribe to device updates
  const subscribeToDevice = useCallback((
    deviceName: string,
    callback: (data: ParsedDeviceData) => void
  ) => {
    const device = devices.get(deviceName);
    if (!device) return null;

    // You might want to implement a more sophisticated subscription system
    // This is a basic implementation
    const intervalId = setInterval(() => {
      const parsedData = parseDeviceData(deviceName);
      if (parsedData) {
        callback(parsedData);
      }
    }, 1000); // Check every second

    return () => clearInterval(intervalId);
  }, [devices, parseDeviceData]);

  return {
    parseDeviceData,
    getAllCentralUnitDevices,
    subscribeToDevice,
    // Helper functions
    getTeamScore: (deviceName: string): number => {
      const data = parseDeviceData(deviceName);
      return data ? data.totalScore : 0;
    },
    getPlayerScore: (deviceName: string, playerIndex: number): number | null => {
      const data = parseDeviceData(deviceName);
      return data && data.scores[playerIndex] !== undefined 
        ? data.scores[playerIndex] 
        : null;
    },
    getPlayerAccuracy: (deviceName: string, playerIndex: number): number | null => {
      const data = parseDeviceData(deviceName);
      return data && data.accuracies[playerIndex] !== undefined 
        ? data.accuracies[playerIndex] 
        : null;
    },
    getPlayerAttempts: (deviceName: string, playerIndex: number): number | null => {
      const data = parseDeviceData(deviceName);
      return data && data.attempts[playerIndex] !== undefined 
        ? data.attempts[playerIndex] 
        : null;
    },
  };
}