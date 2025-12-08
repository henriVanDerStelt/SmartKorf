import { useState, useEffect } from "react";

// Voor TS: bluetooth en characteristic type fix
declare global {
  interface Navigator {
    bluetooth: any;
  }
}

export default function useEspData() {
  const [data, setData] = useState({ home: 0, away: 0 });
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    const connectToESP32 = async () => {
      try {
        console.log("Requesting BLE device...");
        const device = await (navigator as any).bluetooth.requestDevice({
          filters: [{ namePrefix: "ESP32" }],
          optionalServices: ["1234"],
        });

        console.log(`Connected to device: ${device.name}`);
        const server = await device.gatt.connect();

        console.log("Getting primary service...");
        const service = await server.getPrimaryService("1234");

        console.log("Getting characteristic...");
        const characteristic = await service.getCharacteristic("5678");

        setIsConnected(true);

        // Start notificaties
        await characteristic.startNotifications();
        characteristic.addEventListener(
          "characteristicvaluechanged",
          (event: Event) => {
            try {
              const value = new TextDecoder().decode(
                (event.target as any).value
              );
              console.log("ESP32 data received:", value);
              const parsedData = JSON.parse(value);
              setData(parsedData);
            } catch (parseErr) {
              console.error("Error parsing ESP32 data:", parseErr);
            }
          }
        );
      } catch (err) {
        console.error("BLE Connection Error:", (err as Error).message);
        setIsConnected(false);
      }
    };

    connectToESP32();
  }, []);

  return data;
}
