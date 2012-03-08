/*************************************************************************
* Title:    Routines for driving nixie tubes with a Supertex
*           HV5622 driver IC
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* File:     $Id: nixie.c 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include "nixie.h"

/* Below table maps Digits 0-9 to the bits that are being output from the driver
 * ICs. See schematic for more details
 */ 
static const uint16_t u_NixieDigitMap[] =
{ 0x0001, 0x0200, 0x0100, 0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002 };

/************************************************************************
* Name: nixieInit
*
* Description: Initialized port pins that connect to driver IC
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieInit( void )
{
  /* Nixie Drivers - PD3 to PD7 */
  NIXIE_DDR |= _BV( NIXIE_BLANK_BIT );  // Nixie Blank Output
  NIXIE_DDR |= _BV( NIXIE_POL_BIT );    // Nixie Pol Output
  NIXIE_DDR |= _BV( NIXIE_DIN_BIT );    // Nixie Din Output
  NIXIE_DDR |= _BV( NIXIE_LE_BIT );     // Nixie LE Output
  NIXIE_DDR |= _BV( NIXIE_CLK_BIT );    // Nixie Clk Output

  NIXIE_PORT |= _BV( NIXIE_POL_BIT );     // Non Inverted Mode
  NIXIE_PORT &= ~_BV( NIXIE_CLK_BIT );    // Clock Low
  NIXIE_PORT &= ~_BV( NIXIE_LE_BIT );     // LE Low
}

/************************************************************************
* Name: nixieBlank
*
* Description: Blank all nixie tubes
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieBlank( void )
{
  NIXIE_PORT &= ~_BV( NIXIE_BLANK_BIT ); 
}

/************************************************************************
* Name: nixieUnBlank
*
* Description: Unblank all nixie tubes
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieUnBlank( void )
{
  NIXIE_PORT |= _BV( NIXIE_BLANK_BIT );
}

/************************************************************************
* Name: nixieLatchWord
*
* Description: Latch shifted word into driver output buffers
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
static void nixieLatchWord( void )
{
  /* Latch Enable High */
  NIXIE_PORT |= _BV( NIXIE_LE_BIT );

  /* Latch Enable High Delay */
  _delay_ms(1);
  
  /* Latch Enable Low */
  NIXIE_PORT &= ~_BV( NIXIE_LE_BIT );

  /* Latch Enable Low Delay */
  _delay_ms(1);
}

/************************************************************************
* Name: nixieShiftWord
*
* Description: Shift a 32 bit word into driver ICs. Bits shifter out of the
*              first driver get shifted into the second 
*
* Parameters: q_Word - Word to be shifted
*
* Returns: Nothing
************************************************************************/ 
static void nixieShiftWord( uint32_t q_Word )
{
  uint8_t u_Cnt;

  for( u_Cnt = 0; u_Cnt < 32; u_Cnt++ )
  {
    if( q_Word & 0x1 )
    {
      NIXIE_PORT |= _BV( NIXIE_DIN_BIT );
    }
    else
    {
      NIXIE_PORT &= ~_BV( NIXIE_DIN_BIT );
    }

    /* Setup Time Delay */
    _delay_ms(1);

    /* Clock High */
    NIXIE_PORT |= _BV( NIXIE_CLK_BIT );

    /* Clock High Delay */
    _delay_ms(1);

    /* Clock Low */
    NIXIE_PORT &= ~_BV( NIXIE_CLK_BIT );

    /* Clock Low Delay */
    _delay_ms(1);

    /* Shift to next bit */
    q_Word >>= 1;
  }
}

/************************************************************************
* Name: nixieClear
*
* Description: Clear all bits shifted into driver IC
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieClear( void )
{
  /* Shift in 64 0 bits */
  nixieShiftWord(0);
  nixieShiftWord(0);
}

/************************************************************************
* Name: nixieUpdate
*
* Description: Update nixie digits with values in input structure
*
* Parameters: p_Vals - Pointer to structure containing nixie digit values
*
* Returns: Nothing
************************************************************************/ 
void nixieUpdate( NixieDisplayStructType *p_Vals )
{
  uint32_t q_Word;

  /* Work from rightmost nixie to leftmost */ 

  q_Word = 0;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit3 ];
  q_Word <<= 1;
  q_Word |= p_Vals->u_NixieDP2 ? 1 : 0;
  q_Word <<= 10;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit4 ];
  q_Word <<= 10;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit5 ];
  q_Word <<= 1;
  q_Word |= p_Vals->u_NixieDP3 ? 1 : 0;

  /* Shift in first word */
  nixieShiftWord( q_Word );

  q_Word = 0;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit0 ];
  q_Word <<= 10;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit1 ];
  q_Word <<= 1;
  q_Word |= p_Vals->u_NixieDP1 ? 1 : 0;
  q_Word <<= 10;
  q_Word |= u_NixieDigitMap[ p_Vals->u_NixieDigit2 ];
  q_Word <<= 1;

  /* Shift in second word */
  nixieShiftWord( q_Word );

  /* Latch shifted bits into output buffer */
  nixieLatchWord();
}

