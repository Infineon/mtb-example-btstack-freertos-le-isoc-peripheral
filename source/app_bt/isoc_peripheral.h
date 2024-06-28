/*
 * $ Copyright YEAR Cypress Semiconductor $
 */
/*
 * @file isoc_peripheral.h
 *
 * @brief This is the API defines for isoc functions for peripheral device
 */
#ifndef ISOC_PERIPHERAL_H_
#define ISOC_PERIPHERAL_H_

#include "wiced_bt_isoc.h"

void isoc_init();
void isoc_send_data(wiced_bool_t c);
wiced_bool_t isoc_cis_connected();
void isoc_start();

#endif // ISOC_PERIPHERAL_H_

/* [] END OF FILE */
