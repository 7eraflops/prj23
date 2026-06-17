#include "atm90e32_sensor.hpp"

#include <esp_log.h>
#include <cstring>

#include <utility>

namespace {
static const char* TAG = "ATM90_SENSOR";
}

ATM90E32Sensor::ATM90E32Sensor(ConfigManager& config_manager)
    : _config_manager(config_manager), _devices{atm90e32::Device(_transport, 0),
                                                atm90e32::Device(_transport, 1),
                                                atm90e32::Device(_transport, 2),
                                                atm90e32::Device(_transport, 3)} {}

bool ATM90E32Sensor::init() {
    _last_cal = _config_manager.get_calibration_data();
    const auto& hw_cal = _last_cal;

    atm90e32::SpiBusConfig spi_cfg;
    if (_transport.init(spi_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ATM90E32 SPI transport");
        return false;
    }

    constexpr uint16_t M_MODE0_50HZ = 0x0185;
    constexpr uint16_t M_MODE0_60HZ = 0x01A5;
    constexpr uint16_t M_MODE1_GAIN[] = {0x0000, 0x1555, 0x2AAA};

    const uint16_t m_mode0 = hw_cal.line_freq_50hz ? M_MODE0_50HZ : M_MODE0_60HZ;
    const uint16_t m_mode1 = (hw_cal.pga_gain_mode < 3) ? M_MODE1_GAIN[hw_cal.pga_gain_mode] : 0x0000;

    bool any_ok = false;
    for (size_t chip = 0; chip < _devices.size(); ++chip) {
        atm90e32::DeviceConfig device_cfg;
        device_cfg.pl_const_h = hw_cal.pl_const_h;
        device_cfg.pl_const_l = hw_cal.pl_const_l;
        device_cfg.line_freq_mode = m_mode0;
        device_cfg.pga_gain = m_mode1;
        device_cfg.p_start_th = hw_cal.p_start_th;
        device_cfg.q_start_th = hw_cal.q_start_th;
        device_cfg.s_start_th = hw_cal.s_start_th;
        device_cfg.p_phase_th = hw_cal.p_phase_th;
        device_cfg.q_phase_th = hw_cal.q_phase_th;
        device_cfg.s_phase_th = hw_cal.s_phase_th;

        // Hardware ADC mapping has been configured via CHANNEL_MAP_I and CHANNEL_MAP_U
        // to match Logical Line A -> Physical L1, Line B -> L2, Line C -> L3.
        // We can map calibration data directly 1:1.
        for (int i = 0; i < 3; ++i) {
            device_cfg.p_offset[i] = hw_cal.p_offset[i];
            device_cfg.q_offset[i] = hw_cal.q_offset[i];
            device_cfg.pq_gain[i] = hw_cal.pq_gain[i];
            device_cfg.voltage_gain[i] = hw_cal.u_gain[i];
            device_cfg.voltage_offset[i] = hw_cal.u_offset[i];
        }

        const size_t ch_base = chip * CHANNELS_PER_CHIP;
        for (int i = 0; i < 3; ++i) {
            device_cfg.current_gain[i] = hw_cal.i_gain[ch_base + i];
            device_cfg.current_offset[i] = hw_cal.i_offset[ch_base + i];
            device_cfg.phi[i] = hw_cal.phi[ch_base + i];
        }

        const esp_err_t err = _devices[chip].init(device_cfg);
        if (err == ESP_OK) {
            any_ok = true;
        } else {
            ESP_LOGW(TAG, "Chip %u init failed with %s",
                     static_cast<unsigned>(chip), esp_err_to_name(err));
        }
    }

    if (!any_ok) {
        ESP_LOGE(TAG, "No ATM90E32 device initialized successfully");
        return false;
    }

    ESP_LOGI(TAG, "ATM90E32 sensor initialized");
    return true;
}

ATM90E32Sensor::ChannelMap ATM90E32Sensor::map_channel(int channel) const {
    ChannelMap map{};
    map.chip_index = static_cast<std::size_t>(channel / CHANNELS_PER_CHIP);

    const int line = channel % CHANNELS_PER_CHIP;
    switch (line) {
    case 0:
        map.input = atm90e32::LineInput::A;
        break;
    case 1:
        map.input = atm90e32::LineInput::B;
        break;
    case 2:
    default:
        map.input = atm90e32::LineInput::C;
        break;
    }

    return map;
}

atm90e32::LineInput ATM90E32Sensor::phase_to_input(ChannelPhase phase,
                                                    atm90e32::LineInput fallback) const {
    switch (phase) {
    case ChannelPhase::PHASE_L1:
        return atm90e32::LineInput::A;
    case ChannelPhase::PHASE_L2:
        return atm90e32::LineInput::B;
    case ChannelPhase::PHASE_L3:
        return atm90e32::LineInput::C;
    case ChannelPhase::NONE:
    default:
        return fallback;
    }
}

void ATM90E32Sensor::update() {
    const auto& current_cal = _config_manager.get_calibration_data();
    if (std::memcmp(&_last_cal, &current_cal, sizeof(AppConfig::CalibrationData)) != 0) {
        ESP_LOGI(TAG, "Calibration data changed dynamically. Re-initializing sensors...");
        init();
        return;
    }

    for (int channel = 0; channel < TOTAL_CHANNELS; ++channel) {
        const ChannelMap map = map_channel(channel);

        if (!_config_manager.is_channel_active(channel)) {
            _channel_cache[channel] = EnergyData{};
            continue;
        }

        atm90e32::LineInput current_input = map.input;

        ChannelPhase phase = _config_manager.get_channel_phase(channel);
        atm90e32::LineInput voltage_input = phase_to_input(phase, map.input);

        atm90e32::DeviceReading reading;
        esp_err_t err = _devices[map.chip_index].read_line(current_input, reading);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Read failed for channel %d (chip %u): %s", channel,
                     static_cast<unsigned>(map.chip_index), esp_err_to_name(err));
            _channel_cache[channel] = EnergyData{};
            continue;
        }

        if (voltage_input != current_input) {
            float phase_voltage = 0.0F;
            err = _devices[map.chip_index].read_line_voltage(voltage_input, phase_voltage);
            if (err == ESP_OK) {
                reading.voltage = phase_voltage;
                // The ATM90E32 calculates apparent/active power using the native voltage pin.
                // If we are overriding the voltage from another phase, the hardware apparent
                // power is invalid (likely 0 if the native pin is disconnected).
                reading.apparent_power = reading.voltage * reading.current;
            }
        }

        _line_cache[map.chip_index][static_cast<int>(current_input)] = reading;

        const float energy_offset = _config_manager.get_channel_calibration(channel).energy_offset_kwh;

        EnergyData out{};
        out.voltage = reading.voltage;
        out.current = reading.current;
        out.active_power = reading.active_power;
        out.reactive_power = reading.reactive_power;
        out.apparent_power = reading.apparent_power;
        out.power_factor = reading.power_factor;
        out.energy = reading.energy_kwh + energy_offset;
        out.frequency = reading.frequency;

        _channel_cache[channel] = out;
    }
}

EnergyData ATM90E32Sensor::read_channel(int channel) {
    if (channel < 0 || channel >= TOTAL_CHANNELS) {
        return EnergyData{};
    }
    return _channel_cache[channel];
}
