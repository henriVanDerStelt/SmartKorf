import React, { createContext, useContext, useState, ReactNode } from "react";

declare global {
  interface Navigator {
    bluetooth: any;
  }
}

interface EspData {
  home: number;
  away: number;
}

interface EspDataContextType {
  data: EspData;
  isConnected: boolean;
  connectToESP32: () => Promise<void>;
}

const EspDataContext = createContext<EspDataContextType | undefined>(undefined);

export const EspDataProvider = ({ children }: { children: ReactNode }) => {
  const [data, setData] = useState<EspData>({ home: 0, away: 0 });
  const [isConnected, setIsConnected] = useState(false);

  const connectToESP32 = async () => {
    try {
      console.log("Requesting BLE device...");
      const device = await (navigator as any).bluetooth.requestDevice({
        filters: [{ namePrefix: "centralUnit" }],
        optionalServices: ["3bd083ef-9c40-4fd1-992f-d0450276a783"],
      });

      console.log(`Connected to device: ${device.name}`);
      const server = await device.gatt.connect();

      const service = await server.getPrimaryService(
        "3bd083ef-9c40-4fd1-992f-d0450276a783"
      );
      const characteristic = await service.getCharacteristic(
        "a50704f1-ba55-44cf-96ec-2de6ded239d4"
      );

      setIsConnected(true);

      await characteristic.startNotifications();
      characteristic.addEventListener(
        "characteristicvaluechanged",
        (event: Event) => {
          const value = new TextDecoder().decode((event.target as any).value);
          console.log("Bluetooth message received:", value);
          try {
            const parsedData = JSON.parse(value);
            console.log("Parsed data:", parsedData);
            setData(parsedData);
          } catch (error) {
            console.error("Invalid JSON from ESP32:", value, error);
          }
        }
      );
    } catch (err) {
      console.error("BLE Connection Error:", (err as Error).message);
      setIsConnected(false);
    }
  };

  return (
    <EspDataContext.Provider value={{ data, isConnected, connectToESP32 }}>
      {children}
    </EspDataContext.Provider>
  );
};

export const useEspData = () => {
  const context = useContext(EspDataContext);
  if (!context) {
    throw new Error("useEspData must be used within EspDataProvider");
  }
  return context;
};
