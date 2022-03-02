#include "isl94208.h"
#include "config.h"

//Private functions
static uint8_t _GenerateMask(uint8_t length);
static uint16_t _ConvertADCtoMV(uint16_t adcval);

void ISL_Init(void){
    ISL_SetSpecificBits(ISL.ENABLE_FEAT_SET_WRITES, 1);
    ISL_SetSpecificBits(ISL.WKPOL, 1);
    ISL_SetSpecificBits(ISL.ENABLE_FEAT_SET_WRITES, 0);
}

uint8_t ISL_Read_Register(isl_reg_t reg){  //Allows easily retrieving an entire register. Ex. ISL_Read_Register(ISL_CONFIG_REG); result = ISL_RegData.Config
    I2C_ERROR_FLAGS |= I2C1_ReadMemory(ISL_ADDR, reg, &ISL_RegData[reg], 1);
    return ISL_RegData[reg];
}

void ISL_Write_Register(isl_reg_t reg, uint8_t wrdata){
     I2C_ERROR_FLAGS |= I2C1_WriteMemory(ISL_ADDR, reg, &wrdata, 1);
}



/* Sets specific bit in any register while preserving the other bits
 * When setting more than a single bit, bit_addr must be the location of the LEAST significant bit.
 * Example: A register 0xFF has content 11001111 and you want to set the zeros to ones (you want to set the value 0b11 in bits 5 and 4)
 * You would call ISL_SetSpecificBit((uint8_t {0xFF, 4, 0b11}, 2)
 * Meaning, you want to set the register 0xFF with a target location LSB of 4, a value of binary 11, which has a bit length of 2 bits.
 * This is because the value you are setting is shifted left by bit_addr.
 * Most of the time you'll just use something like ISL_SetSpecificBits(ISL.WKPOL, 1) or ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, 0b0110).
*/
void ISL_SetSpecificBits(const isl_locate_t params[3], uint8_t value){
    uint8_t reg_addr = params[REG_ADDRESS];
    uint8_t bit_addr = params[BIT_ADDRESS];
    uint8_t bit_length = params[BIT_LENGTH];
    uint8_t data = (ISL_Read_Register(reg_addr) & ~(_GenerateMask(bit_length) << bit_addr)) | (uint8_t) (value << bit_addr);      //Take the read data from the I2C register, zero out the bits we are setting, then OR in our data
    ISL_Write_Register(reg_addr, data);   //Doing bitwise OR with previous result so we can determine if multiple errors occur
    #ifdef __DEBUG
    ISL_Read_Register(reg_addr);    //Re-read the I2C register so we can confirm any changes by watching variable values in debug.
    #endif
}

uint8_t ISL_GetSpecificBits(const isl_locate_t params[3]){
    uint8_t reg_addr = params[REG_ADDRESS];
    uint8_t bit_addr = params[BIT_ADDRESS];
    uint8_t bit_length = params[BIT_LENGTH];
    return (ISL_Read_Register(reg_addr) >> bit_addr) & _GenerateMask(bit_length); //Shift register containing data to the right until we reach the LSB of what we want, then bitwise AND to discard anything longer than the bit length
}

uint16_t ISL_GetAnalogOut(isl_analogout_t value){
    DAC_SetOutput(0);   //Make sure DAC is set to 0V        8
    ADC_SelectChannel(ADC_PIC_DAC); //Connect ADC to 0V to empty internal ADC sample/hold capacitor //16
    __delay_us(1);  //Wait a little bit //16
    ADC_SelectChannel(ADC_ISL_OUT); //Connect ADC to analog out of ISL94208 16
    ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, value);    //Set the ISL to output desired signal on analog out    11214
    __delay_us(100); //ISL94208 has maximum analog output stabilization time of 0.1ms = 100us //11214
    uint16_t result = ADC_GetConversion(ADC_ISL_OUT); //Finally run the conversion and store the result //12336
    ISL_SetSpecificBits(ISL.ANALOG_OUT_SELECT_4bits, AO_OFF);   //Turn the ISL analog out off again //23557
    return result;
}

void ISL_ReadAllCellVoltages(void){
    CellVoltages[1] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL1) );
    CellVoltages[2] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL2) );
    CellVoltages[3] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL3) );
    CellVoltages[4] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL4) );
    CellVoltages[5] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL5) );
    CellVoltages[6] = _ConvertADCtoMV( ISL_GetAnalogOut(AO_VCELL6) );
}

uint8_t ISL_CalcMaxVoltageCell(void){
    uint8_t maxcell = 1;    //Start by assuming max cell is 1
    for (uint8_t i = 2; i <= 6; i++){   //We can start with cell 2 since we already assumed cell 1 is max until proved otherwise.
        if (CellVoltages[i] > CellVoltages[maxcell]){
            maxcell = i;
        }
    }
    return maxcell;
}

uint8_t ISL_CalcMinVoltageCell(void){
    uint8_t mincell = 1;    //Start by assuming min cell is 1
    for (uint8_t i = 2; i <= 6; i++){   //We can start with cell 2 since we already assumed cell 1 is min until proved otherwise.
        if (CellVoltages[i] < CellVoltages[mincell]){
            mincell = i;
        }
    }
    return mincell;
}

uint16_t ISL_CalcCellVoltageDelta(void){
    return (CellVoltages[ISL_CalcMaxVoltageCell()] - CellVoltages[ISL_CalcMinVoltageCell()]);
}

static uint16_t _ConvertADCtoMV(uint16_t adcval){
    return (uint16_t) ((uint32_t)adcval * 2500 * 2 / 1024);
}

static uint8_t _GenerateMask(uint8_t length){   //Generates a given number of ones in binary. Ex. input 5 = output 0b11111
    uint8_t result = 0b1;
    for (; length > 1; length--){
        result = (uint8_t)(result << 1)+1;
    }
    return result;
}