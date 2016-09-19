/*
 * sys_io.cpp
 *
 * Handles the low level details of system I/O
 *
Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

some portions based on code credited as:
Arduino Due ADC->DMA->USB 1MSPS
by stimmer

*/

#include "sys_io.h"
#include "CANIODevice.h"

#undef HID_ENABLED

SPISettings spi_settings(1000000, MSBFIRST, SPI_MODE3);    

SystemIO::SystemIO()
{
    useRawADC = false;
    useSPIADC = false;
    
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        extendedDigitalOut[i].device = NULL;
        extendedDigitalIn[i].device = NULL;
        extendedAnalogOut[i].device = NULL;
        extendedAnalogIn[i].device = NULL;
    }
    
    numDigIn = NUM_DIGITAL;
    numDigOut = NUM_OUTPUT;
    numAnaIn = NUM_ANALOG;
    numAnaOut = 0;
}

void SystemIO::setup_ADC_params()
{
    int i;
    //requires the value to be contiguous in memory
    for (i = 0; i < 7; i++) {
        sysPrefs->read(EESYS_ADC0_GAIN + 4*i, &adc_comp[i].gain);
        sysPrefs->read(EESYS_ADC0_OFFSET + 4*i, &adc_comp[i].offset);
        Logger::debug("ADC:%d GAIN: %d Offset: %d", i, adc_comp[i].gain, adc_comp[i].offset);
        if (i < NUM_ANALOG) {
            for (int j = 0; j < NumADCSamples; j++) adc_buffer[i][j] = 0;
            adc_pointer[i] = 0;
            adc_values[i] = 0;
            adc_out_vals[i] = 0;
        }
    }
}

void SystemIO::setSystemType(SystemType systemType) {
    if (systemType >= GEVCU1 && systemType <= GEVCU6)
    {
        sysType = systemType;
        sysPrefs->write(EESYS_SYSTEM_TYPE, (uint8_t)sysType);
    }
}

SystemType SystemIO::getSystemType() {
    return sysType;
}

/*
Initialize DMA driven ADC and read in gain/offset for each channel
*/
void SystemIO::setup() {
    int i;
    
    //the first order of business is to figure out what hardware we are running on and fill in
    //the pin tables.

    uint8_t rawadc;
    sysPrefs->read(EESYS_RAWADC, &rawadc);
    if (rawadc != 0) {
        useRawADC = true;
        Logger::info("Using raw ADC mode");
    }
    else useRawADC = false;

    NumADCSamples = 64;

    sysPrefs->read(EESYS_SYSTEM_TYPE, (uint8_t *) &sysType);
    if (sysType == GEVCU2) {
        Logger::info("Running on GEVCU2/DUED hardware.");
        dig[0]=9;
        dig[1]=11;
        dig[2]=12;
        dig[3]=13;
        adc[0][0] = 1;
        adc[0][1] = 0;
        adc[1][0] = 3;
        adc[1][1] = 2;
        adc[2][0] = 5;
        adc[2][1] = 4;
        adc[3][0] = 7;
        adc[3][1] = 6;
        out[0] = 52;
        out[1] = 22;
        out[2] = 48;
        out[3] = 32;
        out[4] = 255;
        out[5] = 255;
        out[6] = 255;
        out[7] = 255;
        NumADCSamples = 32;
    } else if (sysType == GEVCU3) {
        Logger::info("Running on GEVCU3 hardware");
        dig[0]=48;
        dig[1]=49;
        dig[2]=50;
        dig[3]=51;
        adc[0][0] = 3;
        adc[0][1] = 255;
        adc[1][0] = 2;
        adc[1][1] = 255;
        adc[2][0] = 1;
        adc[2][1] = 255;
        adc[3][0] = 0;
        adc[3][1] = 255;
        out[0] = 9;
        out[1] = 8;
        out[2] = 7;
        out[3] = 6;
        out[4] = 255;
        out[5] = 255;
        out[6] = 255;
        out[7] = 255;
        useRawADC = true; //this board does require raw adc so force it.
    } else if (sysType == GEVCU4 || sysType == GEVCU5) {
        Logger::info("Running on GEVCU 4.x/5.x hardware");
        dig[0]=48;
        dig[1]=49;
        dig[2]=50;
        dig[3]=51;
        adc[0][0] = 3;
        adc[0][1] = 255;
        adc[1][0] = 2;
        adc[1][1] = 255;
        adc[2][0] = 1;
        adc[2][1] = 255;
        adc[3][0] = 0;
        adc[3][1] = 255;
        out[0] = 4;
        out[1] = 5;
        out[2] = 6;
        out[3] = 7;
        out[4] = 2;
        out[5] = 3;
        out[6] = 8;
        out[7] = 9;
        useRawADC = true; //this board does require raw adc so force it.
    } else if (sysType == GEVCU6) {
        Logger::info("Running on GEVCU 6.2 hardware");
        dig[0]=48;
        dig[1]=49;
        dig[2]=50;
        dig[3]=51;
        adc[0][0] = 255;
        adc[0][1] = 255; //doesn't use SAM3X analog
        adc[1][0] = 255;
        adc[1][1] = 255;
        adc[2][0] = 255;
        adc[2][1] = 255;
        adc[3][0] = 255;
        adc[3][1] = 255;
        out[0] = 4;
        out[1] = 5;
        out[2] = 6;
        out[3] = 7;
        out[4] = 2;
        out[5] = 3;
        out[6] = 8;
        out[7] = 9;
        useRawADC = false;
        useSPIADC = true;
        pinMode(26, OUTPUT); //Chip Select for first ADC chip
        pinMode(28, OUTPUT); //Chip select for second ADC chip
        pinMode(30, OUTPUT); //chip select for third ADC chip
        digitalWrite(26, HIGH);
        digitalWrite(28, HIGH);
        digitalWrite(30, HIGH);
        SPI.begin();
        pinMode(32, INPUT); //Data Ready indicator

        SPI.begin(); //sets up with default 4Mhz, MSB first
    } else {
        Logger::info("Running on legacy hardware?");
        dig[0]=11;
        dig[1]=9;
        dig[2]=13;
        dig[3]=12;
        adc[0][0] = 1;
        adc[0][1] = 0;
        adc[1][0] = 2;
        adc[1][1] = 3;
        adc[2][0] = 4;
        adc[2][1] = 5;
        adc[3][0] = 7;
        adc[3][1] = 6;
        out[0] = 52;
        out[1] = 22;
        out[2] = 48;
        out[3] = 32;
        out[4] = 255;
        out[5] = 255;
        out[6] = 255;
        out[7] = 255;
        NumADCSamples = 32;
    }

    for (i = 0; i < NUM_DIGITAL; i++) pinMode(dig[i], INPUT);
    for (i = 0; i < NUM_OUTPUT; i++) {
        if (out[i] != 255) {
            pinMode(out[i], OUTPUT);
            digitalWrite(out[i], LOW);
        }
    }    

    setup_ADC_params();

    if (useSPIADC) setupSPIADC();
    else setupFastADC();
}

void SystemIO::installExtendedIO(CANIODevice *device)
{
    bool found = false;
    int counter;
    
    //Logger::debug("Before adding extended IO counts are DI:%i DO:%i AI:%i AO:%i", numDigIn, numDigOut, numAnaIn, numAnaOut);
    //Logger::debug("Num Analog Inputs: %i", device->getAnalogInputCount());
    //Logger::debug("Num Analog Outputs: %i", device->getAnalogOutputCount());
    //Logger::debug("Num Digital Inputs: %i", device->getDigitalInputCount());
    //Logger::debug("Num Digital Outputs: %i", device->getDigitalOutputCount());
   
    if (device->getAnalogInputCount() > 0)
    {
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedAnalogIn[counter].device == NULL)
            {
                for (int i = 0; i < device->getAnalogInputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedAnalogIn[counter + i].device = device;
                    extendedAnalogIn[counter + i].localOffset = i;
                }
                break;
            }
        }
    }
    
    if (device->getAnalogOutputCount() > 0)
    {
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedAnalogOut[counter].device == NULL)
            {
                for (int i = 0; i < device->getAnalogOutputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedAnalogOut[counter + i].device = device;
                    extendedAnalogOut[counter + i].localOffset = i;
                }
                break;
            }
        }
    }

    if (device->getDigitalOutputCount() > 0)
    {
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedDigitalOut[counter].device == NULL)
            {
                for (int i = 0; i < device->getDigitalOutputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedDigitalOut[counter + i].device = device;
                    extendedDigitalOut[counter + i].localOffset = i;
                }
                break;
            }
        }
    }

    if (device->getDigitalInputCount() > 0)
    {
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedDigitalIn[counter].device == NULL)
            {
                for (int i = 0; i < device->getDigitalInputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedDigitalIn[counter + i].device = device;
                    extendedDigitalIn[counter + i].localOffset = i;
                }
                break;
            }
        }
    }
    
    int numDI = NUM_DIGITAL; 
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedDigitalIn[i].device != NULL) numDI++;
        else break;
    }
    numDigIn = numDI;
    
    int numDO = NUM_OUTPUT;
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedDigitalOut[i].device != NULL) numDO++;
        else break;
    }
    numDigOut = numDO;

    int numAI = NUM_ANALOG; 
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedAnalogIn[i].device != NULL) numAI++;
        else break;
    }
    numAnaIn = numAI;

    int numAO = 0; //GEVCU has no real analog outputs - there are PWM but they're on the digital outputs
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedDigitalIn[i].device != NULL) numAO++;
        else break;
    }
    numAnaOut = numAO;
    Logger::debug("After added extended IO the counts are DI:%i DO:%i AI:%i AO:%i", numDigIn, numDigOut, numAnaIn, numAnaOut);
}

int SystemIO::numDigitalInputs()
{
    return numDigIn;
}

int SystemIO::numDigitalOutputs()
{
    return numDigOut;
}

int SystemIO::numAnalogInputs()
{
    return numAnaIn;
}

int SystemIO::numAnalogOutputs()
{
    return numAnaOut;
}


/*
Some of the boards are differential and thus require subtracting one ADC from another to obtain the true value. This function
handles that case. It also applies gain and offset
*/
uint16_t SystemIO::getDiffADC(uint8_t which) {
    uint32_t low, high;

    low = adc_values[adc[which][0]];
    high = adc_values[adc[which][1]];

    if (low < high) {

        //first remove the bias to bring us back to where it rests at zero input volts

        if (low >= adc_comp[which].offset) low -= adc_comp[which].offset;
        else low = 0;
        if (high >= adc_comp[which].offset) high -= adc_comp[which].offset;
        else high = 0;

        //gain multiplier is 1024 for 1 to 1 gain, less for lower gain, more for higher.
        low *= adc_comp[which].gain;
        low = low >> 10; //divide by 1024 again to correct for gain multiplier
        high *= adc_comp[which].gain;
        high = high >> 10;

        //Lastly, the input scheme is basically differential so we have to subtract
        //low from high to get the actual value
        high = high - low;
    }
    else high = 0;

    if (high > 4096) high = 0; //if it somehow got wrapped anyway then set it back to zero

    return high;
}

/*
Exactly like the previous function but for non-differential boards (all the non-prototype boards are non-differential)
*/
uint16_t SystemIO::getRawADC(uint8_t which) {
    uint32_t val;

    if (getSystemType() < GEVCU6)
    {
        val = adc_values[adc[which][0]];

        //first remove the bias to bring us back to where it rests at zero input volts

        if (val >= adc_comp[which].offset) val -= adc_comp[which].offset;
        else val = 0;

        //gain multiplier is 1024 for 1 to 1 gain, less for lower gain, more for higher.
        val *= adc_comp[which].gain;
        val = val >> 10; //divide by 1024 again to correct for gain multiplier

        if (val > 4096) val = 0; //if it somehow got wrapped anyway then set it back to zero
    }
    else
    {
        int32_t valu;
        //first 4 analog readings must match old methods
        if (which < 2)
        {
            valu = getSPIADCReading(CS1, (which & 1) + 1);
        }
        else if (which < 4) valu = getSPIADCReading(CS2, (which & 1) + 1);
        //the next three are new though. 4 = current sensor, 5 = pack high (ref to mid), 6 = pack low (ref to mid)
        else if (which == 4) valu = getSPIADCReading(CS1, 0);
        else if (which == 5) valu = getSPIADCReading(CS3, 1);
        else if (which == 6) valu = getSPIADCReading(CS3, 2);
        val = valu >> 8; //cut reading down to 16 bit value
    }
    return val;
}

/*
get value of one of the 4 analog inputs
On GEVCU5 or less
Uses a special buffer which has smoothed and corrected ADC values. This call is very fast
because the actua1 work is done via DMA and then a separate polled step.
On GEVCU6.2 or higher
Gets reading over SPI which is still pretty fast. The SPI connected chip is 24 bit
but too much of the code for GEVCU uses 16 bit integers for storage so the 24 bit values returned
are knocked down to 16 bit values before being passed along.
*/
uint16_t SystemIO::getAnalogIn(uint8_t which) {
    int base;
    if (which > numAnaIn) {
        return 0;
    }
    
    if (which < NUM_ANALOG) {
        if (getSystemType() == GEVCU6)
        {
            int32_t valu;
            //first 4 analog readings must match old methods

            if (which < 2)
            {
                valu = getSPIADCReading(CS1, (which & 1) + 1);
            }
            else if (which < 4) valu = getSPIADCReading(CS2, (which & 1) + 1);
            //the next three are new though. 4 = current sensor, 5 = pack high (ref to mid), 6 = pack low (ref to mid)
            else if (which == 4) valu = getSPIADCReading(CS1, 0);
            else if (which == 5) valu = getSPIADCReading(CS3, 1);
            else if (which == 6) valu = getSPIADCReading(CS3, 2);
            valu >>= 8;
            valu -= adc_comp[which].offset;
            valu = (valu * adc_comp[which].gain) / 1024;
            return valu;
        }
        else return adc_out_vals[which];
    }
    else //the return makes this superfluous...
    {        
        //handle an extended I/O call
        CANIODevice *dev = extendedAnalogIn[which - NUM_ANALOG].device;
        if (dev) return dev->getAnalogInput(extendedAnalogIn[which - NUM_ANALOG].localOffset);
        return 0;
    }
    return 0; //if it falls through and nothing could provide the answer then return 0
}

boolean SystemIO::setAnalogOut(uint8_t which, int32_t level)
{
    if (which >= numAnaOut) return false;
    CANIODevice *dev;
    dev = extendedAnalogOut[which].device;
    if (dev) dev->setAnalogOutput(extendedAnalogOut[which].localOffset, level);    
}

int32_t SystemIO::getAnalogOut(uint8_t which)
{
    if (which >= numAnaOut) return 0;
    CANIODevice *dev;
    dev = extendedAnalogOut[which].device;
    if (dev) return dev->getAnalogOutput(extendedAnalogOut[which].localOffset);    
}


//the new pack voltage and current functions however, being new, don't have legacy problems so they're 24 bit ADC.
int32_t SystemIO::getCurrentReading()
{
    int32_t valu;
    valu = getSPIADCReading(CS1, 0);
    valu -= (adc_comp[6].offset * 256);
    valu = valu >> 3;
    valu = (valu * adc_comp[6].gain) / 128;
    return valu;
}

int32_t SystemIO::getPackHighReading()
{
    int32_t valu;
    valu = getSPIADCReading(CS3, 1);
    valu -= (adc_comp[4].offset * 256);
    valu = valu >> 3;
    valu = (valu * adc_comp[4].gain) / 128;
    return valu;
}

int32_t SystemIO::getPackLowReading()
{
    int32_t valu;
    valu = getSPIADCReading(CS3, 2);
    valu -= (adc_comp[5].offset * 256);
    valu = valu >> 3;
    valu = (valu * adc_comp[5].gain) / 128;
    return valu;
}

//get value of one of the 4 digital inputs
boolean SystemIO::getDigitalIn(uint8_t which) {
    if (which >= numDigIn) return false;
    
    if (which < NUM_DIGITAL) return !(digitalRead(dig[which]));
    else
    {
        CANIODevice *dev;
        dev = extendedDigitalIn[which - NUM_DIGITAL].device;
        if (dev) return dev->getDigitalInput(extendedDigitalIn[which - NUM_DIGITAL].localOffset);
    }
}

//set output high or not
void SystemIO::setDigitalOutput(uint8_t which, boolean active) {
    if (which >= numDigOut) return;
    
    if (which < NUM_OUTPUT)
    {
        if (out[which] == 255) return;
        if (active)
            digitalWrite(out[which], HIGH);
        else digitalWrite(out[which], LOW);
    }
    else
    {
        CANIODevice *dev;
        dev = extendedDigitalOut[which - NUM_OUTPUT].device;
        if (dev) return dev->setDigitalOutput(extendedDigitalOut[which - NUM_OUTPUT].localOffset, active);
    }
}

//get current value of output state (high?)
boolean SystemIO::getDigitalOutput(uint8_t which) {
    if (which >= numDigOut) return false;
    
    if (which < NUM_OUTPUT)
    {
        if (out[which] == 255) return false;
        return digitalRead(out[which]);
    }
    else
    {
        CANIODevice *dev;
        dev = extendedDigitalOut[which - NUM_OUTPUT].device;
        if (dev) return dev->getDigitalOutput(extendedDigitalOut[which - NUM_OUTPUT].localOffset);
    }
}

/*
 * Move DMA pointers to next buffer.
 */
uint32_t SystemIO::getNextADCBuffer() {
    bufn = (bufn + 1) & 3;
    return (uint32_t) adc_buffer[bufn];
}

/*
When the ADC reads in the programmed # of readings it will do two things:
1. It loads the next buffer and buffer size into current buffer and size
2. It sends this interrupt
This interrupt then loads the "next" fields with the proper values. This is
done with a four position buffer. In this way the ADC is constantly sampling
and this happens virtually for free. It all happens in the background with
minimal CPU overhead.
*/
void ADC_Handler() {    // move DMA pointers to next buffer
    int f = ADC->ADC_ISR;

    if (f & (1 << 27)) { //receive counter end of buffer
        ADC->ADC_RNPR = systemIO.getNextADCBuffer();
        ADC->ADC_RNCR = 256;
    }        
}

bool SystemIO::setupSPIADC()
{
    bool chip1OK = false, chip2OK = false, chip3OK = false;
    byte result;
    //ADC chips use this format for SPI command byte: 0-1 = reserved set as 0, 2 = read en (0=write), 3-7 = register address
    //delay(100); //yeah, probably evil to do...

    SPI.beginTransaction(spi_settings);
    SPI.transfer(0);
    SPI.endTransaction();

    Logger::info("Trying to wait ADC1 as ready");
    for (int i = 0; i < 10; i++)
    {
        SPI.beginTransaction(spi_settings);
        digitalWrite(CS1, LOW); //select first ADC chip
        SPI.transfer(ADE7913_READ | ADE7913_STATUS0);
        result = SPI.transfer(0);
        digitalWrite(CS1, HIGH);
        SPI.endTransaction();
        if (result & 1)  //not ready yet
        {
            delay(6);
            //SerialUSB.println(result);
        }
        else
        {
            chip1OK = true;
            break;
        }
    }
    //SerialUSB.println();

    if (!chip1OK) return false;

    Logger::info("ADC1 is ready. Trying to enable clock out");

    //Now enable the CLKOUT function on first unit so that the other two will wake up
    SPI.beginTransaction(spi_settings);
    digitalWrite(CS1, LOW);
    SPI.transfer(ADE7913_WRITE |  ADE7913_CONFIG);
    SPI.transfer(1 | 2 << 4); //Set clock out enable and ADC_FREQ to 2khz
    digitalWrite(CS1, HIGH);
    SPI.endTransaction();

    //Now we've got to wait 100ms plus around 20ms for the other chips to come up.
    delay(110);
    for (int i = 0; i < 10; i++)
    {
        SPI.beginTransaction(spi_settings);
        digitalWrite(CS2, LOW); //select second ADC chip
        SPI.transfer(ADE7913_READ | ADE7913_STATUS0);
        result = SPI.transfer(0);
        digitalWrite(CS2, HIGH);
        SPI.endTransaction();
        if (result & 1)  //not ready yet
        {
            delay(6);
        }
        else
        {
            chip2OK = true;
            //break;
        }
        SPI.beginTransaction(spi_settings);
        digitalWrite(CS3, LOW); //select third ADC chip
        SPI.transfer(ADE7913_READ | ADE7913_STATUS0);
        result = SPI.transfer(0);
        digitalWrite(CS3, HIGH);
        SPI.endTransaction();
        if (result & 1)  //not ready yet
        {
            delay(6);
        }
        else
        {
            chip3OK = true;
            //break;
        }
        if (chip2OK && chip3OK) break;
    }

    if (!chip2OK || !chip3OK) return false;

    SPI.beginTransaction(spi_settings);
    digitalWrite(CS2, LOW);
    SPI.transfer(ADE7913_WRITE |  ADE7913_CONFIG);
    SPI.transfer(3 << 4 | 1 << 7); //Set ADC_FREQ to 1khz and lower bandwidth to 2khz
    digitalWrite(CS2, HIGH);
    SPI.endTransaction();
    SPI.beginTransaction(spi_settings);
    digitalWrite(CS3, LOW);
    SPI.transfer(ADE7913_WRITE |  ADE7913_CONFIG);
    SPI.transfer(3 << 4 | 1 << 7); //Set ADC_FREQ to 1khz and lower bandwidth to 2khz
    digitalWrite(CS3, HIGH);
    SPI.endTransaction();

    Logger::info("ADC chips 2 and 3 have been successfully started!");

    return true;
}

int32_t SystemIO::getSPIADCReading(int CS, int sensor)
{
    int32_t result;
    int32_t byt;
    //Logger::debug("SPI Read CS: %i Sensor: %i", CS, sensor);
    SPI.beginTransaction(spi_settings);
    digitalWrite(CS, LOW);
    if (sensor == 0) SPI.transfer(ADE7913_READ | ADE7913_AMP_READING);
    if (sensor == 1) SPI.transfer(ADE7913_READ | ADE7913_ADC1_READING);
    if (sensor == 2) SPI.transfer(ADE7913_READ | ADE7913_ADC2_READING);
    byt = SPI.transfer(0);
    result = (byt << 16);
    byt = SPI.transfer(0);
    result = result + (byt << 8);
    byt = SPI.transfer(0);
    result = result + byt;
    //now we've got the whole 24 bit value but it is a signed 24 bit value so we must sign extend
    if (result & (1 << 23)) result |= (255 << 24);
    digitalWrite(CS, HIGH);
    SPI.endTransaction();
    return result;
}

/*
 * adc is the adc port to calibrate, update if true will write the new value to EEPROM automatically
 */
bool SystemIO::calibrateADCOffset(int adc, bool update)
{
    int32_t accum = 0;
    for (int j = 0; j < 400; j++)
    {
        if (adc < 2)
        {
            accum += getSPIADCReading(CS1, (adc & 1) + 1);
        }
        else if (adc < 4) accum += getSPIADCReading(CS2, (adc & 1) + 1);
        //the next three are new though. 4 = current sensor, 5 = pack high (ref to mid), 6 = pack low (ref to mid)
        else if (adc == 4) accum += getSPIADCReading(CS1, 0);
        else if (adc == 5) accum += getSPIADCReading(CS3, 1);
        else if (adc == 6) accum += getSPIADCReading(CS3, 2);

        //normally one shouldn't call watchdog reset in multiple
        //places but this is a special case.
        watchdogReset();
        delay(7);
    }
    accum /= 400;
    if (update) sysPrefs->write(EESYS_ADC0_OFFSET + (4*adc), (uint16_t)(accum));    
    Logger::console("ADC %i offset is now %i", adc, accum);
}

/*
Setup the system to continuously read the proper ADC channels and use DMA to place the results into RAM
Testing to find a good batch of settings for how fast to do ADC readings. The relevant areas:
1. In the adc_init call it is possible to use something other than ADC_FREQ_MAX to slow down the ADC clock
2. ADC_MR has a clock divisor, start up time, settling time, tracking time, and transfer time. These can be adjusted
*/
void SystemIO::setupFastADC() {
    pmc_enable_periph_clk(ID_ADC);
    adc_init(ADC, SystemCoreClock, ADC_FREQ_MAX, ADC_STARTUP_FAST); //just about to change a bunch of these parameters with the next command

    /*
    The MCLK is 12MHz on our boards. The ADC can only run 1MHz so the prescaler must be at least 12x.
    The ADC should take Tracking+Transfer for each read when it is set to switch channels with each read

    Example:
    5+7 = 12 clocks per read 1M / 12 = 83333 reads per second. For newer boards there are 4 channels interleaved
    so, for each channel, the readings are 48uS apart. 64 of these readings are averaged together for a total of 3ms
    worth of ADC in each average. This is then averaged with the current value in the ADC buffer that is used for output.

    If, for instance, someone wanted to average over 6ms instead then the prescaler could be set to 24x instead.
    */
    ADC->ADC_MR = (1 << 7) //free running
                  + (5 << 8) //12x MCLK divider ((This value + 1) * 2) = divisor
                  + (1 << 16) //8 periods start up time (0=0clks, 1=8clks, 2=16clks, 3=24, 4=64, 5=80, 6=96, etc)
                  + (1 << 20) //settling time (0=3clks, 1=5clks, 2=9clks, 3=17clks)
                  + (4 << 24) //tracking time (Value + 1) clocks
                  + (2 << 28);//transfer time ((Value * 2) + 3) clocks

    if (useRawADC)
        ADC->ADC_CHER=0xF0; //enable A0-A3
    else ADC->ADC_CHER=0xFF; //enable A0-A7

    NVIC_EnableIRQ(ADC_IRQn);
    ADC->ADC_IDR=~(1<<27); //dont disable the ADC interrupt for rx end
    ADC->ADC_IER=1<<27; //do enable it
    ADC->ADC_RPR=(uint32_t)adc_buf[0];   // DMA buffer
    ADC->ADC_RCR=256; //# of samples to take
    ADC->ADC_RNPR=(uint32_t)adc_buf[1]; // next DMA buffer
    ADC->ADC_RNCR=256; //# of samples to take
    bufn=obufn=0;
    ADC->ADC_PTCR=1; //enable dma mode
    ADC->ADC_CR=2; //start conversions

    Logger::debug("Fast ADC Mode Enabled");
}

//polls	for the end of an adc conversion event. Then processe buffer to extract the averaged
//value. It takes this value and averages it with the existing value in an 8 position buffer
//which serves as a super fast place for other code to retrieve ADC values
// This is only used when RAWADC is not defined
void SystemIO::adcPoll() {
    if (getSystemType() > GEVCU5) return;
    if (obufn != bufn) {
        uint32_t tempbuff[8] = {0,0,0,0,0,0,0,0}; //make sure its zero'd

        //the eight or four enabled adcs are interleaved in the buffer
        //this is a somewhat unrolled for loop with no incrementer. it's odd but it works
        if (useRawADC) {
            for (int i = 0; i < 256;) {
                tempbuff[3] += adc_buf[obufn][i++];
                tempbuff[2] += adc_buf[obufn][i++];
                tempbuff[1] += adc_buf[obufn][i++];
                tempbuff[0] += adc_buf[obufn][i++];
            }
        }
        else {
            for (int i = 0; i < 256;) {
                tempbuff[7] += adc_buf[obufn][i++];
                tempbuff[6] += adc_buf[obufn][i++];
                tempbuff[5] += adc_buf[obufn][i++];
                tempbuff[4] += adc_buf[obufn][i++];
                tempbuff[3] += adc_buf[obufn][i++];
                tempbuff[2] += adc_buf[obufn][i++];
                tempbuff[1] += adc_buf[obufn][i++];
                tempbuff[0] += adc_buf[obufn][i++];
            }
        }

        //for (int i = 0; i < 256;i++) Logger::debug("%i - %i", i, adc_buf[obufn][i]);

        //now, all of the ADC values are summed over 32/64 readings. So, divide by 32/64 (shift by 5/6) to get the average
        //then add that to the old value we had stored and divide by two to average those. Lots of averaging going on.
        if (useRawADC) {
            for (int j = 0; j < 4; j++) {
                adc_values[j] += (tempbuff[j] >> 6);
                adc_values[j] = adc_values[j] >> 1;
            }
        }
        else {
            for (int j = 0; j < 8; j++) {
                adc_values[j] += (tempbuff[j] >> 5);
                adc_values[j] = adc_values[j] >> 1;
                //Logger::debug("A%i: %i", j, adc_values[j]);
            }
        }

        for (int i = 0; i < NUM_ANALOG; i++) {
            int val;
            if (useRawADC) val = getRawADC(i);
            else val = getDiffADC(i);
//			addNewADCVal(i, val);
//			adc_out_vals[i] = getADCAvg(i);
            adc_out_vals[i] = val;
        }

        obufn = bufn;
    }
}

SystemIO systemIO;


