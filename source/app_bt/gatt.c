/*
 * Copyright 2016-2025, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

/**
 * file gatt.c
 *
 * GATT function file
 *
 */

#include "wiced_memory.h"
#include "wiced_bt_trace.h"
#include "app.h"
#include "cycfg_gap.h"
#include "gatt_utils_lib.h"
#include  "app_terminal_trace.h"
#if GATT_TRACE
# define APP_GATT_TRACE        WICED_BT_TRACE
# define APP_GATT_TRACE_ARRAY(ptr, len)        WICED_BT_TRACE("%A", ptr, len)
# define APP_GATT_TRACE_ARRAY_S(str, ptr, len) \
         WICED_BT_TRACE(str " %A", ptr, len)
# if GATT_TRACE>1
#  define APP_GATT_TRACE2      WICED_BT_TRACE
# else
#  define APP_GATT_TRACE2(...)
# endif
#else
# define APP_GATT_TRACE_ARRAY(ptr, len)
# define APP_GATT_TRACE_ARRAY_S(str, ptr, len)
# define APP_GATT_TRACE(...)
# define APP_GATT_TRACE2(...)
#endif

/*******************************************************************************
 * typedef
 ******************************************************************************/
typedef void (*pfn_free_buffer_t)(uint8_t *);

/*******************************************************************************
 * Private functions
 ******************************************************************************/

/*******************************************************************************
 * Function Name: gatt_free_buffer
 *******************************************************************************
 * Summary:
 *  Frees buffer.
 ******************************************************************************/
static void gatt_free_buffer(uint8_t *p_data)
{
    wiced_bt_free_buffer(p_data);
    APP_GATT_TRACE2("[%s] free 0x%x", __FUNCTION__, p_data);
}

/*******************************************************************************
 * Function Name: gatt_alloc_buffer
 *******************************************************************************
 * Summary:
 *  allocate buffer
 ******************************************************************************/
static uint8_t * gatt_alloc_buffer(uint16_t len)
{
    uint8_t *p = (uint8_t *)wiced_bt_get_buffer(len);
    APP_GATT_TRACE2("[%s] allo len %d, 0x%x", __FUNCTION__, len, p);
    return p;
}

/*******************************************************************************
 * Function Name: gatt_conn_state_change
 *******************************************************************************
 * Summary:
 *  Handles connection state change. This function is called when the
 *  link is up or down. It calls link module for the link event.
 ******************************************************************************/
static wiced_bt_gatt_status_t gatt_conn_state_change(
       wiced_bt_gatt_connection_status_t * p_status )
{
    if(p_status->connected)
    {
        return link_up( p_status );
    }
    else
    {
        return link_down( p_status );
    }
}

/*******************************************************************************
 * Function Name: gatt_req_read_by_type_handler
 *******************************************************************************
 * Summary:
 *  Process Read by type request from peer device
 ******************************************************************************/
static wiced_bt_gatt_status_t gatt_req_read_by_type_handler(
                                uint16_t conn_id,
                                wiced_bt_gatt_opcode_t opcode,
                                wiced_bt_gatt_read_by_type_t *p_read_req,
                                uint16_t len_requested )
{
    int         to_copy;
    uint8_t     * copy_from;
    uint16_t    attr_handle = p_read_req->s_handle;
    uint8_t    *p_rsp = wiced_bt_get_buffer(len_requested);
    uint8_t     pair_len = 0;
    int used = 0;

    if (p_rsp == NULL)
    {
        APP_GATT_TRACE("[%s] no memory len_requested: %d!!", __FUNCTION__,
                len_requested);
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, attr_handle,
                WICED_BT_GATT_INSUF_RESOURCE);
        return WICED_BT_GATT_INSUF_RESOURCE;
    }

    /* Read by type returns all attributes of the specified type,
       between the start and end handles */
    while (WICED_TRUE)
    {
        /// Add your code here
        attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle,
                p_read_req->e_handle, &p_read_req->uuid);

        if (attr_handle == 0)
            break;

        switch(attr_handle)
        {
        case HDLC_GAP_DEVICE_NAME_VALUE:
            to_copy = app_gap_device_name_len;
            copy_from = (uint8_t *) app_gap_device_name;
            break;

        case HDLC_GAP_APPEARANCE_VALUE:
            to_copy = 2;
            copy_from = 
                (uint8_t *)&p_wiced_bt_cfg_settings->p_ble_cfg->appearance;
            break;

        default:
            APP_GATT_TRACE("[%s] found type but no attribute ??", __FUNCTION__);
            wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
                    p_read_req->s_handle, WICED_BT_GATT_ERR_UNLIKELY);
            wiced_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_ERR_UNLIKELY;
        }

        int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(
                p_rsp + used,
                len_requested - used,
                &pair_len,
                attr_handle,
                to_copy,
                copy_from);

        if (filled == 0) {
            break;
        }
        used += filled;

        /* Increment starting handle for next search to one past current */
        attr_handle++;
    }

    if (used == 0)
    {
        APP_GATT_TRACE("[%s] attr not found 0x%04x -  0x%04x Type: 0x%04x",
                __FUNCTION__, p_read_req->s_handle, p_read_req->e_handle,
                p_read_req->uuid.uu.uuid16);

        wiced_bt_gatt_server_send_error_rsp(conn_id,opcode,p_read_req->s_handle,
                WICED_BT_GATT_INVALID_HANDLE);
        wiced_bt_free_buffer(p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_by_type_rsp(conn_id, opcode, pair_len,
            used, p_rsp, (wiced_bt_gatt_app_context_t)wiced_bt_free_buffer);

    return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: gatt_req_read_multi_handler
 *******************************************************************************
 * Summary:
 *  Process read multi request from peer device
 ******************************************************************************/
static wiced_bt_gatt_status_t gatt_req_read_multi_handler(
                                uint16_t conn_id,
                                wiced_bt_gatt_opcode_t opcode,
                                wiced_bt_gatt_read_multiple_req_t *p_read_req,
                                uint16_t len_requested )
{
    uint8_t     *p_rsp = wiced_bt_get_buffer(len_requested);
    int         used = 0;
    int         xx;
    uint16_t    handle;
    int         to_copy;
    uint8_t     * copy_from;

    handle=wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream,0);

    if (p_rsp == NULL)
    {
        APP_GATT_TRACE ("[%s] no memory len_requested: %d!!", __FUNCTION__,
                len_requested);

        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, handle,
                WICED_BT_GATT_INSUF_RESOURCE);
        return WICED_BT_GATT_INSUF_RESOURCE;
    }

    /* Read by type returns all attributes of the specified type,
       between the start and end handles */
    for (xx = 0; xx < p_read_req->num_handles; xx++)
    {
        handle=wiced_bt_gatt_get_handle_from_stream(p_read_req->p_handle_stream,
                                                    xx);

        switch(handle)
        {
        case HDLC_GAP_DEVICE_NAME_VALUE:
            to_copy = app_gap_device_name_len;
            copy_from = (uint8_t *) app_gap_device_name;
            break;

        case HDLC_GAP_APPEARANCE_VALUE:
            to_copy = 2;
            copy_from = 
                (uint8_t *) &p_wiced_bt_cfg_settings->p_ble_cfg->appearance;
            break;

        default:
            APP_GATT_TRACE ("[%s] no handle 0x%04x", __FUNCTION__, handle);
            wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
                    *p_read_req->p_handle_stream, WICED_BT_GATT_ERR_UNLIKELY);
            wiced_bt_free_buffer(p_rsp);
            return WICED_BT_GATT_ERR_UNLIKELY;
        }
        int filled = wiced_bt_gatt_put_read_multi_rsp_in_stream(opcode,
                    p_rsp + used,
                    len_requested - used,
                    handle,
                    to_copy,
                    copy_from);

        if (!filled) {
            break;
        }
        used += filled;
    }

    if (used == 0)
    {
        APP_GATT_TRACE ("[%s] no attr found", __FUNCTION__);

        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
                *p_read_req->p_handle_stream, WICED_BT_GATT_INVALID_HANDLE);
        wiced_bt_free_buffer(p_rsp);
        return WICED_BT_GATT_INVALID_HANDLE;
    }

    /* Send the response */
    wiced_bt_gatt_server_send_read_multiple_rsp(conn_id, opcode, used, p_rsp,
            (wiced_bt_gatt_app_context_t)wiced_bt_free_buffer);

    return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: gatt_req_mtu_handler
 *******************************************************************************
 * Summary:
 *  Process the request for mtu.
 ******************************************************************************/
static wiced_result_t gatt_req_mtu_handler( uint16_t conn_id, uint16_t mtu )
{
    APP_GATT_TRACE("req_mtu: %d", mtu);
    wiced_bt_gatt_server_send_mtu_rsp(conn_id, mtu, cfg_mtu());
    return (wiced_result_t) WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: gatt_req_conf_handler
 *******************************************************************************
 * Summary:
 *  Process indication confirm. The indication must be confirmed before
 *  another indication can be sent.
 ******************************************************************************/
static wiced_bt_gatt_status_t gatt_req_conf_handler(uint16_t conn_id,
                                                    uint16_t handle)
{
    APP_GATT_TRACE("gatt_req_conf_handler, conn 0x%04x hdl 0x%04x",
                   conn_id, handle );
    link_set_indication_pending(FALSE);  // Clear the indication pending flag.
    return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: gatt_req_cb
 *******************************************************************************
 * Summary:
 *  GATT_ATTRIBUTE_REQUEST_EVT handler
 *******************************************************************/
static wiced_bt_gatt_status_t gatt_req_cb(wiced_bt_gatt_attribute_request_t
                                          *p_req )
{
    wiced_bt_gatt_status_t result  = WICED_BT_GATT_SUCCESS;

    APP_GATT_TRACE2("GATT request conn_id:0x%04x opcode:%d", p_req->conn_id,
                    p_req->opcode );

    switch ( p_req->opcode )
    {
        case GATT_REQ_READ:
        case GATT_REQ_READ_BLOB:
            result = app_gatt_read_req_handler( p_req->conn_id,
                     &p_req->data.read_req, p_req->opcode,
                     p_req->len_requested);
            break;

        case GATT_REQ_READ_BY_TYPE:
            result = gatt_req_read_by_type_handler(p_req->conn_id,
                    p_req->opcode,
                    &p_req->data.read_by_type,
                    p_req->len_requested);
            break;

        case GATT_REQ_READ_MULTI:
        case GATT_REQ_READ_MULTI_VAR_LENGTH:
            APP_GATT_TRACE( "req_read_multi_handler" );
            result = gatt_req_read_multi_handler(p_req->conn_id,
                    p_req->opcode,
                    &p_req->data.read_multiple_req,
                    p_req->len_requested);
            break;

        case GATT_REQ_WRITE:
        case GATT_CMD_WRITE:
        case GATT_CMD_SIGNED_WRITE:
            result = app_gatt_write_handler(p_req->conn_id,
                                            &p_req->data.write_req);
            if (result == WICED_BT_GATT_SUCCESS)
            {
                wiced_bt_gatt_server_send_write_rsp(p_req->conn_id,
                    p_req->opcode,
                    p_req->data.write_req.handle);
            }
            else
            {
                wiced_bt_gatt_server_send_error_rsp(p_req->conn_id,
                    p_req->opcode,
                    p_req->data.write_req.handle, result);
            }
            break;

        case GATT_REQ_EXECUTE_WRITE:
            result = WICED_SUCCESS;
            wiced_bt_gatt_server_send_execute_write_rsp(p_req->conn_id,
                                                        p_req->opcode);
            break;

        case GATT_REQ_MTU:
            result = gatt_req_mtu_handler(p_req->conn_id,
                                          p_req->data.remote_mtu);
            break;

        case GATT_HANDLE_VALUE_CONF:
            result = gatt_req_conf_handler(p_req->conn_id,
                                           p_req->data.confirm.handle );
            break;

        case GATT_HANDLE_VALUE_NOTIF:
            break;

        default:
            APP_GATT_TRACE("Unhandled GATT request x%x", p_req->opcode);
            break;
    }

    return result;
}

/********************************************************************
 * Function Name: gatt_callback
 ********************************************************************
 * Summary:
 *  This is a GATT event callback handler
 *******************************************************************/
static wiced_bt_gatt_status_t gatt_callback(wiced_bt_gatt_evt_t event,
                                            wiced_bt_gatt_event_data_t * p_data)
{
    wiced_bt_gatt_status_t result = WICED_BT_GATT_SUCCESS;

    switch(event)
    {
        case GATT_CONNECTION_STATUS_EVT:
            APP_GATT_TRACE2("GATT_CONNECTION_STATUS_EVT");
            result = gatt_conn_state_change(&p_data->connection_status);
            break;

        case GATT_OPERATION_CPLT_EVT:
            APP_GATT_TRACE2("GATT_OPERATION_CPLT_EVT");
            break;

        case GATT_DISCOVERY_CPLT_EVT:
            APP_GATT_TRACE2("GATT_DISCOVERY_CPLT_EVT");
            break;

        case GATT_ATTRIBUTE_REQUEST_EVT:
            APP_GATT_TRACE2("GATT_ATTRIBUTE_REQUEST_EVT");
            result = gatt_req_cb(&p_data->attribute_request);
            break;

        case GATT_CONGESTION_EVT:
            APP_GATT_TRACE2("GATT_CONGESTION_EVT:%d",
                            p_data->congestion.congested);
            break;

        case GATT_GET_RESPONSE_BUFFER_EVT:
            p_data->buffer_request.buffer.p_app_rsp_buffer = 
                gatt_alloc_buffer (p_data->buffer_request.len_requested);
            p_data->buffer_request.buffer.p_app_ctxt=(void *) gatt_free_buffer;
            result = WICED_BT_GATT_SUCCESS;
            break;

        case GATT_APP_BUFFER_TRANSMITTED_EVT:
            {
                pfn_free_buffer_t pfn_free = 
                    (pfn_free_buffer_t)p_data->buffer_xmitted.p_app_ctxt;

                /* If the buffer is dynamic, the context will point to 
                   a function to free it. */
                if (pfn_free)
                {
                    pfn_free(p_data->buffer_xmitted.p_app_data);
                }
                result = WICED_BT_GATT_SUCCESS;
            }
            break;

        default:
            APP_GATT_TRACE("gatts_callback: unhandled event!!!:0x%x", event);
            break;
    }

    return result;
}

/*******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 * Function Name: gatt_read_req_default_handler
 ******************************************************************************
 * Summary:
 *  Default handler to process read request or command from peer device.
 *  The event calls application gatt_read_req_handler first. When it is not
 *  handled in application, this default handler is called.
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_read_req_default_handler(uint16_t conn_id, 
                           wiced_bt_gatt_read_t *p_req,
                           wiced_bt_gatt_opcode_t opcode,
                           uint16_t len_requested)
{
    const gatt_db_lookup_table_t * p_attribute;
    wiced_bt_gatt_status_t result = WICED_BT_GATT_ERROR;
    uint8_t * from;
    /* in case if p_req is NULL, we don't want to crash system calling 
       wiced_bt_gatt_server_send_error_rsp() for p_req->handle */
    uint16_t handle = 0;
    int to_copy = len_requested;

    // make sure p_req is valid
    if(p_req)
    {
        handle = p_req->handle;

        APP_GATT_TRACE("read_attrib - conn:0x%04x hdl:0x%04x ofst:%d len:%d",
                       conn_id, handle, p_req->offset, len_requested );

        result = WICED_BT_GATT_INVALID_HANDLE;
        p_attribute = wiced_bt_util_get_attribute(app_gatt_db_ext_attr_tbl,
                                                  handle);
        if(p_attribute)
        {
            from = p_attribute->p_data + p_req->offset;
            result = WICED_BT_GATT_INVALID_OFFSET;
            if (p_req->offset < p_attribute->max_len)  // validate offset
            {
                result = WICED_BT_GATT_SUCCESS;
                // copy len is over limit
                if (to_copy > p_attribute->max_len - p_req->offset)
                {
                    to_copy = p_attribute->max_len - p_req->offset;
                    APP_GATT_TRACE("read length:%d", to_copy );
                }
            }
        }
    }

    if (result == WICED_BT_GATT_SUCCESS)
    {
        APP_GATT_TRACE_ARRAY_S("read success:", from, to_copy);
        wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode, to_copy,
                                                  from, NULL);
    }
    else
    {
        APP_GATT_TRACE("read failed, reason %d", result );
        wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, handle, result);
    }
    return result;
}

/******************************************************************************
 * Function Name: gatt_write_default_handler
 ******************************************************************************
 * Summary:
 *  Default handler to process write request or command from peer device.
 *  The event calls application gatt_write_handler first. When it is not
 *  handled in application, this default handler is called.
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_write_default_handler(uint16_t conn_id,
                           wiced_bt_gatt_write_req_t * p_wr_data )
{
    if(link_conn_id() != conn_id)
    {
        APP_GATT_TRACE("gatt: write handle to an invalid conn_id:%04x",conn_id);
        return WICED_BT_GATT_ERROR;
    }
    else
    {
        const gatt_db_lookup_table_t * p_attribute =
            wiced_bt_util_get_attribute(app_gatt_db_ext_attr_tbl,
            p_wr_data->handle);
        wiced_bt_gatt_status_t result = WICED_BT_GATT_SUCCESS;

        APP_GATT_TRACE("write_attrib - conn:0x%04x hdl:0x%04x off:%d len:%d",
            conn_id, p_wr_data->handle, p_wr_data->offset, p_wr_data->val_len );
        APP_GATT_TRACE_ARRAY_S("Data:", p_wr_data->p_val, p_wr_data->val_len);

        if(p_attribute)
        {
            if(p_wr_data->offset > p_attribute->max_len)
            {
                APP_GATT_TRACE("Invalid offset, max_len=%d, ofst:%d",
                    p_attribute->max_len, p_wr_data->offset);
                result = WICED_BT_GATT_INVALID_OFFSET;
            }
            else if((p_wr_data->val_len + p_wr_data->offset) >
                    p_attribute->max_len)
            {
                APP_GATT_TRACE("Invalid len");
                result = WICED_BT_GATT_INVALID_ATTR_LEN;
            }
            else
            {
                APP_GATT_TRACE("write_attrib - success");
                // write the data
                memcpy(p_attribute->p_data + p_wr_data->offset,
                       p_wr_data->p_val, p_wr_data->val_len);
            }

#ifndef DISABLE_ENCRYPTION
            // The device must be bonded to be consider success
            if ((result == WICED_BT_GATT_SUCCESS) && !link_is_bonded() )
            {
                result = WICED_BT_GATT_INSUF_AUTHENTICATION;
            }
#endif
        }
        else
        {
            result = WICED_BT_GATT_INVALID_HANDLE;
        }

        return result;
    }
}

/******************************************************************************
 * Function Name: gatt_initialize
 ******************************************************************************
 * Summary:
 *  Initialize gatt database.
 *  The advertisement data CY_BT_ADV_PACKET_DATA_SIZ and cy_bt_adv_packet_data
 *  are generated by BT Configurator in cycfg_gap.h/c.
 *  The gatt database gatt_database, and gatt_database_len are generated by
 *  BT Configurator in cycfg_gatt_db.h/c.
 ******************************************************************************/
wiced_bt_gatt_status_t gatt_initialize()
{
    wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE,
                                            cy_bt_adv_packet_data);

    /* Register with stack to receive GATT callback */
    wiced_bt_gatt_register( gatt_callback );

    return wiced_bt_gatt_db_init( gatt_database, gatt_database_len, NULL );
}

/* end of file */
