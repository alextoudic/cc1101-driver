// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) 2021
*/
#ifndef CC1101_UTILS_H
#define CC1101_UTILS_H

#include "cc1101_internal.h"

unsigned char cc1101_get_mdmcfg2(const cc1101_common_config_t *common_config, const cc1101_rx_config_t *rx_config);

int cc1101_config_validate_rx(cc1101_t *cc1101, const cc1101_rx_config_t *rx_config);
void cc1101_config_apply_rx(cc1101_t *cc1101);
void cc1101_config_rx_to_registers(unsigned char *config, const cc1101_rx_config_t *rx_config);

#ifndef RXONLY
int cc1101_config_validate_tx(cc1101_t *cc1101, const cc1101_tx_config_t *tx_config);
void cc1101_config_apply_tx(cc1101_t *cc1101);
void cc1101_config_tx_to_registers(unsigned char *config, const cc1101_tx_config_t *tx_config);
#endif

#endif