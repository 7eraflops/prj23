# 3-Phase 12-Channel ESP32 Energy Monitor

A custom ESP32-based energy meter measuring up to 12 channels across a 3-phase supply, integrated with Home Assistant via MQTT.

## Current Status
- End-to-end firmware flow is working on ESP32 DevKit: provisioning -> Wi-Fi -> MQTT -> HA discovery -> telemetry -> command handling.
- ATM90E32 real driver path is implemented (`register map`, `SPI transport`, `ATM90E32Sensor` implementing `IEnergySensor`).
- DevKit-only mock-IC path is implemented on a dedicated testing branch.
- Custom PCB and physical calibration are pending hardware availability.

## Overview
- **Measurement**: Uses the ATM90E32AS metering IC to monitor up to 12 circuits (11 used in current planned deployment).
- **Microcontroller**: ESP32-S3.
- **Integration**: Home Assistant MQTT Auto-Discovery for automatic entity configuration.
- **Provisioning**: Local captive portal and web server for Wi-Fi and MQTT setup.
- **Testing**: Uses mock sensor paths for software development before final hardware is available.

## Hardware
- **Custom PCB**: Currently in development.
- **MCU**: ESP32-S3
- **Metering IC**: ATM90E32AS
- **Sensors**: Current Transformers (CT Clamps) and Voltage Dividers

## Firmware Architecture
- `config_manager`: NVS-backed configuration (Wi-Fi, MQTT, channel phase assignment, active mask, calibration groundwork)
- `wifi_manager`: station lifecycle and reconnection
- `mqtt_manager`: client lifecycle, subscribe/publish, callback handling
- `ha_discovery`: Home Assistant discovery payload publishing
- `command_handler`: command topic handling and runtime config updates
- `sensor_manager`: polling task and telemetry publishing loop
- `atm90e32_interface`: register definitions, SPI transport, device abstraction
- `web_server`: provisioning/config pages and OTA endpoint

## Software Stack
- **Framework**: ESP-IDF (C/C++)
- **RTOS**: FreeRTOS
- **Communication**: MQTT (Mosquitto), HTTP (Local Web Server)
- **Data Processing**: Non-Intrusive Load Monitoring (NILM) runs server-side (Home Assistant / Raspberry Pi), not on the ESP32.

---

## Project Roadmap

### Phase 1: Software Foundation & HA Integration (Completed)
*Goal: Build the end-to-end data pipeline on the DevKit using mocked data before the physical hardware arrives.*

- [x] **Config & Provisioning (`config_manager`, `web_setup`)**
  - Save/load Wi-Fi and MQTT credentials using NVS.
  - Initial captive portal for device provisioning.
- [x] **MQTT Manager (`mqtt_manager`)**
  - Implement `esp_mqtt_client` using credentials from `config_manager`.
  - Handle connect/disconnect events and auto-reconnection logic.
- [x] **Home Assistant Auto-Discovery (`ha_discovery`)**
  - Implement HA MQTT Auto-Discovery protocol.
  - Publish JSON configuration payloads for Voltage, Current, Active Power, etc.
- [x] **Hardware Abstraction & Mock Sensor (`sensor_manager`)**
  - Define a generic sensor interface (`IEnergySensor`).
  - Create a `DummyEnergySensor` that generates fluctuating test data.
  - Implement a FreeRTOS telemetry task to poll the sensor and publish JSON payloads.

### Phase 2: Pre-Hardware System Features (Mostly Completed)
*Goal: Implement system-level features that can be tested on an ESP32 dev board before the custom PCBs arrive.*

- [x] **Continuous Web Server**
  - Keep web server active on the local network post-provisioning.
- [x] **Over-The-Air (OTA) Updates**
  - Implement OTA flashing mechanism.
  - Add an `/update` endpoint to the local web server to allow uploading firmware binaries.
- [x] **Two-Way MQTT Communication (Device Control)**
  - Add Home Assistant "Button" entities via HA Discovery (e.g., "Reboot").
  - Subscribe to command topics in `mqtt_manager` and execute device actions.
- [x] **Calibration Storage Groundwork**
  - Add NVS fields in `config_manager` for per-channel calibration settings.
- [ ] **Calibration Web UI**
  - Add a `/calibration` web page to view and edit multipliers dynamically.

### Phase 3: Hardware Driver Preparation (In Progress)
*Goal: Write and validate the ATM90E32AS SPI driver path before custom hardware arrives.*

- [x] **ATM90E32AS Register Map**
  - Create header files defining register addresses and constants for status, calibration, instant values, power, and energy.
- [x] **SPI Interface**
  - Initialize the ESP32-S3 SPI master peripheral for shared bus + per-chip CS.
  - Implement low-level `read16()` and `write16()` functions and split-register 32-bit reads.
- [x] **ATM90E32 Sensor Implementation**
  - Create `ATM90E32Sensor` implementing `IEnergySensor`.
  - Integrate channel phase assignment and active mask behavior with existing runtime config.
- [x] **DevKit Mock IC Validation Path**
  - Add register-level ATM90E32 emulation path on testing branch to simulate fluctuating values and command/read flow.
- [ ] **Hardware Profile Finalization**
  - Finalize tuned mode/gain/threshold values when PCB and front-end are finalized.

### Phase 4: Hardware Bring-Up & Validation
*Goal: Validate the custom PCB and physically calibrate the sensors.*

- [ ] **Physical Bring-Up**
  - Solder and power the PCB.
  - Validate SPI communication (e.g., read ATM90E32AS status/identity paths).
- [ ] **Live Calibration**
  - Connect actual loads and adjust multipliers via the Calibration Web UI.
  - Verify baseline noise and power factor readings.

### Phase 5: Non-Intrusive Load Monitoring (NILM)
*Goal: Differentiate multiple appliances sharing the same electrical circuit.*

#### ESP32 Responsibility (Data Provider)
The ESP32 acts as a high-fidelity data provider to Home Assistant. To aid in NILM separation, it publishes high-frequency updates (1-2 second intervals) per channel, including:
1. Active Power (W)
2. Reactive Power (var)
3. Apparent Power (VA)
4. Power Factor
5. Voltage (V) & Current (A)
6. Energy (kWh) & Frequency (Hz)
*(It also publishes device-level diagnostics like MCU temp, Wi-Fi RSSI, uptime, and free heap).*

#### Server Responsibility (Raspberry Pi 5 / Home Assistant)
Algorithms run on the server using Python (e.g., AppDaemon, custom HA integration, or NILMTK).

- [ ] **Circuit 1: Fridge (Compressor) + Coffee Maker (Resistive Heater)**
  - *Approach:* Step-Detection.
  - *Logic:* Detect flat active-power spikes (Coffee Maker) vs. reactive-power curves (Fridge). Subtract static loads to isolate devices.
- [ ] **Circuit 2: Washing Machine + Heat Pump Dryer**
  - *Approach:* State-Tracking ML Model (HMM / LSTM).
  - *Logic:* Train models on historical data to identify overlapping cycles based on combined Active and Reactive power footprints.
