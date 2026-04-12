#pragma once

#include <driver/spi_master.h>
#include <esp_err.h>
#include <hal/gpio_types.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace atm90e32 {

struct SpiBusConfig {
    spi_host_device_t host = SPI2_HOST;
    int sclk_gpio = 12;
    int miso_gpio = 13;
    int mosi_gpio = 11;
    std::array<int, 4> cs_gpios = {10, 9, 8, 7};
    int clock_hz = 200000;
};

class SpiTransport {
public:
    SpiTransport() = default;
    ~SpiTransport();

    esp_err_t init(const SpiBusConfig& config);
    esp_err_t read16(std::size_t chip_index, uint16_t reg, uint16_t& out_value) const;
    esp_err_t write16(std::size_t chip_index, uint16_t reg, uint16_t value) const;
    esp_err_t read32_split(std::size_t chip_index, uint16_t high_reg, uint16_t low_reg,
                           int32_t& out_value) const;

private:
    esp_err_t transfer16(std::size_t chip_index, bool is_read, uint16_t reg, uint16_t tx_value,
                         uint16_t& rx_value) const;

    bool _initialized = false;
    SpiBusConfig _config{};
    std::array<spi_device_handle_t, 4> _devices{};
};

} // namespace atm90e32
