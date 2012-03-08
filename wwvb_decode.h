/*************************************************************************
* Title:    WWVB decode library
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* File:     $Id: wwvb_decode.h 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/
#ifndef WWVB_DECODE_H
#define WWVB_DECODE_H

#include <stdint.h>

/* Samples per sec can be changed from 1 to 32 */
#define SAMPLES_PER_SEC 20

typedef enum
{
  WWVB_DECODE_RESULT_WAITING_FOR_SYNC,
  WWVB_DECODE_RESULT_IN_PROGRESS,
  WWVB_DECODE_RESULT_SUCCESS
} WWVBDecodeResultEnumType;

typedef struct
{
  uint8_t u_Hours;
  uint8_t u_Minutes;
  uint16_t w_Days;
  uint8_t u_Year;
  uint8_t u_DST;
} WWVBDecodedTimeStructType;

/************************************************************************
* Name: wwvbDecodeInit
*
* Description: Initialize the decode state machine
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void wwvbDecodeInit( void );

/************************************************************************
* Name: wwvbProcessSample
*
* Description: Process the latest sample collected at SAMPLES_PER_SEC rate.
*              If decode is successful, results will be returned in p_Time
*
* Parameters: u_Sample - Input sample
*             p_Time - Pointer to decoded time structure
*
* Returns: Decode status
************************************************************************/ 
WWVBDecodeResultEnumType wwvbProcessSample( uint8_t u_Sample, WWVBDecodedTimeStructType *p_Time );

#endif
