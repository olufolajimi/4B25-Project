/*
	Authored 2016-2018. Phillip Stanley-Marbell.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>

#include "fsl_misc_utilities.h"
#include "fsl_device_registers.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_spi_master_driver.h"
#include "fsl_rtc_driver.h"
#include "fsl_clock_manager.h"
#include "fsl_power_manager.h"
#include "fsl_mcglite_hal.h"
#include "fsl_port_hal.h"

#include "gpio_pins.h"
#include "SEGGER_RTT.h"
#include "warp.h"


extern volatile WarpI2CDeviceState	deviceSI7021State;
extern volatile uint32_t		gWarpI2cBaudRateKbps;



void
initSI7021(const uint8_t i2cAddress, WarpI2CDeviceState volatile *  deviceStatePointer)
{
	deviceStatePointer->i2cAddress	= i2cAddress;
	deviceStatePointer->signalType	= (kWarpTypeMaskHumidity | kWarpTypeMaskTemperature);

	return;
}

WarpStatus
readSensorRegisterSI7021(uint8_t deviceRegister)
{
	uint8_t 	cmdBuf[2]	= {0xFF, 0xFF};
	i2c_status_t	returnValue;


	//
	//TODO: See SI7021 manual.
	//	1.	We should be passing in a 'command code' such as those in table 11
	//	2.	We first write command code, then read results
	//
	//	for now, just fix command code as 'read firmware version'
	//
	
	i2c_device_t slave =
	{
		.address = deviceSI7021State.i2cAddress,
		.baudRate_kbps = gWarpI2cBaudRateKbps
	};

	//TODO: for now, we fix command code as read first byte of serial number
	cmdBuf[0] = 0xFA;
	cmdBuf[1] = 0x0F;

	returnValue = I2C_DRV_MasterSendDataBlocking(
							0 /* I2C peripheral instance */,
							&slave,
							NULL,
							0,
							cmdBuf,
							2,//TODO: for now, we fix command code as two byte 'read firmware version' command
							500 /* timeout in milliseconds */);

	//SEGGER_RTT_printf(0, "\r\nI2C_DRV_MasterSendDataBlocking returned [%d] (set pointer)\n", returnValue);


	/*
	 *	See SI7021 manual, page 20: two reads will be nacked before third succeeds.
	 *	See similar thing in HDC1000 driver where we also send a NULL cmdBuf in
	 *	I2C_DRV_MasterReceiveDataBlocking.
	 */
	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0 /* I2C peripheral instance */,
							&slave,
							NULL,
							0,
							(uint8_t *)deviceSI7021State.i2cBuffer,
							2,
							500 /* timeout in milliseconds */);

	//SEGGER_RTT_printf(0, "\r\nI2C_DRV_MasterReceiveData returned [%d] (read register)\n", returnValue);

	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0 /* I2C peripheral instance */,
							&slave,
							NULL,
							0,
							(uint8_t *)deviceSI7021State.i2cBuffer,
							2,
							500 /* timeout in milliseconds */);

	//SEGGER_RTT_printf(0, "\r\nI2C_DRV_MasterReceiveData returned [%d] (read register)\n", returnValue);

	/*
	 *	Now, this two-byte read should succed according to page 20 of SI7021 manual:
	 */
	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0 /* I2C peripheral instance */,
							&slave,
							NULL,
							0,
							(uint8_t *)deviceSI7021State.i2cBuffer,
							2,
							500 /* timeout in milliseconds */);

	//SEGGER_RTT_printf(0, "\r\nI2C_DRV_MasterReceiveData returned [%d] (read register)\n", returnValue);

	// read humidity value
	cmdBuf[0] = 0xE5;

	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0,
							&slave,
							cmdBuf,
							1,
							(uint8_t *)deviceSI7021State.i2cBuffer,
							2,
							500
							);


	if (returnValue == kStatus_I2C_Success)
	{
		//SEGGER_RTT_printf(0, "\r[0x%02x]	0x%02x\n", cmdBuf[0], deviceMAG3110State.i2cBuffer[0]);
		int humidityReading;
		float humidityValue;
		humidityReading = (deviceSI7021State.i2cBuffer[0]<<8)|(deviceSI7021State.i2cBuffer[1]);
		humidityValue = (125*humidityReading/65536)-6;
		int intPart = (int)humidityValue;
		int decimalPart = (humidityValue - intPart)*100;
		SEGGER_RTT_printf(0,"\r\nRelative humidity: %d.%d percent", intPart, decimalPart);
	}
	else
	{
		//SEGGER_RTT_printf(0, kWarpConstantStringI2cFailure, cmdBuf[0], returnValue);

		return kWarpStatusDeviceCommunicationFailed;
	}

	// read temperature value taken during humidity reading
	cmdBuf[0] = 0xE0;

	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0,
							&slave,
							cmdBuf,
							1,
							(uint8_t *)deviceSI7021State.i2cBuffer,
							2,
							500
							);

	if(returnValue == kStatus_I2C_Success)
	{
		int temperatureReading;
		double temperatureValue;
		temperatureReading = (deviceSI7021State.i2cBuffer[0]<<8)|(deviceSI7021State.i2cBuffer[1]);
		temperatureValue = (175.72*temperatureReading/65536)-46.85;
		int intPart = (int)temperatureValue;
		int decimalPart = (temperatureValue - intPart)*100;
		SEGGER_RTT_printf(0, "\r\nTemperature: %d.%d degrees Celcius", intPart, decimalPart);
	}
	else
	{
		return kWarpStatusDeviceCommunicationFailed;
	}

	return kWarpStatusOK;
}
