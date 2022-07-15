// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.

#include "AzureIotHub.h"
#include "Arduino.h"
#include "HTS221Sensor.h"  //Humidity and Temperature
#include "LPS22HBSensor.h" //Pressure
#include "LIS2MDLSensor.h" //Magnetometer
#include "LSM6DSLSensor.h" //Accelerometer and Gyroscope
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"

#define RGB_LED_BRIGHTNESS 32

// Peripherals
DevI2C *i2c;
HTS221Sensor *tempSensor;
LPS22HBSensor *pressureSensor;
LIS2MDLSensor *magneticSensor;
LSM6DSLSensor *accelgyroSensor;
static RGB_LED rgbLed;

// Temperature sensor variables
static unsigned char tempSensorId;
static float temperature;
static float humidity;
static float pressure;

// Accelerometer, pedometer variables
static unsigned char accelGyroId;
static int stepCount;
static int xAxesData[3];
static int gAxesData[3];
// int32_t axes[3];
// int16_t raws[3];

// Magnetic variables
static unsigned char magSensorId;
static int mAxes[3];

// Message to Cloud variables
static int interval = INTERVAL;

//-----------------------------------------------------------------------------------------------------------------
int getInterval()
{
    return interval;
}

//-----------------------------------------------------------------------------------------------------------------
void blinkLED()
{
    rgbLed.turnOff();
    rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
    delay(500);
    rgbLed.turnOff();
}

//-----------------------------------------------------------------------------------------------------------------
void blinkSendConfirmation()
{
    rgbLed.turnOff();
    rgbLed.setColor(0, 0, RGB_LED_BRIGHTNESS);
    delay(500);
    rgbLed.turnOff();
}

//-----------------------------------------------------------------------------------------------------------------
void parseTwinMessage(DEVICE_TWIN_UPDATE_STATE updateState, const char *message)
{
    JSON_Value *root_value;
    root_value = json_parse_string(message);
    if (json_value_get_type(root_value) != JSONObject)
    {
        if (root_value != NULL)
        {
            json_value_free(root_value);
        }
        LogError("parse %s failed", message);
        return;
    }
    JSON_Object *root_object = json_value_get_object(root_value);

    double val = 0;
    if (updateState == DEVICE_TWIN_UPDATE_COMPLETE)
    {
        JSON_Object *desired_object = json_object_get_object(root_object, "desired");
        if (desired_object != NULL)
        {
            val = json_object_get_number(desired_object, "interval");
        }
    }
    else
    {
        val = json_object_get_number(root_object, "interval");
    }
    if (val > 500)
    {
        interval = (int)val;
        LogInfo(">>>Device twin updated: set interval to %d", interval);
    }
    json_value_free(root_value);
}

//-----------------------------------------------------------------------------------------------------------------
void SensorInit()
{
    // instantiate a helper class of type DevI2C which will handle the details of I2C peripherals. We also need to pass 2 parameters to the instantiation of this helper class which correspond to GPIO pins 18 and 17 respectively (D14=GPIO_18, D15=GPIO_17).
    i2c = new DevI2C(D14, D15);

    // Initialize the temperature sensor
    Screen.print(3, " > Temperature sensor");
	delay(500);
    tempSensor = new HTS221Sensor(*i2c);
    tempSensor->init(NULL);

    // Initialize the pressure sensor
    Screen.print(3, " > Pressure sensor");
	delay(500);
    pressureSensor = new LPS22HBSensor(*i2c);
    pressureSensor->init(NULL);

    // Initialize the magnetic sensor
    Screen.print(3, " > Magnetic sensor");
    delay(500);
    magneticSensor = new LIS2MDLSensor(*i2c);
    magneticSensor->init(NULL);

    // Initialize the motion sensor
    Screen.print(3, " > Motion sensor");
	delay(500);
    accelgyroSensor = new LSM6DSLSensor(*i2c, D4, D5);
    accelgyroSensor->init(NULL);
    accelgyroSensor->enableAccelerator();
    accelgyroSensor->enableGyroscope();
    accelgyroSensor->enablePedometer();
    accelgyroSensor->setPedometerThreshold(LSM6DSL_PEDOMETER_THRESHOLD_MID_LOW);

    stepCount = 0;

    humidity = -1;
    temperature = -1000;
    pressure = 0;
}

//-----------------------------------------------------------------------------------------------------------------
float readTemperature()
{
    tempSensor->reset();

    float temperature = 0;
    tempSensor->getTemperature(&temperature);

    return temperature;
}

//-----------------------------------------------------------------------------------------------------------------
float readHumidity()
{
    tempSensor->reset();

    float humidity = 0;
    tempSensor->getHumidity(&humidity);

    return humidity;
}

//-----------------------------------------------------------------------------------------------------------------
float readPressure()
{
    float pressure = 0;
    pressureSensor->getPressure(&pressure);
    return pressure;
}

//-----------------------------------------------------------------------------------------------------------------
void readAccelerometer()
{
    // Read accelerometer sensor
    accelgyroSensor->getXAxes(xAxesData);
    Serial.printf("Accelerometer X Axes: x=%d, y=%d, z=%d\n", xAxesData[0], xAxesData[1], xAxesData[2]);
}

//-----------------------------------------------------------------------------------------------------------------
void readGyroscope()
{
    // Read gyroscope sensor
    accelgyroSensor->getGAxes(gAxesData);
    Serial.printf("Gyroscope G Axes: x=%d, y=%d, z=%d\n", gAxesData[0], gAxesData[1], gAxesData[2]);
}

//-----------------------------------------------------------------------------------------------------------------
void readMagnetometer()
{
    // Read magnetic sensor
    magneticSensor->readId(&magSensorId);
    magneticSensor->getMAxes(mAxes);
    Serial.printf("magSensorId: %d\n", magSensorId);
    Serial.printf("Magnetometer Axes: x=%d, y=%d, z=%d\n", mAxes[0], mAxes[1], mAxes[2]);
}

//-----------------------------------------------------------------------------------------------------------------
int readPedometer()
{
    int stepCount = 0;
    accelgyroSensor->getStepCounter(&stepCount);
    return stepCount;
}

//-----------------------------------------------------------------------------------------------------------------
bool readMessage(int messageId, char *payload, float *temperatureValue, float *humidityValue)
{
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    char *serialized_string = NULL;

    json_object_set_number(root_object, "messageId", messageId);

    // Obtain values
    float t = readTemperature();
    float h = readHumidity();
    float p = readPressure();
	readGyroscope();
    readAccelerometer();
	readMagnetometer();
    readPedometer();
	
    bool temperatureAlert = false;
	// Set Temp JSON
    if (t != temperature)
    {
        temperature = t;
        *temperatureValue = t;
        json_object_set_number(root_object, "temperature", temperature);
    }
    if (temperature > TEMPERATURE_ALERT)
    {
        temperatureAlert = true;
    }

    // Set Humidity json
    if (h != humidity)
    {
        humidity = h;
        *humidityValue = h;
        json_object_set_number(root_object, "humidity", humidity);
    }

    // Set Pressure JSON
    if (p != pressure)
    {
        pressure = p;
        json_object_set_number(root_object, "pressure", pressure);
    }
	// Set gyro axes
    json_object_set_number(root_object, "accelX", xAxesData[0]);
    json_object_set_number(root_object, "accelY", xAxesData[1]);
    json_object_set_number(root_object, "accelZ", xAxesData[2]);

    // Set gyro axes
    json_object_set_number(root_object, "gccelX", gAxesData[0]);
    json_object_set_number(root_object, "gccelY", gAxesData[1]);
    json_object_set_number(root_object, "gccelZ", gAxesData[2]);
  
    // Set mag axes
    json_object_set_number(root_object, "magX", mAxes[0]);
    json_object_set_number(root_object, "magY", mAxes[1]);
    json_object_set_number(root_object, "magZ", mAxes[2]);

    serialized_string = json_serialize_to_string_pretty(root_value);

    snprintf(payload, MESSAGE_MAX_LEN, "%s", serialized_string);
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
	
    return temperatureAlert;
}

//-----------------------------------------------------------------------------------------------------------------
#if (DEVKIT_SDK_VERSION >= 10602)
void __sys_setup(void)
{
    // Enable Web Server for system configuration when system enter AP mode
    EnableSystemWeb(WEB_SETTING_IOT_DEVICE_CONN_STRING);
}
#endif