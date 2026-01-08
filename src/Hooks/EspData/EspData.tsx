import { useState, useRef } from "react";

declare global {
  interface Navigator {
    bluetooth: any;
  }
}

export default function useEspData() {
  const [data, setData] = useState({ home: 0, away: 0 });
  const [isConnected, setIsConnected] = useState(false);
  const characteristicRef = useRef<any>(null);

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

      characteristicRef.current = characteristic;
      setIsConnected(true);

      await characteristic.startNotifications();
      characteristic.addEventListener(
        "characteristicvaluechanged",
        (event: Event) => {
          const value = new TextDecoder().decode((event.target as any).value);
          console.log("Bluetooth message received:", value);
          try {
            setData(JSON.parse(value));
          } catch {
            console.error("Invalid JSON from ESP32:", value);
          }
        }
      );
    } catch (err) {
      console.error("BLE Connection Error:", (err as Error).message);
      setIsConnected(false);
    }
  };

  const sendMessage = async (message: string) => {
    if (!characteristicRef.current) {
      console.error("Not connected to ESP32");
      return;
    }

    try {
      const encoder = new TextEncoder();
      const data = encoder.encode(message);
      await characteristicRef.current.writeValue(data);
      console.log("Message sent to ESP32:", message);
    } catch (err) {
      console.error("Error sending message:", (err as Error).message);
    }
  };

  const sendHelloWorld = () => {
    sendMessage("hello world");
  };

  return { data, isConnected, connectToESP32, sendMessage, sendHelloWorld };
}
