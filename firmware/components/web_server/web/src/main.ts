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

interface ChannelCal {
  energy_offset_kwh: number;
}

interface CalibrationResponse {
  hardware: Record<string, unknown>;
  channels: ChannelCal[];
}

const statusMsg = document.getElementById("statusMsg") as HTMLDivElement;
const ssidSelect = document.getElementById("ssid") as HTMLSelectElement;

const step1 = document.getElementById("step1") as HTMLDivElement;
const step2 = document.getElementById("step2") as HTMLDivElement;
const step3 = document.getElementById("step3") as HTMLDivElement;
const step4 = document.getElementById("step4") as HTMLDivElement;
const nav = document.getElementById("nav") as HTMLElement;

const wifiForm = document.getElementById("wifiForm") as HTMLFormElement;
const mqttForm = document.getElementById("mqttForm") as HTMLFormElement;
const updateForm = document.getElementById("updateForm") as HTMLFormElement;
const calForm = document.getElementById("calForm") as HTMLFormElement;

const tabWifi = document.getElementById("tabWifi") as HTMLButtonElement;
const tabMqtt = document.getElementById("tabMqtt") as HTMLButtonElement;
const tabUpdate = document.getElementById("tabUpdate") as HTMLButtonElement;
const tabCal = document.getElementById("tabCal") as HTMLButtonElement;

const wifiTitle = document.getElementById("wifiTitle") as HTMLElement;
const mqttTitle = document.getElementById("mqttTitle") as HTMLElement;

const otaProgressContainer = document.getElementById("otaProgressContainer") as HTMLDivElement;
const otaProgressBar = document.getElementById("otaProgressBar") as HTMLDivElement;
const otaStatusText = document.getElementById("otaStatusText") as HTMLParagraphElement;
const otaBtn = document.getElementById("otaBtn") as HTMLButtonElement;

const discoveryList = document.getElementById(
  "discoveryList",
) as HTMLDivElement;

let isConfigured = false;

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

function switchTab(tab: "wifi" | "mqtt" | "cal" | "update") {
  step1.classList.toggle("active", tab === "wifi");
  step2.classList.toggle("active", tab === "mqtt");
  step3.classList.toggle("active", tab === "cal");
  step4.classList.toggle("active", tab === "update");
  
  tabWifi.classList.toggle("active", tab === "wifi");
  tabMqtt.classList.toggle("active", tab === "mqtt");
  tabUpdate.classList.toggle("active", tab === "update");
  tabCal.classList.toggle("active", tab === "cal");

  if (tab === "wifi") fetchNetworks();
  if (tab === "mqtt") discoverBrokers();
  if (tab === "cal") fetchCalibration();
}

tabWifi.addEventListener("click", () => switchTab("wifi"));
tabMqtt.addEventListener("click", () => switchTab("mqtt"));
tabUpdate.addEventListener("click", () => switchTab("update"));
tabCal.addEventListener("click", () => switchTab("cal"));

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
      tabCal.style.display = "block";
      
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
    } else {
      tabUpdate.style.display = "none";
      tabCal.style.display = "none";
    }
  } catch (err) {
    console.error("Config fetch error:", err);
  }
}

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

  setTimeout(() => {
    window.location.href = localUrl;
  }, 5000);
}

function switchToStep2() {
  setStatus("", "none");
  switchTab("mqtt");
}

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

        const mqttType = document.getElementById(
          "mqtt_type",
        ) as HTMLSelectElement;

        let protocol = b.protocol || "mqtt://";

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

mqttForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const formData = new FormData(mqttForm);
  const data = Object.fromEntries(formData.entries());

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

updateForm.addEventListener("submit", (e) => {
  e.preventDefault();
  const fileInput = document.getElementById("otaFile") as HTMLInputElement;
  if (!fileInput.files || fileInput.files.length === 0) return;

  const file = fileInput.files[0];
  const xhr = new XMLHttpRequest();

  otaProgressContainer.style.display = "block";
  otaBtn.disabled = true;
  setStatus("Uploading firmware...", "info");

  xhr.upload.addEventListener("progress", (evt) => {
    if (evt.lengthComputable) {
      const percent = Math.round((evt.loaded / evt.total) * 100);
      otaProgressBar.style.width = percent + "%";
      otaStatusText.textContent = percent + "%";
    }
  });

  xhr.addEventListener("load", () => {
    if (xhr.status === 200) {
      setStatus("Update successful! Rebooting...", "success");
      otaStatusText.textContent = "Done! Rebooting...";
    } else {
      setStatus("Update failed. Please try again.", "error");
      otaBtn.disabled = false;
      otaProgressContainer.style.display = "none";
    }
  });

  xhr.addEventListener("error", () => {
    setStatus("Network error during upload.", "error");
    otaBtn.disabled = false;
    otaProgressContainer.style.display = "none";
  });

  xhr.open("POST", "/api/update");
  xhr.send(file);
});

/* ─────── Calibration ─────── */

const FIELD_LABELS: Record<string, string> = {
  line_freq_50hz: "Line Frequency",
  pga_gain_mode: "ADC Gain (current only)",
  voltage_gain: "Voltage Gain",
  voltage_offset: "Voltage Offset",
  current_gain: "Current Gain",
  current_offset: "Current Offset",
  active_power_offset: "Active Power Offset",
  reactive_power_offset: "Reactive Power Offset",
  power_gain: "Power Gain",
  phase_angle: "Phase Angle",
  meter_constant: "Meter Constant",
  p_start_th: "Active Power Threshold",
  q_start_th: "Reactive Power Threshold",
  s_start_th: "Apparent Power Threshold",
  p_phase_th: "Active Power PLL Threshold",
  q_phase_th: "Reactive Power PLL Threshold",
  s_phase_th: "Apparent Power PLL Threshold",
};

const FIELD_HINTS: Record<string, string> = {
  line_freq_50hz: "Mains frequency. 50 Hz most regions, 60 Hz US/JP.",
  pga_gain_mode: "Amplifies weak current signals. 1× for 100 A CTs, 2× or 4× for smaller CTs. Voltage channel unaffected.",
  voltage_gain: "Scaling factor for voltage measurement accuracy. Tune with a known reference voltage.",
  voltage_offset: "DC bias compensation. Set to 0 with no voltage applied. LSB counts.",
  current_gain: "Calibration multiplier per channel. Tune with a known resistive load. Default 10148.",
  current_offset: "Cancels residual current at zero load. LSB counts.",
  active_power_offset: "Residual watts at zero load. Units: W.",
  reactive_power_offset: "Residual VAR at zero load. Units: VAR.",
  power_gain: "Combined active/reactive power multiplier per phase.",
  phase_angle: "Corrects CT phase lead. Positive angle = CT leads voltage. Units: degrees.",
  meter_constant: "Total meter constant. Determines energy accumulation speed. Default ~140559464 (~140000 pulses/kWh).",
  p_start_th: "Report zero active power below this threshold.",
  q_start_th: "Report zero reactive power below this threshold.",
  s_start_th: "Report zero apparent power below this threshold.",
  p_phase_th: "PLL zero-crossing stability threshold for active power.",
  q_phase_th: "PLL zero-crossing stability threshold for reactive power.",
  s_phase_th: "PLL zero-crossing stability threshold for apparent power.",
};

/* Phase angle conversion: ~0.02°/LSB at 50 Hz */
const DEG_PER_LSB = 0.008789;

function degToReg(deg: number): number {
  let cycles = Math.round(deg / DEG_PER_LSB);
  if (cycles > 255) cycles = 255;
  if (cycles < -255) cycles = -255;
  return cycles;
}

function regToDeg(reg: number): number {
  return Math.round(reg * DEG_PER_LSB * 100) / 100;
}

const POWER_SCALE = 0.00032;

function lsbToPower(lsb: number): string {
  return String(Math.round(lsb * POWER_SCALE * 1000) / 1000);
}

function powerToLsb(pwr: number): number {
  let lsb = Math.round(pwr / POWER_SCALE);
  if (lsb > 32767) lsb = 32767;
  if (lsb < -32768) lsb = -32768;
  return lsb;
}


/* ── Field builders ── */

function buildSelect(
  name: string,
  label: string,
  hint: string,
  options: Array<{ v: string; l: string }>,
  current: string,
): HTMLDivElement {
  const div = document.createElement("div");
  div.className = "field";
  const lbl = document.createElement("label");
  lbl.textContent = label;
  lbl.htmlFor = name;
  div.appendChild(lbl);
  const sel = document.createElement("select");
  sel.id = name;
  sel.name = name;
  for (const opt of options) {
    const el = document.createElement("option");
    el.value = opt.v;
    el.textContent = opt.l;
    if (opt.v === current) el.selected = true;
    sel.appendChild(el);
  }
  div.appendChild(sel);
  const hi = document.createElement("div");
  hi.className = "field-hint";
  hi.textContent = hint;
  div.appendChild(hi);
  return div;
}

function buildField(
  name: string,
  label: string,
  hint: string,
  value: string,
  step?: string,
  min?: string,
  max?: string,
): HTMLDivElement {
  const div = document.createElement("div");
  div.className = "field";
  const lbl = document.createElement("label");
  lbl.textContent = label;
  lbl.htmlFor = name;
  div.appendChild(lbl);
  const inp = document.createElement("input");
  inp.id = name;
  inp.name = name;
  inp.type = "number";
  inp.value = value;
  if (step) inp.step = step;
  if (min !== undefined) inp.min = min;
  if (max !== undefined) inp.max = max;
  div.appendChild(inp);
  const hi = document.createElement("div");
  hi.className = "field-hint";
  hi.textContent = hint;
  div.appendChild(hi);
  return div;
}

/* ── Render sections ── */

function renderSystemCal(hw: Record<string, unknown>): void {
  const container = document.getElementById("systemCal")!;
  container.innerHTML = "";

  const freq = hw["line_freq_50hz"] === true ? "true" : "false";
  container.appendChild(buildSelect(
    "hw_line_freq_50hz", FIELD_LABELS["line_freq_50hz"], FIELD_HINTS["line_freq_50hz"],
    [
      { v: "true", l: "50 Hz" },
      { v: "false", l: "60 Hz" },
    ],
    freq,
  ));

  const gainMode = String(hw["pga_gain_mode"] ?? 0);
  container.appendChild(buildSelect(
    "hw_pga_gain_mode", FIELD_LABELS["pga_gain_mode"], FIELD_HINTS["pga_gain_mode"],
    [
      { v: "0", l: "1×" },
      { v: "1", l: "2×" },
      { v: "2", l: "4×" },
    ],
    gainMode,
  ));
}

function renderColumnGroup(
  container: HTMLElement,
  hw: Record<string, unknown>,
  arrKey: string,
  title: string,
  hint: string,
  count: number,
  format: "decimal" | "degrees" = "decimal",
): void {
  const wrapper = document.createElement("div");
  wrapper.style.cssText = "grid-column:1/-1;margin-bottom:0.5rem;";

  const titleEl = document.createElement("div");
  titleEl.style.cssText = "font-weight:bold;margin-top:0.8rem;color:#e0e0e0;";
  titleEl.textContent = title;
  wrapper.appendChild(titleEl);

  const descEl = document.createElement("p");
  descEl.style.cssText = "font-size:0.85rem;color:#999;margin:0 0 0.5rem 0;";
  descEl.textContent = hint;
  wrapper.appendChild(descEl);

  const arr = hw[arrKey] as number[] | undefined;

  const grid = document.createElement("div");
  grid.style.cssText = "display:grid;grid-template-columns:1fr 1fr 1fr;gap:0.5rem;";

  for (let i = 0; i < count; ++i) {
    const raw = arr ? arr[i] : 0;
    const val = format === "degrees" ? String(regToDeg(raw)) : String(raw);
    const name = `hw_${arrKey}_${i}`;

    const cell = document.createElement("div");
    cell.style.cssText = "display:flex;flex-direction:column;gap:0.2rem;";

    const lbl = document.createElement("span");
    lbl.style.cssText = "font-weight:bold;font-size:0.85rem;color:#e0e0e0;";
    lbl.textContent = `L${i + 1}`;
    cell.appendChild(lbl);

    const inp = document.createElement("input");
    inp.id = name;
    inp.name = name;
    inp.type = "number";
    inp.value = val;
    inp.step = format === "degrees" ? "0.01" : "1";
    cell.appendChild(inp);

    grid.appendChild(cell);
  }

  wrapper.appendChild(grid);
  container.appendChild(wrapper);
}

async function fetchCalibration(): Promise<void> {
  try {
    const res = await fetch("/api/calibration");
    const data: CalibrationResponse = await res.json();
    const hw = data.hardware;
    const channels = data.channels;

    renderSystemCal(hw);

    /* ── Voltage: gain + offset side-by-side ── */
    const vCont = document.getElementById("voltageCal")!;
    vCont.innerHTML = "";
    const vGrid = document.createElement("div");
    vGrid.className = "channel-cal-grid";
    const vHeader = document.createElement("div");
    vHeader.className = "channel-cal-header";
    vHeader.innerHTML = "<span>Line</span><span>Gain</span><span>Offset</span>";
    vGrid.appendChild(vHeader);
    const vGain = hw["voltage_gain"] as number[] | undefined;
    const vOff = hw["voltage_offset"] as number[] | undefined;
    for (let i = 0; i < 3; ++i) {
      const row = document.createElement("div");
      row.className = "channel-cal-row";
      const lbl = document.createElement("span");
      lbl.className = "channel-cal-label";
      lbl.textContent = `L${i + 1}`;
      row.appendChild(lbl);
      const gi = document.createElement("input");
      gi.type = "number";
      gi.step = "1";
      gi.id = `hw_voltage_gain_${i}`;
      gi.name = `hw_voltage_gain_${i}`;
      gi.value = String(vGain ? vGain[i] : 0xC7CE);
      gi.placeholder = "51150";
      gi.min = "0"; gi.max = "65535";
      row.appendChild(gi);
      const oi = document.createElement("input");
      oi.type = "number";
      oi.step = "1";
      oi.id = `hw_voltage_offset_${i}`;
      oi.name = `hw_voltage_offset_${i}`;
      oi.value = String(vOff ? vOff[i] : 0);
      oi.placeholder = "0";
      oi.min = "-32768"; oi.max = "32767";
      row.appendChild(oi);
      vGrid.appendChild(row);
    }
    vCont.appendChild(vGrid);
    vGrid.style.gridColumn = "1 / -1";

    /* ── Current: 12-channel grid ── */
    const chCurrentContainer = document.getElementById("channelCurrentCal")!;
    chCurrentContainer.innerHTML = "";
    const grid = document.createElement("div");
    grid.className = "channel-cal-grid";
    const header = document.createElement("div");
    header.className = "channel-cal-header";
    header.innerHTML = "<span>Channel</span><span>Gain</span><span>Offset</span>";
    grid.appendChild(header);

    const chGainArr = hw["current_gain"] as number[] | undefined;
    const chOffArr = hw["current_offset"] as number[] | undefined;
    for (let i = 0; i < 12; ++i) {
      const row = document.createElement("div");
      row.className = "channel-cal-row";
      const lbl = document.createElement("span");
      lbl.className = "channel-cal-label";
      lbl.textContent = `Ch ${i + 1}`;
      row.appendChild(lbl);

      const gi = document.createElement("input");
      gi.type = "number";
      gi.step = "1";
      gi.id = `hw_current_gain_${i}`;
      gi.name = `hw_current_gain_${i}`;
      gi.value = String(chGainArr ? chGainArr[i] : 0x27A4);
      gi.placeholder = "10148";
      gi.min = "0"; gi.max = "65535";
      row.appendChild(gi);

      const oi = document.createElement("input");
      oi.type = "number";
      oi.step = "1";
      oi.id = `hw_current_offset_${i}`;
      oi.name = `hw_current_offset_${i}`;
      oi.value = String(chOffArr ? chOffArr[i] : 0);
      oi.placeholder = "0";
      oi.min = "-32768"; oi.max = "32767";
      row.appendChild(oi);

      grid.appendChild(row);
    }
    chCurrentContainer.appendChild(grid);

    /* ── Power: L1-L3 rows × 3 parameter columns ── */
    const pCont = document.getElementById("powerCal")!;
    pCont.innerHTML = "";
    const pGrid = document.createElement("div");
    pGrid.className = "channel-cal-grid";
    pGrid.style.gridTemplateColumns = "1fr 1fr 1fr 1fr";
    const pHeader = document.createElement("div");
    pHeader.className = "channel-cal-header";
    pHeader.innerHTML = "<span>Channel</span><span>P Offset (W)</span><span>Q Offset (VAR)</span><span>Power Gain</span>";
    pGrid.appendChild(pHeader);

    const pOff = hw["active_power_offset"] as number[] | undefined;
    const qOff = hw["reactive_power_offset"] as number[] | undefined;
    const pqGain = hw["active_power_gain"] as number[] | undefined;
    for (let i = 0; i < 12; ++i) {
      const row = document.createElement("div");
      row.className = "channel-cal-row";
      const lbl = document.createElement("span");
      lbl.className = "channel-cal-label";
      lbl.textContent = `Ch ${i + 1}`;
      row.appendChild(lbl);
      const a = document.createElement("input");
      a.type = "number"; a.step = "any";
      a.id = `hw_active_power_offset_${i}`; a.name = `hw_active_power_offset_${i}`;
      a.value = pOff ? lsbToPower(pOff[i]) : "0"; a.placeholder = "0";
      a.min = "-10"; a.max = "10";
      row.appendChild(a);
      const b = document.createElement("input");
      b.type = "number"; b.step = "any";
      b.id = `hw_reactive_power_offset_${i}`; b.name = `hw_reactive_power_offset_${i}`;
      b.value = qOff ? lsbToPower(qOff[i]) : "0"; b.placeholder = "0";
      b.min = "-10"; b.max = "10";
      row.appendChild(b);
      const c = document.createElement("input");
      c.type = "number"; c.step = "1";
      c.id = `hw_active_power_gain_${i}`; c.name = `hw_active_power_gain_${i}`;
      c.value = String(pqGain ? pqGain[i] : 0); c.placeholder = "0";
      c.min = "0"; c.max = "65535";
      row.appendChild(c);
      pGrid.appendChild(row);
    }
    pCont.appendChild(pGrid);
    pGrid.style.gridColumn = "1 / -1";

    /* ── Phase ── */
    const phaseCont = document.getElementById("phaseCal")!;
    phaseCont.innerHTML = "";
    
    const phaseGrid = document.createElement("div");
    phaseGrid.className = "channel-cal-grid";
    phaseGrid.innerHTML = `<div class="channel-cal-header"><span>Channel</span><span style="grid-column: span 2;">Phase Angle (deg)</span></div>`;
    
    const phaseArr = hw["phase_angle"] as number[] | undefined;
    for (let i = 0; i < 12; ++i) {
      const v = phaseArr ? phaseArr[i] : 0;
      phaseGrid.innerHTML += `<div class="channel-cal-row"><span class="channel-cal-label">Ch ${i + 1}</span><input type="number" step="0.01" min="-2.24" max="2.24" name="hw_phase_angle_${i}" id="hw_phase_angle_${i}" value="${regToDeg(v)}" style="grid-column: span 2"></div>`;
    }
    phaseCont.appendChild(phaseGrid);

    /* ── Energy offsets ── */
    const chEnergyContainer = document.getElementById("channelEnergyCal")!;
    chEnergyContainer.innerHTML = "";
    const egrid = document.createElement("div");
    egrid.className = "channel-cal-grid";
    const eheader = document.createElement("div");
    eheader.className = "channel-cal-header";
    eheader.innerHTML = "<span>Channel</span><span style=\"grid-column: span 2;\">Offset (kWh)</span>";
    egrid.appendChild(eheader);

    for (let i = 0; i < channels.length; ++i) {
      const row = document.createElement("div");
      row.className = "channel-cal-row";

      const lbl = document.createElement("span");
      lbl.className = "channel-cal-label";
      lbl.textContent = `Ch ${i + 1}`;
      row.appendChild(lbl);

      const inp = document.createElement("input");
      inp.type = "number";
      inp.step = "any";
      inp.name = `ch_energy_offset_${i}`;
      inp.value = String(channels[i].energy_offset_kwh);
      inp.placeholder = "0";
      inp.style.gridColumn = "span 2";
      row.appendChild(inp);

      egrid.appendChild(row);
    }
    chEnergyContainer.appendChild(egrid);

    /* ── Advanced ── */
    const advContainer = document.getElementById("advancedCal")!;
    advContainer.innerHTML = "";
    advContainer.style.gridTemplateColumns = "1fr";
    /* combined meter constant */
    const mcRaw = (((hw["power_constant_h"] as number) ?? 0) * 65536 + ((hw["power_constant_l"] as number) ?? 0)) >>> 0;
    advContainer.appendChild(buildField(
      "hw_meter_constant",
      FIELD_LABELS["meter_constant"],
      FIELD_HINTS["meter_constant"],
      String(mcRaw),
      "1",
      "0",
      "4294967295"
    ));

    const adv: Array<{ key: string; hint: string }> = [
      { key: "p_start_th", hint: "Active power noise floor. Readings below this are zeroed." },
      { key: "q_start_th", hint: "Reactive power noise floor. Readings below this are zeroed." },
      { key: "s_start_th", hint: "Apparent power noise floor. Readings below this are zeroed." },
      { key: "p_phase_th", hint: "Zero-crossing PLL stability gate for active power." },
      { key: "q_phase_th", hint: "Zero-crossing PLL stability gate for reactive power." },
      { key: "s_phase_th", hint: "Zero-crossing PLL stability gate for apparent power." },
    ];
    for (const f of adv) {
      const raw = hw[f.key] as number ?? 0;
      advContainer.appendChild(buildField(
        `hw_${f.key}`,
        FIELD_LABELS[f.key] ?? f.key,
        f.hint,
        String(raw),
        "1",
        "0",
        "65535"
      ));
    }
  } catch (err) {
    console.error("Calibration fetch error:", err);
    setStatus("Failed to load calibration data.", "error");
  }
}

calForm.addEventListener("submit", async (e) => {
  e.preventDefault();

  function getVal(id: string): string {
    const el = document.getElementById(id) as HTMLInputElement | HTMLSelectElement;
    return el ? el.value : "0";
  }

  function readArr(key: string, count: number): number[] {
    const out: number[] = [];
    for (let i = 0; i < count; ++i) {
      out.push(parseInt(getVal(`hw_${key}_${i}`), 10) || 0);
    }
    return out;
  }

  function readDegArr(key: string, count: number): number[] {
    const out: number[] = [];
    for (let i = 0; i < count; ++i) {
      out.push(degToReg(parseFloat(getVal(`hw_${key}_${i}`)) || 0));
    }
    return out;
  }

  function readPowerOffsetArr(key: string, count: number): number[] {
    const out: number[] = [];
    for (let i = 0; i < count; ++i) {
      out.push(powerToLsb(parseFloat(getVal(`hw_${key}_${i}`)) || 0));
    }
    return out;
  }

  const hw: Record<string, unknown> = {};

  hw["line_freq_50hz"] = getVal("hw_line_freq_50hz") === "true";
  hw["pga_gain_mode"] = parseInt(getVal("hw_pga_gain_mode"), 10) || 0;

  hw["voltage_gain"] = readArr("voltage_gain", 3);
  hw["voltage_offset"] = readArr("voltage_offset", 3);
  hw["current_gain"] = readArr("current_gain", 12);
  hw["current_offset"] = readArr("current_offset", 12);
  hw["active_power_offset"] = readPowerOffsetArr("active_power_offset", 12);
  hw["reactive_power_offset"] = readPowerOffsetArr("reactive_power_offset", 12);
  hw["active_power_gain"] = readArr("active_power_gain", 12);
  hw["phase_angle"] = readDegArr("phase_angle", 12);

  const meterConst = parseInt(getVal("hw_meter_constant"), 10) || 0;
  hw["power_constant_h"] = (meterConst >>> 16) & 0xFFFF;
  hw["power_constant_l"] = meterConst & 0xFFFF;
  hw["p_start_th"] = parseInt(getVal("hw_p_start_th"), 10) || 0;
  hw["q_start_th"] = parseInt(getVal("hw_q_start_th"), 10) || 0;
  hw["s_start_th"] = parseInt(getVal("hw_s_start_th"), 10) || 0;
  hw["p_phase_th"] = parseInt(getVal("hw_p_phase_th"), 10) || 0;
  hw["q_phase_th"] = parseInt(getVal("hw_q_phase_th"), 10) || 0;
  hw["s_phase_th"] = parseInt(getVal("hw_s_phase_th"), 10) || 0;

  const channels: ChannelCal[] = [];
  for (let i = 0; i < 12; ++i) {
    const eo = document.querySelector<HTMLInputElement>(`input[name="ch_energy_offset_${i}"]`);
    channels.push({
      energy_offset_kwh: eo ? parseFloat(eo.value) || 0 : 0,
    });
  }

  setStatus("Saving calibration...", "info");
  (document.getElementById("calBtn") as HTMLButtonElement).disabled = true;

  try {
    const res = await fetch("/api/calibration", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ hardware: hw, channels }),
    });

    if (!res.ok) throw new Error("Save failed");

    setStatus("Calibration saved and applied successfully!", "success");
    setTimeout(() => {
      setStatus("", "none");
      (document.getElementById("calBtn") as HTMLButtonElement).disabled = false;
    }, 3000);
  } catch (err) {
    setStatus("Failed to save calibration.", "error");
    (document.getElementById("calBtn") as HTMLButtonElement).disabled = false;
  }
});

/* ── Init ── */

async function init() {
    await fetchConfig();
    fetchNetworks();
}

init();
