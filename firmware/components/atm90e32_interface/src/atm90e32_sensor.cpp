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
                                                atm90e32::Device(_transport, 3)} {
    _last_raw_energy.fill(-1.0F);
}

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

        uint16_t channel_map_u = 0;
        
        for (int i = 0; i < 3; ++i) {
            int channel = chip * CHANNELS_PER_CHIP + i;
            ChannelPhase phase = _config_manager.get_channel_phase(channel);

            uint16_t u_adc;
            int v_idx;

            switch (phase) {
            case ChannelPhase::PHASE_L1:
                u_adc = 6; // U2
                v_idx = 0;
                break;
            case ChannelPhase::PHASE_L2:
                u_adc = 5; // U1
                v_idx = 1;
                break;
            case ChannelPhase::PHASE_L3:
                u_adc = 4; // U0
                v_idx = 2;
                break;
            default:
                u_adc = 6 - i;
                v_idx = i;
                break;
            }

            channel_map_u |= (u_adc << (i * 4));

            device_cfg.voltage_gain[i] = hw_cal.u_gain[v_idx];
            device_cfg.voltage_offset[i] = hw_cal.u_offset[v_idx];
        }

        device_cfg.channel_map_u = channel_map_u;
        device_cfg.channel_map_i = 0x0210; // I0->A, I1->B, I2->C

        const size_t ch_base = chip * CHANNELS_PER_CHIP;
        for (int i = 0; i < 3; ++i) {
            device_cfg.current_gain[i] = hw_cal.i_gain[ch_base + i];
            device_cfg.current_offset[i] = hw_cal.i_offset[ch_base + i];
            device_cfg.phi[i] = hw_cal.phi[ch_base + i];
            device_cfg.p_offset[i] = hw_cal.p_offset[ch_base + i];
            device_cfg.q_offset[i] = hw_cal.q_offset[ch_base + i];
            device_cfg.pq_gain[i] = hw_cal.pq_gain[ch_base + i];
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

        atm90e32::DeviceReading reading;
        esp_err_t err = _devices[map.chip_index].read_line(current_input, reading);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Read failed for channel %d (chip %u): %s", channel,
                     static_cast<unsigned>(map.chip_index), esp_err_to_name(err));
            _channel_cache[channel] = EnergyData{};
            continue;
        }

        _line_cache[map.chip_index][static_cast<int>(current_input)] = reading;

        // Energy accumulation
        float current_raw = reading.energy_kwh;
        if (_last_raw_energy[channel] < 0.0F) {
            _last_raw_energy[channel] = current_raw;
        }

        float delta = current_raw - _last_raw_energy[channel];
        if (delta < -0.1F) { // Chip reset or wraparound
            constexpr float MAX_RAW_ENERGY_KWH = 65536.0F / (100.0F * 3200.0F);
            delta += MAX_RAW_ENERGY_KWH;
        } else if (delta < 0.0F) {
            delta = 0.0F; // Noise or tiny jitter
        }
        
        _last_raw_energy[channel] = current_raw;

        AppConfig::EnergyTotals totals = _config_manager.get_energy_totals();
        totals.total_kwh[channel] += delta;
        _config_manager.set_energy_totals(totals);

        const float energy_offset = _config_manager.get_channel_calibration(channel).energy_offset_kwh;

        EnergyData out{};
        out.voltage = reading.voltage;
        out.current = reading.current;
        out.active_power = reading.active_power;
        out.reactive_power = reading.reactive_power;
        out.apparent_power = reading.apparent_power;
        out.power_factor = reading.power_factor;
        out.energy = totals.total_kwh[channel] + energy_offset;
        out.frequency = reading.frequency;

        _channel_cache[channel] = out;
    }

    if (++_update_count >= 300) { // Every 5 minutes (assuming 1Hz update)
        save_state();
        _update_count = 0;
    }
}

void ATM90E32Sensor::save_state() {
    ESP_LOGI(TAG, "Saving energy totals to NVS...");
    _config_manager.save_energy_totals();
}

EnergyData ATM90E32Sensor::read_channel(int channel) {
    if (channel < 0 || channel >= TOTAL_CHANNELS) {
        return EnergyData{};
    }
    return _channel_cache[channel];
}
