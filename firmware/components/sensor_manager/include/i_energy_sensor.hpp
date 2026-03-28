#pragma once

/**
 * @brief Structure containing the energy readings for a single channel.
 */
struct EnergyData {
    float voltage;        ///< RMS Voltage (V)
    float current;        ///< RMS Current (A)
    float active_power;   ///< Active Power (W)
    float reactive_power; ///< Reactive Power (VAR)
    float apparent_power; ///< Apparent Power (VA)
    float power_factor;   ///< Power Factor
    float energy;         ///< Accumulated Energy (kWh)
    float frequency;      ///< Frequency (Hz)
};

/**
 * @brief Interface for energy sensors to allow mocking and abstraction.
 */
class IEnergySensor {
public:
    virtual ~IEnergySensor() = default;

    /**
     * @brief Initialize the sensor.
     *
     * @return true if successful, false otherwise.
     */
    virtual bool init() = 0;

    /**
     * @brief Update the internal state/readings from the physical or mock hardware.
     */
    virtual void update() = 0;

    /**
     * @brief Get the latest reading for a specific channel.
     *
     * @param channel The channel index (e.g., 0-11 for a 12-channel meter).
     * @return EnergyData containing the latest measurements.
     */
    virtual EnergyData read_channel(int channel) = 0;
};