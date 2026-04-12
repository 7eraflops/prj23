#include "atm90e32_sensor.hpp"

#include <esp_log.h>

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
    atm90e32::SpiBusConfig spi_cfg;
    spi_cfg.simulate = true;
    if (_transport.init(spi_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ATM90E32 SPI transport");
        return false;
    }

    atm90e32::DeviceConfig device_cfg;
    device_cfg.line_freq_mode = 0x0087;
    device_cfg.pga_gain = 0x0000;
    device_cfg.voltage_gain = 0x0000;
    device_cfg.current_gain_a = 0x0000;
    device_cfg.current_gain_b = 0x0000;
    device_cfg.current_gain_c = 0x0000;

    bool any_ok = false;
    for (auto& device : _devices) {
        const esp_err_t err = device.init(device_cfg);
        if (err == ESP_OK) {
            any_ok = true;
        } else {
            ESP_LOGW(TAG, "Chip init failed with %s", esp_err_to_name(err));
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
    case ChannelPhase::PHASE_A:
        return atm90e32::LineInput::A;
    case ChannelPhase::PHASE_B:
        return atm90e32::LineInput::B;
    case ChannelPhase::PHASE_C:
        return atm90e32::LineInput::C;
    case ChannelPhase::NONE:
    default:
        return fallback;
    }
}

void ATM90E32Sensor::update() {
    for (int channel = 0; channel < TOTAL_CHANNELS; ++channel) {
        const ChannelMap map = map_channel(channel);

        if (!_config_manager.is_channel_active(channel)) {
            _channel_cache[channel] = EnergyData{};
            continue;
        }

        ChannelPhase phase = _config_manager.get_channel_phase(channel);
        atm90e32::LineInput requested_input = phase_to_input(phase, map.input);

        atm90e32::DeviceReading reading;
        const esp_err_t err = _devices[map.chip_index].read_line(requested_input, reading);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Read failed for channel %d (chip %u): %s", channel,
                     static_cast<unsigned>(map.chip_index), esp_err_to_name(err));
            _channel_cache[channel] = EnergyData{};
            continue;
        }

        _line_cache[map.chip_index][static_cast<int>(requested_input)] = reading;

        AppConfig::ChannelCalibration calibration = _config_manager.get_channel_calibration(channel);

        EnergyData out{};
        out.voltage = reading.voltage;
        out.current = reading.current * calibration.current_gain;
        out.active_power = reading.active_power * calibration.power_gain;
        out.reactive_power = reading.reactive_power * calibration.power_gain;
        out.apparent_power = reading.apparent_power * calibration.power_gain;
        out.power_factor = reading.power_factor;
        out.energy = reading.energy_kwh + calibration.energy_offset_kwh;
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
