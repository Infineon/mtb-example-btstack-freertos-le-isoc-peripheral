/*
 * $ Copyright YEAR Cypress Semiconductor $
 */

/**
 * file app.h
 *
 * This is the HID over ISOC demo application for HID Device.
 * This application should be used together with
 * isoc_hidh, HID Host, for the demo.
 *
 */
#ifndef APP_H_
#define APP_H_

#include "bt.h"
#include "gatt.h"
#include "host.h"
#include "link.h"
#include "led.h"
#include "button.h"
#include "nvram_lib.h"

#include "cyabs_rtos_impl.h"
#include "isoc_peripheral.h"

/* Priority for GPIO Button Interrupt */
#define GPIO_INTERRUPT_PRIORITY     (7u)

#define ISO_SDU_SIZE                100


/******************************************************************************
 * macros
 *****************************************************************************/
#define cfg_mtu()     (p_wiced_bt_cfg_settings->p_ble_cfg->ble_max_rx_pdu_size)

/******************************************************************************
 * extern
 *****************************************************************************/

/******************************************************************************
 *     Public Function Definitions
 *****************************************************************************/

/*****************************************************************************
 * Function Name: app_remove_host_bonding
 *****************************************************************************
 * Summary:
 *  Virtual cable unplug.
 *  This function will remove all HID host information from NVRAM.
 *
 * Parameters:
 *    none
 *
 * Return:
 *    none
 *
 ******************************************************************************/
void app_remove_host_bonding(void);

/******************************************************************************
 * Function Name: app_gatt_write_handler
 ******************************************************************************
 * Summary:
 *  This function is called when GATT handle write req event is recieved.
 *
 * Parameters:
 *  uint16_t conn_id    -- Connection ID
 *  wiced_bt_gatt_write_req_t *      -- Pointer to gatt_write data
 *
 * Return:
 *  wiced_bt_gatt_status_t
 *
 ******************************************************************************/
wiced_bt_gatt_status_t app_gatt_write_handler( uint16_t conn_id,
                           wiced_bt_gatt_write_req_t * p_wr_data );

/******************************************************************************
 * Function Name: app_gatt_read_req_handler
 ******************************************************************************
 * Summary:
 *  This function is called when GATT handle read req event is recieved.
 *
 * Parameters:
 *  uint16_t conn_id                -- Connection ID
 *  wiced_bt_gatt_read_t *          -- Pointer to gatt_read data
 *  wiced_bt_gatt_opcode_t opcode   -- opcode
 *  uint16_t len_requested          -- The requested length
 *
 * Return:
 *  wiced_bt_gatt_status_t
 *
 ******************************************************************************/
wiced_bt_gatt_status_t app_gatt_read_req_handler(uint16_t conn_id,
                           wiced_bt_gatt_read_t *p_req,
                           wiced_bt_gatt_opcode_t opcode,
                           uint16_t len_requested );

/******************************************************************************
 * Function Name: app_link_up
 ******************************************************************************
 * Summary:
 *  This function is called when link is up
 *
 * Parameters:
 *  wiced_bt_gatt_connection_status_t * p_status -- pointer to the connection
 *  status.
 *
 * Return:
 *  none
 *
 *****************************************************************************/
void app_link_up(wiced_bt_gatt_connection_status_t * p_status);

/******************************************************************************
 * Function Name: app_link_down
 ******************************************************************************
 * Summary:
 *  This function is called when link is down
 *
 * Parameters:
 *  wiced_bt_gatt_connection_status_t * p_status -- pointer to the connection
 *  status.
 *
 * Return:
 *  none
 *
 *****************************************************************************/
void app_link_down(const wiced_bt_gatt_connection_status_t * p_status);

/******************************************************************************
 * Function Name: app_adv_state_changed
 ******************************************************************************
 * Summary:
 *  This function is called when advertisment state is changed
 *
 * Parameters:
 *  wiced_bt_ble_advert_mode_t adv  -- new advertisment mode.
 *
 * Return:
 *  none
 *
 *****************************************************************************/
void app_adv_state_changed(wiced_bt_ble_advert_mode_t old_adv,
                           wiced_bt_ble_advert_mode_t adv);

/******************************************************************************
 * Function Name: app_shutdown
 ******************************************************************************
 * Summary:
 *  This function is called when battery level reaches shutdown voltage.
 *  The device should put power consumption to the lowest to prevent battery
 *  leakage before shutdown.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  none
 *
 *****************************************************************************/
void app_shutdown(void);

/******************************************************************************
 * Function Name: app_init
 ******************************************************************************
 * Summary:
 *  When BT Management Stack is initialized successfully, this function is
 *  called.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  none
 *
 *****************************************************************************/
wiced_result_t app_init(void);

/******************************************************************************
 * Function Name: application_start()
 ******************************************************************************
 *  Entry point to the application. Set device configuration and start Bluetooth
 *  stack initialization.  The actual application initialization (app_init) will
 *  be called when stack reports that Bluetooth device is ready.
 *
 * Parameters:
 *  none
 *
 * Return:
 *  none
 *
 ******************************************************************************/
void application_start( void );

#endif // APP_H_
