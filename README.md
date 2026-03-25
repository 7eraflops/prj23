# 3-Phase 12-Channel ESP32 Energy Monitor

A custom ESP32-based energy meter measuring up to 12 channels across a 3-phase supply, integrated with Home Assistant via MQTT.

## Overview
- **Measurement**: Uses the ATM90E32AS metering IC to monitor up to 12 circuits (11 used).
- **Microcontroller**: ESP32-S3.
- **Integration**: Home Assistant MQTT Auto-Discovery for automatic entity configuration.
- **Provisioning**: Local captive portal and web server for Wi-Fi and MQTT setup.
- **Testing**: Includes a `DummyEnergySensor` implementation to simulate hardware data for software development.

## Hardware
- **Custom PCB**: Currently in development.
- **MCU**: ESP32-S3
- **Metering IC**: ATM90E32AS
- **Sensors**: Current Transformers (CT Clamps) and Voltage Dividers

## Software Stack
- **Framework**: ESP-IDF (C/C++)
- **RTOS**: FreeRTOS
- **Communication**: MQTT (Mosquitto), HTTP (Local Web Server)
- **Data Processing**: Non-Intrusive Load Monitoring (NILM) will be handled server-side (Home Assistant / Raspberry Pi) rather than on the ESP32.

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

### Phase 2: Pre-Hardware System Features (Current)
*Goal: Implement system-level features that can be tested on an ESP32 dev board before the custom PCBs arrive.*

- [ ] **Over-The-Air (OTA) Updates**
  - Implement OTA flashing mechanism.
  - Add an `/update` endpoint to the local web server to allow uploading firmware binaries.
- [ ] **Continuous Web Server & Calibration Config**
  - Modify `web_setup` to remain active on the local network post-provisioning.
  - Add NVS fields in `config_manager` for CT and voltage divider multipliers.
  - Add a `/calibration` web page to view and edit multipliers dynamically.
- [ ] **Two-Way MQTT Communication (Device Control)**
  - Add Home Assistant "Button" entities via HA Discovery (e.g., "Reboot").
  - Subscribe to command topics in `mqtt_manager` and execute device actions.

### Phase 3: Hardware Driver Preparation
*Goal: Write the SPI driver for the ATM90E32AS using the datasheet.*

- [ ] **ATM90E32AS Register Map**
  - Create header files defining register addresses (e.g., `UrmsA`, `IrmsA`, `SysStatus`).
- [ ] **SPI Interface**
  - Initialize the ESP32-S3 SPI master peripheral.
  - Write low-level `read16()` and `write16()` functions.
- [ ] **ATM90E32 Sensor Implementation**
  - Create `ATM90E32Sensor` implementing `IEnergySensor`.

### Phase 4: Hardware Bring-Up & Validation
*Goal: Validate the custom PCB and physically calibrate the sensors.*

- [ ] **Physical Bring-Up**
  - Solder and power the PCB.
  - Validate SPI communication (e.g., read the ATM90E32AS Chip ID).
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
Algorithms will run on the server using Python (e.g., AppDaemon, custom HA integration, or NILMTK).

- [ ] **Circuit 1: Fridge (Compressor) + Coffee Maker (Resistive Heater)**
  - *Approach:* Step-Detection.
  - *Logic:* Detect flat active-power spikes (Coffee Maker) vs. reactive-power curves (Fridge). Subtract static loads to isolate devices.
- [ ] **Circuit 2: Washing Machine + Heat Pump Dryer**
  - *Approach:* State-Tracking ML Model (HMM / LSTM).
  - *Logic:* Train models on historical data to identify overlapping cycles based on combined Active and Reactive power footprints.