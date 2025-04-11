/*
 * $ Copyright YEAR Cypress Semiconductor $
 */

/**
 * file bt.c
 *
 * BT management functions
 *
 */

#include "wiced_bt_stack.h"
#include "wiced_memory.h"
#include "wiced_bt_trace.h"
#include "cy_retarget_io.h"
#ifdef ENABLE_BT_SPY_LOG
#include "cybt_debug_uart.h"
#include "cybt_platform_trace.h"
#endif
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "nvram_lib.h"
#include "app.h"
#include "app_bt_utils.h"
#include  "app_terminal_trace.h"

/******************************************************************************
 * Defines
 ******************************************************************************/
#define APP_STACK_HEAP_SIZE 0x1000                      // stack size
#define VS_ID_LOCAL_IDENTITY "local_identity"

/******************************************************************************
 * Structures
 ******************************************************************************/
static struct {
    wiced_bt_dev_local_addr_ext_t   dev;
    wiced_bt_ble_advert_mode_t      adv_mode, intended_adv_mode;
    uint8_t                         adv_bdAddr[BD_ADDR_LEN];
} bt = {0};

const wiced_bt_cfg_settings_t * p_wiced_bt_cfg_settings=&wiced_bt_cfg_settings;

static wiced_bt_cfg_isoc_t cfg_isoc = {
    .max_sdu_size = ISO_SDU_SIZE,
    .channel_count = 1,
    .max_cis_conn = 1,
    .max_cig_count = 1,
    .max_buffers_per_cis = 4,
    .max_big_count = 0
};

/* Custom Bluetooth stack configuration */
static wiced_bt_cfg_settings_t wiced_bt_cfg_settings_custom;

static void bt_get_cfg()
{
    //if bt-configurator does not assign isoc_cfg, we have to assign it manually
    if (!p_wiced_bt_cfg_settings->p_isoc_cfg)
    {
        memcpy(&wiced_bt_cfg_settings_custom, p_wiced_bt_cfg_settings,
               sizeof(wiced_bt_cfg_settings_t));
        wiced_bt_cfg_settings_custom.p_isoc_cfg = &cfg_isoc;
        p_wiced_bt_cfg_settings = &wiced_bt_cfg_settings_custom;
    }
}

/******************************************************************************
 * Variables
 ******************************************************************************/
#ifdef ISOC_PERIPHERAL_1
// forced to use addr 0x1234567890DD
wiced_bt_device_address_t dev_addr = {0x12, 0x34, 0x56, 0x78, 0x90, 0xDD};
#else // ISOC_PERIPHERAL_2
// forced to use addr 0x1234567890EE
wiced_bt_device_address_t dev_addr = {0x12, 0x34, 0x56, 0x78, 0x90, 0xEE};
#endif

/******************************************************************************
 *     Private Functions
 ******************************************************************************/
#ifdef ENABLE_BT_SPY_LOG
static cybt_result_t debug_uart_send_hci_trace(uint8_t type,
                                               uint16_t data_size,
                                               uint8_t* p_data)
{
    // Don't show ISOC traces.
    if ((type == 4) || (type == 5))
    {
        return CYBT_SUCCESS;
    }
    if(type == 0)
    {
       /* HCI_BLE_Extended_Advertising_Report or 
          0x13 = HCI_Number_Of_Completed_Packets */
       if(
          ((p_data[0] == 0x3e) && (p_data[2] == 0x0d))||
          (p_data[0] == 0x13)
          )
       {
            // skip
            return CYBT_SUCCESS;
       }
    }
    return cybt_debug_uart_send_hci_trace(type, data_size, p_data);
}
#endif
/********************************************************************
 * Function Name: app_bt_management
 ********************************************************************
 * Summary:
 *  This is the callback function from BT Management.
 ********************************************************************/
static wiced_result_t app_bt_management(wiced_bt_management_evt_t event,
                          wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_result_t result = WICED_BT_SUCCESS;
    uint8_t *p_keys;
    wiced_bt_dev_encryption_status_t  *p_encryption_status;

    WICED_BT_TRACE( "BT event: %d, %s", event, get_btm_event_name(event) );

    switch( event )
    {
        /* Bluetooth  stack enabled */
        case BTM_ENABLED_EVT:
            if ( p_event_data->enabled.status == WICED_BT_SUCCESS )
            {
#ifdef ENABLE_BT_SPY_LOG
                /* Register HCI Trace callback */
                wiced_bt_dev_register_hci_trace((wiced_bt_hci_trace_cback_t*) 
                                                debug_uart_send_hci_trace);
                cybt_platform_set_trace_level(CYBT_TRACE_ID_STACK, CYBT_TRACE_LEVEL_DEBUG);
#endif
                WICED_BT_TRACE("BTM initialized");

                wiced_bt_set_local_bdaddr (dev_addr, BLE_ADDR_PUBLIC);

                /* read extended device info */
                wiced_bt_dev_read_local_addr_ext(&bt.dev);
#ifdef ENABLE_BT_SPY_LOG
                WICED_BT_TRACE("Local Addr: %B", dev_info()->local_addr);
#endif
                app_init();
#ifdef AUTO_PAIRING
                bt_enter_pairing();
#endif
            }
            else
            {
                WICED_BT_TRACE("** BT Enable failed, status:%d",
                               p_event_data->enabled.status);
                CY_ASSERT(0);
            }
            break;

        case BTM_USER_CONFIRMATION_REQUEST_EVT:
            WICED_BT_TRACE("BTM_USER_CONFIRMATION_REQUEST_EVT: Numeric_value:"
                "%d \n", (int)p_event_data->user_confirmation_request.numeric_value);
            wiced_bt_dev_confirm_req_reply( WICED_BT_SUCCESS,
                p_event_data->user_confirmation_request.bd_addr);
            break;

        case BTM_PASSKEY_NOTIFICATION_EVT:
#ifdef ENABLE_BT_SPY_LOG
            WICED_BT_TRACE("PassKey Notification. BDA %B, Key %d \n",
                           p_event_data->user_passkey_notification.bd_addr,
                           p_event_data->user_passkey_notification.passkey );
#endif
            wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS,
                p_event_data->user_passkey_notification.bd_addr);
            break;

        case BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT:
            WICED_BT_TRACE("BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT");
            /* save keys to NVRAM */
            p_keys = (uint8_t *)&p_event_data->local_identity_keys_update;
            nvram_write (VS_ID_LOCAL_IDENTITY, p_keys,
                         sizeof( wiced_bt_local_identity_keys_t ));
            break;

        case  BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT:
            WICED_BT_TRACE("BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT");
            /* read keys from NVRAM */
            p_keys = (uint8_t *)&p_event_data->local_identity_keys_request;
            if (!nvram_read( VS_ID_LOCAL_IDENTITY, p_keys,
                sizeof(wiced_bt_local_identity_keys_t)))
            {
                WICED_BT_TRACE("Local Identity Key not available");
            }
            break;

        case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
            {
                wiced_bt_device_link_keys_t *p_link_keys = 
                    &p_event_data->paired_device_link_keys_update;
#ifdef ENABLE_BT_SPY_LOG
                WICED_BT_TRACE("BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT BdAddr:"
                               "%B",p_link_keys->bd_addr);
#endif
                host_set_link_key(p_link_keys->bd_addr, p_link_keys,
                                  link_transport());
                link_set_bonded(TRUE); // Now we are bonded
            }
            break;

        case  BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
            WICED_BT_TRACE("BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT");
            if (host_get_link_key(
                p_event_data->paired_device_link_keys_request.bd_addr,
                &p_event_data->paired_device_link_keys_request))
            {
                // We have the link key, it is a bonded device
                link_set_bonded(TRUE);
            }
            else
            {
#ifdef ENABLE_BT_SPY_LOG
                WICED_BT_TRACE("requsted %B link_key not available",
                    p_event_data->paired_device_link_keys_request.bd_addr);
#endif
                result = WICED_BT_ERROR;
            }
            break;

        case BTM_ENCRYPTION_STATUS_EVT:
            p_encryption_status = &p_event_data->encryption_status;
#ifdef ENABLE_BT_SPY_LOG
            WICED_BT_TRACE("Encryption address:%B result:%d (%sencrypted)",
                p_encryption_status->bd_addr,
                p_event_data->encryption_status.result,
                p_event_data->encryption_status.result == WICED_SUCCESS? "": 
                "not ");
#else
            (void)p_encryption_status;
#endif
            link_set_encrypted(p_event_data->encryption_status.result 
                               == WICED_SUCCESS);
            break;

        case BTM_PAIRING_COMPLETE_EVT:
            WICED_BT_TRACE("BTM_PAIRING_COMPLETE_EVT: %d ",
               p_event_data->pairing_complete.pairing_complete_info.ble.reason);
            break;

        case BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT:
            WICED_BT_TRACE("BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT");
            p_event_data->pairing_io_capabilities_ble_request.local_io_cap = 
                BTM_IO_CAPABILITIES_NONE;
            p_event_data->pairing_io_capabilities_ble_request.oob_data = 
                BTM_OOB_NONE;
            p_event_data->pairing_io_capabilities_ble_request.auth_req =
                BTM_LE_AUTH_REQ_SC | BTM_LE_AUTH_REQ_BOND;  /* LE sec bonding */
            p_event_data->pairing_io_capabilities_ble_request.max_key_size = 16;
            p_event_data->pairing_io_capabilities_ble_request.init_keys =
                BTM_LE_KEY_PENC|BTM_LE_KEY_PID|BTM_LE_KEY_PCSRK|BTM_LE_KEY_PLK;
            p_event_data->pairing_io_capabilities_ble_request.resp_keys =
                BTM_LE_KEY_PENC|BTM_LE_KEY_PID|BTM_LE_KEY_PCSRK|BTM_LE_KEY_PLK;
            break;

        case BTM_SECURITY_REQUEST_EVT:
            WICED_BT_TRACE("BTM_SECURITY_REQUEST_EVT");
             /* Use the default security */
            wiced_bt_ble_security_grant(p_event_data->security_request.bd_addr,
                                        WICED_BT_SUCCESS);
            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            WICED_BT_TRACE("BTM_BLE_ADVERT_STATE_CHANGED_EVT");
            {
                wiced_bt_ble_advert_mode_t      new_adv_mode = 
                    p_event_data->ble_advert_state_changed;

                if (new_adv_mode == BTM_BLE_ADVERT_OFF && !link_is_connected())
                {
                    /*if the adv is off and previous state was 
                     BTM_BLE_ADVERT_DIRECTED_HIGH, we switch to 
                     BTM_BLE_ADVERT_DIRECTED_LOW */
                    if (bt.intended_adv_mode == BTM_BLE_ADVERT_DIRECTED_HIGH)
                    {
                        // start high duty cycle directed advertising.
                        if (bt_start_advertisements(BTM_BLE_ADVERT_DIRECTED_LOW,
                                                    host_addr_type(),
                                                    bt.adv_bdAddr))
                        {
                            WICED_BT_TRACE("Failed to start low duty cycle"
                                           " directed advertising!!!");
                        }
                        break;
                    }
                    /* if the adv is off and previous state was
                       BTM_BLE_ADVERT_UNDIRECTED_HIGH, we switch to
                       BTM_BLE_ADVERT_UNDIRECTED_LOW*/
                    else if (bt.intended_adv_mode 
                             == BTM_BLE_ADVERT_UNDIRECTED_HIGH)
                    {
                        // start high duty cycle directed advertising.
                        if (bt_start_advertisements(
                            BTM_BLE_ADVERT_UNDIRECTED_LOW, 0, NULL))
                        {
                            WICED_BT_TRACE("Failed to start low duty cycle"
                                           " undirected advertising!!!");
                        }
                        break;
                    }
                }
                app_adv_state_changed(bt.adv_mode, new_adv_mode);
                bt.adv_mode = new_adv_mode;
            }
            break;

        case BTM_BLE_SCAN_STATE_CHANGED_EVT:
            WICED_BT_TRACE("Scan State Change: %d",
                           p_event_data->ble_scan_state_changed );
            break;

        case BTM_BLE_CONNECTION_PARAM_UPDATE:
            WICED_BT_TRACE("BTM_BLE_CONNECTION_PARAM_UPDATE status:%d interval:"
                "%d latency:%d timeout:%d",
                p_event_data->ble_connection_param_update.status,
                p_event_data->ble_connection_param_update.conn_interval,
                p_event_data->ble_connection_param_update.conn_latency,
                p_event_data->ble_connection_param_update.supervision_timeout);
            if (!p_event_data->ble_connection_param_update.status)
            {
                link_set_parameter_updated(TRUE);
            }
            break;

        case BTM_BLE_PHY_UPDATE_EVT:
#ifdef ENABLE_BT_SPY_LOG
            WICED_BT_TRACE("PHY update: status:%d Tx:%d Rx:%d BDA %B",
                           p_event_data->ble_phy_update_event.status,
                p_event_data->ble_phy_update_event.tx_phy,
                p_event_data->ble_phy_update_event.rx_phy,
                p_event_data->ble_phy_update_event.bd_address);
#endif
            break;

        default:
            WICED_BT_TRACE("Unhandled management event: %d!!!", event );
            break;
    }

    return result;
}

/******************************************************************************
 *     Public Functions
 ******************************************************************************/

/********************************************************************
 * Function Name: dev_info
 ********************************************************************
 * Summary:
 *  This function returns the device infomation data structure.
 ********************************************************************/
wiced_bt_dev_local_addr_ext_t * dev_info()
{
    return &bt.dev;
}

/********************************************************************
 * Function Name: bt_enter_reconnect
 ********************************************************************
 * Summary:
 *  It the device is paired, it starts directed adv or undirect with filter
 *  for reconnecting to paired host.
 ********************************************************************/
void bt_enter_reconnect(void)
{
    WICED_BT_TRACE("Enter reconnect");

    if (host_is_paired())
    {
        /* NOTE!!! wiced_bt_start_advertisement could modify the value of 
           bdAddr, so MUST use a copy.*/
        memcpy(bt.adv_bdAddr, host_addr(), BD_ADDR_LEN);

        // start high duty cycle directed advertising.
        if (bt_start_advertisements(
            BTM_BLE_ADVERT_DIRECTED_HIGH, host_addr_type(), bt.adv_bdAddr))
        {
            WICED_BT_TRACE("Failed to start high duty cycle directed"
                           " advertising!!!");
        }
    }
}

/********************************************************************
 * Function Name: bt_enter_pairing
 ********************************************************************
 * Summary:
 *  Starts undirect advs
 ********************************************************************/
void bt_enter_pairing(void)
{
    bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
}

/********************************************************************
 * Function Name: bt_init
 ********************************************************************
 * Summary:
 *  This is one of the first function to be called upon device reset.
 *  It initialize BT Stack. When BT stack is up, it will call application
 *  to continue system initialization.
 ********************************************************************/
wiced_result_t bt_init()
{
    bt_get_cfg();

    wiced_result_t result = wiced_bt_stack_init(app_bt_management,
                                                p_wiced_bt_cfg_settings);

    if (result == WICED_BT_SUCCESS)
    {
        /* Create default heap */
        if (wiced_bt_create_heap("app", NULL, APP_STACK_HEAP_SIZE,
                                 NULL, WICED_TRUE) == NULL)
        {
            WICED_BT_TRACE("create default heap error: size %d",
                           APP_STACK_HEAP_SIZE);
            result = WICED_BT_NO_RESOURCES;
        }
    }
    return result;
}

/********************************************************************
 * Function Name: bt_enter_connect()
 ********************************************************************
 * Summary:
 *  When the device is paired, it reconnect to the paired host.
 *  Otherwise, it enter pairing for a new host.
 *******************************************************************/
void bt_enter_connect()
{
    host_is_paired() ? bt_enter_reconnect() : bt_enter_pairing();
}

/********************************************************************
 * Function Name: bt_start_advertisements()
 ********************************************************************
 * Summary:
 *  Saves the current adv mode and calls wiced_bt_start_advertisements()
 *******************************************************************/
wiced_result_t bt_start_advertisements(wiced_bt_ble_advert_mode_t advert_mode,
               wiced_bt_ble_address_type_t directed_advertisement_bdaddr_type,
               wiced_bt_device_address_ptr_t directed_advertisement_bdaddr_ptr)
{
    bt.intended_adv_mode = advert_mode;
    return wiced_bt_start_advertisements(advert_mode,
                                         directed_advertisement_bdaddr_type,
                                         directed_advertisement_bdaddr_ptr);
}

/* end of file */
