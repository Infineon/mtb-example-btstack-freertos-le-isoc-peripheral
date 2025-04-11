/*
 * $ Copyright YEAR Cypress Semiconductor $
 */
/*
 * isoc_peripheral.c
 */

#include "wiced_bt_trace.h"
#include "wiced_bt_types.h"
#include "wiced_timer.h"
#include "wiced_memory.h"
#include "iso_data_handler.h"
#include "cyhal.h"
#include "app.h"
#include  "app_terminal_trace.h"
/******************************************************************************
 *  defines
 ******************************************************************************/
#if ISOC_TRACE
# define APP_ISOC_TRACE                        WICED_BT_TRACE
# define APP_ISOC_TRACE_ARRAY(ptr, len)        WICED_BT_TRACE("%A", ptr, len)
# define APP_ISOC_TRACE_S_ARRAY(str, ptr, len) \
         WICED_BT_TRACE("%s %A", str, ptr, len)
#else
# define APP_ISOC_TRACE(...)
# define APP_ISOC_TRACE_ARRAY(ptr, len)
# define APP_ISOC_TRACE_S_ARRAY(str, ptr, len)
#endif

#define ISOC_MAX_BURST_COUNT                4

// sdu interval in micro-second
#define ISO_SDU_INTERVAL                    10000

#define ISOC_TIMEOUT_IN_MSECONDS            ISO_SDU_INTERVAL / 1000
//4 minute keep alive timer to ensure app and controller psn
#define ISOC_KEEP_ALIVE_TIMEOUT_IN_SECONDS  120
                                                   // stays synchronized

#define ISOC_STATS    // ISOC statistics periodically printed with this flag
#ifdef ISOC_STATS
#define ISOC_STATS_TIMEOUT                  5
#endif

// ISOC statistics periodically printed with this flag
#define ISOC_MONITOR_FOR_DROPPED_SDUs
#ifdef ISOC_MONITOR_FOR_DROPPED_SDUs
#define ISOC_ERROR_DROPPED_SDU_VSE_OPCODE   0x008b

#pragma pack(1)
typedef struct
{
    uint16_t  connHandle;
    uint16_t  psn;
    uint32_t  timestamp;
    uint16_t  expected_psn;
    uint32_t  expected_timestamp;
} isoc_error_dropped_sdu_t;
#pragma pack()
#endif

/******************************************************************************
 *  local variables
 ******************************************************************************/
#pragma pack(1)
typedef struct
{
    uint16_t    cis_conn_handle;
    uint16_t    sequence_num;
    uint8_t     button_state;
} iso_rx_data_central_button_state_type_t;
#pragma pack()

static struct
{
    uint16_t acl_conn_handle;
    uint16_t cis_conn_handle;
    uint16_t max_payload;
    wiced_ble_isoc_cis_established_evt_t  cis_established_data;
    wiced_timer_t isoc_send_data_timer;
    wiced_timer_t isoc_keep_alive_timer;
} isoc = {0};

typedef enum
{
    SN_IDLE,
    SN_PENDING,
    SN_VALID,
} sequence_number_state_e;

static uint16_t iso_sdu_count = 0;
static wiced_bool_t pressed_saved;
static uint16_t sequence = 0;
static sequence_number_state_e sequence_number_state;

#define CONTROLLER_ISO_DATA_PACKET_BUFS   8
static uint8_t number_of_iso_data_packet_bufs = CONTROLLER_ISO_DATA_PACKET_BUFS;


static uint32_t isoc_rx_count = 0;
static uint32_t isoc_tx_count = 0;
wiced_timer_t iso_stats_timer;
wiced_ble_isoc_data_path_direction_t dp_dir;


/*******************************************************************************
 * private functions
 ******************************************************************************/
static void isoc_send_null_payload(void);
static void isoc_get_psn_start( WICED_TIMER_PARAM_TYPE param );

void app_send_dummy(uint16_t handle)
{
    uint8_t* p_buf = iso_dhm_get_data_buffer();
    iso_dhm_send_packet(sequence, handle, WICED_FALSE, p_buf, 0);
}
#define  VSC_0XFDFA
#ifdef VSC_0XFDFA
#pragma pack( push, 1 )
typedef struct {
    uint8_t status;
    uint16_t connHandle;
    uint16_t packetSeqNum;
    uint32_t timeStamp;
    uint8_t  timeOffset[3];
}tREAD_PSN_EVT;
#pragma pack( pop )

static void read_psn_cb(wiced_bt_dev_vendor_specific_command_complete_params_t 
                *p_command_complete_params)
{
    tREAD_PSN_EVT * evt=(tREAD_PSN_EVT *)p_command_complete_params->p_param_buf;
    int toffset = evt->timeOffset[0];

    toffset |= (evt->timeOffset[1] & 0x0ff)<<8;
    toffset |= (evt->timeOffset[2] & 0x0ff)<< 16;
    APP_ISOC_TRACE("[%s] status:%d handle:0x%x psn=%d timestamp:%d"
                   " time_offset:%d\n", __FUNCTION__,
                   evt->status & 0x0FF, evt->connHandle& 0x0FFFF,
                   evt->packetSeqNum& 0x0FFFF,
                   (int)evt->timeStamp, toffset);

    if(evt->status != 0)
    {
        APP_ISOC_TRACE("[%s] status %d", __FUNCTION__, evt->status);
        sequence_number_state = SN_IDLE;
        return;
    }

    if( sequence_number_state != SN_PENDING )
        return;

    // If initial transmission, no need to increment
    if( evt->packetSeqNum == 0 )
        sequence = evt->packetSeqNum;
    else
        sequence = evt->packetSeqNum + 2;

    sequence_number_state = SN_VALID;

    // Send NULL payload if the idle timer is running
    if( wiced_is_timer_in_use(&isoc.isoc_keep_alive_timer) )
    {
        isoc_send_null_payload();
    }
}

#define READ_PSN_VSC_OPCODE   (0xFDFA)
void start_read_psn_using_vsc(uint16_t hdl)
{
    wiced_bt_dev_vendor_specific_command (READ_PSN_VSC_OPCODE, 2,
                                          (uint8_t *)&hdl,read_psn_cb);
}

#else
/*******************************************************************************
 * Function Name: isoc_read_tx_sync_complete_cback
 *******************************************************************************
 * Summary:
 *
 ******************************************************************************/
static void isoc_read_tx_sync_complete_cback(
                wiced_bt_isoc_read_tx_sync_complete_t *p_event_data)
{
    APP_ISOC_TRACE("[%s] status:%d handle:0x%x psn=%d  timestamp:%d"
                   " time_offset:%d", __FUNCTION__, p_event_data->status,
                   p_event_data->conn_hdl, p_event_data->psn,
                   p_event_data->tx_timestamp, p_event_data->time_offset);

    if(p_event_data->status != 0)
    {
        APP_ISOC_TRACE("[%s] status %d", __FUNCTION__, p_event_data->status);
        sequence_number_state = SN_IDLE;
        return;
    }
    if( isoc.cis_conn_handle != p_event_data->conn_hdl )
    {
        APP_ISOC_TRACE("[%s] Invalid cis_handle %d", __FUNCTION__,
                       p_event_data->conn_hdl);
        return;
    }

    if( sequence_number_state != SN_PENDING )
    {
        APP_ISOC_TRACE("[%s] Bad state %d", __FUNCTION__, 
                       sequence_number_state);
        return;
    }

    // If initial transmission, no need to increment
    if( p_event_data->psn == 0 )
        sequence = p_event_data->psn;
    else
        sequence = p_event_data->psn + 2;

    sequence_number_state = SN_VALID;

    // Send NULL payload if the idle timer is running
    if( wiced_is_timer_in_use(&isoc.isoc_keep_alive_timer) )
    {
        isoc_send_null_payload();
    }
}
#endif
/*******************************************************************************
 * Function Name: isoc_send_null_payload
 *******************************************************************************
 * Summary:
 *
 ******************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void isoc_send_null_payload(void)
{
    wiced_bool_t result;
    uint8_t* p_buf = NULL;

    if(sequence_number_state != SN_VALID)
        return;

    // Allocate buffer for ISOC header
    if((p_buf = iso_dhm_get_data_buffer()) != NULL)
    {
        result = iso_dhm_send_packet(sequence,
                                     isoc.cis_established_data.cis.cis_conn_handle,
                                     WICED_FALSE, p_buf, 0);

        APP_ISOC_TRACE("[%s] sent null payload handle %02x result %d",
                       __FUNCTION__, isoc.cis_established_data.cis.cis_conn_handle,
                       result);

        // Set PSN state back to idle
        sequence_number_state = SN_IDLE;
    }
}
CY_SECTION_RAMFUNC_END

/*******************************************************************************
 * Function Name: isoc_send_data_handler
 *******************************************************************************
 * Summary:
 *  Updates the send buffer and submits data to the controller
 ******************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void isoc_send_data_handler( WICED_TIMER_PARAM_TYPE param )
{
    wiced_bool_t result;
    uint32_t data_length;
    uint8_t* p_buf = NULL;
    uint8_t* p = NULL;
    wiced_bool_t pressed = pressed_saved;

    if(sequence_number_state != SN_VALID)
        return;

    // Submit data to the controller only if it has bufs available
    if(number_of_iso_data_packet_bufs)
    {
        if((p_buf = iso_dhm_get_data_buffer()) != NULL)
        {
               p = p_buf;

            UINT16_TO_STREAM(p, isoc.cis_established_data.cis.cis_conn_handle);
            UINT16_TO_STREAM(p, sequence);
            UINT8_TO_STREAM(p, pressed);

#if 0  // Normally you would only send the required payload but here we want to 
       // exercise the max_sdu_size to stress the system more
            data_length = p_data - p_buf;
#else
            data_length = isoc.max_payload;
#endif

            /* Set P_TX gpio link high to indicate calling lower layer to 
               send data */
            set_gpio_high(P_TX);

            // pass data to data handler module
            result = iso_dhm_send_packet(sequence,
                     isoc.cis_established_data.cis.cis_conn_handle, WICED_FALSE,
                     p_buf, data_length);

            if(result)
            {
                number_of_iso_data_packet_bufs--;
                isoc_tx_count++;
            }
            APP_ISOC_TRACE("[%s] handle:0x%x SN:%d data_length:%d sdu_count:%d"
                           " result:%d", __FUNCTION__,
                           isoc.cis_established_data.cis.cis_conn_handle,
                           sequence, (int)data_length, iso_sdu_count, result);

            // Set P_TX gpio link low to indicate return from lower layer
            set_gpio_low(P_TX);
        }
     }

    sequence++;

    iso_sdu_count--;

    if(iso_sdu_count == 0)
    {
        wiced_stop_timer(&isoc.isoc_send_data_timer);

        sequence_number_state = SN_IDLE;

        // Start keep alive timer
        wiced_start_timer(&isoc.isoc_keep_alive_timer,
                          ISOC_KEEP_ALIVE_TIMEOUT_IN_SECONDS);

        APP_ISOC_TRACE("Started keep alive timer");
    }
}
CY_SECTION_RAMFUNC_END

/*******************************************************************************
 * Function Name: isoc_stop
 *******************************************************************************
 * Summary:
 *  Called upon disconnection or failure to establish CIS.
 ******************************************************************************/
static void isoc_stop(void)
{
    wiced_stop_timer(&isoc.isoc_send_data_timer);
    APP_ISOC_TRACE("[%s] enabled HCI trace", __FUNCTION__);
    wiced_bt_dev_update_debug_trace_mode(TRUE);
    wiced_bt_dev_update_hci_trace_mode(TRUE);

    led_off(LED_RED);
    led_blink_stop(LED_RED);

    isoc_rx_count = 0;
    isoc_tx_count = 0;

#ifdef ISOC_STATS
    wiced_stop_timer(&iso_stats_timer);
#endif
}

#ifdef ISOC_STATS
/******************************************************************************
 * Function Name: isoc_stats_timeout
 ******************************************************************************
 * Summary:
 *  Prints isoc stats upon timeout
 ******************************************************************************/
static void isoc_stats_timeout( WICED_TIMER_PARAM_TYPE param )
{
    APP_ISOC_TRACE("[ISOC STATS] isoc_rx_count:%d  isoc_tx_count:%d",
                   (int)isoc_rx_count, (int)isoc_tx_count);
}
#endif

/******************************************************************************
 * Function Name: isoc_management_cback
 ******************************************************************************
 * Summary:
 *  This is the callback function for ISOC Management.
 ******************************************************************************/
static void isoc_management_cback(wiced_ble_isoc_event_t event,
                                  wiced_ble_isoc_event_data_t *p_event_data)
{
    APP_ISOC_TRACE("[%s] %d", __FUNCTION__, event);
    wiced_result_t result = WICED_SUCCESS;
    wiced_ble_isoc_setup_data_path_info_t data_path_info =
    {   .isoc_conn_hdl = isoc.cis_established_data.cis.cis_conn_handle,
        .data_path_dir = WICED_BLE_ISOC_DPD_INPUT,
        .data_path_id = WICED_BLE_ISOC_DPID_HCI,
        .controller_delay = 0,
        .codec_id = {0,0,0,0,0},
        .csc_length = 0,
        .p_csc = NULL,
        .p_app_ctx = &dp_dir,
    };
    wiced_ble_isoc_cis_t isoc_cis =
    {
        .acl_conn_handle = p_event_data->cis_request.acl_conn_handle,
        .cig_id = p_event_data->cis_request.cig_id,
        .cis_conn_handle = p_event_data->cis_request.cis_conn_handle,
        .cis_id = p_event_data->cis_request.cis_id,
    };
    wiced_ble_isoc_data_path_direction_t *p_dir = (wiced_ble_isoc_data_path_direction_t *)p_event_data->datapath.p_app_ctx;
    switch (event)
    {
    case WICED_BLE_ISOC_SET_CIG_CMD_COMPLETE_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_SET_CIG_CMD_COMPLETE");
        break;

    case WICED_BLE_ISOC_CIS_REQUEST_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_CIS_REQUEST");

        isoc.acl_conn_handle = p_event_data->cis_request.acl_conn_handle;
        isoc.cis_conn_handle = p_event_data->cis_request.cis_conn_handle;
        result = wiced_ble_isoc_peripheral_accept_cis(&isoc_cis);
        APP_ISOC_TRACE("[%s] accept cis %d", __FUNCTION__, result);
        break;

    case WICED_BLE_ISOC_CIS_ESTABLISHED_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_CIS_ESTABLISHED");
        if(WICED_BT_SUCCESS == p_event_data->cis_established_data.status)
        {
            memcpy(&isoc.cis_established_data, 
                   &p_event_data->cis_established_data,
                   sizeof(wiced_ble_isoc_cis_established_evt_t));
            APP_ISOC_TRACE("[%s] CIS established %d %d %d ", __FUNCTION__,
                           isoc.cis_established_data.cis.cis_id,
                           isoc.cis_established_data.cis.cis_id,
                           isoc.cis_established_data.cis.cis_conn_handle);

            data_path_info.isoc_conn_hdl = isoc.cis_established_data.cis.cis_conn_handle;
            dp_dir = WICED_BLE_ISOC_DPD_INPUT;
            data_path_info.data_path_dir = WICED_BLE_ISOC_DPD_INPUT;


#if defined(CYW55572) || WICED_BTSTACK_VERSION_MINOR > 8
            result = (wiced_result_t) wiced_ble_isoc_setup_data_path(&data_path_info);
#else
            result = (wiced_result_t) wiced_ble_isoc_setup_data_path(&data_path_info);
#endif
            APP_ISOC_TRACE("[%s] setup_data_path %d", __FUNCTION__, result);
        }
        else
        {
            APP_ISOC_TRACE("[%s] CIS establishment failure status: %d",
                           __FUNCTION__,
                           p_event_data->cis_established_data.status);
            memset(&isoc.cis_established_data, 0,
                   sizeof(wiced_ble_isoc_cis_established_evt_t));
            isoc_stop();
        }
        break;

    case WICED_BLE_ISOC_CIS_DISCONNECTED_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_CIS_DISCONNECTED");
        isoc_stop();
        APP_ISOC_TRACE("[%s] CIS Disconnected cig: %d  cis: %d %d %d reason:%d",
                       __FUNCTION__,
                       p_event_data->cis_disconnect.cis.cig_id,
                       p_event_data->cis_disconnect.cis.cis_id,
                       p_event_data->cis_disconnect.cis.cis_conn_handle,
                       isoc.cis_established_data.cis.cis_conn_handle,
                       p_event_data->cis_disconnect.reason);
        memset(&isoc.cis_established_data, 0,
               sizeof(wiced_ble_isoc_cis_established_evt_t));
        if (wiced_ble_isoc_is_cis_connected_with_conn_hdl(
            p_event_data->cis_disconnect.cis.cis_conn_handle))
        {
            result = (wiced_result_t) wiced_ble_isoc_remove_data_path(
                p_event_data->cis_disconnect.cis.cis_conn_handle,
                WICED_BLE_ISOC_DPD_INPUT_BIT);
            APP_ISOC_TRACE("[%s] remove DP, result: %d", __FUNCTION__, result);
        }
        break;

    case WICED_BLE_ISOC_DATA_PATH_SETUP_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_DATA_PATH_SETUP");
        if(WICED_BT_SUCCESS != p_event_data->datapath.status)
        {
            APP_ISOC_TRACE("[%s] Datapath setup failure, status: %d",
                __FUNCTION__, p_event_data->datapath.status);
            return;
        }
        
        APP_ISOC_TRACE("[%s] data_path_dir = %d ",
                       __FUNCTION__, *p_dir);

        if(isoc.cis_established_data.cis.cis_conn_handle
           != p_event_data->datapath.conn_hdl)
        {
            APP_ISOC_TRACE("[%s] Connection Handle mismatch in Datapath"
                           " Status ",__FUNCTION__);
            return;
        }

        if ( *p_dir== WICED_BLE_ISOC_DPD_INPUT)
        {
            dp_dir = WICED_BLE_ISOC_DPD_OUTPUT;
            data_path_info.data_path_dir = WICED_BLE_ISOC_DPD_OUTPUT;
#if defined(CYW55572) || WICED_BTSTACK_VERSION_MINOR > 8
    result = (wiced_result_t) wiced_ble_isoc_setup_data_path(&data_path_info);
#else
    result = (wiced_result_t) wiced_ble_isoc_setup_data_path(&data_path_info);
#endif
            APP_ISOC_TRACE("[%s] setup_data_path result=%d", __FUNCTION__,
                           result);
        }
        else
        {
            isoc_start();
            app_send_dummy(p_event_data->datapath.conn_hdl);
        }
        break;

    case WICED_BLE_ISOC_DATA_PATH_REMOVED_EVT:
        APP_ISOC_TRACE("WICED_BLE_ISOC_DATA_PATH_REMOVED");
        if(WICED_BT_SUCCESS != p_event_data->datapath.status)
        {
            APP_ISOC_TRACE("[%s] Datapath remove failure ", __FUNCTION__);
            return;
        }

        APP_ISOC_TRACE("[%s] Datapath removed, Disconnect CIS ", __FUNCTION__);
        result = wiced_ble_isoc_disconnect_cis(p_event_data->datapath.conn_hdl);
        APP_ISOC_TRACE("[%s] disconnect cis on DP removed %d", __FUNCTION__,
                       result);
        break;

    default:
        APP_ISOC_TRACE("[%s] Unhandled event %d", __FUNCTION__, event);
        break;
    }

    CY_UNUSED_PARAMETER(result);
}

/******************************************************************************
 * Function Name: rx_handler
 ******************************************************************************
 * Summary:
 *  Handles received ISOC data
 *****************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void rx_handler(uint16_t cis_handle, uint8_t *p_data, uint32_t length)
{
    iso_rx_data_central_button_state_type_t* p_rx_data = (
        iso_rx_data_central_button_state_type_t*) p_data;

    //APP_ISOC_TRACE("[%s] length:%d", __FUNCTION__, length);

    if (length >= sizeof(iso_rx_data_central_button_state_type_t))
    {
        set_gpio_high(P_DBG1);

        APP_ISOC_TRACE("[rx_data] cis_conn_handle:0x%x SN:%d button_state:%d",
            p_rx_data->cis_conn_handle, p_rx_data->sequence_num,
            p_rx_data->button_state);
        isoc_rx_count++;

        set_gpio_low(P_DBG1);

        led_blink2(LED_RED, 1, 250, 250);
    }
}
CY_SECTION_RAMFUNC_END

/******************************************************************************
 * Function Name: isoc_get_psn_start
 ******************************************************************************
 * Summary:
 *  Returns the PSN start value for the current transmission packet.
 *****************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void isoc_get_psn_start(WICED_TIMER_PARAM_TYPE param)
{
    if(sequence_number_state == SN_IDLE &&
       isoc.cis_established_data.cis.cis_conn_handle)
    {
        APP_ISOC_TRACE("[%s] sending HCI_BLE_ISOC_READ_TX_SYNC for handle %02x",
                       __FUNCTION__, isoc.cis_established_data.cis.cis_conn_handle);
#ifndef VSC_0XFDFA
        wiced_bt_isoc_read_tx_sync(isoc.cis_established_data.cis_conn_handle,
                                   WICED_TRUE,isoc_read_tx_sync_complete_cback);
#else
            start_read_psn_using_vsc(isoc.cis_established_data.cis.cis_conn_handle);
#endif
        sequence_number_state = SN_PENDING;
    }
}
CY_SECTION_RAMFUNC_END


/******************************************************************************
 * Function Name: isoc_send_data_num_complete_packets_evt
 ******************************************************************************
 * Summary:
 *  Handle Number of Complete Packets event from controller
 *****************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void isoc_send_data_num_complete_packets_evt(uint16_t cis_handle,
                                                    uint16_t num_sent)
{
    /*APP_ISOC_TRACE("[%s] 0x%02x %d %d %d", __FUNCTION__, cis_handle, 
     num_sent, number_of_iso_data_packet_bufs,
     number_of_iso_data_packet_bufs+num_sent); */
    number_of_iso_data_packet_bufs += num_sent;
    wiced_start_timer(&isoc.isoc_keep_alive_timer,
                      ISOC_KEEP_ALIVE_TIMEOUT_IN_SECONDS);
}
CY_SECTION_RAMFUNC_END

#ifdef ISOC_MONITOR_FOR_DROPPED_SDUs
/******************************************************************************
 * Function Name: isoc_vse_cback
 ******************************************************************************
 * Summary:
 *  VSE callback used to monitor for dropped sdu error events from
 *  the controller
 *****************************************************************************/
static void isoc_vse_cback(uint8_t len, uint8_t *p)
{
    uint16_t opcode;
    isoc_error_dropped_sdu_t* p_isoc_error_dropped_sdu_vse;

    STREAM_TO_UINT16(opcode, p);

    if(opcode == ISOC_ERROR_DROPPED_SDU_VSE_OPCODE)
    {
        p_isoc_error_dropped_sdu_vse = (isoc_error_dropped_sdu_t*) p;

        APP_ISOC_TRACE("[ISOC_ERROR_DROPPED_SDU %02x] PSN: %d  Expected_PSN: %d"
                       "  timestamp: %d  expected_ts: %d",
                       p_isoc_error_dropped_sdu_vse->connHandle,
                       p_isoc_error_dropped_sdu_vse->psn,
                       p_isoc_error_dropped_sdu_vse->expected_psn,
                       (int)p_isoc_error_dropped_sdu_vse->timestamp,
                       (int)p_isoc_error_dropped_sdu_vse->expected_timestamp);

        // set sequence to next expected PSN
        sequence = p_isoc_error_dropped_sdu_vse->expected_psn + 1;
        if (wiced_is_timer_in_use(&isoc.isoc_keep_alive_timer))
        {
            // idle packet is failed. so send it again
            app_send_dummy(p_isoc_error_dropped_sdu_vse->connHandle);
        }else
        {
            isoc_tx_count--;
        }
    }
}
#endif // ISOC_MONITOR_FOR_DROPPED_SDUs

/*******************************************************************************
 * public functions
 ******************************************************************************/
/******************************************************************************
 * Function Name: isoc_cis_connected
 ******************************************************************************
 * Summary:
 *  Returns TRUE if CIS connected, else FALSE
 *****************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
wiced_bool_t isoc_cis_connected(void)
{
    return isoc.cis_established_data.cis.cis_conn_handle != 0;
}
CY_SECTION_RAMFUNC_END

/******************************************************************************
 * Function Name: isoc_send_data
 ******************************************************************************
 * Summary:
 *  Called once the ISOC data patch has been established.
 *****************************************************************************/
void isoc_start(void)
{
    //APP_ISOC_TRACE("[%s], disabled HCI trace", __FUNCTION__);
    //wiced_bt_dev_update_hci_trace_mode(FALSE);
    led_blink_stop(LED_RED);
    led_on(LED_RED);

    sequence = 0;

#ifdef ISOC_STATS
    wiced_start_timer(&iso_stats_timer, ISOC_STATS_TIMEOUT);
#endif
}

/******************************************************************************
 * Function Name: isoc_send_data
 ******************************************************************************
 * Summary:
 *  Called when configured for burst tx mode when button is pressed.
 *  Number of packets sent defined by ISOC_MAX_BURST_COUNT.
 *****************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
void isoc_send_data(wiced_bool_t c)
{
    // save button state
    pressed_saved = c;

    // start to burst out data
    iso_sdu_count += ISOC_MAX_BURST_COUNT;

    // stop keep alive timer if it is running
    if (wiced_is_timer_in_use(&isoc.isoc_keep_alive_timer))
    {
        wiced_stop_timer(&isoc.isoc_keep_alive_timer);
    }

    isoc_get_psn_start(0);

    // start to burst out data
    if (!wiced_is_timer_in_use(&isoc.isoc_send_data_timer))
    {
        /* APP_ISOC_TRACE("[%s] start %dms timer", __FUNCTION__,
           ISOC_TIMEOUT_IN_MSECONDS); */
        wiced_start_timer(&isoc.isoc_send_data_timer, ISOC_TIMEOUT_IN_MSECONDS);
    }
}
CY_SECTION_RAMFUNC_END

/******************************************************************************
 * Function Name: isoc_init
 ******************************************************************************
 * Summary:
 *  Registers ISOC callbacks.
 *  Sets Phy preferences to ISOC.
 *****************************************************************************/
void isoc_init(void)
{
    wiced_result_t status;
    wiced_bt_ble_phy_preferences_t phy_preferences = {0};

    wiced_ble_isoc_cfg_t isoc_config = {
        .max_bis =0,
        .max_cis =1,
    };
    APP_ISOC_TRACE("[%s]", __FUNCTION__);

    // Register ISOC management callback
    wiced_ble_isoc_init(&isoc_config, &isoc_management_cback);


    isoc.max_payload = p_wiced_bt_cfg_settings->p_isoc_cfg->max_sdu_size;

    // Init ISOC data handler module and register ISOC receive data handler
    iso_dhm_init(p_wiced_bt_cfg_settings->p_isoc_cfg,
                 isoc_send_data_num_complete_packets_evt, rx_handler);

    // Register ISOC management callback

    // Set to 2M phy
    phy_preferences.rx_phys = WICED_BLE_ISOC_LE_2M_PHY;
    phy_preferences.tx_phys = WICED_BLE_ISOC_LE_2M_PHY;
    status = wiced_bt_ble_set_default_phy(&phy_preferences);
    APP_ISOC_TRACE("[%s] Set default phy status %d", __FUNCTION__, status);

    sequence_number_state = SN_IDLE;

    // Init send data timer
    wiced_init_timer(&isoc.isoc_send_data_timer, isoc_send_data_handler, 0,
                     WICED_MILLI_SECONDS_PERIODIC_TIMER);

    // Init keep alive timer
    wiced_init_timer(&isoc.isoc_keep_alive_timer, isoc_get_psn_start, 0,
                     WICED_SECONDS_PERIODIC_TIMER);

#ifdef ISOC_STATS
    // Init stats timer
    wiced_init_timer(&iso_stats_timer, isoc_stats_timeout, 0, 
                     WICED_SECONDS_PERIODIC_TIMER);
#endif

#ifdef ISOC_MONITOR_FOR_DROPPED_SDUs
    wiced_bt_dev_register_vse_callback(isoc_vse_cback);
#endif

    CY_UNUSED_PARAMETER( status );
}
