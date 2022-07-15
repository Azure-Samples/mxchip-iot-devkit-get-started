// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 
// To get started please visit https://microsoft.github.io/azure-iot-developer-kit/docs/projects/connect-iot-hub?utm_source=ArduinoExtension&utm_medium=ReleaseNote&utm_campaign=VSCode
#include "AZ3166WiFi.h"         //WiFi
#include "AzureIotHub.h"
#include "DevKitMQTTClient.h"
#include "config.h"
#include "utility.h"
#include "SystemTickCounter.h"

// Indicate whether WiFi is ready
static bool hasWifi = false;

// Temperature sensor variables
static float temperature;
static float humidity;
static float pressure;

// Accelerometer, pedometer variables

// Magnetic variables

// Message to Cloud variables
int messageCount = 1;
int sentMessageCount = 0;
static bool messageSending = true;
static uint64_t send_interval_ms;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
//-----------------------------------------------------------------------------------------------------------------
static void InitWiFi()
{
  Screen.print(2, "Connecting...");
  delay(500);
  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    delay(500);
    hasWifi = true;
    Screen.print(2, "Running... \r\n");
    delay(500);
  }
  else
  {
    hasWifi = false;
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

//-----------------------------------------------------------------------------------------------------------------
static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    blinkSendConfirmation();
    sentMessageCount++;
  }

  Screen.print(1, "> IoT Hub");
  char line1[20];
  sprintf(line1, "Count: %d/%d",sentMessageCount, messageCount); 
  Screen.print(2, line1);

  char line2[20];
  sprintf(line2, "T:%.2f H:%.2f", temperature, humidity);
  Screen.print(3, line2);

  // char line3[20];
  // sprintf(line3, "P:%.2f", pressure);
  // Screen.print(3, line3);

  messageCount++;
}

//-----------------------------------------------------------------------------------------------------------------
static void MessageCallback(const char* payLoad, int size)
{
  blinkLED();
  Screen.print(1, payLoad, true);
}

//-----------------------------------------------------------------------------------------------------------------
static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }

  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  parseTwinMessage(updateState, temp);
  free(temp);
}

//-----------------------------------------------------------------------------------------------------------------
static int DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature, humidity and pressure data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature, humidity and pressure data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch
//-----------------------------------------------------------------------------------------------------------------
void setup()
{
  // put your setup code here, to run once:
  Screen.init();
  Screen.print(0, "IoT DevKit");
  Screen.print(1, "Initializing...");
  delay(500);
  
  Screen.print(2, " > Serial");
  Serial.begin(115200);

  // Initialize the WiFi module
  Screen.print(3, " > WiFi");
  hasWifi = false;
  InitWiFi();
  if (!hasWifi)
  {
    return;
  }

  LogTrace("HappyPathSetup", NULL);

  // Initialize LEDs
  Screen.print(2, " > Inet LEDs");
  delay(500);
  blinkLED();
  
  Screen.print(2, " > Init Sensors");
  delay(500);
  SensorInit();

  Screen.print(3, " > IoT Hub");
  delay(500);
  
  DevKitMQTTClient_Init(true);
  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "IoT-DevKit-MXChip");
  DevKitMQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  DevKitMQTTClient_SetMessageCallback(MessageCallback);
  DevKitMQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  DevKitMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

  send_interval_ms = SystemTickCounterRead();
}

//-----------------------------------------------------------------------------------------------------------------
void loop()
{
  // put your main code here, to run repeatedly:
  if (hasWifi)
  {
    if (messageSending && (int)(SystemTickCounterRead() - send_interval_ms) >= getInterval())
    {
      // Send temperature data
      char messagePayload[MESSAGE_MAX_LEN];

      bool temperatureAlert = readMessage(messageCount, messagePayload, &temperature, &humidity);
      EVENT_INSTANCE *message = DevKitMQTTClient_Event_Generate(messagePayload, MESSAGE);
      DevKitMQTTClient_Event_AddProp(message, "temperatureAlert", temperatureAlert ? "true" : "false");
      DevKitMQTTClient_SendEventInstance(message);
      
      send_interval_ms = SystemTickCounterRead();
    }
    else
    {
      DevKitMQTTClient_Check();
    }
  }
  delay(1000);  // Every 1 second delay
}
