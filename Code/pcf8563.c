/*************************************************************************
* Title:    NXP PCF8563 Real Time Clock Driver 
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* Depends On: Peter Fleury I2C driver
* File:     $Id: pcf8563.c 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/
#include <stdint.h>
#include "pcf8563.h"
#include "i2cmaster.h"

#define RTC_DEVICE_ADDR 0xA2

#define RTC_REG_CTL_STATUS_1  0x00
#define RTC_REG_CTL_STATUS_2  0x01
#define RTC_REG_VL_SECONDS    0x02
#define RTC_REG_MINUTES       0x03
#define RTC_REG_HOURS         0x04
#define RTC_REG_DAYS          0x05
#define RTC_REG_WEEKDAYS      0x06
#define RTC_REG_CENT_MONTHS   0x07
#define RTC_REG_YEARS         0x08
#define RTC_REG_MINUTE_ALARM  0x09
#define RTC_REG_HOUR_ALARM    0x0A
#define RTC_REG_DAY_ALARM     0x0B
#define RTC_REG_WEEKDAY_ALARM 0x0C
#define RTC_REG_CLKOUT        0x0D
#define RTC_REG_TIMER_CTL     0x0E
#define RTC_REG_TIMER         0x0F

/************************************************************************
* Name: bcdToInt
*
* Description: Convert BCD value to uint8
*
* Parameters: u_bcdVal - BCD value to be converted to int
*
* Returns: Integer value
************************************************************************/ 
static uint8_t bcdToInt( uint8_t u_bcdVal )
{
  uint8_t u_Temp;

  u_Temp = ( u_bcdVal >> 4 ) * 10;
  u_Temp += u_bcdVal & 0xF;

  return( u_Temp ); 
}

/************************************************************************
* Name: intToBcd
*
* Description: Convert uint8 to BCD
*
* Parameters: u_intVal - Integer value to be converted to BCD
*
* Returns: BCD value
************************************************************************/ 
static uint8_t intToBcd( uint8_t u_intVal )
{
  uint8_t u_Temp;
  uint8_t u_Tens;

  u_Tens = u_intVal / 10;
  u_intVal -= u_Tens * 10;

  u_Temp = u_Tens << 4;
  u_Temp += u_intVal;

  return( u_Temp );
}

/************************************************************************
* Name: rtcWriteTime
*
* Description: Write time to RTC
*
* Parameters: p_Time - Pointer to RTC time structure
*
* Returns: Nothing
************************************************************************/ 
void rtcWriteTime( RtcTimeStructType *p_Time )
{
  i2c_start_wait( RTC_DEVICE_ADDR + I2C_WRITE );
  i2c_write( RTC_REG_VL_SECONDS ); 

  i2c_write( intToBcd( p_Time->u_Seconds & 0x7F ) );
  i2c_write( intToBcd( p_Time->u_Minutes & 0x7F ) );
  i2c_write( intToBcd( p_Time->u_Hours & 0x3F ) );

  i2c_stop();
}

/************************************************************************
* Name: rtcReadTime
*
* Description: Read time from RTC
*
* Parameters: p_Time - Pointer to RTC time structure
*
* Returns: Nothing
************************************************************************/ 
uint8_t rtcReadTime( RtcTimeStructType *p_Time )
{
  uint8_t u_Temp;

  i2c_start_wait( RTC_DEVICE_ADDR + I2C_WRITE );
  i2c_write( RTC_REG_VL_SECONDS ); 
  
  i2c_rep_start( RTC_DEVICE_ADDR + I2C_READ );
  
  u_Temp = i2c_readAck();
  p_Time->u_Valid = u_Temp & 0x80 ? 0 : 1;
  p_Time->u_Seconds = bcdToInt( u_Temp & 0x7F );
  p_Time->u_Minutes = bcdToInt( i2c_readAck() & 0x7F );
  p_Time->u_Hours = bcdToInt( i2c_readNak() & 0x3F );

  i2c_stop();

  return( 1 );
}

/************************************************************************
* Name: rtcInit
*
* Description: Initialize RTC interface
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void rtcInit( void )
{
  i2c_init();
}
