#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace atm90e32::mock {

class MockChips {
public:
    static constexpr std::size_t CHIP_COUNT = 4;

    MockChips();
    uint16_t read(std::size_t chip_index, uint16_t reg_addr);
    void write(std::size_t chip_index, uint16_t reg_addr, uint16_t value);

private:
    struct ChipState {
        std::unordered_map<uint16_t, uint16_t> regs;
        uint32_t tick = 0;
        uint16_t profile_seed = 0;
    };

    std::array<ChipState, CHIP_COUNT> _chips;

    static uint16_t wave(uint32_t tick, uint16_t phase, uint16_t amplitude);
    uint16_t read_power_reg(ChipState& chip, uint16_t reg_addr, uint16_t irms_reg, uint16_t factor,
                            bool high_word) const;
};

} // namespace atm90e32::mock
