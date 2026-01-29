import React, {
  createContext,
  useContext,
  useState,
  useRef,
  ReactNode,
} from "react";

declare global {
  interface Navigator {
    bluetooth: any;
  }
}

interface DeviceData {
  name: string;
  data: string;
  rssi: number;
  connected: boolean;
}

interface GatewayMessage {
  sender?: string;
  data?: string;
  rssi?: number;
  timestamp?: number;
}

interface EspDataContextType {
  devices: Map<string, DeviceData>;
  isConnected: boolean;
  connectionStatus: string;
  logMessages: string[];
  connectToGateway: () => Promise<void>;
  disconnectGateway: () => Promise<void>;
  refreshDevices: () => Promise<void>;
  sendCommand: (command: string) => Promise<void>;
  clearLog: () => void;
}

const EspDataContext = createContext<EspDataContextType | undefined>(undefined);

export const EspDataProvider = ({ children }: { children: ReactNode }) => {
  const [devices, setDevices] = useState<Map<string, DeviceData>>(new Map());
  const [isConnected, setIsConnected] = useState(false);
  const [connectionStatus, setConnectionStatus] = useState("Disconnected");
  const [logMessages, setLogMessages] = useState<string[]>([
    "PWA loaded. Click 'Connect to Gateway' to start.",
  ]);

  const deviceRef = useRef<any>(null);
  const serverRef = useRef<any>(null);
  const dataCharacteristicRef = useRef<any>(null);
  const statusCharacteristicRef = useRef<any>(null);
  const commandCharacteristicRef = useRef<any>(null);

  // UUIDs (must match ESP32 gateway code)
  const GATEWAY_SERVICE_UUID = "12345678-1234-1234-1234-123456789abc";
  const GATEWAY_CHAR_DATA_UUID = "22345678-1234-1234-1234-123456789abc";
  const GATEWAY_CHAR_STATUS_UUID = "32345678-1234-1234-1234-123456789abc";
  const GATEWAY_CHAR_COMMAND_UUID = "42345678-1234-1234-1234-123456789abc";

  // Log function
  const addLog = (message: string) => {
    const timestamp = new Date().toLocaleTimeString();
    const logEntry = `[${timestamp}] ${message}`;
    setLogMessages((prev) => [logEntry, ...prev.slice(0, 19)]); // Keep last 20 entries
    console.log(message);
  };

  const clearLog = () => {
    setLogMessages(["[Log cleared]"]);
  };

  // Parse nested JSON in data field
  const parseNestedJsonData = (data: string): string => {
    try {
      // Try to parse the data as JSON
      const parsed = JSON.parse(data);

      // If it has a score field, extract just the score
      if (parsed && typeof parsed === "object" && "score" in parsed) {
        return parsed.score.toString();
      }

      // If it's just a number, return it
      if (typeof parsed === "number") {
        return parsed.toString();
      }

      // Return the original data if we can't extract a score
      return data;
    } catch (error) {
      // If it's not valid JSON, check if it's just a number
      const num = parseFloat(data);
      if (!isNaN(num)) {
        return num.toString();
      }
      return data;
    }
  };

  // Attempt to parse gateway payloads even if they are poorly formatted
  const safeParseGatewayPayload = (
    payload: string,
  ):
    | Array<GatewayMessage | DeviceData>
    | GatewayMessage
    | DeviceData[]
    | null => {
    try {
      // First try regular JSON
      return JSON.parse(payload);
    } catch (e1) {
      // Fallback: many gateways send arrays of single-quoted JSON strings
      // Example: ["{'sender':'CentralUnit','data':'{"Score":[2,4]}'}", ...]
      try {
        // Replace single quotes with double quotes for JSON validity
        const normalized = payload.replace(/'/g, '"');
        const outer = JSON.parse(normalized);
        if (
          Array.isArray(outer) &&
          outer.every((el: any) => typeof el === "string")
        ) {
          // Each element is a JSON string; parse each into an object
          const parsedInner = outer
            .map((s: string) => {
              try {
                return JSON.parse(s);
              } catch (e2) {
                return null;
              }
            })
            .filter((x) => x !== null) as Array<GatewayMessage | DeviceData>;
          return parsedInner;
        }
        // If outer was not an array of strings, return it as-is
        return outer;
      } catch (e2) {
        // Fallback: try to extract inner JSON if payload is quoted JSON
        try {
          const trimmed = payload.trim();
          if (
            (trimmed.startsWith('"') && trimmed.endsWith('"')) ||
            (trimmed.startsWith("'") && trimmed.endsWith("'"))
          ) {
            const inner = trimmed.slice(1, -1);
            return JSON.parse(inner);
          }
        } catch (e3) {
          // give up
        }
        addLog(`safeParse failed: ${String(e1)} | ${String(e2)}`);
        return null;
      }
    }
  };

  // Handle incoming data from gateway
  const handleGatewayData = (data: string) => {
    try {
      // First, try to parse the main JSON (robustly)
      const jsonData = safeParseGatewayPayload(data) as
        | GatewayMessage
        | DeviceData[]
        | Array<GatewayMessage | DeviceData>
        | null;

      if (jsonData === null) {
        throw new Error("Failed to parse gateway payload");
      }

      if (Array.isArray(jsonData)) {
        // Device list received - parse each device's data
        const newDevices = new Map<string, DeviceData>();
        jsonData.forEach((item: any) => {
          // Normalize shape: handle both DeviceData and GatewayMessage
          const deviceName = item.name || item.sender || "Unknown";
          const rssi = item.rssi ?? 0;
          const connected = true;
          const rawData = item.data ?? item.value ?? "";
          const parsedData =
            typeof rawData === "string"
              ? parseNestedJsonData(rawData)
              : parseNestedJsonData(JSON.stringify(rawData));

          newDevices.set(deviceName, {
            name: deviceName,
            data: parsedData,
            rssi,
            connected,
          });
        });
        setDevices(newDevices);
        addLog(`Received device list (${jsonData.length} devices)`);
      } else if (jsonData.sender && jsonData.data !== undefined) {
        // Single device data received
        const deviceName = jsonData.sender;

        // Parse nested JSON in data field
        const parsedData = parseNestedJsonData(jsonData.data as string);

        const deviceData = {
          name: deviceName,
          data: parsedData,
          rssi: jsonData.rssi || 0,
          connected: true,
        };

        setDevices((prev) => {
          const newDevices = new Map(prev);
          newDevices.set(deviceName, deviceData);
          return newDevices;
        });

        addLog(`Data from ${deviceName}: ${parsedData}`);
      }
    } catch (error) {
      addLog(`Error parsing data: ${error}. Raw data: ${data}`);

      // Try to handle as raw score data (just a number)
      const num = parseFloat(data);
      if (!isNaN(num)) {
        // This might be a raw score from a sender
        const deviceData = {
          name: "Unknown",
          data: num.toString(),
          rssi: 0,
          connected: true,
        };

        setDevices((prev) => {
          const newDevices = new Map(prev);
          newDevices.set("Unknown", deviceData);
          return newDevices;
        });

        addLog(`Parsed raw score: ${num}`);
      }
    }
  };

  // Connect to BLE Gateway
  const connectToGateway = async () => {
    try {
      addLog("Requesting BLE device...");
      setConnectionStatus("Searching for gateway...");

      // Request device with the correct name from ESP32 code
      const device = await (navigator as any).bluetooth.requestDevice({
        filters: [{ name: "ESP32-BLE-Gateway" }],
        optionalServices: [GATEWAY_SERVICE_UUID],
      });

      deviceRef.current = device;
      addLog(`Found device: ${device.name}`);

      deviceRef.current.addEventListener("gattserverdisconnected", () => {
        addLog("Disconnected from BLE Gateway");
        setIsConnected(false);
        setConnectionStatus("Disconnected");
      });

      addLog("Connecting to GATT server...");
      setConnectionStatus("Connecting...");
      const server = await deviceRef.current.gatt.connect();
      serverRef.current = server;

      addLog("Getting service...");
      const service =
        await serverRef.current.getPrimaryService(GATEWAY_SERVICE_UUID);

      addLog("Getting characteristics...");
      // Get data characteristic
      const dataCharacteristic = await service.getCharacteristic(
        GATEWAY_CHAR_DATA_UUID,
      );
      dataCharacteristicRef.current = dataCharacteristic;

      // Get status characteristic
      const statusCharacteristic = await service.getCharacteristic(
        GATEWAY_CHAR_STATUS_UUID,
      );
      statusCharacteristicRef.current = statusCharacteristic;

      // Get command characteristic
      const commandCharacteristic = await service.getCharacteristic(
        GATEWAY_CHAR_COMMAND_UUID,
      );
      commandCharacteristicRef.current = commandCharacteristic;

      // Subscribe to data notifications
      await dataCharacteristicRef.current.startNotifications();
      dataCharacteristicRef.current.addEventListener(
        "characteristicvaluechanged",
        (event: Event) => {
          try {
            const value = new TextDecoder().decode((event.target as any).value);
            handleGatewayData(value);
          } catch (err) {
            addLog(`Error processing notification: ${err}`);
          }
        },
      );

      // Subscribe to status notifications
      await statusCharacteristicRef.current.startNotifications();

      // Read initial status
      const statusValue = await statusCharacteristicRef.current.readValue();
      const status = new TextDecoder().decode(statusValue);
      addLog(`Gateway status: ${status}`);

      setIsConnected(true);
      setConnectionStatus("Connected");
      addLog("✅ Successfully connected to BLE Gateway");

      // Request initial device list
      await refreshDevices();
    } catch (err) {
      addLog(`Connection failed: ${(err as Error).message}`);
      setIsConnected(false);
      setConnectionStatus("Connection failed");
    }
  };

  // Disconnect from gateway
  const disconnectGateway = async () => {
    if (deviceRef.current && deviceRef.current.gatt.connected) {
      try {
        deviceRef.current.gatt.disconnect();
        addLog("Disconnected from BLE Gateway");
      } catch (err) {
        addLog(`Error disconnecting: ${(err as Error).message}`);
      }
    }
    setIsConnected(false);
    setConnectionStatus("Disconnected");
    setDevices(new Map());

    // Clear refs
    deviceRef.current = null;
    serverRef.current = null;
    dataCharacteristicRef.current = null;
    statusCharacteristicRef.current = null;
    commandCharacteristicRef.current = null;
  };

  // Refresh device list
  const refreshDevices = async () => {
    if (!commandCharacteristicRef.current) {
      addLog("Not connected to gateway");
      return;
    }

    try {
      const encoder = new TextEncoder();
      await commandCharacteristicRef.current.writeValue(
        encoder.encode("get_devices"),
      );
      addLog("Requested device list from gateway");
    } catch (err) {
      addLog(`Error refreshing devices: ${(err as Error).message}`);
    }
  };

  // Send command to gateway
  const sendCommand = async (command: string) => {
    if (!commandCharacteristicRef.current) {
      addLog("Not connected to gateway");
      return;
    }

    try {
      const encoder = new TextEncoder();
      await commandCharacteristicRef.current.writeValue(
        encoder.encode(command),
      );
      addLog(`Command sent: ${command}`);
    } catch (err) {
      addLog(`Error sending command: ${(err as Error).message}`);
    }
  };

  return (
    <EspDataContext.Provider
      value={{
        devices,
        isConnected,
        connectionStatus,
        logMessages,
        connectToGateway,
        disconnectGateway,
        refreshDevices,
        sendCommand,
        clearLog,
      }}
    >
      {children}
    </EspDataContext.Provider>
  );
};

export const useEspData = () => {
  const context = useContext(EspDataContext);
  console.log("EspDataContext:", context);
  if (!context) {
    throw new Error("useEspData must be used within EspDataProvider");
  }
  return context;
};
