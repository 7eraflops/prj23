interface Network {
  ssid: string;
  rssi: number;
  secure: boolean;
}

interface Broker {
  hostname: string;
  ip: string;
  port: number;
  protocol: string;
  txt: Record<string, string>;
}

interface AppConfig {
  ssid: string;
  mqtt_ip: string;
  mqtt_user: string;
  configured: boolean;
}

const statusMsg = document.getElementById("statusMsg") as HTMLDivElement;
const ssidSelect = document.getElementById("ssid") as HTMLSelectElement;

const step1 = document.getElementById("step1") as HTMLDivElement;
const step2 = document.getElementById("step2") as HTMLDivElement;
const nav = document.getElementById("nav") as HTMLElement;

const wifiForm = document.getElementById("wifiForm") as HTMLFormElement;
const mqttForm = document.getElementById("mqttForm") as HTMLFormElement;

const tabWifi = document.getElementById("tabWifi") as HTMLButtonElement;
const tabMqtt = document.getElementById("tabMqtt") as HTMLButtonElement;

const wifiTitle = document.getElementById("wifiTitle") as HTMLElement;
const mqttTitle = document.getElementById("mqttTitle") as HTMLElement;

const discoveryList = document.getElementById(
  "discoveryList",
) as HTMLDivElement;

let isConfigured = false;

/**
 * Display a status message to the user
 */
function setStatus(
  message: string,
  type: "info" | "error" | "success" | "none",
) {
  if (type === "none") {
    statusMsg.style.display = "none";
    return;
  }
  statusMsg.innerHTML = message;
  statusMsg.className = `status ${type}`;
  statusMsg.style.display = "block";
}

/**
 * Switch between tabs
 */
function switchTab(tab: "wifi" | "mqtt") {
  if (tab === "wifi") {
    step1.classList.add("active");
    step2.classList.remove("active");
    tabWifi.classList.add("active");
    tabMqtt.classList.remove("active");
    fetchNetworks();
  } else {
    step1.classList.remove("active");
    step2.classList.add("active");
    tabWifi.classList.remove("active");
    tabMqtt.classList.add("active");
    discoverBrokers();
  }
}

tabWifi.addEventListener("click", () => switchTab("wifi"));
tabMqtt.addEventListener("click", () => switchTab("mqtt"));

/**
 * Fetch current configuration
 */
async function fetchConfig() {
  try {
    const response = await fetch("/api/config");
    const config: AppConfig = await response.json();
    isConfigured = config.configured;

    if (isConfigured) {
      nav.style.display = "flex";
      wifiTitle.textContent = "Wi-Fi Settings";
      mqttTitle.textContent = "MQTT Settings";
      (document.getElementById("wifiBtn") as HTMLButtonElement).textContent = "Update Wi-Fi";
      (document.getElementById("saveBtn") as HTMLButtonElement).textContent = "Update MQTT";
      
      // Populate MQTT fields
      if (config.mqtt_ip) {
        const parts = config.mqtt_ip.split("://");
        let protocol = "mqtt://";
        let hostPort = config.mqtt_ip;
        if (parts.length > 1) {
            protocol = parts[0] + "://";
            hostPort = parts[1];
        }
        
        const hostParts = hostPort.split(":");
        (document.getElementById("mqtt_type") as HTMLSelectElement).value = protocol;
        (document.getElementById("mqtt_host") as HTMLInputElement).value = hostParts[0];
        if (hostParts.length > 1) {
            (document.getElementById("mqtt_port") as HTMLInputElement).value = hostParts[1];
        }
      }
      (document.getElementById("mqtt_user") as HTMLInputElement).value = config.mqtt_user || "";
    }
  } catch (err) {
    console.error("Config fetch error:", err);
  }
}

/**
 * Fetch available Wi-Fi networks
 */
async function fetchNetworks() {
  try {
    const response = await fetch("/api/scan");
    const networks: Network[] = await response.json();

    const currentSsid = ssidSelect.value;
    ssidSelect.innerHTML = '<option value="">Select a network...</option>';
    networks.forEach((net) => {
      const opt = document.createElement("option");
      opt.value = net.ssid;
      opt.textContent = `${net.ssid} (${net.rssi}dBm)${net.secure ? " 🔒" : ""}`;
      ssidSelect.appendChild(opt);
    });
    if (currentSsid) ssidSelect.value = currentSsid;
  } catch (err) {
    console.error("Scan error:", err);
  }
}

/**
 * Handle Wi-Fi form submission (Step 1)
 */
wifiForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const formData = new FormData(wifiForm);
  const payload = Object.fromEntries(formData.entries());

  setStatus("Saving WiFi configuration and connecting...", "info");
  (document.getElementById("wifiBtn") as HTMLButtonElement).disabled = true;

  try {
    const response = await fetch("/api/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });

    if (!response.ok) throw new Error("Save failed");

    // We start polling
    pollConnection();
  } catch (err) {
    console.warn(
      "Save request error, starting poll anyway (network switch probable)",
    );
    pollConnection();
  }
});

async function pollConnection() {
  let attempts = 0;
  let successCount = 0;

  const interval = setInterval(async () => {
    attempts++;
    try {
      const res = await fetch("/api/wifi-status", {
        signal: AbortSignal.timeout(1500),
      });
      const data = await res.json();

      if (data.connected && data.sta_ip !== "0.0.0.0") {
        successCount++;
        // Ensure connection is stable
        if (successCount >= 2) {
          clearInterval(interval);
          handleConnectedState(data.sta_ip);
        }
      } else {
        successCount = 0;
        if (attempts > 40) {
          clearInterval(interval);
          setStatus(
            "Timed out waiting for WiFi. Please check SSID/Password and try again.",
            "error",
          );
          (document.getElementById("wifiBtn") as HTMLButtonElement).disabled =
            false;
        }
      }
    } catch (e) {
      // Fetch error is expected while switching network or if IP changes
      if (attempts > 15) {
        clearInterval(interval);
        handlePotentialIPShift();
      }
    }
  }, 2000);
}

function handleConnectedState(staIp: string) {
  const localUrl = "http://energy-meter.local";
  const ipUrl = `http://${staIp}`;
  const isCurrentlyLocal =
    window.location.hostname === "energy-meter.local" ||
    window.location.hostname === staIp;

  if (!isCurrentlyLocal) {
    setStatus(
      `<strong>Connected to Wi-Fi!</strong><br>
       The device is now reachable at its new IP: <strong>${staIp}</strong><br><br>
       <a href="${localUrl}" style="color:white;font-weight:bold;display:block;margin-bottom:10px;">Go to energy-meter.local</a>
       <a href="${ipUrl}" style="color:white;text-decoration:underline;">Or use direct IP: ${staIp}</a>`,
      "success",
    );

    // Attempt automatic redirect to mDNS after 4 seconds
    setTimeout(() => {
      window.location.href = localUrl;
    }, 4000);
  } else {
    if (!isConfigured) {
        switchToStep2();
    } else {
        setStatus("Wi-Fi Connected!", "success");
        setTimeout(() => setStatus("", "none"), 3000);
        (document.getElementById("wifiBtn") as HTMLButtonElement).disabled = false;
    }
  }
}

function handlePotentialIPShift() {
  const localUrl = "http://energy-meter.local";
  setStatus(
    `<strong>Connection Lost (Mode Change)</strong><br>
       The device has likely connected to Wi-Fi and moved to its new address.<br><br>
       <a href="${localUrl}" style="color:white;font-weight:bold;">Continue at energy-meter.local</a>`,
    "info",
  );

  // Try to auto-redirect
  setTimeout(() => {
    window.location.href = localUrl;
  }, 5000);
}

function switchToStep2() {
  setStatus("", "none"); // Clear the "Saving WiFi..." banner
  switchTab("mqtt");
}

/**
 * Discover MQTT Brokers via mDNS
 */
async function discoverBrokers() {
  discoveryList.innerHTML = "<p>Searching for local MQTT brokers (mDNS)...</p>";
  try {
    const res = await fetch("/api/mqtt-brokers");
    const brokers: Broker[] = await res.json();

    if (brokers.length === 0) {
      discoveryList.innerHTML = "<p>No brokers discovered automatically.</p>";
      return;
    }

    discoveryList.innerHTML = "<strong>Discovered Brokers:</strong>";
    brokers.forEach((b) => {
      const div = document.createElement("div");
      div.className = "discovery-item";
      div.textContent = `${b.hostname} (${b.ip}:${b.port})`;
      div.onclick = () => {
        (document.getElementById("mqtt_host") as HTMLInputElement).value = b.ip;
        (document.getElementById("mqtt_port") as HTMLInputElement).value =
          b.port.toString();

        // Protocol selection
        const mqttType = document.getElementById(
          "mqtt_type",
        ) as HTMLSelectElement;

        let protocol = b.protocol || "mqtt://";

        // Fallbacks in case backend logic missed it
        if (b.txt && (b.txt.protocol === "ws" || b.txt.transport === "ws")) {
          protocol = "ws://";
        } else if ([8080, 8083, 9001].includes(b.port)) {
          protocol = "ws://";
        }

        mqttType.value = protocol;
      };
      discoveryList.appendChild(div);
    });
  } catch (e) {
    discoveryList.innerHTML = "";
  }
}

/**
 * Handle MQTT form submission (Step 2)
 */
mqttForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const formData = new FormData(mqttForm);
  const data = Object.fromEntries(formData.entries());

  // Re-construct the full URI for the backend
  const mqtt_uri = `${data.mqtt_type}${data.mqtt_host}:${data.mqtt_port}`;

  const payload = {
    mqtt_uri,
    mqtt_user: data.mqtt_user,
    mqtt_pass: data.mqtt_pass,
  };

  setStatus("Saving MQTT configuration...", "info");
  (document.getElementById("saveBtn") as HTMLButtonElement).disabled = true;

  try {
    const response = await fetch("/api/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });

    if (!response.ok) throw new Error("Save failed");

    if (!isConfigured) {
        setStatus("All set! Rebooting device...", "success");
        await fetch("/api/reboot", { method: "POST" });

        // Disable inputs
        Array.from(mqttForm.elements).forEach((el) => {
            (el as HTMLInputElement).disabled = true;
        });
    } else {
        setStatus("MQTT configuration saved!", "success");
        setTimeout(() => setStatus("", "none"), 3000);
        (document.getElementById("saveBtn") as HTMLButtonElement).disabled = false;
    }
  } catch (err) {
    setStatus("Failed to save. Try again.", "error");
    (document.getElementById("saveBtn") as HTMLButtonElement).disabled = false;
  }
});

// Initialization
async function init() {
    await fetchConfig();
    fetchNetworks();
}

init();
