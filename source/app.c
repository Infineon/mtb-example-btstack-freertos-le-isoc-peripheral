/*
 * $ Copyright YEAR Cypress Semiconductor $
 */

/**
 * file app.c
 *
 * This is the LE Isochronous demo application for Peripheral role. This 
 * application should be used together with le-isoc-central code example 
 * running on a peer device.
 *
 * Related Document: See README.md
 *
 */

/*******************************************************************************
*        Header Files
*******************************************************************************/
#include "cybt_platform_trace.h"
#include "wiced_bt_trace.h"
#include "wiced_bt_types.h"
#include "app.h"


/******************************************************************************
 *     Public Functions
 ******************************************************************************/

/******************************************************************************
 * Function Name: app_remove_host_bonding
 ******************************************************************************
 * Summary:
 *  Virtual cable unplug.
 *  This function will remove all HID host information from NVRAM.
 ******************************************************************************/
void app_remove_host_bonding(void)
{
    if (link_is_connected())
    {
        WICED_BT_TRACE( "app_remove_host_bonding disconnect" );
        bt_disconnect();
    }

    while (host_is_paired())
    {
        uint8_t *bonded_bdadr = host_addr();

        WICED_BT_TRACE( "remove bonded device : %B", bonded_bdadr );
        wiced_bt_dev_delete_bonded_device( bonded_bdadr );

        host_remove();
    }
}

/******************************************************************************
 * Function Name: app_gatt_write_handler
 ******************************************************************************
 * Summary:
 *  This function is called when GATT handle write req event is recieved.
 *****************************************************************************/
wiced_bt_gatt_status_t app_gatt_write_handler(uint16_t conn_id,
                           wiced_bt_gatt_write_req_t * p_wr_data )
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_ATTRIBUTE_NOT_FOUND;

    if (result == WICED_BT_GATT_ATTRIBUTE_NOT_FOUND)
    {
        // let the default handler to take care of it
        result = gatt_write_default_handler( conn_id, p_wr_data);
    }

    return result;
}

/******************************************************************************
 * Function Name: app_gatt_read_req_handler
 ******************************************************************************
 * Summary:
 *  This function is called when GATT handle read req event is recieved.
 *****************************************************************************/
wiced_bt_gatt_status_t app_gatt_read_req_handler( uint16_t conn_id,
                                                  wiced_bt_gatt_read_t *p_req,
                                                  wiced_bt_gatt_opcode_t opcode,
                                                  uint16_t len_requested )
{
    /* let the default handler to take case of it */
    return gatt_read_req_default_handler(conn_id, p_req, opcode,
                                         len_requested );
}

/******************************************************************************
 * Function Name: app_link_up
 ******************************************************************************
 * Summary:
 *  This function is called when link is up
 *****************************************************************************/
void app_link_up(wiced_bt_gatt_connection_status_t * p_status)
{
    WICED_BT_TRACE("%s Link is up, conn_id:%04x peer_addr:%B type:%d",
                   link_transport() == BT_TRANSPORT_LE ? "LE" : "BREDR",
                   p_status->conn_id, p_status->bd_addr, p_status->addr_type);

    led_on(LED_LINK);
}

/******************************************************************************
 * Function Name: app_link_down
 ******************************************************************************
 * Summary:
 *  This function is called when link is down
 *****************************************************************************/
void app_link_down(const wiced_bt_gatt_connection_status_t * p_status)
{
    /* if we have multiple links, the link_conn_id() can return the secondary
       conn_id. */
    uint16_t conn_id = link_conn_id();

    WICED_BT_TRACE("Link down, id:0x%04x reason: %d",  p_status->conn_id,
                   p_status->reason );

    // if no more link, we turn off LED
    if (!conn_id)
    {
        led_off(LED_LINK);
    }
    else
    {
        p_status = link_connection_status();
        if (p_status)
        {
            WICED_BT_TRACE("conn_id:%04x peer_addr:%B type:%d now is active",
                           p_status->conn_id, p_status->bd_addr,
                           p_status->addr_type);
        }
    }
}

/******************************************************************************
 * Function Name: app_adv_state_changed
 ******************************************************************************
 * Summary:
 *  This function is called when advertisment state is changed
 *****************************************************************************/
void app_adv_state_changed(wiced_bt_ble_advert_mode_t old_adv,
                           wiced_bt_ble_advert_mode_t adv)
{
    if (adv == BTM_BLE_ADVERT_OFF)
    {
        WICED_BT_TRACE("Advertisement Stopped");
        led_blink_stop(LED_LINK);
    }
    else
    {
        if (old_adv == BTM_BLE_ADVERT_OFF)
        {
            WICED_BT_TRACE("Advertisement %d started", adv);
            // use faster blink LINK line to indicate reconnecting
            led_blink(LED_LINK, 0, host_is_paired() ? 200: 500);
        }
        else
        {
            WICED_BT_TRACE("Advertisement State Change: %d -> %d",old_adv,adv);
        }
    }
}

/******************************************************************************
 * Function Name: app_shutdown
 ******************************************************************************
 * Summary:
 *  This function is called when battery level reaches shutdown voltage.
 *  The device should put power consumption to the lowest to prevent battery
 *  leakage before shutdown.
 *****************************************************************************/
void app_shutdown(void)
{
    WICED_BT_TRACE("app_shutdown");

    if(link_is_connected())
    {
        WICED_BT_TRACE( "app_shutdown disconnect" );
        bt_disconnect();
    }

    __disable_irq(); // Disable Interrupts
}

/******************************************************************************
 * Function Name: app_init
 ******************************************************************************
 * Summary:
 *  When BT Management Stack is initialized successfully, 
 *  this function is called.
 *****************************************************************************/
wiced_result_t app_init(void)
{
    WICED_BT_TRACE("app_init");

    /* Initialize each submodule */
    host_init();
    button_init();
    gatt_initialize();
    isoc_init();
    led_init();

    /* Allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_TRUE, FALSE);

    return WICED_BT_SUCCESS;
}

/*******************************************************************************
 * Function Name: application_start()
 *******************************************************************************
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
void application_start( void )
{
    /* WICED_BT_TRACE starts to work after bt_init()*/
    bt_init(); 
    /* use default default NVRAM read/write*/
    nvram_init(NULL);

    WICED_BT_TRACE("\n\n******* LE ISOC Peripheral Application Start ********");
    WICED_BT_TRACE("Name:\"%s\"", app_gap_device_name);
    WICED_BT_TRACE("DEV=%d btstack:%d.%d.%d", CHIP, WICED_BTSTACK_VERSION_MAJOR,
                   WICED_BTSTACK_VERSION_MINOR, WICED_BTSTACK_VERSION_PATCH);

#if defined(LED_SUPPORT) || defined(BUTTON_SUPPORT)
 #ifdef LED_SUPPORT
  #define LED_STRING " LED"
 #else
  #define LED_STRING ""
 #endif
 #ifdef BUTTON_SUPPORT
  #define BUTTON_STRING " BUTTON"
 #else
  #define BUTTON_STRING ""
 #endif
    WICED_BT_TRACE("Supported Component:" LED_STRING BUTTON_STRING);
#endif

#if GATT_TRACE || LED_TRACE || HOST_TRACE || NVRAM_TRACE
 #if GATT_TRACE
  #define GATT_TSTRING " GATT"
 #else
  #define GATT_TSTRING ""
 #endif
 #if LED_TRACE
  #define LED_TSTRING " LED"
 #else
  #define LED_TSTRING ""
 #endif
 #if HOST_TRACE
  #define HOST_TSTRING " HOST"
 #else
  #define HOST_TSTRING ""
 #endif
 #if NVRAM_TRACE
  #define NVRAM_TSTRING " NVRAM"
 #else
  #define NVRAM_TSTRING ""
 #endif
    WICED_BT_TRACE("Enabled Trace:" GATT_TSTRING LED_TSTRING 
                   HOST_TSTRING NVRAM_TSTRING);
#endif
}

/* end of file */
