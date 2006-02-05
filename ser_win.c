/*  
  File:         ser_win.c
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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "serial.h"


static int ser_GetBaudRate( int zBaud );
static int FlushBuffer( tsSerialPort *psSerPrt );

int ser_Open( tsSerialPort *psSerPrt, char *pacPort, int zBaud )
{
    int zRtnv = -1;
    DCB sDcb;
    COMMTIMEOUTS sTimeouts;

    /* Set comm port to use */
    psSerPrt->zComPort = atoi( pacPort + 3 );

    /* Open handle to comms port */
    psSerPrt->hCom = CreateFile( pacPort, GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING, 0, NULL );

    if( psSerPrt->hCom != INVALID_HANDLE_VALUE )
    {

        /* Update the com port parameters */
        if( 0 != GetCommState( psSerPrt->hCom, &sDcb ))
        {
            sDcb.BaudRate = ser_GetBaudRate( zBaud );
            sDcb.ByteSize = 8;
            sDcb.Parity   = NOPARITY;
            sDcb.StopBits = ONESTOPBIT;
            sDcb.fBinary  = TRUE;
            sDcb.fNull    = FALSE;
            sDcb.fDtrControl = DTR_CONTROL_DISABLE;
            sDcb.fRtsControl = RTS_CONTROL_DISABLE;
            sDcb.fOutxCtsFlow = FALSE;
            sDcb.fOutxDsrFlow = FALSE;
            sDcb.fDsrSensitivity = FALSE;
            sDcb.fTXContinueOnXoff = TRUE;
            sDcb.fOutX = FALSE;
            sDcb.fInX = FALSE;
            sDcb.fErrorChar = FALSE;
            sDcb.fAbortOnError = FALSE;
            sDcb.EofChar = 0;
            sDcb.EvtChar = 0;
        
            if( 0 != SetCommState( psSerPrt->hCom, &sDcb ))
            {
                if( 0 != GetCommTimeouts( psSerPrt->hCom, &sTimeouts ))
                {
                    sTimeouts.ReadIntervalTimeout        = 0xffffffff;
                    sTimeouts.ReadTotalTimeoutMultiplier = 0;
                    sTimeouts.ReadTotalTimeoutConstant   = 0;
        
                    SetCommTimeouts( psSerPrt->hCom, &sTimeouts );

                    zRtnv = FlushBuffer( psSerPrt );
                }
            }
        }
    }

    return( zRtnv );
}


int ser_Close( tsSerialPort *psSerPrt )
{
    if( psSerPrt->hCom != INVALID_HANDLE_VALUE )
    {
        CloseHandle( psSerPrt->hCom );
        psSerPrt->hCom = INVALID_HANDLE_VALUE;
    }

    return( 0 );
}


int ser_RxPoll( tsSerialPort *psSerPrt )
{
    int zRtnv = -1;
    DWORD Errors;
    COMSTAT Statistics;

    if( FALSE != ClearCommError( psSerPrt->hCom, &Errors, &Statistics ))
    {
        zRtnv = Statistics.cbInQue;
    }
    
    return( zRtnv );
}


int ser_Write( tsSerialPort *psSerPrt, void *pvBuff, int zLen )
{
    int zRtnv = -1;
    unsigned long lWritten = 0;

    if( !WriteFile( psSerPrt->hCom, pvBuff, zLen, &lWritten, NULL ))
    {
        lWritten = 0;
    }
    if( 0 != lWritten )
    {
        zRtnv = ( int )lWritten;
    }
    
    return( zRtnv );
}


int ser_Read( tsSerialPort *psSerPrt, void *pvBuff, int zLen, int zTimeout,
              tfSerialCallback fPktChk )
{
    int zRtnv = -1;
    int zDataAvail;
    int zTotalBytesRead = 0;
    int zBytesRead = 0;
    unsigned char *pbBuff = (unsigned char *)pvBuff;

    /* Round the Time out up to the number of milliseconds from microseconds */
    zTimeout = ( zTimeout + 10000 ) / 10000;
    
    zDataAvail = ser_RxPoll( psSerPrt );

    if( 0 == zDataAvail )
    {
        while(( zTimeout > 0 ) && ( 0 == zDataAvail ))
        {
            Sleep( 1 );
            zDataAvail = ser_RxPoll( psSerPrt );
            zTimeout--;
        }

        if(( 0 == zTimeout ) && ( 0 == zDataAvail ))
        {
            zRtnv = 0;
        }
    }

    if( 0 != zDataAvail );
    {
        while(( zTimeout > 0 ) && ( zTotalBytesRead < zLen ) &&
              ( 0 != fPktChk( pbBuff, zBytesRead )))
        {
            zDataAvail = ser_RxPoll( psSerPrt );
            if( 0 != zDataAvail )
            {
                /* Read data from port */
                zRtnv = ReadFile( psSerPrt->hCom,
                                  pbBuff + zTotalBytesRead,
                                  zLen - zTotalBytesRead,
                                  (unsigned long *)&zBytesRead, NULL );

                zTotalBytesRead += zBytesRead;
        
                if( FALSE != zRtnv )
                {
                    zRtnv = zTotalBytesRead;
                }
            }
            zTimeout--;
            Sleep( 1 );
        }
    }

    return( zRtnv );
} 



int ser_SetDtrTo( tsSerialPort *psSerPrt, int zState )
{
    if( 0 != zState )
    {
        EscapeCommFunction( psSerPrt->hCom, SETDTR );
    }
    else
    {
        EscapeCommFunction( psSerPrt->hCom, CLRDTR );
    }

    return( 0 );
}

int ser_SetRtsTo( tsSerialPort *psSerPrt, int zState )
{
    if( 0 != zState )
    {
        EscapeCommFunction( psSerPrt->hCom, SETRTS );
    }
    else
    {
        EscapeCommFunction( psSerPrt->hCom, CLRRTS );
    }

    return( 0 );
}


static int FlushBuffer( tsSerialPort *psSerPrt )
{
    int zRtnv = -1;

    /* Flush comms buffers */
    if( 0 != PurgeComm( psSerPrt->hCom, PURGE_TXABORT | PURGE_RXABORT |
                        PURGE_TXCLEAR | PURGE_RXCLEAR ))
    {
        zRtnv = 0;
    }

    return( zRtnv );
}


static int ser_GetBaudRate( int zBaud )
{
    int zRtnv = CBR_9600;

    if( zBaud <= 110 )
    {
        zRtnv = CBR_110;
    }
    else if( zBaud <= 300 )
    {
        zRtnv = CBR_300;
    }
    else if( zBaud <= 600 )
    {
        zRtnv = CBR_600;
    }
    else if( zBaud <= 1200 )
    {
        zRtnv = CBR_1200;
    }
    else if( zBaud <= 2400 )
    {
        zRtnv = CBR_2400;
    }
    else if( zBaud <= 4800 )
    {
        zRtnv = CBR_4800;
    }
    else if( zBaud <= 9600 )
    {
        zRtnv = CBR_9600;
    }
    else if( zBaud <= 14400 )
    {
        zRtnv = CBR_14400;
    }
    else if( zBaud <= 19200 )
    {
        zRtnv = CBR_19200;
    }
    else if( zBaud <= 38400 )
    {
        zRtnv = CBR_38400;
    }
    else if( zBaud <= 56000 )
    {
        zRtnv = CBR_56000;
    }
    else if( zBaud <= 57600 )
    {
        zRtnv = CBR_57600;
    }
    else if( zBaud <= 115200 )
    {
        zRtnv = CBR_115200;
    }
    else if( zBaud <= 128000 )
    {
        zRtnv = CBR_128000;
    }
    else if( zBaud <= 256000 )
    {
        zRtnv = CBR_256000;
    }

    return( zRtnv );
}
