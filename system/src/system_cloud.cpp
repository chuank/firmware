/**
 ******************************************************************************
 * @file    system_cloud.cpp
 * @author  Satish Nair, Zachary Crockett, Mohit Bhoite, Matthew McGowan
 * @version V1.0.0
 * @date    13-March-2013
 *
 * Updated: 14-Feb-2014 David Sidrane <david_s5@usa.net>
 * @brief
 ******************************************************************************
  Copyright (c) 2013-2015 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#include "spark_wiring.h"
#include "spark_wiring_network.h"
#include "spark_wiring_cloud.h"
#include "system_network.h"
#include "system_task.h"
#include "system_update.h"
#include "system_cloud_internal.h"
#include "string_convert.h"
#include "spark_protocol_functions.h"
#include "spark_protocol.h"
#include "socket_hal.h"
#include "core_hal.h"
#include "core_subsys_hal.h"
#include "deviceid_hal.h"
#include "inet_hal.h"
#include "rtc_hal.h"
#include "ota_flash_hal.h"
#include "product_store_hal.h"
#include "rgbled.h"
#include "spark_macros.h"
#include "string.h"
#include <stdarg.h>
#include "append_list.h"


SubscriptionScope::Enum convert(Spark_Subscription_Scope_TypeDef subscription_type)
{
    return(subscription_type==MY_DEVICES) ? SubscriptionScope::MY_DEVICES : SubscriptionScope::FIREHOSE;
}

bool spark_subscribe(const char *eventName, EventHandler handler, void* handler_data,
        Spark_Subscription_Scope_TypeDef scope, const char* deviceID, void* reserved)
{
    auto event_scope = convert(scope);
    bool success = spark_protocol_add_event_handler(sp, eventName, handler, event_scope, deviceID, handler_data);
    if (success && spark_connected())
    {
        if (deviceID)
            success = spark_protocol_send_subscription_device(sp, eventName, deviceID);
        else
            success = spark_protocol_send_subscription_scope(sp, eventName, event_scope);
    }
    return success;
}


inline EventType::Enum convert(Spark_Event_TypeDef eventType) {
    return eventType==PUBLIC ? EventType::PUBLIC : EventType::PRIVATE;
}

bool spark_send_event(const char* name, const char* data, int ttl, Spark_Event_TypeDef eventType, void* reserved)
{
    return spark_protocol_send_event(sp, name, data, ttl, convert(eventType), NULL);
}

bool spark_variable(const char *varKey, const void *userVar, Spark_Data_TypeDef userVarType, void* reserved)
{
    User_Var_Lookup_Table_t* item = NULL;
    if (NULL != userVar && NULL != varKey && strlen(varKey)<=USER_VAR_KEY_LENGTH)
    {
        if ((item=find_var_by_key_or_add(varKey)))
        {
            item->userVar = userVar;
            item->userVarType = userVarType;
            memset(item->userVarKey, 0, USER_VAR_KEY_LENGTH);
            memcpy(item->userVarKey, varKey, USER_VAR_KEY_LENGTH);
        }
    }
    return item!=NULL;
}

/**
 * This is the original released signature for firmware version 0 and needs to remain like this.
 * (The original returned void - we can safely change to
 */
bool spark_function(const char *funcKey, p_user_function_int_str_t pFunc, void* reserved)
{
    bool result;
    if (funcKey) {
        cloud_function_descriptor desc;
        desc.funcKey = funcKey;
        desc.fn = call_raw_user_function;
        desc.data = (void*)pFunc;
        result = spark_function_internal(&desc, NULL);
    }
    else {
        result = spark_function_internal((cloud_function_descriptor*)pFunc, reserved);
    }
    return result;
}

bool spark_connected(void)
{
    if (SPARK_CLOUD_SOCKETED && SPARK_CLOUD_CONNECTED)
        return true;
    else
        return false;
}

void spark_connect(void)
{
    //Schedule Spark's cloud connection and handshake
    SPARK_WLAN_SLEEP = 0;
    SPARK_CLOUD_CONNECT = 1;
}

void spark_disconnect(void)
{
    //Schedule Spark's cloud disconnection
    SPARK_CLOUD_CONNECT = 0;
}

void spark_process(void)
{
    // run the background processing loop, and specifically also pump cloud events
    Spark_Idle_Events(true);
}

String spark_deviceID(void)
{
    unsigned len = HAL_device_ID(NULL, 0);
    uint8_t id[len];
    HAL_device_ID(id, len);
    return bytes2hex(id, len);
}


