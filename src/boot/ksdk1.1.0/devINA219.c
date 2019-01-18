













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


extern volatile WarpI2CDeviceState	deviceINA219State;
extern volatile uint32_t		gWarpI2cBaudRateKbps;



void
initINA219(WarpI2CDeviceState volatile * deviceStatePointer)
{
	deviceStatePointer->i2cAddress  = 0x40;
	deviceStatePointer->signalType  = (kWarpTypeMaskCurrent);

	return;

}

WarpStatus
readSensorRegisterINA219(uint8_t deviceRegister)
{

//	SEGGER_RTT_printf(0, "\n\rReachedWithinReadSensorRegisterINA219");

	uint8_t cmdBuf[1]	= {0x00};
	uint8_t txBuf[2]	= {0xFF, 0xFF};
	i2c_status_t		returnValue;


	i2c_device_t slave =
	{
		.address = deviceINA219State.i2cAddress,
		.baudRate_kbps = gWarpI2cBaudRateKbps
	};

// Configuration
	returnValue = I2C_DRV_MasterSendDataBlocking(
							0,
							&slave,
							cmdBuf,
							1,
							txBuf,
							2,
							100
							);

if(returnValue == kStatus_I2C_Success){
//	SEGGER_RTT_printf(0, "\n\rSuccessfullyConfigured");
} else {
//	SEGGER_RTT_printf(0, "\n\rUnsuccessfulConfiguration");
}

// Calibration
	txBuf[0] = 0xA0;
	txBuf[1] = 0x00;
	cmdBuf[0] = 0x05; //from manual

	returnValue = I2C_DRV_MasterSendDataBlocking(
							0,
							&slave,
							cmdBuf,
							1,
							txBuf,
							2,
							100
							);

if(returnValue == kStatus_I2C_Success){
//	SEGGER_RTT_printf(0, "\n\rSuccessfullyCalibrated");
} else {
//	SEGGER_RTT_printf(0, "\n\rUnsuccessfulCalibration");
}

// write to sensor to tell it what register we want to read from
	txBuf[0] = 0x01;
	txBuf[1] = 0x01;
	cmdBuf[0] = 0x04;

	returnValue = I2C_DRV_MasterSendDataBlocking(
							0,
							&slave,
							cmdBuf,
							1,
							txBuf,
							0,
							100
							);

if(returnValue == kStatus_I2C_Success){
//	SEGGER_RTT_printf(0, "\n\rSuccessfullyWroteRegisterNumber");
} else {
//	SEGGER_RTT_printf(0, "\n\rFailedToWriteRegisterNumber");
}

// read 1000 values
	for(int i=0; i<1000; i++) {
		returnValue = I2C_DRV_MasterReceiveDataBlocking(
								0,
								&slave,
								NULL,
								0,
								(uint8_t *)deviceINA219State.i2cBuffer,
								2,
								500
								);



	if (returnValue == kStatus_I2C_Success)
	{
		int reading;
		reading = (deviceINA219State.i2cBuffer[0]<<8)|(deviceINA219State.i2cBuffer[1]);
		SEGGER_RTT_printf(0, "\n\r%d", reading);
	}
	else
	{
		return kWarpStatusDeviceCommunicationFailed;
	}

	}

return kWarpStatusOK;
}
