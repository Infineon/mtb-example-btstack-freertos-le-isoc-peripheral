/*
 * $ Copyright YEAR Cypress Semiconductor $
 */

/**
 * file host.c
 *
 * host management function file
 *
 * Abstract: This file implements the Host List storing/retrieving to/from NVRAM
 */
#include "wiced_bt_trace.h"
#include "wiced_bt_cfg.h"
#include "wiced_timer.h"
#include "nvram_lib.h"
#include "app.h"

#if HOST_TRACE_EN
# define HOST_TRACE        WICED_BT_TRACE
# if HOST_TRACE>1
#  define HOST_TRACE2      WICED_BT_TRACE
# else
#  define HOST_TRACE2(...)
# endif
#else
# define HOST_TRACE(...)
# define HOST_TRACE2(...)
#endif

#define COMMIT_DELAY 1000     // 1 sec to commit

#define HOST_LIST_ELEMENT_SIZE sizeof(host_info_t)
#define HOST_LIST_SIZE (HOST_LIST_MAX * HOST_LIST_ELEMENT_SIZE)

#define HOST_INFO_NOT_FOUND    0xff
#define HOST_INFO_INDEX_TOP    0

#define VS_ID_HOST_LIST "host_list"

typedef struct
{
    /// Flag to indicate that BRR is to be used with this host
    uint16_t    brrEnabled : 1;

    /// Flag to indicate that UCD has been enabled and to be used with this host
    uint16_t    ucdEnabled : 1;
    /// Flag indicating whether link key is present
    uint16_t    linkKeyPresent : 1;

} br_edr_host_info_t;

typedef struct
{
    uint16_t notification_flags;
    uint16_t indication_flags;
    uint8_t  addrType;

} ble_host_info_t;

typedef union
{
    br_edr_host_info_t br_edr;
    ble_host_info_t    le;
} bt_host_info_t;

typedef struct
{
    /// BD address of the bonded host
    wiced_bt_device_address_t       bdAddr;

    /// Link key of the bonded host
    wiced_bt_device_link_keys_t     link_keys;

    // transport
    wiced_bt_transport_t            transport;

    bt_host_info_t       bt;
} host_info_t;

static struct
{
    // host list NVRAM cache
    host_info_t list[HOST_LIST_MAX];

    // paired host count
    uint8_t count;

    // timer to commit NVRAM host list
    wiced_timer_t commitTimer;

} host={0};

/*******************************************************************************
 * Private functions
 ******************************************************************************/

/********************************************************************
 * Function Name: host_commit_timer_cb
 ********************************************************************
 * Summary:
 *  This function is the timeout handler for commit_timer
 *******************************************************************/
static void host_commit_timer_cb( TIMER_PARAM_TYPE arg )
{
    cy_rslt_t result;

    HOST_TRACE("host commit to NVRAM");
    result = nvram_write(VS_ID_HOST_LIST, (uint8_t *)host.list, HOST_LIST_SIZE);
    // save host info to NVRAM
    if(result)
    {
        HOST_TRACE("host info failed to commit to NVRAM");
    }
}

/********************************************************************
 * Function Name: host_update_to_nvram
 ********************************************************************
 * Summary:
 *  Save host info to NVRAM. It provides delayed commit so multiple
 *  commits within delayed time period can be commit to NVRAM together
 *  to minimize NVRAM access. (To save flash lifespam)
 *******************************************************************/
static void host_update_to_nvram(uint32_t delay)
{
    if (wiced_is_timer_in_use(&host.commitTimer))
    {
        wiced_stop_timer(&host.commitTimer);
    }

    if (delay)
    {
        wiced_start_timer(&host.commitTimer, delay);
    }
    else
    {
        host_commit_timer_cb(0);
    }
}

/********************************************************************
 * Function Name: host_clear
 ********************************************************************
 * Summary:
 *  initialize host list
 *******************************************************************/
static void host_clear(void)
{
    host.count = 0;

    memset(host.list, 0x00, HOST_LIST_SIZE);
}

#if HOST_LIST_MAX > 1
/********************************************************************
 * Function Name: host_shift_down
 ********************************************************************
 * Summary:
 *  Shifts down the list at index.
 *  If the index in invalid, it return false and does nothing.
 *  Otherwise, host.count increased by 1 after shifting.
 *
 *  For example,if the index 1 and the host.count is 4(we have 4 host elements),
 *  element 1-3 are moved to elements 2-4 and element 1 is cleared.
 *  Element 0 is untouched.
 *******************************************************************/
static uint8_t host_shift_down(uint8_t index)
{
    // make sure we have room to shift and the index is valid
    if (index <= host.count && host.count < HOST_LIST_MAX)
    {
        // check if need to shift
        if (index < host.count)
        {
            // Use memmove to ensure that overalpping areas are moved correctly
            memmove(&host.list[index+1],
                    &host.list[index],
                    HOST_LIST_ELEMENT_SIZE*(host.count - index));
        }

        /* This condition cannot be false because "index <= host.count 
           && host.count < HOST_LIST_MAX" but just to make coverity happy */
        if (index < HOST_LIST_MAX)
        {
            // Clear the new element data at index
            memset(&host.list[index], 0, HOST_LIST_ELEMENT_SIZE);
        }

        // Now we have one more host element
        host.count++;

        return TRUE;
    }
    return FALSE;
}
#endif

/********************************************************************
 * Function Name: host_shift_up
 ********************************************************************
 * Summary:
 *  Shifts the host list up to the index. After a valid shifting,
 *  btpairingHostInfoListNum will be declemented.
 *
 * For example if this is called with index = 1 and we have host.count = 4,
 *  element 2-3 are moved to elements 1-2 and element 3 is cleared.
 *  The host.count will become 3. Element 0 is untouched.
 ********************************************************************/
static void host_shift_up(uint8_t index)
{
    // make sure index is valid
    if (index < host.count)
    {
        // We are removing one host
        host.count--;

#if HOST_LIST_MAX > 1
        // Use memmove to ensure that overalpping areas are moved correctly
        memmove(&host.list[index],
                &host.list[index+1],
                HOST_LIST_ELEMENT_SIZE*(host.count - index));

#endif
        if (host.count < HOST_LIST_MAX)
        {
            // Clear the freed element
            memset(&host.list[host.count], 0, HOST_LIST_ELEMENT_SIZE);
        }
    }
}

/********************************************************************
 * Function Name: host_findAddr
 ********************************************************************
 * Summary:
 *  Returns the pointer of the host element by giveing address
 ********************************************************************/
static uint8_t host_findAddr(const wiced_bt_device_address_t bdAddr)
{
    uint8_t index;

    // Go through all the valid entries in the table
    for (index=HOST_INFO_INDEX_TOP; index < host.count; index++)
    {
        if (memcmp(&host.list[index].bdAddr, bdAddr, BD_ADDR_LEN) == 0)
        {
            // Got it! Return the index
            return index;
        }
    }

    // If we get here, the address doesn't exist.
    return HOST_INFO_NOT_FOUND;
}

/********************************************************************
 * Function Name: host_del
 ********************************************************************
 * Summary:
 *  delete host
 *  if host is LE and private address, remove it from resolving list also.
 ********************************************************************/
static void host_del(uint8_t i)
{
    // make sure index is valid
    if (i < host.count)
    {
        if ((host.list[i].transport == BT_TRANSPORT_LE) &&  // LE transport
             host.list[i].bt.le.addrType)   // not public address
        {
            wiced_bt_dev_remove_device_from_address_resolution_db(
                &host.list[i].link_keys);
        }
        wiced_bt_dev_delete_bonded_device(host.list[i].bdAddr);

        // delete current host element
        host_shift_up(i);

    }
}

/********************************************************************
 * Function Name: host_activate
 ********************************************************************
 * Summary:
 *  Activate the host (move the host to element 0)
 *   if host is new, add a new host at the top and return TRUE
 *   if host already exist and not on the top, move to the top, returns TRUE
 *   otherwise, it is already on the top, return FALSE for doing nothing.
 ********************************************************************/
static wiced_bool_t host_activate(const wiced_bt_device_address_t bdAddr)
{
    uint8_t index = host_findAddr(bdAddr);

    if (index != HOST_INFO_INDEX_TOP)
    {
        HOST_TRACE("new host %B", bdAddr);
#if HOST_LIST_MAX <= 1
        host.count=1;
#else
        host_info_t tempHost = host.list[index];
        wiced_bool_t found = index != < HOST_LIST_MAX;

        // if host is already in the list, save it
        if (found)
        {
            // save current host info
            tempHost = host.list[index];
            host_del(index);
        }

        // now we make room at the top for the new host
        host_shift_down(HOST_INFO_INDEX_TOP);

        HOST_TRACE("%s host %", found ? "Updating" : "Adding", bdAddr);
        if (found)
        {
            // restore original host info
            host.list[HOST_INFO_INDEX_TOP] = tempHost;
        }
        else
#endif
        {
            // initialize host info
            memset(&host.list[HOST_INFO_INDEX_TOP], 0, sizeof(host_info_t));
            memcpy(host.list[HOST_INFO_INDEX_TOP].bdAddr, bdAddr, BD_ADDR_LEN);
            // default transport to LE
            host.list[HOST_INFO_INDEX_TOP].transport = BT_TRANSPORT_LE;
        }
        host_update_to_nvram(COMMIT_DELAY);
    }
    return index != HOST_INFO_INDEX_TOP;
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/

/********************************************************************
 * Function Name: host_get_link_key
 ********************************************************************
 * Summary:
 *  get a copy of active link key data
 ********************************************************************/
wiced_bool_t host_get_link_key(const wiced_bt_device_address_t bdAddr,
                               wiced_bt_device_link_keys_t * link_key)
{
    uint8_t index = host_findAddr(bdAddr);

    if (index < HOST_LIST_MAX && (link_key != NULL))
    {
        HOST_TRACE("Found link key for %B", bdAddr);
        memcpy(link_key, &host.list[index].link_keys,
               sizeof(wiced_bt_device_link_keys_t));
        return TRUE;
    }
    HOST_TRACE("No link key found for %B", bdAddr);
    return FALSE;
}

/********************************************************************
 * Function Name: host_set_link_key
 ********************************************************************
 * Summary:
 *  This function is called when the host is bonded and data will be
 *  saved into NVRAM database
 *
 *  if host does not exist in database, it creates and add to new host.
 *  if host already exist, replace current host and move to active host.
 ********************************************************************/
void host_set_link_key(const wiced_bt_device_address_t bdAddr,
                       wiced_bt_device_link_keys_t * link_keys,
                       uint8_t transport)
{
    host_info_t * ptr = &host.list[HOST_INFO_INDEX_TOP];

    HOST_TRACE("%s link key for %B", link_keys ? "Update":"Clear", bdAddr);
   // bdAddr host will be placed to the top (active host) of the host list
    host_activate(bdAddr);

    if (link_keys)
    {
        // update link key
        memcpy(&ptr->link_keys, link_keys, sizeof(wiced_bt_device_link_keys_t));
        ptr->transport = transport;
        ptr->bt.le.addrType = link_keys->key_data.ble_addr_type;

        // add for LE type
        if (transport == BT_TRANSPORT_LE)
        {
            if (host_addr_type())
            {
                wiced_bt_dev_add_device_to_address_resolution_db ( link_keys );
            }
        }
    }
    host_update_to_nvram(COMMIT_DELAY);
}

/********************************************************************
 * Function Name: host_init
 ********************************************************************
 * Summary:
 *  Read Host information from NVRAM VS section and initialize
 *  host list database.
 ********************************************************************/
void host_init()
{
    host_clear();

    //timer to allow commit nvram write
    wiced_init_timer(&host.commitTimer, host_commit_timer_cb, 0,
                     WICED_MILLI_SECONDS_TIMER );

    HOST_TRACE("Host Init");
#ifdef AUTO_PAIRING
    WICED_BT_TRACE("Host pairing infomation is erased");
    host_update_to_nvram(0);
#else
    if (nvram_read(VS_ID_HOST_LIST, (uint8_t *)host.list,
                   HOST_LIST_SIZE) == CY_RSLT_SUCCESS)
    {
        uint8_t index = 0;
        wiced_bt_device_address_t nullAddr = {0};

        while (index < HOST_LIST_MAX)
        {
            if (!memcmp(nullAddr, host.list[index].bdAddr, BD_ADDR_LEN))
            {
                break;
            }
            HOST_TRACE("%d. %B (%s host)",index, host.list[index].bdAddr,
                       host.list[index].transport == BT_TRANSPORT_LE ? "LE" :
                       "BR/EDR");
            if (host.list[index].transport == BT_TRANSPORT_LE 
                && host.list[index].bt.le.addrType)
            {
                wiced_bt_dev_add_device_to_address_resolution_db (
                    &host.list[index].link_keys );
            }
            index++;
        }
        host.count = index;
    }
    else
    {
        HOST_TRACE("Host info not found in NVRAM");
    }

    if( !host.count)
    {
        HOST_TRACE("Host list empty");
    }
#endif
}

/******************************************************************************
 * Function Name: host_remove
 ******************************************************************************
 * Summary:
 *  Delete the active host from the list
 ******************************************************************************/
wiced_bool_t host_remove()
{
    if (host.count)
    {
        host_del(HOST_INFO_INDEX_TOP);
        host_update_to_nvram(COMMIT_DELAY);
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************
 * Function Name: host_remove_addr
 ******************************************************************************
 * Summary:
 *  Delete the bd_addr host from the list
 ******************************************************************************/
wiced_bool_t host_remove_addr(wiced_bt_device_address_t bdAddr)
{
    if (host_exist(bdAddr))
    {
        host_remove();
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************
 * Function Name: host_remove_all
 ******************************************************************************
 * Summary:
 *  Delete all hosts from the list
 ******************************************************************************/
void host_remove_all(void)
{
    while (host_remove());
}

/******************************************************************************
 * Function Name: host_addr
 ******************************************************************************
 * Summary:
 *  return active host's bd_addr
 *   If no device is paired, it returns a pointer points to an
 *   address with all 00.
 ******************************************************************************/
uint8_t * host_addr()
{
    static wiced_bt_device_address_t null_addr = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    return host_is_paired() ? host.list[0].bdAddr : null_addr;
}

/******************************************************************************
 * Function Name: host_exist
 ******************************************************************************
 * Summary:
 *  return TRUE if given bd_addr is in the host list
 ******************************************************************************/
wiced_bool_t host_exist(const wiced_bt_device_address_t host_bd_addr)
{
    return host_findAddr(host_bd_addr) != HOST_INFO_NOT_FOUND;
}

/******************************************************************************
 * Function Name: host_count
 ******************************************************************************
 * Summary:
 *  returns number of hosts paired
 ******************************************************************************/
uint8_t host_count(void)
{
    return host.count;
}

/*******************************************************************************
 * Function Name: host_addr_type
 *******************************************************************************
 * Summary:
 *  returns the active host type for LE link.
 *  returns 0 if the link is not LE.
 ******************************************************************************/
uint8_t host_addr_type()
{
    return host_transport()==BT_TRANSPORT_LE ? host.list[0].bt.le.addrType : 0;
}

/*******************************************************************************
 * Function Name: host_transport
 *******************************************************************************
 * Summary:
 *  returns the active host transport type
 *
 * returns BT_TRANSPORT_LE
 *         BT_TRANSPORT_BR_EDR
 *         or 0 if not paired
 ******************************************************************************/
wiced_bt_transport_t host_transport()
{
    return host_is_paired() ? host.list[0].transport : 0;
}

/* end of file */
