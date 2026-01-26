import { useState, useEffect, useCallback } from "react";
import { useEspData } from "../Contexts/EspDataContext";

interface BleJsonData {
  [key: string]: any;
}

type DataChangeCallback = (newData: BleJsonData, deviceName: string) => void;

function UseBleJsonReceiver() {
  const { devices } = useEspData();
  const [previousData, setPreviousData] = useState<Map<string, BleJsonData>>(
    new Map(),
  );
  const [callbacks, setCallbacks] = useState<DataChangeCallback[]>([]);

  // Watch for changes in devices and trigger events
  useEffect(() => {
    devices.forEach((device, deviceName) => {
      const prevDeviceData = previousData.get(deviceName);

      // Maak object van device data
      const currentData: BleJsonData = {
        value: device.data,
        rssi: device.rssi,
        connected: device.connected,
      };

      // Controleer of data veranderd is
      if (
        !prevDeviceData ||
        JSON.stringify(prevDeviceData) !== JSON.stringify(currentData)
      ) {
        // Trigger alle callbacks met nieuwe data
        callbacks.forEach((callback) => {
          callback(currentData, deviceName);
        });

        // Update previous data
        setPreviousData((prev) => {
          const newMap = new Map(prev);
          newMap.set(deviceName, currentData);
          return newMap;
        });
      }
    });
  }, [devices, callbacks, previousData]);

  // Subscribe functie voor callbacks
  const subscribe = useCallback((callback: DataChangeCallback) => {
    setCallbacks((prev) => [...prev, callback]);

    // Return unsubscribe functie
    return () => {
      setCallbacks((prev) => prev.filter((cb) => cb !== callback));
    };
  }, []);

  return {
    subscribe,
    getAllDeviceData: () => {
      const allData = new Map<string, BleJsonData>();
      devices.forEach((device, name) => {
        allData.set(name, {
          value: device.data,
          rssi: device.rssi,
          connected: device.connected,
        });
      });
      return allData;
    },
  };
}

export default UseBleJsonReceiver;
