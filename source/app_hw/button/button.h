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
 * @file button.h
 *
 * @brief   This file consists of the function prototypes that are
 *          necessary for developing push button use cases.
 */
#ifndef BUTTON_H_
#define BUTTON_H_

#ifdef BUTTON_SUPPORT

#include "button_lib.h"

#define BUTTON_MAX 1     // We only have one button
#define USER_BUTTON 0    // index 0

/*
 * The global variable for the key to be sent
 */
extern uint8_t keyValue;

/*******************************************************************************
 * Function prototype
 ******************************************************************************/

/********************************************************************
 * Function Name: button_init
 ********************************************************************
 * Summary:
 *  Initialize for button
 *******************************************************************/
void button_init();
#else
 #define button_init()
#endif     // BUTTON_SUPPORT

#endif      /* BUTTON_H_ */

/* [] END OF FILE */
