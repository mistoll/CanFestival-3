/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN
AVR Port: Andreas GLAUSER and Peter CHRISTEN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// AVR implementation of the  CANopen timer driver, uses Timer 1 (16 bit)

#if F_CPU != 8000000
#error This file assumes a clock speed of 8MHz
#endif


// Includes for the Canfestival driver
#include "canfestival.h"
#include "timer.h"

// Define the timer registers
#define TimerAlarm        OCR1B
#define TimerCounter      TCNT1

/************************** Modul variables **********************************/
// Store the last timer value to calculate the elapsed time
static TIMEVAL last_time_set = TIMEVAL_MAX;     

void initTimer(void)
/******************************************************************************
Initializes the timer, turn on the interrupt and put the interrupt time to zero
INPUT	void
OUTPUT	void
******************************************************************************/
{
  TimerAlarm = 0;		// Set it back to the zero
	// Set timer 1 for CANopen operation tick 8us max, time is 524ms
  TCCR1B = 1 << CS11 | 1 << CS10;       // Timer 1 normal, mit CK/64
  TIMSK1 = 1 << OCIE1B;                 // Enable the interrupt
}

void setTimer(TIMEVAL value)
/******************************************************************************
Set the timer for the next alarm.
INPUT	value TIMEVAL (unsigned long)
OUTPUT	void
******************************************************************************/
{
  TimerAlarm += (int)value;	// Add the desired time to timer interrupt time
}

TIMEVAL getElapsedTime(void)
/******************************************************************************
Return the elapsed time to tell the Stack how much time is spent since last call.
INPUT	void
OUTPUT	value TIMEVAL (unsigned long) the elapsed time
******************************************************************************/
{
  unsigned int timer = TimerCounter;            // Copy the value of the running timer
  if (timer > last_time_set)                    // In case the timer value is higher than the last time.
    return (timer - last_time_set);             // Calculate the time difference
  else if (timer < last_time_set)
    return (last_time_set - timer);             // Calculate the time difference
  else
    return TIMEVAL_MAX;
}

#ifdef  __IAR_SYSTEMS_ICC__
#pragma type_attribute = __interrupt
#pragma vector=TIMER3_COMPB_vect
void TIMER1_COMPB_interrupt(void)
#else	// GCC
ISR(TIMER1_COMPB_vect)
#endif	// GCC
/******************************************************************************
Interruptserviceroutine Timer 1 Compare B for the CAN timer
******************************************************************************/
{
  last_time_set = TimerCounter;
  TimeDispatch();                               // Call the time handler of the stack to adapt the elapsed time
}



