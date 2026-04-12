#include "atm90e32_spi.hpp"

#include "atm90e32_mock.hpp"
#include <esp_log.h>

#include <cstring>
#include <memory>

namespace atm90e32 {

static const char* TAG = "ATM90_SPI";

SpiTransport::~SpiTransport() {
    if (!_initialized || _config.simulate) {
        return;
    }

    for (auto& device : _devices) {
        if (device != nullptr) {
            spi_bus_remove_device(device);
            device = nullptr;
        }
    }

    spi_bus_free(_config.host);
    _initialized = false;
}

esp_err_t SpiTransport::init(const SpiBusConfig& config) {
    if (_initialized) {
        return ESP_OK;
    }

    _config = config;

    if (_config.simulate) {
        _mock = std::make_shared<mock::MockChips>();

        _initialized = true;
        ESP_LOGW(TAG, "ATM90E32 SPI transport in simulation mode");
        return ESP_OK;
    }

    spi_bus_config_t bus_cfg = {};
    bus_cfg.miso_io_num = _config.miso_gpio;
    bus_cfg.mosi_io_num = _config.mosi_gpio;
    bus_cfg.sclk_io_num = _config.sclk_gpio;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;

    esp_err_t err = spi_bus_initialize(_config.host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
        return err;
    }

    for (std::size_t i = 0; i < _devices.size(); ++i) {
        spi_device_interface_config_t dev_cfg = {};
        dev_cfg.clock_speed_hz = _config.clock_hz;
        dev_cfg.mode = 3;
        dev_cfg.spics_io_num = _config.cs_gpios[i];
        dev_cfg.queue_size = 2;
        dev_cfg.command_bits = 0;
        dev_cfg.address_bits = 0;
        dev_cfg.dummy_bits = 0;

        err = spi_bus_add_device(_config.host, &dev_cfg, &_devices[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add SPI device %u: %s", static_cast<unsigned>(i),
                     esp_err_to_name(err));
            return err;
        }
    }

    _initialized = true;
    return ESP_OK;
}

esp_err_t SpiTransport::transfer16(std::size_t chip_index, bool is_read, uint16_t reg,
                                   uint16_t tx_value, uint16_t& rx_value) const {
    if (_config.simulate) {
        return simulate_transfer(chip_index, is_read, reg, tx_value, rx_value);
    }

    if (!_initialized || chip_index >= _devices.size() || _devices[chip_index] == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t reg_frame = reg;
    if (is_read) {
        reg_frame |= 0x8000;
    } else {
        reg_frame &= static_cast<uint16_t>(~0x8000);
    }

    reg_frame = static_cast<uint16_t>((reg_frame >> 8) | (reg_frame << 8));
    tx_value = static_cast<uint16_t>((tx_value >> 8) | (tx_value << 8));

    spi_transaction_t t = {};
    t.length = 32;
    uint8_t tx_buffer[4] = {
        static_cast<uint8_t>(reg_frame >> 8),
        static_cast<uint8_t>(reg_frame & 0xFF),
        static_cast<uint8_t>(tx_value >> 8),
        static_cast<uint8_t>(tx_value & 0xFF),
    };
    uint8_t rx_buffer[4] = {};

    t.tx_buffer = tx_buffer;
    t.rx_buffer = rx_buffer;

    const esp_err_t err = spi_device_transmit(_devices[chip_index], &t);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t rx_raw = static_cast<uint16_t>((rx_buffer[2] << 8) | rx_buffer[3]);
    rx_value = static_cast<uint16_t>((rx_raw >> 8) | (rx_raw << 8));
    return ESP_OK;
}

esp_err_t SpiTransport::simulate_transfer(std::size_t chip_index, bool is_read, uint16_t reg,
                                          uint16_t tx_value, uint16_t& rx_value) const {
    if (!_initialized || !_mock) {
        return ESP_ERR_INVALID_STATE;
    }

    if (is_read) {
        rx_value = _mock->read(chip_index, reg);
        return ESP_OK;
    }

    _mock->write(chip_index, reg, tx_value);
    rx_value = 0;
    return ESP_OK;
}

esp_err_t SpiTransport::read16(std::size_t chip_index, uint16_t reg, uint16_t& out_value) const {
    return transfer16(chip_index, true, reg, 0xFFFF, out_value);
}

esp_err_t SpiTransport::write16(std::size_t chip_index, uint16_t reg, uint16_t value) const {
    uint16_t sink = 0;
    return transfer16(chip_index, false, reg, value, sink);
}

esp_err_t SpiTransport::read32_split(std::size_t chip_index, uint16_t high_reg, uint16_t low_reg,
                                     int32_t& out_value) const {
    uint16_t high = 0;
    uint16_t low = 0;

    esp_err_t err = read16(chip_index, high_reg, high);
    if (err != ESP_OK) {
        return err;
    }

    err = read16(chip_index, low_reg, low);
    if (err != ESP_OK) {
        return err;
    }

    out_value = static_cast<int32_t>((static_cast<uint32_t>(high) << 16) | static_cast<uint32_t>(low));
    return ESP_OK;
}

} // namespace atm90e32
