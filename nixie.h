/*************************************************************************
* Title:    Routines for driving nixie tubes with a Supertex
*           HV5622 driver IC
* Author:   Scott Stickeler ( sstickeler@gmail.com )
* File:     $Id: nixie.h 9 2011-11-23 04:43:33Z Scott $
**************************************************************************/

#ifndef NIXIE_H
#define NIXIE_H

#include <stdint.h>
#include <avr/io.h>

/* Configure output port and port bits below */
#define NIXIE_BLANK_BIT  3
#define NIXIE_POL_BIT    4
#define NIXIE_DIN_BIT    5
#define NIXIE_LE_BIT     6
#define NIXIE_CLK_BIT    7
#define NIXIE_PORT       PORTD
#define NIXIE_DDR        DDRD

/* Structure contains values for each of the nixie tubes and indicator lights */
typedef struct
{
  uint8_t u_NixieDigit0;
  uint8_t u_NixieDigit1;
  uint8_t u_NixieDP1;
  uint8_t u_NixieDigit2;
  uint8_t u_NixieDigit3;
  uint8_t u_NixieDP2;
  uint8_t u_NixieDigit4;
  uint8_t u_NixieDigit5;
  uint8_t u_NixieDP3;
} NixieDisplayStructType;

/************************************************************************
* Name: nixieInit
*
* Description: Initialized port pins that connect to driver IC
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieInit( void );

/************************************************************************
* Name: nixieBlank
*
* Description: Blank all nixie tubes
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieBlank( void );

/************************************************************************
* Name: nixieUnBlank
*
* Description: Unblank all nixie tubes
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieUnBlank( void );

/************************************************************************
* Name: nixieClear
*
* Description: Clear all bits shifted into driver IC
*
* Parameters: None
*
* Returns: Nothing
************************************************************************/ 
void nixieClear( void );

/************************************************************************
* Name: nixieUpdate
*
* Description: Update nixie digits with values in input structure
*
* Parameters: p_Vals - Pointer to structure containing nixie digit values
*
* Returns: Nothing
************************************************************************/ 
void nixieUpdate( NixieDisplayStructType *p_Vals );

#endif
