#include "atm90e32_device.hpp"

#include <esp_log.h>
#include <esp_timer.h>

namespace atm90e32 {

static const char* TAG = "ATM90_DEVICE";

namespace {

struct LineRegisters {
    reg::Instant voltage;
    reg::Instant current;
    reg::Instant power_factor;
    reg::PowerHigh p_high;
    reg::PowerLow p_low;
    reg::PowerHigh q_high;
    reg::PowerLow q_low;
    reg::PowerHigh s_high;
    reg::PowerLow s_low;
    reg::Energy energy;
};

constexpr LineRegisters line_registers(LineInput input) {
    switch (input) {
    case LineInput::A:
        return {
            reg::Instant::URMS_A,
            reg::Instant::IRMS_A,
            reg::Instant::PF_MEAN_A,
            reg::PowerHigh::P_MEAN_A,
            reg::PowerLow::P_MEAN_A_LSB,
            reg::PowerHigh::Q_MEAN_A,
            reg::PowerLow::Q_MEAN_A_LSB,
            reg::PowerHigh::S_MEAN_A,
            reg::PowerLow::S_MEAN_A_LSB,
            reg::Energy::AP_ENERGY_A,
        };
    case LineInput::B:
        return {
            reg::Instant::URMS_B,
            reg::Instant::IRMS_B,
            reg::Instant::PF_MEAN_B,
            reg::PowerHigh::P_MEAN_B,
            reg::PowerLow::P_MEAN_B_LSB,
            reg::PowerHigh::Q_MEAN_B,
            reg::PowerLow::Q_MEAN_B_LSB,
            reg::PowerHigh::S_MEAN_B,
            reg::PowerLow::S_MEAN_B_LSB,
            reg::Energy::AP_ENERGY_B,
        };
    case LineInput::C:
    default:
        return {
            reg::Instant::URMS_C,
            reg::Instant::IRMS_C,
            reg::Instant::PF_MEAN_C,
            reg::PowerHigh::P_MEAN_C,
            reg::PowerLow::P_MEAN_C_LSB,
            reg::PowerHigh::Q_MEAN_C,
            reg::PowerLow::Q_MEAN_C_LSB,
            reg::PowerHigh::S_MEAN_C,
            reg::PowerLow::S_MEAN_C_LSB,
            reg::Energy::AP_ENERGY_C,
        };
    }
}

} // namespace

Device::Device(SpiTransport& transport, std::size_t chip_index)
    : _transport(transport), _chip_index(chip_index) {}

esp_err_t Device::write_cfg(reg::CalibrationConfig reg_addr, uint16_t value) {
    const esp_err_t err = _transport.write16(_chip_index, to_addr(reg_addr), value);
    if (err != ESP_OK) {
        mark_write_error();
    }
    return err;
}

esp_err_t Device::write_status(reg::StatusControl reg_addr, uint16_t value) {
    const esp_err_t err = _transport.write16(_chip_index, to_addr(reg_addr), value);
    if (err != ESP_OK) {
        mark_write_error();
    }
    return err;
}

esp_err_t Device::read_instant(reg::Instant reg_addr, uint16_t& out_value) {
    const esp_err_t err = _transport.read16(_chip_index, to_addr(reg_addr), out_value);
    if (err != ESP_OK) {
        mark_read_error();
    }
    return err;
}

esp_err_t Device::init(const DeviceConfig& cfg) {
    esp_err_t err = write_status(reg::StatusControl::SOFT_RESET, ATM90E32_RESET_MAGIC);
    if (err != ESP_OK)
        return err;

    err = write_status(reg::StatusControl::CFG_REG_ACC_EN, ATM90E32_CONFIG_UNLOCK_MAGIC);
    if (err != ESP_OK)
        return err;

    err = write_status(reg::StatusControl::METER_EN, 0x0001);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::PL_CONST_H, 0x0861);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PL_CONST_L, 0xC468);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::M_MODE0, cfg.line_freq_mode);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::M_MODE1, cfg.pga_gain);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::U_GAIN_A, cfg.voltage_gain);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_GAIN_A, cfg.current_gain_a);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_GAIN_B, cfg.voltage_gain);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_GAIN_B, cfg.current_gain_b);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_GAIN_C, cfg.voltage_gain);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_GAIN_C, cfg.current_gain_c);
    if (err != ESP_OK)
        return err;

    err = write_status(reg::StatusControl::CFG_REG_ACC_EN, ATM90E32_CONFIG_LOCK_MAGIC);
    if (err != ESP_OK)
        return err;

    return verify_comm();
}

esp_err_t Device::verify_comm() {
    uint16_t value = 0;
    const esp_err_t err = _transport.read16(_chip_index, to_addr(reg::StatusControl::LAST_SPI_DATA), value);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }
    (void)value;
    mark_ok();
    return ESP_OK;
}

esp_err_t Device::read_power(LineInput input, float& active, float& reactive, float& apparent) {
    const LineRegisters regs = line_registers(input);

    int32_t p = 0;
    int32_t q = 0;
    int32_t s = 0;

    esp_err_t err = _transport.read32_split(_chip_index, to_addr(regs.p_high), to_addr(regs.p_low), p);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }

    err = _transport.read32_split(_chip_index, to_addr(regs.q_high), to_addr(regs.q_low), q);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }

    err = _transport.read32_split(_chip_index, to_addr(regs.s_high), to_addr(regs.s_low), s);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }

    active = static_cast<float>(p) * POWER_SCALE;
    reactive = static_cast<float>(q) * POWER_SCALE;
    apparent = static_cast<float>(s) * POWER_SCALE;
    return ESP_OK;
}

esp_err_t Device::read_energy(LineInput input, float& out_energy_kwh) {
    const LineRegisters regs = line_registers(input);
    uint16_t value = 0;
    const esp_err_t err = _transport.read16(_chip_index, to_addr(regs.energy), value);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }

    out_energy_kwh = static_cast<float>(value) * ENERGY_SCALE_KWH;
    return ESP_OK;
}

esp_err_t Device::read_line(LineInput input, DeviceReading& out) {
    const LineRegisters regs = line_registers(input);

    uint16_t raw_voltage = 0;
    uint16_t raw_current = 0;
    uint16_t raw_pf = 0;
    uint16_t raw_freq = 0;

    esp_err_t err = read_instant(regs.voltage, raw_voltage);
    if (err != ESP_OK)
        return err;

    err = read_instant(regs.current, raw_current);
    if (err != ESP_OK)
        return err;

    err = read_instant(regs.power_factor, raw_pf);
    if (err != ESP_OK)
        return err;

    err = read_instant(reg::Instant::FREQ, raw_freq);
    if (err != ESP_OK)
        return err;

    out.voltage = static_cast<float>(raw_voltage) * VOLTAGE_SCALE;
    out.current = static_cast<float>(raw_current) * CURRENT_SCALE;
    out.power_factor = static_cast<float>(static_cast<int16_t>(raw_pf)) * POWER_FACTOR_SCALE;
    out.frequency = static_cast<float>(raw_freq) * FREQUENCY_SCALE;

    err = read_power(input, out.active_power, out.reactive_power, out.apparent_power);
    if (err != ESP_OK)
        return err;

    err = read_energy(input, out.energy_kwh);
    if (err != ESP_OK)
        return err;

    mark_ok();
    return ESP_OK;
}

esp_err_t Device::read_line_voltage(LineInput input, float& voltage) {
    const LineRegisters regs = line_registers(input);
    uint16_t raw = 0;
    const esp_err_t err = read_instant(regs.voltage, raw);
    if (err != ESP_OK) {
        mark_read_error();
        return err;
    }
    mark_ok();
    voltage = static_cast<float>(raw) * VOLTAGE_SCALE;
    return ESP_OK;
}

const DeviceHealth& Device::health() const {
    return _health;
}

void Device::mark_ok() {
    _health.online = true;
    _health.last_success_us = esp_timer_get_time();
}

void Device::mark_read_error() {
    _health.online = false;
    ++_health.read_errors;
}

void Device::mark_write_error() {
    _health.online = false;
    ++_health.write_errors;
}

} // namespace atm90e32
