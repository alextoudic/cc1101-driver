// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) 2021
*/
#include "cc1101_config.h"

#include "cc1101_spi.h"

extern uint max_packet_size;

//0x@VH@,  @<<@// @RN@@<<@  @Rd@
unsigned char DEFAULT_CONFIG[] = {
    0x29, // IOCFG2        GDO2 Output Pin Configuration
    0x2E, // IOCFG1        GDO1 Output Pin Configuration
    0x01, // IOCFG0        GDO0 Output Pin Configuration
    0x07, // FIFOTHR       RX FIFO and TX FIFO Thresholds
    0xD3, // SYNC1         Sync Word, High Byte
    0x91, // SYNC0         Sync Word, Low Byte
    0xFF, // PKTLEN        Packet Length
    0x04, // PKTCTRL1      Packet Automation Control
    0x00, // PKTCTRL0      Packet Automation Control
    0x00, // ADDR          Device Address
    0x00, // CHANNR        Channel Number
    0x0F, // FSCTRL1       Frequency Synthesizer Control
    0x00, // FSCTRL0       Frequency Synthesizer Control
    0x1E, // FREQ2         Frequency Control Word, High Byte
    0xC4, // FREQ1         Frequency Control Word, Middle Byte
    0xEC, // FREQ0         Frequency Control Word, Low Byte
    0x8C, // MDMCFG4       Modem Configuration
    0x22, // MDMCFG3       Modem Configuration
    0x02, // MDMCFG2       Modem Configuration
    0x22, // MDMCFG1       Modem Configuration
    0xF8, // MDMCFG0       Modem Configuration
    0x47, // DEVIATN       Modem Deviation Setting
    0x07, // MCSM2         Main Radio Control State Machine Configuration
    0x30, // MCSM1         Main Radio Control State Machine Configuration
    0x04, // MCSM0         Main Radio Control State Machine Configuration
    0x76, // FOCCFG        Frequency Offset Compensation Configuration
    0x6C, // BSCFG         Bit Synchronization Configuration
    0x03, // AGCCTRL2      AGC Control
    0x40, // AGCCTRL1      AGC Control
    0x91, // AGCCTRL0      AGC Control
    0x87, // WOREVT1       High Byte Event0 Timeout
    0x6B, // WOREVT0       Low Byte Event0 Timeout
    0xF8, // WORCTRL       Wake On Radio Control
    0x56, // FREND1        Front End RX Configuration
    0x10, // FREND0        Front End TX Configuration
    0xA9, // FSCAL3        Frequency Synthesizer Calibration
    0x0A, // FSCAL2        Frequency Synthesizer Calibration
    0x20, // FSCAL1        Frequency Synthesizer Calibration
    0x0D, // FSCAL0        Frequency Synthesizer Calibration
    0x41, // RCCTRL1       RC Oscillator Configuration
    0x00, // RCCTRL0       RC Oscillator Configuration
    0x59, // FSTEST        Frequency Synthesizer Calibration Control
    0x7F, // PTEST         Production Test
    0x3F, // AGCTEST       AGC Test
    0x88, // TEST2         Various Test Settings
    0x31, // TEST1         Various Test Settings
    0x0B, // TEST0         Various Test Settings
};

/* 
* Checks if a baud rate is valid for a given modulation
*
* Arguments:
*   baud_rate - baud rate value (e, m)
*   modulation - modulation mode
*
* Returns:
*   1 - Valid
*   0 - Invalid
*
* Min/Max Values (Page 8 - Table 3)
* 
* Min:
*       e    m
* 0.6 - 0x04 0x84 -> 0.601292
* 26  - 0x0a 0x07 -> 26.0849
*
* Max:
*       e    m
* 250 - 0x0d 0x3b -> 249.938965
* 300 - 0x0d 0x7a -> 299.926758
* 500 - 0x0e 0x3b -> 499.87793
*/

#define BAUD_RATE_0_6 0x0484
#define BAUD_RATE_26 0x0a07
#define BAUD_RATE_250 0x0d3b
#define BAUD_RATE_300 0x0d7a
#define BAUD_RATE_500 0x0e3b

int validate_baud_rate(unsigned short baud_rate, unsigned char modulation)
{
    unsigned short min = 0;
    unsigned short max = 0;

    switch (modulation)
    {
    case MOD_2FSK:
        min = BAUD_RATE_0_6;
        max = BAUD_RATE_500;
        break;
    case MOD_GFSK:
    case MOD_OOK:
        min = BAUD_RATE_0_6;
        max = BAUD_RATE_250;
        break;
    case MOD_4FSK:
        min = BAUD_RATE_0_6;
        max = BAUD_RATE_300;
        break;
    case MOD_MSK:
        min = BAUD_RATE_26;
        max = BAUD_RATE_500;
        break;
    }

    if (baud_rate < min || baud_rate > max)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
* Validates a common config struct
* 
* Arguments:
*   cc1101: cc1101 device (only used for error printing)
*   config: a Common config struct
*
* Returns:
* 1  - Valid Config
* 0 - Invalid config
*/
int cc1101_config_validate_common(cc1101_t *cc1101, const cc1101_common_config_t *config)
{

    unsigned short baud_rate = config->baud_rate_exponent << 8 | config->baud_rate_mantissa;

    /* Frequency -> Multiplier Formula:
    *
    *    multiplier = (freq * 2**16) / XTAL_FREQUENCY
    *
    *    e.g 
    *    756184 = (300 * 2**16) / 26
    *
    *  Valid ranges - 300-348 MHz, 387-464 MHz, 779-928 MHz 
    *
    *  299.999756 -> 756184
    *  347.999939 -> 877174
    *  386.999939 -> 975478
    *  463.999786 -> 1169564
    *  778.999878 -> 1963559
    *  928.000000 -> 2339131
    *
    */

    if (!(
            (config->frequency >= 756184 && config->frequency <= 877174) ||
            (config->frequency >= 975478 && config->frequency <= 1169564) ||
            (config->frequency >= 1963559 && config->frequency <= 2339131)))
    {

        CC1101_ERROR(cc1101, "Invalid Frequency - %X", config->frequency);
        return 0;
    }

    if (validate_baud_rate(baud_rate, config->modulation) == 0)
    {
        CC1101_ERROR(cc1101, "Invalid Baud Rate - E:%02x M:%02x", config->baud_rate_exponent, config->baud_rate_mantissa);
        return 0;
    }

    // Sync word is allowed to be any 16 bit value, or the same 16-bit value twice
    if (config->sync_word > 0xFFFF)
    {
        if ((config->sync_word & 0x0000FFFF) != (config->sync_word >> 16))
        {
            CC1101_ERROR(cc1101, "Invalid Sync Word - %08lx", config->sync_word);
            return 0;
        }
    }

    if (config->deviation_exponent > 0x07 || config->deviation_mantissa > 0x07)
    {
        CC1101_ERROR(cc1101, "Invalid Deviation - E: %02x M: %02x", config->deviation_exponent, config->deviation_mantissa);
        return 0;
    }

    return 1;
}

#ifndef RXONLY
/*
* Validates a TX config struct
* 
* Arguments:
*   cc1101: cc1101 device (only used for error printing)
*   tx_config: a TX config struct
*
* Returns:
* 1  - Valid Config
* 0 - Invalid config
*/
int cc1101_config_validate_tx(cc1101_t *cc1101, const cc1101_tx_config_t *tx_config)
{

    // Validate the common configuration
    if (!cc1101_config_validate_common(cc1101, (cc1101_common_config_t *)tx_config))
    {
        return 0;
    }

    // No additional validation for the TX config. Any byte is valid for tx_power

    return 1;
}

/*
* Converts a TX config to a set of CC1101 configuration registers that can be written to the device
* 
* Arguments:
*   config: pointer to a byte array which will contain the configuration register values 
*   tx_config: a TX config struct
*
*/
void cc1101_config_tx_to_registers(unsigned char *config, const cc1101_tx_config_t *tx_config)
{
    // Copy the default config
    memcpy(config, &DEFAULT_CONFIG, sizeof(cc1101_device_config_t));

    // Standard Configuration
    config[IOCFG0] = 0x01;
    config[PKTCTRL1] = 0x04;
    config[PKTCTRL0] = 0x00;
    config[MDMCFG1] = 0x22;
    config[MCSM0] = 0x14;
    config[TEST0] = 0x0B;
    config[FSCAL3] = 0xA9;
    config[FSCAL2] = 0x0A;
    config[FSCAL1] = 0x20;
    config[FSCAL0] = 0x0D;

    // User Configuration

    // Shift frequency multiplier across registers
    config[FREQ2] = tx_config->common.frequency >> 16;
    config[FREQ1] = tx_config->common.frequency >> 8;
    config[FREQ0] = tx_config->common.frequency & 0x00FF;

    // Set baud rate exponent. RX bandwidth exponent and mantissa set to 0 (not used in TX)
    config[MDMCFG4] = tx_config->common.baud_rate_exponent;
    config[MDMCFG3] = tx_config->common.baud_rate_mantissa;
    config[MDMCFG2] = cc1101_get_mdmcfg2(&tx_config->common, NULL);

    config[SYNC1] = (tx_config->common.sync_word & 0x0000FF00) >> 8;
    config[SYNC0] = tx_config->common.sync_word & 0x000000FF;

    config[DEVIATN] = tx_config->common.deviation_exponent << 4 | tx_config->common.deviation_mantissa;

    if (tx_config->common.modulation == MOD_OOK)
    {
        config[FREND0] = 0x11; // PATABLE position 1 is used for OOK on, position 0 for off (default 0)
    }
    else
    {
        config[FREND0] = 0x10; // PATABLE position 0 for all other modulations (i.e disable power ramping)
    }
}

/*
* Apply a stored TX config to the hardware
* 
* Arguments:
*   cc1101: cc1101 device
*
*/
void cc1101_config_apply_tx(cc1101_t *cc1101)
{
    cc1101_device_config_t device_config;
    cc1101_patable_t patable = {0};

    // Convert the configuration to a set of register values
    cc1101_config_tx_to_registers(device_config, &cc1101->tx_config);

    // Set the PATABLE value
    if (cc1101->tx_config.common.modulation == MOD_OOK)
    {
        // OOK uses PATABLE[0] for off power and PATABLE[1] for on power
        patable[1] = cc1101->tx_config.tx_power;
    }
    else
    {
        patable[0] = cc1101->tx_config.tx_power;
    }

    // Write the registers and PATABLE to the device
    cc1101_spi_write_config_registers(cc1101, device_config, sizeof(cc1101_device_config_t));
    cc1101_spi_write_patable(cc1101, patable, sizeof(patable));
}
#endif

/*
* Validates an RX config struct
* 
* Arguments:
*   cc1101: cc1101 device (only used for error printing)
*   rx_config: an RX config struct
*
* Returns:
* 1  - Valid Config
* 0 - Invalid config
*/
int cc1101_config_validate_rx(cc1101_t *cc1101, const cc1101_rx_config_t *rx_config)
{

    // Validate the common configuration
    if (!cc1101_config_validate_common(cc1101, (cc1101_common_config_t *)rx_config))
    {
        return 0;
    }

    switch (rx_config->max_lna_gain)
    {
    case 0:
    case 3:
    case 6:
    case 7:
    case 9:
    case 12:
    case 15:
    case 17:
        break;
    default:
        CC1101_ERROR(cc1101, "Invalid Max LNA Gain %d dB", rx_config->max_lna_gain);
        return 0;
    }

    switch (rx_config->max_dvga_gain)
    {
    case 0:
    case 6:
    case 12:
    case 18:
        break;
    default:
        CC1101_ERROR(cc1101, "Invalid Max DVGA Gain %d dB", rx_config->max_dvga_gain);
        return 0;
    }

    switch (rx_config->magn_target)
    {
    case 24:
    case 27:
    case 30:
    case 33:
    case 36:
    case 38:
    case 40:
    case 42:
        break;
    default:
        CC1101_ERROR(cc1101, "Invalid Channel Filter Target Amplitude %d dB", rx_config->magn_target);
        return 0;
    }

    if (rx_config->carrier_sense_mode == CS_DISABLED)
    {
        // Do nothing
    }
    else if (rx_config->carrier_sense_mode == CS_ABSOLUTE)
    {
        // Absolute carrier sense threshold must be between -7 dB and 7 dB
        if (rx_config->carrier_sense < -7 || rx_config->carrier_sense > 7)
        {
            CC1101_ERROR(cc1101, "Invalid Absolute Carrier Sense Threshold %d dB", rx_config->carrier_sense);
            return 0;
        }
    }
    else if (rx_config->carrier_sense_mode == CS_RELATIVE)
    {
        // Relative carrier sense threshold must be either 6, 10 or 14 dB
        switch (rx_config->carrier_sense)
        {
        case 6:
        case 10:
        case 14:
            break;
        default:
            CC1101_ERROR(cc1101, "Invalid Relative Carrier Sense Threshold %d dB", rx_config->carrier_sense);
            return 0;
        }
    }
    else
    {
        CC1101_ERROR(cc1101, "Invalid Carrier Sense Mode %d", rx_config->carrier_sense_mode);
        return 0;
    }

    // Validate the packet length value provided from userspace
    if (rx_config->packet_length == 0 || rx_config->packet_length > max_packet_size)
    {
        CC1101_ERROR(cc1101, "Invalid Receive Packet Length %d", rx_config->packet_length);
        return 0;
    }

    // Validate Bandwidth
    if (rx_config->bandwidth_exponent > 3 || rx_config->bandwidth_mantissa > 3)
    {
        CC1101_ERROR(cc1101, "Invalid Deviation - E: %02x M: %02x", rx_config->bandwidth_exponent, rx_config->bandwidth_mantissa);
        return 0;
    }

    return 1;
}

/*
* Converts an RX config to a set of CC1101 configuration registers that can be written to the device
* 
* Arguments:
*   config: pointer to a byte array which will contain the configuration register values 
*   rx_config: an RX config struct
*
*/
void cc1101_config_rx_to_registers(unsigned char *config, const cc1101_rx_config_t *rx_config)
{
    // Copy the default config
    memcpy(config, &DEFAULT_CONFIG, sizeof(cc1101_device_config_t));

    // Standard Configuration
    config[IOCFG0] = 0x01;
    config[PKTCTRL1] = 0x04;
    config[PKTCTRL0] = 0x00;
    config[MDMCFG1] = 0x22;
    config[MCSM0] = 0x14;
    config[TEST0] = 0x0B;
    config[FSCAL3] = 0xA9;
    config[FSCAL2] = 0x0A;
    config[FSCAL1] = 0x20;
    config[FSCAL0] = 0x0D;

    // User Configuration
    config[FREQ2] = rx_config->common.frequency >> 16;
    config[FREQ1] = rx_config->common.frequency >> 8;
    config[FREQ0] = rx_config->common.frequency & 0x00FF;

    config[MDMCFG4] = rx_config->bandwidth_exponent << 6 | rx_config->bandwidth_mantissa << 4 | rx_config->common.baud_rate_exponent;
    config[MDMCFG3] = rx_config->common.baud_rate_mantissa;
    config[MDMCFG2] = cc1101_get_mdmcfg2(&rx_config->common, rx_config);

    config[SYNC1] = (rx_config->common.sync_word & 0x0000FF00) >> 8;
    config[SYNC0] = rx_config->common.sync_word & 0x000000FF;

    config[DEVIATN] = rx_config->common.deviation_exponent << 4 | rx_config->common.deviation_mantissa;

    // Set the MAGN_TARGET from the config
    switch (rx_config->magn_target)
    {
    case 27:
        config[AGCCTRL2] = 1;
        break;
    case 30:
        config[AGCCTRL2] = 2;
        break;
    case 33:
        config[AGCCTRL2] = 3;
        break;
    case 36:
        config[AGCCTRL2] = 4;
        break;
    case 38:
        config[AGCCTRL2] = 5;
        break;
    case 40:
        config[AGCCTRL2] = 6;
        break;
    case 42:
        config[AGCCTRL2] = 7;
        break;
    default:
        config[AGCCTRL2] = 0;
    }

    // Set the MAX_DVGA_GAIN from the config
    switch (rx_config->max_dvga_gain)
    {
    case 6:
        config[AGCCTRL2] |= (1 << 6);
        break;
    case 12:
        config[AGCCTRL2] |= (2 << 6);
        break;
    case 18:
        config[AGCCTRL2] |= (3 << 6);
        break;
    default:
        break;
    }

    // Set the MAX_LNA_GAIN from the config
    switch (rx_config->max_lna_gain)
    {
    case 3:
        config[AGCCTRL2] |= (1 << 3);
        break;
    case 6:
        config[AGCCTRL2] |= (2 << 3);
        break;
    case 7:
        config[AGCCTRL2] |= (3 << 3);
        break;
    case 9:
        config[AGCCTRL2] |= (4 << 3);
        break;
    case 12:
        config[AGCCTRL2] |= (5 << 3);
        break;
    case 15:
        config[AGCCTRL2] |= (6 << 3);
        break;
    case 17:
        config[AGCCTRL2] |= (7 << 3);
        break;
    default:
        break;
    }

    // Set the CARRIER_SENSE_REL_THR and CARRIER_SENSE_ABS_THR based on the config value
    // Set AGC_LNA_PRIORITY to the default value
    if (rx_config->carrier_sense_mode == CS_ABSOLUTE)
    {
        config[AGCCTRL1] = 0x40 | ((char)rx_config->carrier_sense & 0x0F);
    }
    else if (rx_config->carrier_sense_mode == CS_RELATIVE)
    {
        switch (rx_config->carrier_sense)
        {
        case 6:
            config[AGCCTRL1] = 0x58; // Default AGC_LNA_PRIORITY. CARRIER_SENSE_REL_THR 6dB increase in RSSI. CS absolute threshold disabled
            break;
        case 10:
            config[AGCCTRL1] = 0x68; // Default AGC_LNA_PRIORITY. CARRIER_SENSE_REL_THR 10dB increase in RSSI. CS absolute threshold disabled
            break;
        default:
            config[AGCCTRL1] = 0x78; // Default AGC_LNA_PRIORITY. CARRIER_SENSE_REL_THR 14dB increase in RSSI. CS absolute threshold disabled
            break;
        }
    }
}

/*
* Apply a stored RX config to the hardware
* 
* Arguments:
*   cc1101: cc1101 device
*
*/
void cc1101_config_apply_rx(cc1101_t *cc1101)
{
    cc1101_device_config_t device_config;

    // Convert the configuration to a set of register values
    cc1101_config_rx_to_registers(device_config, &cc1101->rx_config);

    // Write the registers to the device
    cc1101_spi_write_config_registers(cc1101, device_config, sizeof(cc1101_device_config_t));
}

/*
* Get the value for the MDMCFG2 register from common config. This value is needed by the RX interrupt handler
* 
* Arguments:
*   config: common config for TX or RX
*   rx_config: an RX configuration. Can be NULL for TX
*/
unsigned char cc1101_get_mdmcfg2(const cc1101_common_config_t *config, const cc1101_rx_config_t *rx_config)
{

    unsigned char value = (config->modulation << 4) & 0x70; // Enable DC Filter. Modulation from config.

    if (rx_config == NULL || rx_config->carrier_sense_mode == CS_DISABLED)
    {
        if (config->sync_word > 0 && config->sync_word <= 0xFFFF)
        {
            value = value | 0x02; //Manchester encoding disabled. 16 sync bits, carrier sense disabled
        }
        else if (config->sync_word > 0xFFFF)
        {
            value = value | 0x03; //Manchester encoding disabled. 32 sync bits, carrier sense disabled
        }
    }
    else
    {
        if (config->sync_word == 0)
        {
            value = value | 0x04; //Manchester encoding disabled. no sync word, carrier sense enabled
        }
        else if (config->sync_word > 0 && config->sync_word <= 0xFFFF)
        {
            value = value | 0x06; //Manchester encoding disabled. 16 sync bits, carrier sense enabled
        }
        else
        {
            value = value | 0x07; //Manchester encoding disabled. 32 sync bits, carrier sense enabled
        }
    }

    return value;
}
