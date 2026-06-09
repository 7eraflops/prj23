#pragma once

#include "atm90e32_registers.hpp"
#include "atm90e32_spi.hpp"

#include <esp_err.h>

#include <cstdint>
#include <cstddef>

namespace atm90e32 {

enum class LineInput : uint8_t {
    A = 0,
    B = 1,
    C = 2,
};

struct DeviceReading {
    float voltage = 0.0F;
    float current = 0.0F;
    float active_power = 0.0F;
    float reactive_power = 0.0F;
    float apparent_power = 0.0F;
    float power_factor = 0.0F;
    float energy_kwh = 0.0F;
    float frequency = 0.0F;
};

struct DeviceConfig {
    uint16_t line_freq_mode = 0x0185;
    uint16_t pga_gain = 0x0000;
    uint16_t voltage_gain = 0x0000;
    uint16_t current_gain_a = 0x0000;
    uint16_t current_gain_b = 0x0000;
    uint16_t current_gain_c = 0x0000;
};

struct DeviceHealth {
    bool online = false;
    uint32_t read_errors = 0;
    uint32_t write_errors = 0;
    int64_t last_success_us = 0;
};

class Device {
public:
    Device(SpiTransport& transport, std::size_t chip_index);

    esp_err_t init(const DeviceConfig& cfg);
    esp_err_t verify_comm();
    esp_err_t read_line(LineInput input, DeviceReading& out);
    esp_err_t read_line_voltage(LineInput input, float& voltage);

    const DeviceHealth& health() const;

private:
    esp_err_t write_cfg(reg::CalibrationConfig reg_addr, uint16_t value);
    esp_err_t write_status(reg::StatusControl reg_addr, uint16_t value);
    esp_err_t read_instant(reg::Instant reg_addr, uint16_t& out_value);

    esp_err_t read_power(LineInput input, float& active, float& reactive, float& apparent);
    esp_err_t read_energy(LineInput input, float& out_energy_kwh);

    void mark_ok();
    void mark_read_error();
    void mark_write_error();

    SpiTransport& _transport;
    std::size_t _chip_index;
    DeviceHealth _health;
};

} // namespace atm90e32
