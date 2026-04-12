#pragma once

#include "atm90e32_device.hpp"
#include "atm90e32_spi.hpp"
#include "config_manager.hpp"
#include "i_energy_sensor.hpp"

#include <array>

class ATM90E32Sensor : public IEnergySensor {
public:
    explicit ATM90E32Sensor(ConfigManager& config_manager);

    bool init() override;
    void update() override;
    EnergyData read_channel(int channel) override;

private:
    static constexpr int CHIP_COUNT = 4;
    static constexpr int CHANNELS_PER_CHIP = 3;
    static constexpr int TOTAL_CHANNELS = CHIP_COUNT * CHANNELS_PER_CHIP;

    struct ChannelMap {
        std::size_t chip_index;
        atm90e32::LineInput input;
    };

    ChannelMap map_channel(int channel) const;
    atm90e32::LineInput phase_to_input(ChannelPhase phase, atm90e32::LineInput fallback) const;

    ConfigManager& _config_manager;
    atm90e32::SpiTransport _transport;
    std::array<atm90e32::Device, CHIP_COUNT> _devices;
    std::array<std::array<atm90e32::DeviceReading, CHANNELS_PER_CHIP>, CHIP_COUNT> _line_cache;
    std::array<EnergyData, TOTAL_CHANNELS> _channel_cache;
};
