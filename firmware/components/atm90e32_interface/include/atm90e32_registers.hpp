#pragma once

#include <cstdint>

namespace atm90e32 {

namespace reg {

enum class StatusControl : uint16_t {
    METER_EN = 0x00,
    CHANNEL_MAP_I = 0x01,
    CHANNEL_MAP_U = 0x02,
    SAG_PEAK_DET_CFG = 0x05,
    ZX_CONFIG = 0x07,
    SAG_TH = 0x08,
    FREQ_LO_TH = 0x0C,
    FREQ_HI_TH = 0x0D,
    SOFT_RESET = 0x70,
    EMM_STATE0 = 0x71,
    EMM_STATE1 = 0x72,
    EMM_INT_STATE0 = 0x73,
    EMM_INT_STATE1 = 0x74,
    EMM_INT_EN0 = 0x75,
    EMM_INT_EN1 = 0x76,
    LAST_SPI_DATA = 0x78,
    CRC_ERR_STATUS = 0x79,
    CRC_DIGEST = 0x7A,
    CFG_REG_ACC_EN = 0x7F,
};

enum class CalibrationConfig : uint16_t {
    PL_CONST_H = 0x31,
    PL_CONST_L = 0x32,
    M_MODE0 = 0x33,
    M_MODE1 = 0x34,
    P_START_TH = 0x35,
    Q_START_TH = 0x36,
    S_START_TH = 0x37,
    P_PHASE_TH = 0x38,
    Q_PHASE_TH = 0x39,
    S_PHASE_TH = 0x3A,
    CS1 = 0x3B,

    P_OFFSET_A = 0x41,
    Q_OFFSET_A = 0x42,
    P_OFFSET_B = 0x43,
    Q_OFFSET_B = 0x44,
    P_OFFSET_C = 0x45,
    Q_OFFSET_C = 0x46,
    PQ_GAIN_A = 0x47,
    PHI_A = 0x48,
    PQ_GAIN_B = 0x49,
    PHI_B = 0x4A,
    PQ_GAIN_C = 0x4B,
    PHI_C = 0x4C,

    U_GAIN_A = 0x61,
    I_GAIN_A = 0x62,
    U_OFFSET_A = 0x63,
    I_OFFSET_A = 0x64,
    U_GAIN_B = 0x65,
    I_GAIN_B = 0x66,
    U_OFFSET_B = 0x67,
    I_OFFSET_B = 0x68,
    U_GAIN_C = 0x69,
    I_GAIN_C = 0x6A,
    U_OFFSET_C = 0x6B,
    I_OFFSET_C = 0x6C,
    CS2 = 0x6D,
};

enum class Instant : uint16_t {
    URMS_A = 0xD9,
    URMS_B = 0xDA,
    URMS_C = 0xDB,
    IRMS_A = 0xDD,
    IRMS_B = 0xDE,
    IRMS_C = 0xDF,
    FREQ = 0xF8,
    PF_MEAN_A = 0xBD,
    PF_MEAN_B = 0xBE,
    PF_MEAN_C = 0xBF,
    P_ANGLE_A = 0xF9,
    P_ANGLE_B = 0xFA,
    P_ANGLE_C = 0xFB,
    TEMP = 0xFC,
};

enum class PowerHigh : uint16_t {
    P_MEAN_A = 0xB1,
    P_MEAN_B = 0xB2,
    P_MEAN_C = 0xB3,
    Q_MEAN_A = 0xB5,
    Q_MEAN_B = 0xB6,
    Q_MEAN_C = 0xB7,
    S_MEAN_A = 0xB9,
    S_MEAN_B = 0xBA,
    S_MEAN_C = 0xBB,
};

enum class PowerLow : uint16_t {
    P_MEAN_A_LSB = 0xC1,
    P_MEAN_B_LSB = 0xC2,
    P_MEAN_C_LSB = 0xC3,
    Q_MEAN_A_LSB = 0xC5,
    Q_MEAN_B_LSB = 0xC6,
    Q_MEAN_C_LSB = 0xC7,
    S_MEAN_A_LSB = 0xC9,
    S_MEAN_B_LSB = 0xCA,
    S_MEAN_C_LSB = 0xCB,
};

enum class Energy : uint16_t {
    AP_ENERGY_T = 0x80,
    AP_ENERGY_A = 0x81,
    AP_ENERGY_B = 0x82,
    AP_ENERGY_C = 0x83,
};

} // namespace reg

constexpr uint16_t ATM90E32_RESET_MAGIC = 0x789A;
constexpr uint16_t ATM90E32_CONFIG_UNLOCK_MAGIC = 0x55AA;
constexpr uint16_t ATM90E32_CRC_WRITE_ENABLE_MAGIC = 0xAA55;
constexpr uint16_t ATM90E32_CONFIG_LOCK_MAGIC = 0x0000;

constexpr float VOLTAGE_SCALE = 0.01F;
constexpr float CURRENT_SCALE = 0.001F;
constexpr float POWER_SCALE = 0.00032F;
constexpr float POWER_FACTOR_SCALE = 0.001F;
constexpr float FREQUENCY_SCALE = 0.01F;
constexpr float ENERGY_SCALE_KWH = 1.0F / (100.0F * 3200.0F);

inline constexpr uint16_t to_addr(reg::StatusControl r) {
    return static_cast<uint16_t>(r);
}
inline constexpr uint16_t to_addr(reg::CalibrationConfig r) {
    return static_cast<uint16_t>(r);
}
inline constexpr uint16_t to_addr(reg::Instant r) {
    return static_cast<uint16_t>(r);
}
inline constexpr uint16_t to_addr(reg::PowerHigh r) {
    return static_cast<uint16_t>(r);
}
inline constexpr uint16_t to_addr(reg::PowerLow r) {
    return static_cast<uint16_t>(r);
}
inline constexpr uint16_t to_addr(reg::Energy r) {
    return static_cast<uint16_t>(r);
}

} // namespace atm90e32
