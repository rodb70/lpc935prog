/*  
  File:         serial.h
  Written by:   Rod Boyce
  e-mail:       rod@boyce.net.nz

  This file is part of lpc935-prog
  
  lpc935-prog is free software; you can redistribute it and/or modify
  it under the terms of the Lesser GNU General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  lpc935-prog is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser
  GNU General Public License for more details.

  You should have received a copy of the Lesser GNU General Public
  License along with lpc935-prog; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA
*/
#ifndef SERIAL_H
#define SERIAL_H

#ifdef LINUX
typedef struct
{
    int fdSer;
    struct termios sOldTio;
    struct termios sNewTio;
} tsSerialPort;
#else
#if defined(WINDOWS) || defined(WIN32) ||defined(_WIN32)
typedef struct
{
    HANDLE hCom;     /* Com port handle */
    int zComPort;
} tsSerialPort;
#endif
#endif

typedef int (*tfSerialCallback)(void *, int);

int ser_Open( tsSerialPort *psSerPrt, char *pacPort, int zBaud );
int ser_Close( tsSerialPort *psSerPrt );
int ser_RxPoll( tsSerialPort *psSerPrt );
int ser_Write( tsSerialPort *psSerPrt, void *pvBuff, int zLen );
int ser_Read( tsSerialPort *psSerPrt, void *pvBuff, int zLen, int zTimeout,
              tfSerialCallback fPktChk );

int ser_SetDtrTo( tsSerialPort *psSerPrt, int zState );
int ser_SetRtsTo( tsSerialPort *psSerPrt, int zState );

#endif
