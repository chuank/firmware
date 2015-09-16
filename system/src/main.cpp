/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Satish Nair, Zachary Crockett, Zach Supalla and Mohit Bhoite
 * @version V1.0.0
 * @date    13-March-2013
 *
 * Updated: 14-Feb-2014 David Sidrane <david_s5@usa.net>
 *
 * @brief   Main program body.
 ******************************************************************************
  Copyright (c) 2013-2015 Particle Industries, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
  ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "debug.h"
#include "system_event.h"
#include "system_mode.h"
#include "system_task.h"
#include "system_network.h"
#include "system_network_internal.h"
#include "core_hal.h"
#include "syshealth_hal.h"
#include "watchdog_hal.h"
#include "system_cloud.h"
#include "system_user.h"
#include "system_update.h"
#include "usb_hal.h"
#include "system_mode.h"
#include "rgbled.h"
#include "ledcontrol.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t TimingLED;
static volatile uint32_t TimingIWDGReload;

/**
 * KNowing the current listen mode isn't sufficient to determine the correct action (since that may or may not have changed)
 * we need also to know the listen mode at the time the button was pressed.
 */
static volatile bool wasListeningOnButtonPress;
static volatile uint16_t buttonPushed;

/* Extern variables ----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

// this is called on multiple threads - ideally need a mutex
void HAL_Notify_Button_State(uint8_t button, uint8_t pressed)
{
    if (button==0)
    {
        if (pressed) {
            wasListeningOnButtonPress = WLAN_SMART_CONFIG_START;
            buttonPushed = HAL_Timer_Get_Milli_Seconds();
            if (!wasListeningOnButtonPress)
                system_notify_event(button_status, 0);
        }
        else
        {
            uint16_t duration = HAL_Timer_Get_Milli_Seconds()-buttonPushed;
            if (!WLAN_SMART_CONFIG_START)
                system_notify_event(button_status, duration);
            buttonPushed = 0;
            if (duration>3000 && duration<8000 && wasListeningOnButtonPress && WLAN_SMART_CONFIG_START)
                WLAN_SMART_CONFIG_START = 0;
        }
    }
}

/*******************************************************************************
 * Function Name  : HAL_SysTick_Handler
 * Description    : Decrements the various Timing variables related to SysTick.
 * Input          : None
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
extern "C" void HAL_SysTick_Handler(void)
{
    if (LED_RGB_IsOverRidden())
    {
#ifndef SPARK_NO_CLOUD
        if (LED_Spark_Signal != 0)
        {
            LED_Signaling_Override();
        }
#endif
    }
    else if (TimingLED != 0x00)
    {
        TimingLED--;
    }
    else if(WLAN_SMART_CONFIG_START || SPARK_FLASH_UPDATE || Spark_Error_Count)
    {
        //Do nothing
    }
    else if(SPARK_LED_FADE)
    {
        LED_Fade(LED_RGB);
        TimingLED = 20;//Breathing frequency kept constant
    }
    else if(SPARK_CLOUD_CONNECTED)
    {
        LED_SetRGBColor(system_mode()==SAFE_MODE ? RGB_COLOR_MAGENTA : RGB_COLOR_CYAN);
        LED_On(LED_RGB);
        SPARK_LED_FADE = 1;
    }
    else
    {
        LED_Toggle(LED_RGB);
        if(SPARK_CLOUD_SOCKETED || (WLAN_CONNECTED && !WLAN_DHCP))
            TimingLED = 50;         //50ms
        else
            TimingLED = 100;        //100ms
    }

    if(SPARK_FLASH_UPDATE)
    {
#ifndef SPARK_NO_CLOUD
        if (TimingFlashUpdateTimeout >= TIMING_FLASH_UPDATE_TIMEOUT)
        {
            //Reset is the only way now to recover from stuck OTA update
            HAL_Core_System_Reset();
        }
        else
        {
            TimingFlashUpdateTimeout++;
        }
#endif
    }
    else if(WLAN_SMART_CONFIG_START && HAL_Core_Mode_Button_Pressed(10000))
    {
        WLAN_DELETE_PROFILES = 1;
        HAL_Core_Mode_Button_Reset();
    }
    // determine if the button press needs to change the state (and hasn't done so already))
    else if(!WLAN_SMART_CONFIG_START && HAL_Core_Mode_Button_Pressed(3000) && !wasListeningOnButtonPress)
    {
        if (SPARK_WLAN_STARTED) {
            wlan_connect_cancel(true);
        }
        // fire the button event to the user, then enter listening mode (so no more button notifications are sent)
        // there's a race condition here - the HAL_notify_button_state function should
        // be thread safe, but currently isn't.
        HAL_Notify_Button_State(0, false);
        WLAN_SMART_CONFIG_START = 1;
        HAL_Notify_Button_State(0, true);
    }


#ifdef IWDG_RESET_ENABLE
    if (TimingIWDGReload >= TIMING_IWDG_RELOAD)
    {
        TimingIWDGReload = 0;

        /* Reload WDG counter */
        HAL_Notify_WDT();
        DECLARE_SYS_HEALTH(CLEARED_WATCHDOG);
    }
    else
    {
        TimingIWDGReload++;
    }
#endif
}

void manage_safe_mode()
{
    uint16_t flag = (HAL_Bootloader_Get_Flag(BOOTLOADER_FLAG_STARTUP_MODE));
    if (flag != 0xFF) { // old bootloader
        if (flag & 1) {
            set_system_mode(SAFE_MODE);
        }
    }
}

/*******************************************************************************
 * Function Name  : main.
 * Description    : main routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void app_setup_and_loop(void)
{
    HAL_Core_Init();
    // We have running firmware, otherwise we wouldn't have gotten here
    DECLARE_SYS_HEALTH(ENTERED_Main);
    DEBUG("Hello from Spark!");

    manage_safe_mode();

#if defined (START_DFU_FLASHER_SERIAL_SPEED) || defined (START_YMODEM_FLASHER_SERIAL_SPEED)
    USB_USART_LineCoding_BitRate_Handler(system_lineCodingBitRateHandler);
#endif

    SPARK_WLAN_Setup(Multicast_Presence_Announcement);

    /* Main loop */
    while (1)
    {
        DECLARE_SYS_HEALTH(ENTERED_WLAN_Loop);
        Spark_Idle();

        static uint8_t SPARK_WIRING_APPLICATION = 0;
        if(SPARK_WLAN_SLEEP || !SPARK_CLOUD_CONNECT || SPARK_CLOUD_CONNECTED || SPARK_WIRING_APPLICATION || (system_mode()!=AUTOMATIC))
        {
            if(!SPARK_FLASH_UPDATE)
            {
                if ((SPARK_WIRING_APPLICATION != 1))
                {
                    //Execute user application setup only once
                    DECLARE_SYS_HEALTH(ENTERED_Setup);
                    if (system_mode()!=SAFE_MODE)
                     setup();
                    SPARK_WIRING_APPLICATION = 1;
                }

                //Execute user application loop
                DECLARE_SYS_HEALTH(ENTERED_Loop);
                if (system_mode()!=SAFE_MODE)
                    loop();
                    DECLARE_SYS_HEALTH(RAN_Loop);
                }
            }
        }
    }

#ifdef USE_FULL_ASSERT

/*******************************************************************************
 * Function Name  : assert_failed
 * Description    : Reports the name of the source file and the source line number
 *                  where the assert_param error has occurred.
 * Input          : - file: pointer to the source file name
 *                  - line: assert_param error line source number
 * Output         : None
 * Return         : None
 *******************************************************************************/
void assert_failed(uint8_t* file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
	{
	}
}
#endif
