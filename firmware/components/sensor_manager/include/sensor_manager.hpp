#pragma once

#include <memory>
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "i_energy_sensor.hpp"
#include "mqtt_manager.hpp"

/**
 * @brief Manages the energy sensor lifecycle and periodic telemetry publishing.
 */
class SensorManager {
public:
    /**
     * @brief Construct a new Sensor Manager object
     * 
     * @param sensor Unique pointer to an implementation of IEnergySensor (e.g., DummyEnergySensor)
     * @param mqtt_manager Reference to the MqttManager for publishing telemetry
     * @param num_channels Number of channels the sensor manages
     */
    SensorManager(std::unique_ptr<IEnergySensor> sensor, MqttManager& mqtt_manager, int num_channels = 12);

    /**
     * @brief Destroy the Sensor Manager object, ensuring the telemetry task is stopped.
     */
    ~SensorManager();

    /**
     * @brief Start the telemetry FreeRTOS task.
     * 
     * @return true if the task was started successfully, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the telemetry FreeRTOS task.
     */
    void stop();

private:
    std::unique_ptr<IEnergySensor> _sensor;
    MqttManager& _mqtt_manager;
    int _num_channels;
    
    TaskHandle_t _task_handle;
    bool _running;

    static void task_entry(void* arg);
    void task_loop();
    
    void publish_telemetry();
};