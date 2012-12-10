/************************************************************************
* Title:  Nixie Clock with WWVB decode support
* Author: Scott Stickeler ( sstickeler@gmail.com )
* SVN:    $Id: atomicnixie.c 9 2011-11-23 04:43:33Z Scott $
************************************************************************/

#include <avr/sfr_defs.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdint.h>
#include <stdio.h>
#include "wwvb_decode.h"
#include "uart.h"
#include "pcf8563.h"
#include "nixie.h"

#define UTC_PST_OFFSET              8
#define WWVB_UPDATE_THRESHOLD_HOURS 6

#define C_ISR_FLAG_PPS                     0x01
#define C_ISR_FLAG_HOUR_BUTTON             0x02
#define C_ISR_FLAG_MINUTE_BUTTON           0x04
#define C_ISR_FLAG_WWVB_UPDATE             0x08
#define C_ISR_FLAG_WWVB_DECODE_IN_PROGRESS 0x10
#define C_ISR_FLAG_WWVB_DECODE_FAIL        0x20

#define SIXTY_HZ_CLK_BIT    2
#define SIXTY_HZ_CLK_PORT   PORTD
#define SIXTY_HZ_CLK_DDR    DDRD

#define HOURS_BIT        0
#define HOURS_PORT       PORTC
#define HOURS_DDR        DDRC

#define MINS_BIT         1
#define MINS_PORT        PORTC
#define MINS_DDR         DDRC

#define WWVB_BIT         0
#define WWVB_PORT        PORTB
#define WWVB_DDR         DDRB
#define WWVB_PIN         PINB

/* Event flags set by ISR handlers */
static volatile uint8_t u_ISRFlags = 0;
static volatile WWVBDecodedTimeStructType z_wwvbTime;
static volatile uint8_t u_HoursSinceLastWWVBUpdate = 24;

/* Needed for printf() */
static int uart_putchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                             _FDEV_SETUP_WRITE);

/* Structure containing current time */
typedef struct
{
  uint8_t u_Hours;
  uint8_t u_Minutes;
  uint8_t u_Seconds;
  uint8_t u_WWVBValidIndicator;
} TimeStructType;

/************************************************************************
* Name: uart_putchar
*
* Description: Output a single character to the UART
*
* Parameters: c - character to output, stream - pointer to output stream 
*
* Returns: Nothing
************************************************************************/ 
static int uart_putchar(char c, FILE *stream)
{
  uart_putc( c );
  return 0;
}

/************************************************************************
* Name: incrementHours
*
* Description: Helper function to increment the hour
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void incrementHours( TimeStructType *p_Time )
{
  if( ++p_Time->u_Hours > 23 )
    p_Time->u_Hours = 0;

  if( u_HoursSinceLastWWVBUpdate < 24 )
    u_HoursSinceLastWWVBUpdate++;
}

/************************************************************************
* Name: incrementMinutes
*
* Description: Helper function to increment the minute
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void incrementMinutes( TimeStructType *p_Time )
{
  if( ++p_Time->u_Minutes > 59 )
  {
    p_Time->u_Minutes = 0;
    incrementHours( p_Time );
  }
}

/************************************************************************
* Name: incrementSeconds
*
* Description: Helper function to increment the second
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void incrementSeconds( TimeStructType *p_Time )
{
  if( ++p_Time->u_Seconds > 59 )
  {
    p_Time->u_Seconds = 0;
    incrementMinutes( p_Time );
  }
}

/************************************************************************
* Name: updateTime
*
* Description: Updates the nixie display with the current time
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void updateTime( TimeStructType *p_Time )
{
  NixieDisplayStructType z_Nixies;
  uint8_t u_Temp;

  /* Hours */
  u_Temp = p_Time->u_Hours;

  if( u_Temp > 12 )
    u_Temp -= 12;
  else
  if( u_Temp == 0 )
    u_Temp = 12;

  z_Nixies.u_NixieDigit0 = u_Temp / 10;

  if( u_Temp >= 10 )
    u_Temp -= 10;

  z_Nixies.u_NixieDigit1 = u_Temp;
  
  /* Minutes */
  u_Temp = p_Time->u_Minutes;
  z_Nixies.u_NixieDigit2 = u_Temp / 10;

  if( u_Temp >= 10 )
    u_Temp -= 10 * z_Nixies.u_NixieDigit2;

  z_Nixies.u_NixieDigit3 = u_Temp;

  /* Seconds */
  u_Temp = p_Time->u_Seconds;
  z_Nixies.u_NixieDigit4 = u_Temp / 10;

  if( u_Temp >= 10 )
    u_Temp -= 10 * z_Nixies.u_NixieDigit4;

  z_Nixies.u_NixieDigit5 = u_Temp;

  /* Decimal Points */
  z_Nixies.u_NixieDP1 = 0;
  z_Nixies.u_NixieDP2 = 0;
  z_Nixies.u_NixieDP3 = p_Time->u_WWVBValidIndicator;

  /* Update display */
  nixieUpdate( &z_Nixies );
}

/************************************************************************
* Name: initRegisters
*
* Description: Configure AVR registers with desired clock, I/O, etc
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
static void initRegisters( void )
{
  /* Set 4MHz system frequency */
  CLKPR = _BV(CLKPCE);
  CLKPR = _BV(CLKPS0); // 8MHz internal OSC div 2 ( 4MHz )

  /* WWVB Module Input - PB0 */
  WWVB_DDR &= ~_BV( WWVB_BIT );    // WWVB Input
  WWVB_PORT &= ~_BV( WWVB_BIT );   // Disable pullup

  /* Pushbutton inputs - PC0 Hours, PC1 Mins */
  HOURS_DDR &= ~_BV( HOURS_BIT );  // Hours Input
  HOURS_PORT |= _BV( HOURS_BIT );  // Enable Pullup

  MINS_DDR &= ~_BV( MINS_BIT );    // Minutes Input
  MINS_PORT |= _BV( MINS_BIT );    // Enable Pullup

  /* 60 Hz Powerline Clock */
  SIXTY_HZ_CLK_DDR &= ~_BV( SIXTY_HZ_CLK_BIT );   // 60Hz input
  PCMSK2 |= _BV( PCINT18 );        // Enable PCINT18
  PCICR |= _BV(PCIE2);             // Enable Pin Change Interrupt 2
  
  /* 32.768kHz counter, 32Hz interrupt */
  ASSR = _BV( EXCLK ) | _BV( AS2 ); // Enable external clock input
  TCCR2A = _BV( WGM21 );            // CTC Mode
  TCCR2B = _BV( CS21 );             // Div 8 prescaler
  OCR2A = 0x7F;                     // Rollover at 128
  OCR2B = 0;
  TIMSK2 = _BV( OCIE2A ); // Compare match interrupt enable

  nixieInit();

  /* Clear Nixies */
  nixieBlank();
  nixieClear();
  nixieUnBlank();
}

/************************************************************************
* Name: ISRGetFlags
*
* Description: Retrieve current flags set by interrupt handlers. Flags 
*              indicate events that need to be processed in the main loop
*
* Parameters: None
*
* Returns: uint8_t containing current flags
************************************************************************/ 
static uint8_t ISRGetFlags( void )
{
  uint8_t u_ISRFlagsTemp;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    u_ISRFlagsTemp = u_ISRFlags;
    u_ISRFlags = 0;
  }

  return ( u_ISRFlagsTemp );
}

/************************************************************************
* Name: setRTC
*
* Description: Save current time to battery backed RTC
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void setRTC( TimeStructType *p_Time )
{
  RtcTimeStructType z_RtcTime;

  z_RtcTime.u_Hours = p_Time->u_Hours;
  z_RtcTime.u_Minutes = p_Time->u_Minutes;
  z_RtcTime.u_Seconds = p_Time->u_Seconds;

  rtcWriteTime( &z_RtcTime );
}

/************************************************************************
* Name: updateTimeFromWWVB
*
* Description: Update the current time with the decoded WWVB data. Adjusts
*              for time zone and daylight savings
*
* Parameters: p_Time - Pointer to time struct
*
* Returns: Nothing
************************************************************************/ 
static void updateTimeFromWWVB( TimeStructType *p_Time )
{
  int8_t b_Hours;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    b_Hours = z_wwvbTime.u_Hours - UTC_PST_OFFSET;

    if( z_wwvbTime.u_DST == 1 || z_wwvbTime.u_DST == 3 )
      b_Hours++;

    p_Time->u_Minutes = z_wwvbTime.u_Minutes + 1;
  }

  if( b_Hours < 0 )
    p_Time->u_Hours = (uint8_t)b_Hours + 12;
  else
    p_Time->u_Hours = (uint8_t)b_Hours;

  p_Time->u_Seconds = 0;

  /* Hours or minutes may have rolled over from above computations, if so adjust
   * the time accordingly
   */
  if( p_Time->u_Minutes == 60 )
  {
    p_Time->u_Minutes = 0;
    p_Time->u_Hours++;
  }

  if( p_Time->u_Hours > 23 )
    p_Time->u_Hours = 0;

  updateTime( p_Time );
  setRTC( p_Time );
}

/************************************************************************
* Name: main
*
* Description: Program entry and main processing loop
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
int main( void )
{
  uint8_t u_Flags;

  /* Time structures */
  RtcTimeStructType z_RtcTime;
  TimeStructType z_Time = { 12, 0, 0, 0 };

  /* Initialize chip */
  initRegisters();

  /* Initialize UART */
  uart_init( UART_BAUD_SELECT_DOUBLE_SPEED( 9600, 4000000 ) );

  /* Initialize WWVB decode library */
  wwvbDecodeInit();

  /* Enable Interrupts */
  sei();

  /* Initialize printf() */
  stdout = &mystdout;

  /* Initialize RTC */
  rtcInit();

  /* Set time if RTC valid */
  rtcReadTime( &z_RtcTime );

  if( z_RtcTime.u_Valid )
  {
    z_Time.u_Hours = z_RtcTime.u_Hours;
    z_Time.u_Minutes = z_RtcTime.u_Minutes;
    z_Time.u_Seconds = z_RtcTime.u_Seconds;
  }

  /* Display initial time */
  updateTime( &z_Time );

  /* Event loop */
  for(;;)
  {
    u_Flags = ISRGetFlags();

    /* 1PPS tick */
    if( u_Flags & C_ISR_FLAG_PPS )
    {
      incrementSeconds( &z_Time );
      updateTime( &z_Time );
    }

    /* Hour / Minute pushbuttons pressed */
    if( ( u_Flags & C_ISR_FLAG_HOUR_BUTTON ) || 
      ( u_Flags & C_ISR_FLAG_MINUTE_BUTTON ) )
    {

      if( u_Flags & C_ISR_FLAG_HOUR_BUTTON )
      {
        incrementHours( &z_Time );
      }

      if( u_Flags & C_ISR_FLAG_MINUTE_BUTTON )
      {
        /* Save current hour in case it increments */
        uint8_t u_CurrentHour = z_Time.u_Hours;

        incrementMinutes( &z_Time );

        /* Restore current hour */
        z_Time.u_Hours = u_CurrentHour;
      }

      /* Update current time and display */
      updateTime( &z_Time );

      /* Update RTC */
      setRTC( &z_Time );

      /* Clear WWVB update timer so next update will be accepted */
      u_HoursSinceLastWWVBUpdate = 24;

      /* Clear WWVB Valid indicator */
      z_Time.u_WWVBValidIndicator = 0;
    }

    /* Decoded WWVB frame received */
    if( u_Flags & C_ISR_FLAG_WWVB_UPDATE )
    {
      /* Update time and set WWVB valid indicator */
      updateTimeFromWWVB( &z_Time );
      z_Time.u_WWVBValidIndicator = 1;
    }

    /* Error free WWVB decode in progress */
    if( u_Flags & C_ISR_FLAG_WWVB_DECODE_IN_PROGRESS )
    {
      z_Time.u_WWVBValidIndicator = 1;
    }

    /* Failed WWVB decode */
    if( u_Flags & C_ISR_FLAG_WWVB_DECODE_FAIL )
    {
      z_Time.u_WWVBValidIndicator = 0;
    }
  }
}

/************************************************************************
* Name: PCINT2 ISR
*
* Description: Pin change interrupt handler. Pin change interrupt is fed
*              from 60Hz powerline reference
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
ISR( PCINT2_vect )
{
  static uint8_t u_HoursDebounce = 0;
  static uint8_t u_MinutesDebounce = 0;
  static uint8_t u_LastSample = 0;
  uint8_t u_CurrentSample;

  /* Identify rising edge of 60Hz clock and use to generate 1Hz tick */
  u_CurrentSample = PIND & _BV(SIXTY_HZ_CLK_BIT) ? 1 : 0;

  if( !u_LastSample && u_CurrentSample ) //Only count rising edges
  {
    /* Debounce pushbuttons */
    u_HoursDebounce = 
      ( u_HoursDebounce << 1 ) | ( PINC & _BV( HOURS_BIT ) ? 1 : 0 ) | 0xF0;
    u_MinutesDebounce = 
      ( u_MinutesDebounce << 1 ) | ( PINC & _BV( MINS_BIT ) ? 1 : 0 ) | 0xF0;

    if( u_HoursDebounce == 0xF8 )
      u_ISRFlags |= C_ISR_FLAG_HOUR_BUTTON;

    if( u_MinutesDebounce == 0xF8 )
      u_ISRFlags |= C_ISR_FLAG_MINUTE_BUTTON;    
  }

  u_LastSample = u_CurrentSample;
}

/************************************************************************
* Name: TIMER2_COMPA ISR
*
* Description: Timer 2 compare match interrupt handler. Fed from 32kHz RTC clock
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
ISR( TIMER2_COMPA_vect )
{
  WWVBDecodeResultEnumType e_Result;
  static uint8_t u_SampleCnt = 0;

  // Use the 32kHz crystal as the time source for our 1Hz tick
  if( ++u_SampleCnt == SAMPLES_PER_SEC )
  {
    u_SampleCnt = 0;
    u_ISRFlags |= C_ISR_FLAG_PPS;
  }

  /* Read current WWVB sample and feed to decode library */
  e_Result = wwvbProcessSample( WWVB_PIN & _BV( WWVB_BIT ) ? 1 : 0, 
    ( WWVBDecodedTimeStructType *)&z_wwvbTime );

  /* Only update the time if the update threshold has been passed. This is
   * to prevent potentially visible updates happening too often
   */
  if( u_HoursSinceLastWWVBUpdate > WWVB_UPDATE_THRESHOLD_HOURS )
  {
    if( e_Result == WWVB_DECODE_RESULT_SUCCESS )
    {
      /* Full message decoded successfully */
      u_ISRFlags |= C_ISR_FLAG_WWVB_UPDATE;
      u_HoursSinceLastWWVBUpdate = 0;
    }
    else
    if( e_Result == WWVB_DECODE_RESULT_IN_PROGRESS )
    {
      /* Partial message decoded successfully */
      u_ISRFlags |= C_ISR_FLAG_WWVB_DECODE_IN_PROGRESS;
    }
    else
    {
      /* Unsuccessfull decode */
      u_ISRFlags |= C_ISR_FLAG_WWVB_DECODE_FAIL;
    }
  }
}

