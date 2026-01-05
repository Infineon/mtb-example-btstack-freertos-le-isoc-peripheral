/*
 * (c) 2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
 */

/**
 * File Name: button.c
 *
 * Description: This file consists of the function prototypes that are
 *              necessary for developing push button use cases.
 */

/*******************************************************************************
 *                              INCLUDES
 ******************************************************************************/
#ifdef BUTTON_SUPPORT

#include "wiced_bt_trace.h"
#include "app.h"
#include  "app_terminal_trace.h"
static button_cfg_t button_cfg[BUTTON_MAX] = {
  { CYBSP_USER_BTN, CYBSP_BTN_OFF },
};


/********************************************************************
 * Function Name: button_state_changed
 ********************************************************************
 * Summary:
 *  key state change handler
 *******************************************************************/
CY_SECTION_RAMFUNC_BEGIN
static void button_state_changed(uint32_t changed)
{
    if (changed & (1<<USER_BUTTON)) // is user button changed?
    {
        wiced_bool_t pressed = button_is_pressed(USER_BUTTON);

        if (link_is_connected())
        {
            if (isoc_cis_connected())
            {
                isoc_send_data( pressed );
                return;
            }
        }
        else if (pressed)
        {
//            WICED_BT_TRACE("User button down");
            if (host_is_paired())
            {
                if (bt_is_advertising())
                {
//                    WICED_BT_TRACE("Removing host %B bonding", host_addr());
                    host_remove();

                    bt_stop_advertisement();
//                    WICED_BT_TRACE("Enter pairing");
                    bt_enter_pairing();
                }
                else
                {
//                    WICED_BT_TRACE("reconnecting to host %B", host_addr());
                    bt_enter_connect();
                }
            }
            else
            {
                if (bt_is_advertising())
                {
//                    WICED_BT_TRACE("Stop pairing");
                    bt_stop_advertisement();
                }
                else
                {
//                    WICED_BT_TRACE("Enter pairing");
                    bt_enter_pairing();
                }
            }
        }
    }
}
CY_SECTION_RAMFUNC_END

/******************************************************************************
*   PUBLIC FUNCTIONS
*******************************************************************************/

/******************************************************************************
 * Function Name: button_init
 ******************************************************************************
 * Summary:
 *  Initialize for button
*******************************************************************************/
void button_init()
{
    button_lib_init(BUTTON_MAX, button_cfg, button_state_changed);
}
#endif /* BUTTON_SUPPORT */

/* end of file */
