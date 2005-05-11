/*  
  File:         ser_linux.c
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
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

#include "serial.h"

/* Private functions */
static int ser_GetLinuxBaud( int zBaud );


int ser_Open( tsSerialPort *psSerPrt, char *pacPort, int zBaud )
{
    struct termios *oldtio;
    struct termios *newtio;
    int zRtnv = -1;

    psSerPrt->fdSer = open( pacPort, O_RDWR | O_NOCTTY  );
    if( 0 < psSerPrt->fdSer )
    {
        oldtio = &psSerPrt->sOldTio;
        newtio = &psSerPrt->sNewTio;

        /* save current port settings */
        tcgetattr( psSerPrt->fdSer, oldtio );
        *newtio = *oldtio;

        /* set new port settings for raw input processing */
        newtio->c_lflag &= ~( ICANON | ECHO | ECHOCTL | ECHONL | ISIG | NOFLSH | IEXTEN );
        newtio->c_oflag &= ~( ONLCR | OPOST | OLCUC | ONOCR | OCRNL | ONLRET );
        newtio->c_iflag &= ~( ICRNL | INPCK | ISTRIP | IUCLC| IXOFF | IXON | IGNCR );
        newtio->c_cflag &= ~( HUPCL | CRTSCTS | CSIZE );
        newtio->c_cflag |= ( CS8 | CLOCAL | CREAD );
        newtio->c_cc[ VMIN ] = 1;
        newtio->c_cc[ VTIME ] = 0;
        cfsetispeed( newtio, ser_GetLinuxBaud( zBaud ));
        cfsetospeed( newtio, ser_GetLinuxBaud( zBaud ));
        tcsetattr( psSerPrt->fdSer, TCSANOW, newtio );

        tcflush( psSerPrt->fdSer, TCIFLUSH );
        ser_SetDtrTo( psSerPrt, 1 );
        ser_SetRtsTo( psSerPrt, 1 );

        zRtnv = 0;
    }

    return( zRtnv );
}


int ser_Close( tsSerialPort *psSerPrt )
{
    /* restore old port settings */
    tcsetattr( psSerPrt->fdSer, TCSANOW, &psSerPrt->sOldTio );
    close( psSerPrt->fdSer );
    psSerPrt->fdSer = 0;

    return( 0 );
}


int ser_RxPoll( tsSerialPort *psSerPrt )
{
    int zNbrBytes;
    
    ioctl( psSerPrt->fdSer, FIONREAD, &zNbrBytes );

    return( zNbrBytes );
}


int ser_Write( tsSerialPort *psSerPrt, void *pvBuff, int zLen )
{
    unsigned char *pbBuf = pvBuff;
    int zWritten = 0;
    int zWrite = 0;

    do
    {
        zWrite = write( psSerPrt->fdSer, pbBuf + zWritten, zLen - zWritten );
        if( zWritten >= 0 )
        {
            zWritten += zWrite;
        }
        else
        {
            /* What to do incase of an error */
            zWritten = zWrite;
            break;
        }
    } while( zWritten < zLen );
    usleep( 250000 );
    
    return( zWritten );
}

int ser_Read( tsSerialPort *psSerPrt, void *pvBuff, int zLen, int zTimeout,
              tfSerialCallback fPktChk )
{
    unsigned char *pbBuf = pvBuff;
    int zBytesAvail = 0;
    int zRead = 0;
    int zBytesRxd = 0;

    zBytesAvail = ser_RxPoll( psSerPrt );

    /* Round the Time out up to the number of 100 milliseconds from microseconds */
    zTimeout = ( zTimeout + 10000 ) / 10000;

    if( 0 == zBytesAvail )
    {
        while(( zTimeout > 0 ) && ( 0 == zBytesAvail ))
        {
            usleep( 10000 );
            zBytesAvail = ser_RxPoll( psSerPrt );
            zTimeout--;
        }
    }

    if( 0!= zBytesAvail )
    {
        while(( zTimeout > 0 ) && ( zBytesRxd < zLen ) &&
              ( 0 != fPktChk( pbBuf, zBytesRxd )))
        {
            /* There is data in the buffer so read it out */
            if( 0 != ser_RxPoll( psSerPrt ))
            {
                do
                {
                    zRead = read( psSerPrt->fdSer, pbBuf + zBytesRxd, zLen - zBytesRxd );
                    if( zRead > 0 )
                    {
                        zBytesRxd += zRead;
                    }
                    else
                    {
                        zBytesRxd = zRead;
                        break;
                    }
                } while( zBytesRxd < zBytesAvail );
            }

            if( zBytesRxd < 0 )
            {
                /* An error occured so get out a here to */
                break;
            }
            
            zTimeout--;
            usleep( 10000 );
        }
    }
    
    return( zBytesRxd );
}

int ser_SetDtrTo( tsSerialPort *psSerPrt, int zState )
{
    unsigned int zStatus;

    ioctl( psSerPrt->fdSer, TIOCMGET, &zStatus );

    if( 0 != zState )
    {
        zStatus |= TIOCM_DTR;
    }
    else
    {
        zStatus &= ~( TIOCM_DTR );
    }

    ioctl( psSerPrt->fdSer, TIOCMSET, &zStatus );

    return( 0 );
}


int ser_SetRtsTo( tsSerialPort *psSerPrt, int zState )
{
    unsigned int zStatus;

    ioctl( psSerPrt->fdSer, TIOCMGET, &zStatus );

    if( 0 != zState )
    {
        zStatus |= TIOCM_RTS;
    }
    else
    {
        zStatus &= ~( TIOCM_RTS );
    }

    ioctl( psSerPrt->fdSer, TIOCMSET, &zStatus );

    return( 0 );
}


static int ser_GetLinuxBaud( int zBaud )
{
    int zRtnv = B2400;
    
    if( zBaud <= 50 )
    {
        zRtnv = B50;
    }
    else if( zBaud <= 75 )
    {
        zRtnv = B75;
    }
    else if( zBaud <= 110 )
    {
        zRtnv = B110;
    }
    else if( zBaud <= 134 )
    {
        zRtnv = B134;
    }
    else if( zBaud <= 150 )
    {
        zRtnv = B150;
    }
    else if( zBaud <= 200 )
    {
        zRtnv = B200;
    }
    else if( zBaud <= 300 )
    {
        zRtnv = B300;
    }
    else if( zBaud <= 600 )
    {
        zRtnv = B600;
    }
    else if( zBaud <= 1200 )
    {
        zRtnv = B1200;
    }
    else if( zBaud <= 1800 )
    {
        zRtnv = B1800;
    }
    else if( zBaud <= 2400 )
    {
        zRtnv = B2400;
    }
    else if( zBaud <= 4800 )
    {
        zRtnv = B4800;
    }
    else if( zBaud <= 9600 )
    {
        zRtnv = B9600;
    }
    else if( zBaud <= 19200 )
    {
        zRtnv = B19200;
    }
    else if( zBaud <= 38400 )
    {
        zRtnv = B38400;
    }
    else if( zBaud <= 57600 )
    {
        zRtnv = B57600;
    }
    else if( zBaud <= 115200 )
    {
        zRtnv = B115200;
    }
    else if( zBaud <= 230400 )
    {
        zRtnv = B230400;
    }
    else if( zBaud <= 460800 )
    {
        zRtnv = B460800;
    }
    else if( zBaud <= 500000 )
    {
        zRtnv = B500000;
    }
    else if( zBaud <= 576000 )
    {
        zRtnv = B576000;
    }
    else if( zBaud <= 921600 )
    {
        zRtnv = B921600;
    }
    else if( zBaud <= 1000000 )
    {
        zRtnv = B1000000;
    }
    else if( zBaud <= 1152000 )
    {
        zRtnv = B1152000;
    }
    else if( zBaud <= 1500000 )
    {
        zRtnv = B1500000;
    }
    else if( zBaud <= 2000000 )
    {
        zRtnv = B2000000;
    }
    else if( zBaud <= 2500000 )
    {
        zRtnv = B2500000;
    }
    else if( zBaud <= 3000000 )
    {
        zRtnv = B3000000;
    }
    else if( zBaud <= 3500000 )
    {
        zRtnv = B3500000;
    }
    else if( zBaud <= 4000000 )
    {
        zRtnv = B4000000;
    }

    return( zRtnv );
}
