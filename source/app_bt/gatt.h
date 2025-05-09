/*
 * $ Copyright YEAR Cypress Semiconductor $
 */

/**
 * file gatt.h
 *
 * GATT function header file
 *
 */

#ifndef APP_GATTS_H__
#define APP_GATTS_H__

#include "wiced_bt_gatt.h"
#include "wiced_bt_ble.h"
#include "cycfg_gatt_db.h"  /* generated by BT configurator */

/*******************************************************************************
 * Function Name: gatt_read_req_default_handler
 *******************************************************************************
 * Summary:
 *  Default handler to process read request or command from peer device.
 *  The event calls application gatt_read_req_handler first. When it is not
 *  handled in application, this default handler is called.
 *
 * Parameters:
 *  uint16_t conn_id
 *  wiced_bt_gatt_write_req_t * p_wr_data
 *
 * Return:
 *  wiced_bt_gatt_status_t
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_write_default_handler(uint16_t conn_id,
                          wiced_bt_gatt_write_req_t * p_wr_data);

/***********************************************************
 * Function Name: gatt_write_default_handler
 ***********************************************************
 * Summary:
 *  Default handler to process write request or command from peer device.
 *  The event calls application gatt_write_handler first. When it is not
 *  handled in application, this default handler is called.
 *
 * Parameters:
 *  uint16_t conn_id
 *  wiced_bt_gatt_write_req_t * p_wr_data
 *
 * Return:
 *  wiced_bt_gatt_status_t
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_read_req_default_handler( uint16_t conn_id,
                           wiced_bt_gatt_read_t *p_req,
                           wiced_bt_gatt_opcode_t opcode,
                           uint16_t len_requested );

/*******************************************************************************
 * Function Name: gatt_initialize
 *******************************************************************************
 * Summary:
 *  This function uses BT configurator generated source to
 *  initialize gatt database.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  wiced_bt_gatt_status_t
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_initialize();

#endif //APP_GATTS_H__
