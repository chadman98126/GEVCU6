#include "CANIODevice.h"


CANIODevice::CANIODevice(void)
{
	numDigitalOutputs = 0;
	numAnalogOutputs = 0;
	numDigitalInputs = 0;
	numAnalogInputs = 0;
}

void CANIODevice::setup()
{
	Device::setup();
}

void CANIODevice::tearDown()
{
	//Device::tearDown();
}

DeviceType CANIODevice::getType()
{
	return DEVICE_IO;
}

void CANIODevice::handleCanFrame(CAN_FRAME *frame)
{
}

void CANIODevice::handleMessage(uint32_t msg, void* data)
{
	Device::handleMessage(msg, data);
}

int CANIODevice::getDigitalOutputCount()
{
	return numDigitalOutputs;
}

int CANIODevice::getAnalogOutputCount()
{
	return numAnalogOutputs;
}

int CANIODevice::getDigitalInputCount()
{
	return numDigitalInputs;
}

int CANIODevice::getAnalogInputCount()
{
	return numAnalogInputs;
}

//a bunch of do nothing implementations of these functions so derived classes that don't support certain
//I/O types and modes can just ignore them

void CANIODevice::setDigitalOutput(int which, bool hi)
{
	//do nothing if this version gets called
}

bool CANIODevice::getDigitalOutput(int which)
{
	return false;
}

void CANIODevice::setAnalogOutput(int which, int value)
{
}

int16_t CANIODevice::getAnalogOutput(int which)
{
	return 0;
}

bool CANIODevice::getDigitalInput(int which)
{
	return false;
}

int16_t CANIODevice::getAnalogInput(int which)
{
	return 0;
}

void CANIODevice::setLatchingMode(int which, LatchModes::LATCHMODE mode)
{
}

void CANIODevice::unlockLatch(int which)
{
}