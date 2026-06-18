#pragma once

/**
 * @brief Structure containing the energy readings for a single channel.
 */
struct EnergyData {
    float voltage = 0.0F;        ///< RMS Voltage (V)
    float current = 0.0F;        ///< RMS Current (A)
    float active_power = 0.0F;   ///< Active Power (W)
    float reactive_power = 0.0F; ///< Reactive Power (VAR)
    float apparent_power = 0.0F; ///< Apparent Power (VA)
    float power_factor = 0.0F;   ///< Power Factor
    float energy = 0.0F;         ///< Accumulated Energy (kWh)
    float frequency = 0.0F;      ///< Frequency (Hz)
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
     * @brief Update the sensor's internal state by reading from hardware.
     * @note This should be called periodically (e.g. once per second).
     */
    virtual void update() = 0;

    /**
     * @brief Save any persistent state to NVS.
     * @note Called gracefully on shutdown or periodically.
     */
    virtual void save_state() = 0;

    /**
     * @brief Read the current energy data for a specific channel.
     *
     * @param channel The channel index to read (e.g. 0-11)
     * @return EnergyData containing the latest measurements
     */
    virtual EnergyData read_channel(int channel) = 0;
};