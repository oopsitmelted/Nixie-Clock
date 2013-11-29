/*************************************************************************
* Title:    NXP PCF8563 Real Time Clock Driver 
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* File:     $Id: pcf8563.h 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/
#ifndef PCF8563_H
#define PCF8563_H

#include <stdint.h>

typedef struct
{
  uint8_t u_Seconds;
  uint8_t u_Minutes;
  uint8_t u_Hours;
  uint8_t u_Valid;
} RtcTimeStructType;

/************************************************************************
* Name: rtcInit
*
* Description: Initialize RTC interface
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void rtcInit( void );

/************************************************************************
* Name: rtcWriteTime
*
* Description: Write time to RTC
*
* Parameters: p_Time - Pointer to RTC time structure
*
* Returns: Nothing
************************************************************************/ 
void rtcWriteTime( RtcTimeStructType *p_Time );

/************************************************************************
* Name: rtcReadTime
*
* Description: Read time from RTC
*
* Parameters: p_Time - Pointer to RTC time structure
*
* Returns: Nothing
************************************************************************/ 
uint8_t rtcReadTime( RtcTimeStructType *p_Time );

#endif
