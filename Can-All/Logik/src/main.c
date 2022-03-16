//-----------------------------------------------------------------------------
// F560_Ports_SwitchLED.c
//-----------------------------------------------------------------------------
// Copyright 2015 Silicon Laboratories, Inc.
// http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
//
// Program Description:
//
// This program demonstrates how to configure port pins as digital inputs
// and outputs.  The C8051F560 target board has one push-button switch
// connected to a port pin and one LED.  The program constantly checks the
// status of the switch and if it is pushed, it turns on the LED.
//
// Resources Used:
// * Internal Oscillator: MCU clock source
//
// How To Test:
//
// 1) Download code to a 'F560 target board
// 2) Ensure that the Switch and LED pins on the J19 header are
//    properly shorted
// 3) Push the button (P1.4) and see that the LED turns on
//
//
// Target:         C8051F56x (Side A of a C8051F560-TB)
// Tool chain:     Simplicity Studio / Keil C51 9.51
// Command Line:   None
//
// Release 1.2 / 8  MAY 2015 (DL)
//    -Updated to use with Simplicity Studio
//
// Release 1.1 / 11 MAR 2010 (GP)
//    -Tested with Raisonance
//
// Release 1.0 / 15 JAN 2009 (GP)
//    -Initial Revision


//-----------------------------------------------------------------------------
// Global CONSTANTS
//-----------------------------------------------------------------------------

#define VREF                 2200      // ADC Voltage Reference (mV)
#define VACUUM_MIN           100       // [mBar] disable vacuum pump at this level
#define VACUUM_MAX           300      // [mBar] enable vacuum pump at this level

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include <SI_C8051F550_Defs.h>
#include <SI_C8051F550_Register_Enums.h>
#include "../inc/Init.h"
#include "../inc/HighSideSwitch.h"
#include "../inc/compiler_defs.h"
#include "../inc/Common.h"
#include "../inc/Errors.h"

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
unsigned doInit(void);
void highSide_Init(void);
int checkBoard(void);
int getHighSideSwitchData(HighSideSwitchData *Switch_A, HighSideSwitchData *Switch_B);
int getHighSideSwitchACurrent_mA(void);
int getHighSideSwitchBCurrent_mA(void);
int DoMainStartup(void);
int DoPumpStartup(void);
int DoRevHeatStartup(void);
int DoRunMain(void);
int DoRunPump(void);
int DoRunRevHeater(void);
int getOwnCanNode(void);
int getAdcReading(int pin, int *adc_value);
int getPressureReading_mbar(int *value);

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
BoardType Board = MAIN;
StateMachine CURRENT_STATE = ERROR;
int TemperatureFactor = 1;  //todo figure out factor
int TemperatureOffset = 0;
int CurrentFactor = 1;  //todo figure out factor
int CurrentOffset = 0;
int VoltageFactor = 1;  //todo figure out factor
int VoltageOffset = 0;
int VacuumFactor = 1;   //todo figure out factor
int VacuumOffset = 0;

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
// todo needed?
void SiLabs_Startup (void)
{
   PCA0MD &= ~0x40;                    // Disable the watchdog timer
}
 
//-----------------------------------------------------------------------------
// Pin Declarations
// todo: maybe move to Init.h
//-----------------------------------------------------------------------------
SI_SBIT(C_Port, SFR_P0, 0);
SI_SBIT(DIA_EN, SFR_P0, 1);
SI_SBIT(SEL1, SFR_P0, 2);
SI_SBIT(SEL2, SFR_P0, 3);
SI_SBIT(UART_TX, SFR_P0, 4);
SI_SBIT(UART_RX, SFR_P0, 5);
SI_SBIT(CAN_TX, SFR_P0, 6);
SI_SBIT(CAN_RX, SFR_P0, 7);
SI_SBIT(SNS_A, SFR_P1, 0);
SI_SBIT(SNS_B, SFR_P1, 1);
SI_SBIT(EN_A, SFR_P1, 2);
SI_SBIT(EN_B, SFR_P1, 3);
SI_SBIT(LATCH, SFR_P1, 4);
SI_SBIT(ST_A, SFR_P1, 5);
SI_SBIT(ST_B, SFR_P1, 6);
SI_SBIT(A_PORT, SFR_P1, 7);
SI_SBIT(B_PORT, SFR_P2, 0);
SI_SBIT(C2D, SFR_P2, 1);

//leads to problems with generic code (highsideSwitch init etc (works with EN_*)
//#ifndef Board == MAIN
//SI_SBIT(BenderError, SFR_P1, 7);  //A_Port
//SI_SBIT(InterlockSig, SFR_P0, 0); //B_Port
//SI_SBIT(OilPressureWarningLight, SFR_P1, 2);  //EN_A
//SI_SBIT(InterlockRelais, SFR_P1, 3);  //EN_B
//#endif
//
//#ifndef Board == VACUUM_WATER_PUMP
//SI_SBIT(Vacum_ADC, SFR_P1, 7);  //A_Port
//SI_SBIT(VacuumPump, SFR_P1, 2); //EN_A
//SI_SBIT(WatherPump, SFR_P1, 3); //EN_B
//#endif
//
//#ifndef Board == REVERSE_HEATER
//SI_SBIT(Rev_GearSelected, SFR_P1, 7);  //A_Port (Gearbox Sensor)
//SI_SBIT(HeaterMeltFuse, SFR_P2, 0); //B_Port (RevDirSelected)
//SI_SBIT(Rev_DirectionSelected, SFR_P0, 0); //C_Port
//SI_SBIT(HeaterRelais, SFR_P1, 2); //EN_A
//SI_SBIT(ReverseLight, SFR_P1, 3); //EN_B
//#endif

//-----------------------------------------------------------------------------
// main() Routine
//-----------------------------------------------------------------------------

void main (void)
{

   int error;
   error = NO_ERROR;
   SFRPAGE = LEGACY_PAGE;              // Set SFR Page for PCA0MD

   // Init board auxillary's
   CURRENT_STATE = INIT;
   error += doInit();
   error += checkBoard();

   if(error > 0)
     {
       // todo: init failed, what now?
     }

   // startup
   CURRENT_STATE = STARTUP;
   switch(Board)
   {
     case(MAIN):
            DoMainStartup();
         break;
     case(VACUUM_WATER_PUMP):
            DoPumpStartup();
         break;
     case(REVERSE_HEATER):
            DoRevHeatStartup();
         break;
   }





   while (1)  //todo: define reasoneable cycling rate
   {
     switch(Board)
     {
       case(MAIN):
           break;
       case(VACUUM_WATER_PUMP):
           break;
       case(REVERSE_HEATER):
           break;
     }


       //enable battery's
       //wait ??
       //check Invertor (??)
       //check HighSide Switches

   }                                   // end of while(1)
}                                      // end of main()

//initialize generic Board Auxillery (ED, SYSCLK, ADC, etc.)
unsigned doInit(void)
{
  WDT_Init ();
  SYSCLK_Init ();
  UART0_Init ();
  PORT_Init ();             // Initialize Port I/O
//  PIN_Init();
  Timer0_Init ();
  PCA0_Init ();
  OSCILLATOR_Init ();       // Initialize Oscillator
  CAN_Init(getOwnCanNode);
  highSide_Init();
  ADC0_Init();

  return NO_ERROR;
}

//init the High Side Switches
void highSide_Init(void)
{
  LATCH = LOW;  // restart switch if failure occures todo: change later, if error handling is OK
  DIA_EN = LOW;   // enable SNS output for both switches
}

//check board auxillary functins (switches, can, todo: what else?)
int checkBoard(void)
{
  int error = 0;

  if(ST_A == LOW)
    {
      error = BOARD_HSS_A_ERROR;
    }
  else if(ST_B == LOW)
    {
      error = BOARD_HSS_B_ERROR;
    }
  //todo maybe check CAN
  //todo different stuff?

  return error;
}

// check highside diagnostic's
int getHighSideSwitchData(HighSideSwitchData *Switch_A, HighSideSwitchData *Switch_B)
{
  int adcReading;

  Switch_A->current = 0;//I_Sens = 4.35 mA @ I_out = 20A; I_Sens = 1.74 mA @ I_out = 8A; I_Sens = 0.65 mA @ I_out = 3A; I_Sens = 0.217 mA @ I_out = 1A
  Switch_A->voltage = 0;//I_Sens = 2.25 mA @ T = 150°C; I_Sens = 1.52 mA @ T = 85°C; I_Sens = 0.85 mA @ T = 25°C
  Switch_A->temperature = 0;//I_Sens = 2.25 mA @ T = 150°C; I_Sens = 1.52 mA @ T = 85°C; I_Sens = 0.85 mA @ T = 25°C
  Switch_B->current = 0;
  Switch_B->voltage = 0;
  Switch_B->temperature = 0;

  DIA_EN = HIGH;
  //Check output Current
  SEL1 = LOW;
  SEL2 = LOW;

  if(EN_A)
  {
    //todo Calculate Value according to I_Sens
      //getAdcReading()
      Switch_A->current = getAdcReading(P1_0, &adcReading)*CurrentFactor+CurrentOffset;
  }
  if(EN_B)
  {
      //todo Calculate Value according to I_Sens
      Switch_B->current = getAdcReading(P1_1, &adcReading)*CurrentFactor+CurrentOffset;
  }

  //Check output Voltage
  SEL1 = HIGH;
  SEL2 = HIGH;

  //todo Calculate Value according to I_Sens
  Switch_A->voltage = getAdcReading(P1_0, &adcReading)*VoltageFactor+VoltageOffset;
  Switch_B->voltage = getAdcReading(P1_1, &adcReading)*VoltageFactor+VoltageOffset;

  //Check Temperature
  SEL1 = HIGH;
  SEL2 = LOW;

  //todo Calculate Value according to I_Sens
  Switch_A->temperature = getAdcReading(P1_0, &adcReading)*TemperatureFactor+TemperatureOffset;
  Switch_B->temperature = getAdcReading(P1_1, &adcReading)*TemperatureFactor+TemperatureOffset;

  return NO_ERROR;
}

int getHighSideSwitchACurrent_mA()
{
  int adcReading;

  DIA_EN = HIGH;
  //Check output Current
  SEL1 = LOW;
  SEL2 = LOW;

  return getAdcReading(P1_0, &adcReading)*CurrentFactor+CurrentOffset;
}

int getHighSideSwitchBCurrent_mA()
{
  int adcReading;

  DIA_EN = HIGH;
  //Check output Current
  SEL1 = LOW;
  SEL2 = LOW;

  return getAdcReading(P1_1, &adcReading)*CurrentFactor+CurrentOffset;
}

//return own CAN node Number
int getOwnCanNode(void)
{
  //Define CAN Node
  switch(Board)
  {
    case(MAIN):
          return CAN_NO_CAN_ALL_MAIN;
        break;
    case(VACUUM_WATER_PUMP):
          return CAN_NO_CAN_ALL_PUMP;
        break;
    case(REVERSE_HEATER):
           return CAN_NO_CAN_ALL_REV_HEATER;
        break;
    default:
      return -1;
  }
}

//check, enable inverter, battery and bender
int DoMainStartup(void)
{
  int error = NO_ERROR;

  //todo(CAN) check inverter state

  // check interlock loop
  if(C_Port == HIGH)
    {
      //todo interlock loop is wired, should be LOW -> wiring problem?
    }
  EN_B = HIGH;  //enable Interlock Relais (close interlock loop)
  if(C_Port == LOW)
      {
        EN_B = LOW; //disable Interlock Relais (close interlock loop)
        return BOARD_INTERLOCK_ERROR; //One of the Battery's is missing -> abord
      }

  //todo(CAN) enable battery's
  //todo(CAN) check inverter (every thing should be OK)
  //todo(CAN) check BENDER

  return error;
}

//check inverter tempereature, check vacuum sensor
int DoPumpStartup(void)
{
  int error = NO_ERROR;
  int vacuumLevel = 0;

  //get Vacuum Level
  error += getPressureReading_mbar(&vacuumLevel);

  //enable Vacuum Pump
  EN_A = HIGH;

//  Wait_ms((U16) 500); todo: does not work

  if(getHighSideSwitchACurrent_mA() < 100)  //check if Pump is running
  {
    error = BOARD_VACUUM_PUMP_ERROR; //pump should be running, seams not ON
  }

  //  Wait_ms((U16) 500); todo: does not work

  error += getPressureReading_mbar(&vacuumLevel);

  if(vacuumLevel+50 >= vacuumLevel) //check if Vacuum is building up
  {
    if(error == BOARD_VACUUM_PUMP_ERROR)  // pump is not running, no Vacuum is building up
      {
        error = BOARD_VACUUM_PUMP_ERROR;
      }
    else
      {
        error = BOARD_VACUUM_SYSTEM_ERROR;  // pump is running, but vacuum is not building up
      }
    }

  //disable Vacuum Pump
  EN_A = LOW;

  //  Wait_ms((U16) 500); todo: does not work

  if(getHighSideSwitchACurrent_mA() > 100)  //check if Pump is OFF
  {
    error = BOARD_VACUUM_PUMP_ERROR; //pump should be running, seams not ON
  }


  //todo(CAN) get inverter temperature (error if not working)

  if(error == NO_ERROR) //Board seams OK, signal accordingly
  {
    C_Port = HIGH;
  }
  return error;
}

//check heater melting fuse
int DoRevHeatStartup(void)
{
  int error = NO_ERROR;

  //todo: set internal PullUp of B_Port (P2.0)

  if(B_PORT == HIGH)  //check Heater Melting Fuse
    {
      error = HEATER_FUSE_ERROR;
    }

  return error;}

int DoRunMain(void)
{
  unsigned error = NO_ERROR;

  //todo(CAN): check invertor state
  //todo(CAN): battery state invertor state


  // check GPIO state's
  //Bender Error
  if(A_PORT == HIGH)  //todo check if HIGH signals error alternative: check by todo(CAN)
    {
      error += BENDER_ERROR_ERROR;
    }
  //pump's Signal
  if(B_PORT != HIGH)
    {
      error += BOARD_PUMPS_EROR;
    }
  //Interlock Signal
  if(C_Port != HIGH)
    {
      error += BOARD_INTERLOCK_ERROR + ERROR_LEVEL_FATAL;
    }


 return error;
}
int DoRunPump(void)
{
  int error = NO_ERROR;
  int vacuumLevel;

  error += getPressureReading_mbar(&vacuumLevel);

  if(error != NO_ERROR)
    {
      //todo: refine error handling (maybe pump should turn on)
      return error;
    }

  if(vacuumLevel > VACUUM_MAX)
    {
      EN_A = HIGH;
    }

  if (vacuumLevel < VACUUM_MIN)
    {
      EN_B = LOW;
    }

  //todo(CAN) get inverter temperature
  {

  }
  // check vacuum level -> enable vacuum pump if necessery
  // check inverter temperature -> enable wather pump if necessery


  return error;
}
int DoRunRevHeater(void)
{
  int error = NO_ERROR;

  // do reverse logic
  // check melting fuse

  return error;
}

//returns the DAC0 reading of the given pin(HEX)
int getAdcReading(int pin, int *value)
{
  //todo: uint*_t is not recognized

//  int adcReading = 0;
//
//  ADC0MX = pin;
//
//  //---------------------------
//  uint16_t i;                              // Sample counter
//  uint32_t accumulator = 0L;               // Where the ADC samples are integrated
//  uint16_t currval;                        // Current value of ADC0
//
//  uint8_t SFRPAGE_save = SFRPAGE;
//  SFRPAGE = LEGACY_PAGE;
//
//  ADC0CN_ADINT = 0;                         // Clear end-of-conversion indicator
//  ADC0CN_ADBUSY = 1;                        // Initiate conversion
//
//  // Accumulate 4096 samples and average to get a 16-bit result
//  // 4096 samples = 12 bits; 12 extra bits + 12 samples per bit = 24 bits
//  // Shift by 8 bits to calculate a 16-bit result.
//
//  i = 0;
//  do
//  {
//     while (!ADC0CN_ADINT);                 // Wait for conversion to complete
//     ADC0CN_ADINT = 0;                      // Clear end-of-conversion indicator
//
//     currval = ADC0;                  // Store latest ADC conversion
//     ADC0CN_ADBUSY = 1;                     // Initiate conversion
//     accumulator += currval;          // Accumulate
//     i++;                             // Update counter
//  } while (i != 4096);
//
//  accumulator = accumulator >> 8;     // 8-bit shift to go from 24 to 16 bits
//
//  *value = (uint16_t) (((uint32_t) accumulator * (uint32_t) VREF) / (uint32_t) 65536);
//
//  // Note that the numbers were rounded when equation was solved.
//  // This rounding does introduce error to the calculation, but it is
//  // negligible when looking for an estimated temperature.
//
//  SFRPAGE = SFRPAGE_save;

  return NO_ERROR;

}
//-----------------------------------------------------------------------------

// calculate and return the pressure measured by the pressure sensor
int getPressureReading_mbar(int *value)
{
  double vacuumSensorReading = 0;
  int error = NO_ERROR;

  //Vacuum Sensor 0kPa @ 0.5V, -100kPa @ 4.5V
  error += getAdcReading(P1_7, &vacuumSensorReading)*VacuumFactor+VacuumOffset;

  //calculate pressure [mBar], based on 101kPa (athmosperic pressure @ 0müm)
  *value = 101 - (int)vacuumSensorReading;
  return error;
}
// Initialization Subroutines
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------