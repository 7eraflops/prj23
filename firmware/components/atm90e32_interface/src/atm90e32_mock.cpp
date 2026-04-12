#include "atm90e32_mock.hpp"

#include "atm90e32_registers.hpp"

namespace atm90e32::mock {

MockChips::MockChips() {
    for (std::size_t chip = 0; chip < _chips.size(); ++chip) {
        auto& c = _chips[chip];
        c.profile_seed = static_cast<uint16_t>((chip + 1) * 37);
        c.regs[to_addr(reg::StatusControl::LAST_SPI_DATA)] = 0xA5A5;
        c.regs[to_addr(reg::StatusControl::EMM_STATE0)] = 0x0001;
        c.regs[to_addr(reg::StatusControl::EMM_STATE1)] = 0x0001;
        c.regs[to_addr(reg::Instant::URMS_A)] = static_cast<uint16_t>(22980 + chip * 30);
        c.regs[to_addr(reg::Instant::URMS_B)] = static_cast<uint16_t>(23040 + chip * 30);
        c.regs[to_addr(reg::Instant::URMS_C)] = static_cast<uint16_t>(22920 + chip * 30);
        c.regs[to_addr(reg::Instant::IRMS_A)] = static_cast<uint16_t>(1100 + chip * 210);
        c.regs[to_addr(reg::Instant::IRMS_B)] = static_cast<uint16_t>(900 + chip * 180);
        c.regs[to_addr(reg::Instant::IRMS_C)] = static_cast<uint16_t>(500 + chip * 120);
        c.regs[to_addr(reg::Instant::PF_MEAN_A)] = 972;
        c.regs[to_addr(reg::Instant::PF_MEAN_B)] = 955;
        c.regs[to_addr(reg::Instant::PF_MEAN_C)] = 988;
        c.regs[to_addr(reg::Instant::FREQ)] = 5000;
        c.regs[to_addr(reg::Energy::AP_ENERGY_A)] = static_cast<uint16_t>(30 + chip * 10);
        c.regs[to_addr(reg::Energy::AP_ENERGY_B)] = static_cast<uint16_t>(20 + chip * 8);
        c.regs[to_addr(reg::Energy::AP_ENERGY_C)] = static_cast<uint16_t>(10 + chip * 6);
    }
}

uint16_t MockChips::wave(uint32_t tick, uint16_t phase, uint16_t amplitude) {
    const uint16_t tri = static_cast<uint16_t>((tick + phase) % (amplitude * 2));
    if (tri <= amplitude) {
        return tri;
    }
    return static_cast<uint16_t>((amplitude * 2) - tri);
}

uint16_t MockChips::read_power_reg(ChipState& chip, uint16_t reg_addr, uint16_t irms_reg, uint16_t factor,
                                   bool high_word) const {
    (void)reg_addr;
    const uint32_t jitter = chip.tick % 11;
    const uint32_t amps = static_cast<uint32_t>(chip.regs[irms_reg]) + jitter;
    const uint32_t power = amps * static_cast<uint32_t>(factor);
    if (high_word) {
        return static_cast<uint16_t>((power >> 16) & 0xFFFF);
    }
    return static_cast<uint16_t>(power & 0xFFFF);
}

uint16_t MockChips::read(std::size_t chip_index, uint16_t reg_addr) {
    if (chip_index >= _chips.size()) {
        return 0;
    }

    auto& chip = _chips[chip_index];
    uint16_t value = chip.regs[reg_addr];

    if (reg_addr == to_addr(reg::Instant::URMS_A) || reg_addr == to_addr(reg::Instant::URMS_B) ||
        reg_addr == to_addr(reg::Instant::URMS_C)) {
        value = static_cast<uint16_t>(value + wave(chip.tick, chip.profile_seed, 18));
    } else if (reg_addr == to_addr(reg::Instant::IRMS_A) || reg_addr == to_addr(reg::Instant::IRMS_B) ||
               reg_addr == to_addr(reg::Instant::IRMS_C)) {
        value = static_cast<uint16_t>(value + wave(chip.tick, static_cast<uint16_t>(chip.profile_seed + 7), 45));
    } else if (reg_addr == to_addr(reg::Instant::PF_MEAN_A) || reg_addr == to_addr(reg::Instant::PF_MEAN_B) ||
               reg_addr == to_addr(reg::Instant::PF_MEAN_C)) {
        value = static_cast<uint16_t>(950 + wave(chip.tick, static_cast<uint16_t>(chip.profile_seed + 11), 45));
    } else if (reg_addr == to_addr(reg::Instant::FREQ)) {
        value = static_cast<uint16_t>(4995 + wave(chip.tick, static_cast<uint16_t>(chip.profile_seed + 17), 10));
    } else if (reg_addr == to_addr(reg::PowerHigh::P_MEAN_A)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_A), 44, true);
    } else if (reg_addr == to_addr(reg::PowerLow::P_MEAN_A_LSB)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_A), 44, false);
    } else if (reg_addr == to_addr(reg::PowerHigh::P_MEAN_B)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_B), 41, true);
    } else if (reg_addr == to_addr(reg::PowerLow::P_MEAN_B_LSB)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_B), 41, false);
    } else if (reg_addr == to_addr(reg::PowerHigh::P_MEAN_C)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_C), 38, true);
    } else if (reg_addr == to_addr(reg::PowerLow::P_MEAN_C_LSB)) {
        value = read_power_reg(chip, reg_addr, to_addr(reg::Instant::IRMS_C), 38, false);
    } else if (reg_addr == to_addr(reg::PowerLow::Q_MEAN_A_LSB) || reg_addr == to_addr(reg::PowerLow::Q_MEAN_B_LSB) ||
               reg_addr == to_addr(reg::PowerLow::Q_MEAN_C_LSB)) {
        value = static_cast<uint16_t>(2600 + wave(chip.tick, static_cast<uint16_t>(chip.profile_seed + 19), 240));
    } else if (reg_addr == to_addr(reg::PowerLow::S_MEAN_A_LSB) || reg_addr == to_addr(reg::PowerLow::S_MEAN_B_LSB) ||
               reg_addr == to_addr(reg::PowerLow::S_MEAN_C_LSB)) {
        value = static_cast<uint16_t>(4600 + wave(chip.tick, static_cast<uint16_t>(chip.profile_seed + 23), 330));
    } else if (reg_addr == to_addr(reg::Energy::AP_ENERGY_A) || reg_addr == to_addr(reg::Energy::AP_ENERGY_B) ||
               reg_addr == to_addr(reg::Energy::AP_ENERGY_C)) {
        value = static_cast<uint16_t>(value + 1 + (chip.tick % 2));
        chip.regs[reg_addr] = value;
    }

    chip.regs[to_addr(reg::StatusControl::LAST_SPI_DATA)] = value;
    ++chip.tick;
    return value;
}

void MockChips::write(std::size_t chip_index, uint16_t reg_addr, uint16_t value) {
    if (chip_index >= _chips.size()) {
        return;
    }
    auto& chip = _chips[chip_index];
    chip.regs[reg_addr] = value;
    chip.regs[to_addr(reg::StatusControl::LAST_SPI_DATA)] = value;
}

} // namespace atm90e32::mock
