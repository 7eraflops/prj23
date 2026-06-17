#include "atm90e32_device.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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

    vTaskDelay(pdMS_TO_TICKS(50));

    err = write_status(reg::StatusControl::CFG_REG_ACC_EN, ATM90E32_CONFIG_UNLOCK_MAGIC);
    if (err != ESP_OK)
        return err;

    err = write_status(reg::StatusControl::METER_EN, 0x0001);
    if (err != ESP_OK)
        return err;

    // Only the voltage labels (L1, L2, L3) are reversed on the PCB, current CTs are not.
    // L1 is physically connected to Line C (U2)
    // L2 is physically connected to Line B (U1)
    // L3 is physically connected to Line A (U0)
    // Restore default mapping for currents so CT1 stays with Logical Phase A:
    // ChannelMapI (0x01): Phase C (I2), Phase B (I1), Phase A (I0) -> 0x0210
    err = write_status(reg::StatusControl::CHANNEL_MAP_I, 0x0210);
    if (err != ESP_OK)
        return err;

    // ChannelMapU (0x02): Phase C (U0), Phase B (U1), Phase A (U2) -> 0x0456
    err = write_status(reg::StatusControl::CHANNEL_MAP_U, 0x0456);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::PL_CONST_H, cfg.pl_const_h);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PL_CONST_L, cfg.pl_const_l);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::M_MODE0, cfg.line_freq_mode);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::M_MODE1, cfg.pga_gain);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::P_START_TH, cfg.p_start_th);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::Q_START_TH, cfg.q_start_th);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::S_START_TH, cfg.s_start_th);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::P_PHASE_TH, cfg.p_phase_th);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::Q_PHASE_TH, cfg.q_phase_th);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::S_PHASE_TH, cfg.s_phase_th);
    if (err != ESP_OK)
        return err;

    uint16_t cs1 = 0;
    cs1 += cfg.pl_const_h;
    cs1 += cfg.pl_const_l;
    cs1 += cfg.line_freq_mode;
    cs1 += cfg.pga_gain;
    cs1 += cfg.p_start_th;
    cs1 += cfg.q_start_th;
    cs1 += cfg.s_start_th;
    cs1 += cfg.p_phase_th;
    cs1 += cfg.q_phase_th;
    cs1 += cfg.s_phase_th;
    err = write_cfg(reg::CalibrationConfig::CS1, cs1);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::P_OFFSET_A, static_cast<uint16_t>(cfg.p_offset[0]));
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::Q_OFFSET_A, static_cast<uint16_t>(cfg.q_offset[0]));
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::P_OFFSET_B, static_cast<uint16_t>(cfg.p_offset[1]));
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::Q_OFFSET_B, static_cast<uint16_t>(cfg.q_offset[1]));
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::P_OFFSET_C, static_cast<uint16_t>(cfg.p_offset[2]));
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::Q_OFFSET_C, static_cast<uint16_t>(cfg.q_offset[2]));
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::PQ_GAIN_A, cfg.pq_gain[0]);
    if (err != ESP_OK)
        return err;

    uint16_t phi_reg[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        if (cfg.phi[i] < 0) {
            phi_reg[i] = (1 << 15) | ((-cfg.phi[i]) & 0xFF);
        } else {
            phi_reg[i] = (cfg.phi[i] & 0xFF);
        }
    }

    err = write_cfg(reg::CalibrationConfig::PHI_A, phi_reg[0]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PQ_GAIN_B, cfg.pq_gain[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PHI_B, phi_reg[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PQ_GAIN_C, cfg.pq_gain[2]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::PHI_C, phi_reg[2]);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::U_GAIN_A, cfg.voltage_gain[0]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_GAIN_B, cfg.voltage_gain[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_GAIN_C, cfg.voltage_gain[2]);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::U_OFFSET_A, cfg.voltage_offset[0]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_OFFSET_B, cfg.voltage_offset[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::U_OFFSET_C, cfg.voltage_offset[2]);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::I_GAIN_A, cfg.current_gain[0]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_GAIN_B, cfg.current_gain[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_GAIN_C, cfg.current_gain[2]);
    if (err != ESP_OK)
        return err;

    err = write_cfg(reg::CalibrationConfig::I_OFFSET_A, cfg.current_offset[0]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_OFFSET_B, cfg.current_offset[1]);
    if (err != ESP_OK)
        return err;
    err = write_cfg(reg::CalibrationConfig::I_OFFSET_C, cfg.current_offset[2]);
    if (err != ESP_OK)
        return err;

    uint16_t cs2 = 0;
    for (int i = 0; i < 3; ++i) {
        cs2 += static_cast<uint16_t>(cfg.p_offset[i]);
        cs2 += static_cast<uint16_t>(cfg.q_offset[i]);
    }
    for (int i = 0; i < 3; ++i) {
        cs2 += cfg.pq_gain[i];
        cs2 += phi_reg[i];
    }
    for (int i = 0; i < 3; ++i) {
        cs2 += cfg.voltage_gain[i];
        cs2 += cfg.current_gain[i];
        cs2 += cfg.voltage_offset[i];
        cs2 += cfg.current_offset[i];
    }
    err = write_cfg(reg::CalibrationConfig::CS2, cs2);
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
