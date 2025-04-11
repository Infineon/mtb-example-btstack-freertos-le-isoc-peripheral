/*
 *  Copyright 2025, Cypress Semiconductor Corporation (an Infineon company) or
 *  an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 * 
 *  This software, including source code, documentation and related
 *  materials ("Software") is owned by Cypress Semiconductor Corporation
 *  or one of its affiliates ("Cypress") and is protected by and subject to
 *  worldwide patent protection (United States and foreign),
 *  United States copyright laws and international treaty provisions.
 *  Therefore, you may use this Software only as provided in the license
 *  agreement accompanying the software package from which you
 *  obtained this Software ("EULA").
 *  If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 *  non-transferable license to copy, modify, and compile the Software
 *  source code solely for use in connection with Cypress's
 *  integrated circuit products.  Any reproduction, modification, translation,
 *  compilation, or representation of this Software except as specified
 *  above is prohibited without the express written permission of Cypress.
 * 
 *  Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 *  reserves the right to make changes to the Software without notice. Cypress
 *  does not assume any liability arising out of the application or use of the
 *  Software or any product or circuit described in the Software. Cypress does
 *  not authorize its products for use in any products where a malfunction or
 *  failure of the Cypress product may reasonably be expected to result in
 *  significant property damage, injury or death ("High Risk Product"). By
 *  including Cypress's product in a High Risk Product, the manufacturer
 *  of such system or application assumes all risk of such use and in doing
 *  so agrees to indemnify Cypress against all liability.
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
