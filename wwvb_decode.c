/*************************************************************************
* Title:    WWVB decode library
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* File:     $Id: wwvb_decode.c 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <avr/sfr_defs.h>
#include <avr/io.h>
#include "wwvb_decode.h"
#include "uart.h"

/* Define bit durations in terms of # of samples of the sampling clock rate */
#define ZERO_BIT_DURATION_SAMPLES ( ( 200 * SAMPLES_PER_SEC ) / 1000 )
#define ONE_BIT_DURATION_SAMPLES  ( ( 500 * SAMPLES_PER_SEC ) / 1000 )
#define SYNC_BIT_DURATION_SAMPLES ( ( 800 * SAMPLES_PER_SEC ) / 1000 )
#define ONE_MINUS_ZERO_BIT_DURATION_SAMPLES ( ONE_BIT_DURATION_SAMPLES - ZERO_BIT_DURATION_SAMPLES )
#define SYNC_MINUS_ONE_BIT_DURATION_SAMPLES ( SYNC_BIT_DURATION_SAMPLES - ONE_BIT_DURATION_SAMPLES )
#define FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES ( SAMPLES_PER_SEC - SYNC_BIT_DURATION_SAMPLES ) 

#define DECODE_BIT_ERR -1
#define DECODE_BIT_SYNC 2

/* Decode States */
typedef enum
{
  DECODE_SYNC_1,
  DECODE_SYNC_2,
  DECODE_MINUTES_40,
  DECODE_MINUTES_20,
  DECODE_MINUTES_10,
  DECODE_RESERVED_1,
  DECODE_MINUTES_8,
  DECODE_MINUTES_4,
  DECODE_MINUTES_2,
  DECODE_MINUTES_1,
  DECODE_SYNC_3,
  DECODE_RESERVED_2,
  DECODE_RESERVED_3,
  DECODE_HOURS_20,
  DECODE_HOURS_10,
  DECODE_RESERVED_4,
  DECODE_HOURS_8,
  DECODE_HOURS_4,
  DECODE_HOURS_2,
  DECODE_HOURS_1,
  DECODE_SYNC_4,
  DECODE_RESERVED_5,
  DECODE_RESERVED_6,
  DECODE_DAYS_200,
  DECODE_DAYS_100,
  DECODE_RESERVED_7,
  DECODE_DAYS_80,
  DECODE_DAYS_40,
  DECODE_DAYS_20,
  DECODE_DAYS_10,
  DECODE_SYNC_5,
  DECODE_DAYS_8,
  DECODE_DAYS_4,
  DECODE_DAYS_2,
  DECODE_DAYS_1,
  DECODE_RESERVED_8,
  DECODE_RESERVED_9,
  DECODE_UT1_ADD_1,
  DECODE_UT1_SUB,
  DECODE_UT1_ADD_2,
  DECODE_SYNC_6,
  DECODE_UT1_CORR_POINT_8,
  DECODE_UT1_CORR_POINT_4,
  DECODE_UT1_CORR_POINT_2,
  DECODE_UT1_CORR_POINT_1,
  DECODE_RESERVED_10,
  DECODE_YEAR_80,
  DECODE_YEAR_40,
  DECODE_YEAR_20,
  DECODE_YEAR_10,
  DECODE_SYNC_7,
  DECODE_YEAR_8,
  DECODE_YEAR_4,
  DECODE_YEAR_2,
  DECODE_YEAR_1,
  DECODE_RESERVED_11,
  DECODE_LEAP_YEAR,
  DECODE_LEAP_SECOND,
  DECODE_DST_1,
  DECODE_DST_2,
  DECODE_MAX
} decode_state_enum_type;

static decode_state_enum_type e_decode_state = DECODE_SYNC_1;
static WWVBDecodedTimeStructType z_DecodedTime;
static uint32_t q_SampleHistory = 0xFFFFFFFF;
static uint8_t u_BitCount = 0;

/************************************************************************
* Name: wwvbCountBits
*
* Description: Count the number of bits out of the leftmost u_NumBits of
*              q_Word which are set to '1'
*
* Parameters: q_Word - Word containing bits to be counted 
*             u_NumBits - Number of bits ( from left ) of q_Word to be counted
*
* Returns: Number of bits
************************************************************************/ 
static uint8_t wwvbCountBits( uint32_t q_Word, uint8_t u_NumBits )
{
  uint8_t u_Cnt = 0;

  if( u_NumBits > 31 ) 
    return 0;
  
  while( q_Word && u_NumBits )
  {
    if( q_Word & 0x80000000 )
      u_Cnt++;

    q_Word <<= 1;
    u_NumBits--;
  }

  return u_Cnt;
}

/************************************************************************
* Name: wwvbDecodeBit
*
* Description: Given of set of SAMPLES_PER_SEC samples, attempt to detect
*              a valid ONE, ZERO, or SYNC bit
*
* Parameters: q_Samples - Word containing input SAMPLES_PER_SEC samples
*
* Returns: 0, 1, SYNC or ERR
************************************************************************/ 
static int8_t wwvbDecodeBit( uint32_t q_Samples )
{
  q_Samples <<= 32 - SAMPLES_PER_SEC;

  /* First check to see if there are enough zero samples to indicate a '0' bit */
  if( wwvbCountBits( q_Samples, ZERO_BIT_DURATION_SAMPLES ) <  ( ZERO_BIT_DURATION_SAMPLES / 2 ) )
  {
    q_Samples <<= ZERO_BIT_DURATION_SAMPLES;

    /* Yes, now check to see if there are enough zero samples to indicate a '1' bit */
    if( wwvbCountBits( q_Samples, ONE_MINUS_ZERO_BIT_DURATION_SAMPLES ) < 
        ( ONE_MINUS_ZERO_BIT_DURATION_SAMPLES / 2 ) )
    {
      q_Samples <<= ONE_MINUS_ZERO_BIT_DURATION_SAMPLES;

      /* Yes, now check to see if there are enough zero samples to indicate a 'SYNC' bit */
      if( wwvbCountBits( q_Samples, SYNC_MINUS_ONE_BIT_DURATION_SAMPLES ) < 
          ( SYNC_MINUS_ONE_BIT_DURATION_SAMPLES / 2 ) )
      {
        q_Samples <<= SYNC_MINUS_ONE_BIT_DURATION_SAMPLES;

        /* Yup. Remaining samples need to be high for this to be a valid bit */
        if( wwvbCountBits( q_Samples, FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES ) < 
            ( FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES / 2 ) )
        {
          /* Nope, flag an error */
          return DECODE_BIT_ERR; 
        }
        else
        {
          /* Successfully detected a SYNC bit */
          return DECODE_BIT_SYNC;
        }
      }
      else
      {
        q_Samples <<= SYNC_MINUS_ONE_BIT_DURATION_SAMPLES;
        
        /* This might be a '1' bit. Remaining samples must be high for this to be true */
        if( wwvbCountBits( q_Samples, FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES ) < 
            ( FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES / 2 ) )
        {
          /* Nope, error */
          return DECODE_BIT_ERR;
        }
        else
        {
          /* Yes, this is a '1' */
          return 1;
        }
      }
    }
    else
    {
      q_Samples <<= ONE_MINUS_ZERO_BIT_DURATION_SAMPLES;

      /* Looks like this might be a '0' bit. Ensure remaining samples are '1's */
      if( wwvbCountBits( q_Samples, SYNC_MINUS_ONE_BIT_DURATION_SAMPLES ) < 
          ( SYNC_MINUS_ONE_BIT_DURATION_SAMPLES / 2 ) )
      {
        /* No, return an error */
        return DECODE_BIT_ERR;
      }
      else
      {
        q_Samples <<= SYNC_MINUS_ONE_BIT_DURATION_SAMPLES;

        /* So far so good. Check the last set of samples */
        if( wwvbCountBits( q_Samples, FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES ) < 
            ( FRAME_MINUS_SYNC_BIT_DURATION_SAMPLES / 2 ) )
        {
          /* Too many '0' samples, error */
          return DECODE_BIT_ERR;
        }
        else
        {
          /* Detected a '0' */
          return 0;
        }
      }
    }
  }
  else 
    return DECODE_BIT_ERR;
}

/************************************************************************
* Name: wwvbDecodeInit
*
* Description: Initialize the decode state machine
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void wwvbDecodeInit( void )
{
  e_decode_state = DECODE_SYNC_1;
}

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
WWVBDecodeResultEnumType wwvbProcessSample( uint8_t u_Sample, WWVBDecodedTimeStructType *p_Time )
{
  if( u_Sample > 1 )
    return( WWVB_DECODE_RESULT_WAITING_FOR_SYNC );

  /* Shift in the current sample */
  q_SampleHistory <<= 1;
  q_SampleHistory |= u_Sample;

  /* If we are waiting for the initial sync use a sliding window until it is 
   * detected
   */
  if( e_decode_state == DECODE_SYNC_1 )
  {
    if( wwvbDecodeBit( q_SampleHistory ) == DECODE_BIT_SYNC )
    {
      e_decode_state = DECODE_SYNC_2;
      u_BitCount = 0;
    }
  }
  else
  {
    char x_DecodedBit;

    /* Now that we are sync'd up, wait until SAMPLES_PER_SEC bits are collected before
     * the next decode
     */
    if( ++u_BitCount < SAMPLES_PER_SEC )
    {
      return( WWVB_DECODE_RESULT_IN_PROGRESS );
    }

    u_BitCount = 0;

    /* Attempt to decode a bit */
    x_DecodedBit = wwvbDecodeBit( q_SampleHistory );

    if( ( x_DecodedBit == DECODE_BIT_ERR ) ||
      ( ( x_DecodedBit != DECODE_BIT_SYNC &&
          ( e_decode_state == DECODE_SYNC_2 ||
            e_decode_state == DECODE_SYNC_3 ||
            e_decode_state == DECODE_SYNC_4 ||
            e_decode_state == DECODE_SYNC_5 ||
            e_decode_state == DECODE_SYNC_6 ||
            e_decode_state == DECODE_SYNC_7 ) ) ) )

    {
      /* Error detected, reset  state and sample history */
      e_decode_state = DECODE_SYNC_1;
      q_SampleHistory = 0xFFFFFFFF;
      uart_putc('S');
      return( WWVB_DECODE_RESULT_IN_PROGRESS );
    }

    /* Valid bit detected, build decoded time structure given current decode state */
    switch( e_decode_state )
    {
      case DECODE_SYNC_2:
      {
        memset( &z_DecodedTime, 0, sizeof( z_DecodedTime ) );
        break;
      }

      case DECODE_MINUTES_40:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 40; 
        break;
      }

      case DECODE_MINUTES_20:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 20; 
        break;
      }

      case DECODE_MINUTES_10:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 10; 
        break;
      }
          
      case DECODE_MINUTES_8:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 8;
        break;
      }

      case DECODE_MINUTES_4:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 4;
        break;
      }

      case DECODE_MINUTES_2:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 2;
        break;
      }

      case DECODE_MINUTES_1:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Minutes += 1;

        if( z_DecodedTime.u_Minutes >= 60 )
          e_decode_state = DECODE_SYNC_1;

        break;
      }

      case DECODE_HOURS_20:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 20;
        break;
      }

      case DECODE_HOURS_10:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 10;
        break;
      }

      case DECODE_HOURS_8:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 8;
        break;
      }

      case DECODE_HOURS_4:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 4;
        break;
      }

      case DECODE_HOURS_2:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 2;
        break;
      }

      case DECODE_HOURS_1:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Hours += 1;

        if( z_DecodedTime.u_Hours >= 24 )
        {
          e_decode_state = DECODE_SYNC_1;
        }
        break;
      }

      case DECODE_DAYS_200:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 200;
        break;
      }

      case DECODE_DAYS_100:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 100;
        break;
      }

      case DECODE_DAYS_80:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 80;
        break;
      }

      case DECODE_DAYS_40:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 40;
        break;
      }

      case DECODE_DAYS_20:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 20;
        break;
      }

      case DECODE_DAYS_10:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 10;
        break;
      }

      case DECODE_DAYS_8:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 8;
        break;
      }

      case DECODE_DAYS_4:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 4;
        break;
      }

      case DECODE_DAYS_2:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 2;
        break;
      }

      case DECODE_DAYS_1:
      {
        if( x_DecodedBit )
          z_DecodedTime.w_Days += 1;

        if( z_DecodedTime.w_Days > 366 )
        {
          e_decode_state = DECODE_SYNC_1;
        }
        break;
      }

      case DECODE_YEAR_80:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 80;
        break;
      }

      case DECODE_YEAR_40:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 40;
        break;
      }

      case DECODE_YEAR_20:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 20;
        break;
      }

      case DECODE_YEAR_10:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 10;
        break;
      }

      case DECODE_YEAR_8:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 8;
        break;
      }

      case DECODE_YEAR_4:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 4;
        break;
      }

      case DECODE_YEAR_2:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 2;
        break;
      }

      case DECODE_YEAR_1:
      {
        if( x_DecodedBit )
          z_DecodedTime.u_Year += 1;
        break;
      }

      case DECODE_DST_1:
      {
        z_DecodedTime.u_DST <<= 1;

        if( x_DecodedBit )
        {
          z_DecodedTime.u_DST |= 0x1;
        }
        break;
      }

      case DECODE_DST_2:
      {
        z_DecodedTime.u_DST <<= 1;

        if( x_DecodedBit )
        {
          z_DecodedTime.u_DST |= 0x1;
        }
        break;
      }

      default:
        break;
    }
    
    /* Increment to next state */
    e_decode_state = (decode_state_enum_type) ( (uint8_t)( e_decode_state ) + 1 );
    uart_putc('.');
    
    /* Check to see if we have reached the end of the one minute frame, if so 
     * reset state 
     */ 
    if( e_decode_state == DECODE_MAX )
    {
      e_decode_state = DECODE_SYNC_1;
      *p_Time = z_DecodedTime;
      return( WWVB_DECODE_RESULT_SUCCESS ); 
    }
  }
    
  if( e_decode_state == DECODE_SYNC_1 )
    return( WWVB_DECODE_RESULT_WAITING_FOR_SYNC );
  else
    return( WWVB_DECODE_RESULT_IN_PROGRESS );
}
