/*  
  File:         lpc935-prog.c
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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <popt.h>
#ifdef LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif
#if defined(WINDOWS) || defined(WIN32) ||defined(_WIN32)
#include <windows.h>
#include <time.h>
#endif

#include "ihex.h"
#include "serial.h"

/* Defines for the reset and power down logic */
#define LN_LO (0)
#define LN_HI (1)
#define PWR_OFF LN_HI
#define PWR_ON  LN_LO
#define RST_HI LN_LO
#define RST_LO LN_HI

/* Defines for the Auto baud resync */
#define BAUD_SYNC_ERR_CNT 4
#define AUTO_BAUD_CHAR 'U'
#define AUTO_BAUD_STR "U"

/* Defines from the user manual for the LPC935 From Table 19-2 page 147 */

/* Record types */
/* 00 Program used program code memory */
/* 01 Read version ID */
#define READ_VERSION_ID 0x01
/* 02 Misc write functions */
#define MISC_WRITE_FN 0x02
#define PUT_UCFG1 0x00
#define PUT_BOOTV 0x02
#define PUT_STATB 0x03
#define PUT_SECB0 0x08
#define PUT_SECB1 0x09
#define PUT_SECB2 0x0a
#define PUT_SECB3 0x0b
#define PUT_SECB4 0x0c
#define PUT_SECB5 0x0d
#define PUT_SECB6 0x0e
#define PUT_SECB7 0x0f
/* #define PUT_CCP   0x10 This is a guess based on an error in the manual */
/* 03 Misc read functions */
#define MISC_READ_FN 0x03
#define GET_UCFG1 0x00
#define GET_BOOTV 0x02
#define GET_STATB 0x03
#define GET_SECB0 0x08
#define GET_SECB1 0x09
#define GET_SECB2 0x0a
#define GET_SECB3 0x0b
#define GET_SECB4 0x0c
#define GET_SECB5 0x0d
#define GET_SECB6 0x0e
#define GET_SECB7 0x0f
#define GET_MANID 0x10
#define GET_DEVID 0x11
#define GET_DERID 0x12
/* 04 Erase sector page */
#define ERASE_SECTOR_PAGE 0x04
#define DO_PAGE   0x00
#define DO_SECTOR 0x01
/* 05 Read sector CRC */
#define READ_SECTOR_CRC 0x05
/* 06 Read global CRC */
#define READ_GLOBAL_CRC 0x06
/* 07 Direct load of baud rate */
#define DIRECT_LOAD_BAUD_RATE 0x07
/* 08 Reset MCU */
#define RESET_MCU 0x08


typedef enum
{
    eWDTE = 0x80,  /**< Watchdog time enable */
    eRPE = 0x40, /**< Reset pin enable */
    eBOE = 0x20, /**< Brownout detect enable */
    eWDSE = 0x10, /**< Watchdos Safely enable bit */
    eFOSC2 = 0x04, /**< Oscillator configuration bit 2 */
    eFOSC1 = 0x02, /**< Oscillator configuration bit 1 */
    eFOSC0 = 0x01 /**< Oscillator configuration bit 0 */
} teUCFG1_BITS;

typedef enum
{
    eEDISx = 0x04, /**< Disable the ability to erase the sector prtected by this register  */
    eSPEDISx = 0x02, /**< Disable the ability to program or erase this sector */
    eMOVCDISx = 0x01 /**< Disable the movc instruction for this sector */
} teSECx;

typedef enum
{
    eDCCP = 0x80, /**< Disable Clear Configuration Protection command */
    eCWP = 0x40, /**< Configuration Write protect bit. */
    eAWP = 0x20, /**< Activate Write protect bit. */
    eBSB = 0x01 /**< Boot Status Bit. */
} teBOOTSTAT;


typedef enum
{
    eNO_COMMAND, /**< Ignore the first command could be used for a default state */
    eREAD, /**< Command read from the micro-controller */
    eERASE, /**< Erase either a sector of a chip */
    eRESET, /**< Reset the mocro-controller */
    eWRITE, /**< Write to a flash based register on the chip */
    ePROG, /**< Progrqam a file to the micro-controller */
    
    eNO_MORE_COMMANDS /**< End of list token ignore all commands greater than this */
} tePROG_COMMAND;

int zBaud = 4800; /**< Baud rate to talk to the micro with */
int zShowDebug = 0; /**< If set then print out lots of debug infomation */
int zSecBytex = -1; /**< The security byte to read */
int zOperAddr = 0; /**< The address that a operation will be performed on */
char *pacComPort; /**< The communications port to used to talk to the micro */
char *pacHexFile; /**< The hex filename to program into the microcontroller */
char *pacSubCommand = NULL; /**< This is the sub command that is required */

tePROG_COMMAND eProgCommand; /**< The command to perform on the micro-controller */

/* These enums and strings must be kept in sync */
typedef enum
{
    READ_IDS = 0, READ_VER, RW_STATB, RW_BOOTV,
    RW_UCFG1, RW_SECX, READ_GCRC, READ_SCRC,

    ERASE_SECTOR, ERASE_PAGE,
    
    END_OF_SUBCMD_LIST
} teSUB_COMMAND_ID;

char *pacCommandList[] =
{
    "ids", "version", "statb", "bootv", "ucfg1", "secx", "gcrc", "scrc",
    "sector", "page",

    NULL
};
    

/* This is the popt struct that defines the main commands that are performed when reading the
   command line input.
 */
struct poptOption optionsTable[] =
{
    { "prog", 'g', POPT_ARG_NONE, &pacSubCommand, ePROG,
      "Program an intel hex file to micro", "" },

    { "write", 'w', POPT_ARG_STRING, &pacSubCommand, eWRITE,
      "Write a control register to the micro", "ucfg1|bootv|statb" },

    { "read", 'r', POPT_ARG_STRING, &pacSubCommand, eREAD,
      "Read a control register from the micro", "ids|version|statb|bootv|ucfg1|secx|gcrc|scrc" },
    
    { "erase", 'e', POPT_ARG_STRING, &pacSubCommand, eERASE,
      "Erase a sector or page from the flash", "sector|page" },
    
    { "reset", 's', POPT_ARG_NONE, 0, eRESET, "Reset the micro-controller", NULL },
    
    { "address", 'a', POPT_ARG_INT, &zOperAddr, 0, "Sector address for Op", "SECTOR" },
    { "data", 'd', POPT_ARG_INT, &zSecBytex, 0, "Data byte to write to the micro", "DATA" },

    { "baud", 'b', POPT_ARG_INT, &zBaud, 0, "baud rate to communicate with", "BAUD" },
    { "port", 'p', POPT_ARG_STRING, &pacComPort, 0, "Communications port to use", "PORT" },

    { "verbose", 'v', POPT_ARG_NONE, &zShowDebug, 0, "Print out debug infomation", 0 },

    POPT_AUTOHELP
    POPT_TABLEEND
};


/* Private local functions */
static void usage( poptContext optCon, int exitcode, char *error, char *addl );
static unsigned char lpc_GetReplyByte( char *pacTxd, char *pacRxd );
static unsigned long lpc_GetReplyLong( char *pacTxd, char *pacRxd );
static void debug_printf( const char *format, ... );
static int lpc_ReadIds( tsSerialPort *psSerPrt );
static int lpc_ReadUcfg1( tsSerialPort *psSerPrt );
static int lpc_ReadBootV( tsSerialPort *psSerPrt );
static int lpc_ReadStatB( tsSerialPort *psSerPrt );
static int lpc_ReadSecX( tsSerialPort *psSerPrt, unsigned char bSecX );
static int lpc_ReadVersion( tsSerialPort *psSerPrt );
static int lpc_ReadGlobalCrc( tsSerialPort *psSerPrt );
static int lpc_ReadSectorCrc( tsSerialPort *psSerPrt, unsigned short wSectorAddr );
static int lpc_EraseSector( tsSerialPort *psSerPrt, unsigned short wSectorAddr );
static int lpc_ErasePage( tsSerialPort *psSerPrt, unsigned short wPageAddr );
static int lpc_Reset( tsSerialPort *psSerPrt );
static int lpc_WriteUcfg1( tsSerialPort *psSerPrt, unsigned char bNewCfg1 );
static int lpc_WriteBootV( tsSerialPort *psSerPrt, unsigned char bNewBootV );
static int lpc_WriteStatB( tsSerialPort *psSerPrt, unsigned char bNewStatB );
static int lpc_WriteSecx( tsSerialPort *psSerPrt, unsigned char bSecxReg, unsigned char bSecxDat );
static int lpc_PlaceInBootLoaderMode( tsSerialPort *psSerPrt );
static int lpc_SyncBaud( tsSerialPort *psSerPrt );
static int lpc_Program( tsSerialPort *psSerPrt, char *pacFilename );
static void udelay( int uS );
int lpc_RxdPacket( void *pvBuf, int zLen );


int main( const int argc, const char **argv)
{
    poptContext optCon; /* context for parsing command-line options */
    char c; /* used for argument parsing */
    tsSerialPort sSerPrt;
    char *pacArg;
    unsigned char bDat;

    
    eProgCommand = ePROG;
    optCon = poptGetContext( NULL, argc, argv, optionsTable, 0 );
    poptSetOtherOptionHelp( optCon, "[OPTIONS]* <filename>" );
    
    if (argc < 2)
    {
        poptPrintUsage( optCon, stdout, 0 );
        exit( 1 );
    }
    
    /* Now do options processing, get portname */
    while(( c = poptGetNextOpt( optCon )) >= 0 )
    {
        switch( c )
        {
          case( eWRITE ) :
              eProgCommand = eWRITE;
              break;

          case( eREAD ) :
              eProgCommand = eREAD;
              break;

          case( eERASE ) :
              eProgCommand = eERASE;
              break;

          case( eRESET ) :
              eProgCommand = eRESET;
              pacSubCommand = "reset";
              break;

          case( ePROG ) :
              eProgCommand = ePROG;
              pacSubCommand = "program";
              break;
        }
    }

    if((pacSubCommand != NULL ) && ( -1 != ser_Open( &sSerPrt, pacComPort, zBaud )))
    {
        /* Once the serial port is opened it must also power up the board and force entry into the
           boodloader mode */
        if( 0 == lpc_PlaceInBootLoaderMode( &sSerPrt ))
        {
            debug_printf( "Micro placed in boot loader mode sucessfully\n" );
        }
        else
        {
            debug_printf( "Failed to place micro in bootloader mode\n" );
        }
        
        /* Then perform the required command */
        switch( eProgCommand )
        {
          case( ePROG ) :
              pacArg = (void *)poptGetArg( optCon );
              lpc_Program( &sSerPrt, pacArg );
              break;
              
          case( eWRITE ) :
              pacArg = (void *)poptGetArg( optCon );
              if( 0 == strcasecmp( pacCommandList[ RW_UCFG1 ], pacSubCommand ))
              {
                  bDat = strtol( pacArg, NULL, 0 );
                  lpc_WriteUcfg1( &sSerPrt, bDat );
              }
              else if( 0 == strcasecmp( pacCommandList[ RW_BOOTV ], pacSubCommand ))
              {
                  bDat = strtol( pacArg, NULL, 0 );
                  lpc_WriteBootV( &sSerPrt, bDat );
              }
              else if( 0 == strcasecmp( pacCommandList[ RW_STATB ], pacSubCommand ))
              {
                  bDat = strtol( pacArg, NULL, 0 );
                  lpc_WriteStatB( &sSerPrt, bDat );
              }
              else if(( 0 == strcasecmp( pacCommandList[ RW_SECX ], pacSubCommand )) &&
                      ( -1 != zSecBytex ))
              {
                  bDat = strtol( pacArg, NULL, 0 );
                  lpc_WriteSecx( &sSerPrt, zSecBytex, bDat );
              }
              else
              {
                  printf( "Unknown write sub-command: %s\n", pacSubCommand );
              }
              break;

          case( eREAD ) :
              if( 0 == strcasecmp( pacCommandList[ READ_IDS ], pacSubCommand ))
              {
                  lpc_ReadIds( &sSerPrt );
              }
              else if( 0 == strcasecmp( pacCommandList[ READ_VER ], pacSubCommand ))
              {
                  lpc_ReadVersion( &sSerPrt );
              }
              else if( 0 == strcasecmp( pacCommandList[ RW_STATB ], pacSubCommand ))
              {
                  lpc_ReadStatB( &sSerPrt );
              }
              else if( 0 == strcasecmp( pacCommandList[ RW_BOOTV ], pacSubCommand ))
              {
                  lpc_ReadBootV( &sSerPrt );
              }
              else if( 0 == strcasecmp( pacCommandList[ RW_UCFG1 ], pacSubCommand ))
              {
                  lpc_ReadUcfg1( &sSerPrt );
              }
              else if(( 0 == strcasecmp( pacCommandList[ RW_SECX ], pacSubCommand )) &&
                      ( -1 != zSecBytex ))
              {
                  lpc_ReadSecX( &sSerPrt, zSecBytex );
              }
              else if( 0 == strcasecmp( pacCommandList[ READ_GCRC ], pacSubCommand ))
              {
                  lpc_ReadGlobalCrc( &sSerPrt );
              }
              else if( 0 == strcasecmp( pacCommandList[ READ_SCRC ], pacSubCommand ))
              {
                  lpc_ReadSectorCrc( &sSerPrt, zOperAddr );
              }
              else
              {
                  printf( "Unknown read sub-command: %s\n", pacSubCommand );
              }
              break;

          case( eERASE ) :
              if( 0 == strcasecmp( pacCommandList[ ERASE_SECTOR ], pacSubCommand ))
              {
                  lpc_EraseSector( &sSerPrt, zOperAddr );
              }
              else if( 0 == strcasecmp( pacCommandList[ ERASE_PAGE ], pacSubCommand ))
              {
                  lpc_ErasePage( &sSerPrt, zOperAddr );
              }
              else
              {
                  printf( "Unknown erase sub-command: %s\n", pacSubCommand );
              }
              break;

          case( eRESET ) :
              lpc_Reset( &sSerPrt );
              break;

          default :
              printf( "Command not implemented yet\n" );
              break;
        }
    }

    ser_Close( &sSerPrt );
    
    return( 0 );
}


static void usage( poptContext optCon, int zExitCode, char *pacError, char *pacAddl )
{
    poptPrintUsage( optCon, stdout, 0 );
    if( 0 != pacError )
    {
        printf( "%s: %s", pacError, pacAddl );
    }
    
    exit( zExitCode );
}    


static unsigned char lpc_GetReplyByte( char *pacTxd, char *pacRxd )
{
    unsigned char bRplyByte = 0;
    char *pacRply;
    
    if(( 0 < strlen( pacRxd )) &&
       ( '.' == *( pacRxd + strlen( pacRxd ) - 3 )) &&
       ( 0 == strncasecmp( pacTxd, pacRxd, strlen( pacTxd ))))
    {
        /* Received reply and the replay was OK */
        pacRply = pacRxd + strlen( pacTxd ); 
        bRplyByte = nibble( *( pacRply )) << 4;
        bRplyByte |= nibble( *( pacRply + 1 ));
    }

    return( bRplyByte );
}


static unsigned long lpc_GetReplyLong( char *pacTxd, char *pacRxd )
{
    unsigned long lRplyLong = 0xbad0bad0;
    char *pacRply;
    int i;

    if(( 0 < strlen( pacRxd )) &&
       ( '.' == *( pacRxd + strlen( pacRxd ) - 3 )) &&
       ( 0 == strncasecmp( pacTxd, pacRxd, strlen( pacTxd ))))
    {
        pacRply = pacRxd + strlen( pacTxd );
        for( i = 0; i < 8; i++ )
        {
            lRplyLong <<= 4;
            lRplyLong |= nibble( *( pacRply + i ));
        }
    }

    return( lRplyLong );
}


static void debug_printf( const char *pacFormat, ... )
{
    char p[ 2048 ];
    va_list ap;

    if( 0 != zShowDebug )
    {

        /* Try to print in the allocated space. */
        va_start( ap, pacFormat );
        vsnprintf( p, sizeof( p ), pacFormat, ap );
        va_end( ap );

        printf( p );
        fflush( stdout );
    }
}


static int lpc_ReadIds( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize;
        
    debug_printf( "Read lpc935 system ids from port %s baud = %d\n", pacComPort, zBaud );

    /* Read manufacture id */
    bDat = GET_MANID;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );
        printf( "Manufacture Id: 0x%02x\n", bDat );
    }

    /* Read device id */
    bDat = GET_DEVID;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );
        printf( "Device Id:..... 0x%02x\n", bDat );
    }

    /* read derivitive id */
    bDat = GET_DERID;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );
        printf( "Derivative Id:. 0x%02x\n", bDat );
    }

    return( 0 );
}


static int lpc_ReadUcfg1( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize;
        
    debug_printf( "Read lpc935 system UCFG1 from port %s baud = %d\n", pacComPort, zBaud );

    /* Read manufacture id */
    bDat = GET_UCFG1;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );
        printf( "UCFG1 returned is 0x%02x\nDecoding...\n", bDat );
        if(( bDat & eWDTE ) == eWDTE )
        {
            printf( "WDTE is enabled\n" );
        }
        else
        {
            printf( "WDTE is disabled\n" );
        }
        if(( bDat & eRPE ) == eRPE )
        {
            printf( "RPE  is enabled\n" );
        }
        else
        {
            printf( "RPE  is disabled\n" );
        }
        if(( bDat & eBOE ) == eBOE )
        {
            printf( "BOE  is enabled\n" );
        }
        else
        {
            printf( "BOE  is disabled\n" );
        }
        if(( bDat & eWDSE ) == eWDSE )
        {
            printf( "WDSE is enabled\n" );
        }
        else
        {
            printf( "WDSE is disabled\n" );
        }

        printf( "Oscillator configuration is:\n" );
        switch( bDat & ( eFOSC2 | eFOSC1 | eFOSC0 ))
        {
          case( 7 ) :
              printf( "    External input on XTAL1\n" );
              break;

          case( 4 ) :
              printf( "    Watchdog Oscillator, 400 kHz (+20/ -30% tolerance).\n" );
              break;

          case( 3 ) :
              printf( "    Internal RC oscillator, 7.373 MHz +-2.5%.\n" );
              break;

          case( 2 ) :
              printf( "    Low frequency crystal, 20 kHz to 100 kHz.\n" );
              break;

          case( 1 ) :
              printf( "    Medium frequency crystal or resonator, 100 kHz to 4 MHz.\n" );
              break;

          case( 0 ) :
              printf( "    High frequency crystal or resonator, 4 MHz to 12 MHz.\n" );
              break;

          default :
              printf( "    Bad FOSC value we've got problems here...(hk stv)\n" );
              break;
        }
    }

    return( 0 );
}


static int lpc_ReadBootV( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize;

    debug_printf( "Read lpc935 boot vector from port %s baud = %d\n", pacComPort, zBaud );

    /* Read manufacture id */
    bDat = GET_BOOTV;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );

        printf( "The boot vector returned was: 0x%02x\n", bDat );
    }

    return( 0 );
}


static int lpc_ReadStatB( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize = 0;

    debug_printf( "Read lpc935 status byte from port %s baud = %d\n", pacComPort, zBaud );

    /* Read manufacture id */
    bDat = GET_STATB;
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s - len=%d\n", acRply, bReplySize );
    if( 0 < bReplySize )
    {
        bDat = lpc_GetReplyByte( acIhexStr, acRply );
        printf( "Status byte is 0x%02x\nDecoding...\n", bDat );
        if(( bDat & eDCCP ) == eDCCP )
        {
            printf( "DCCP (Disable Clear Configuration Protection) bit is set\n" );
        }
        else
        {
            printf( "DCCP (Disable Clear Configuration Protection) bit is clear\n" );
        }
        if(( bDat & eCWP ) == eCWP )
        {
            printf( "CWP (Configuration Write Protect) bit is set\n" );
        }
        else
        {
            printf( "CWP (Configuration Write Protect) bit is clear\n" );
        }
        if(( bDat & eAWP ) == eAWP )
        {
            printf( "AWP (Activate Write Protection) bit is set\n" );
        }
        else
        {
            printf( "AWP (Activate Write Protection) bit is clear\n" );
        }
        if(( bDat & eBSB ) == eBSB )
        {
            printf( "BSB (Boot Status) bit is set\n" );
        }
        else
        {
            printf( "BSB (Boot Status) bit is clear\n" );
        }
    }

    return( 0 );
}



static int lpc_ReadSecX( tsSerialPort *psSerPrt, unsigned char bSecX )
{
    unsigned char abSecByteAddr[] = { GET_SECB0, GET_SECB1, GET_SECB2, GET_SECB3,
                                      GET_SECB4, GET_SECB5, GET_SECB6, GET_SECB7 };
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize;

    debug_printf( "Read lpc935 security byte from port %s baud = %d\n", pacComPort, zBaud );

    if( bSecX < sizeof( abSecByteAddr ))
    {
        /* Read security byte X */
        bDat = abSecByteAddr[ bSecX ];
        snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_READ_FN, &bDat, sizeof( bDat ), 0 );
        debug_printf( "Sending %s\n", acIhexStr );
        memset( acRply, 0, sizeof( acRply ));
        ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
        bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
        debug_printf( "Read %s\n", acRply );
        if( 0 < bReplySize )
        {
            bDat = lpc_GetReplyByte( acIhexStr, acRply );
            printf( "SEC%d readback 0x%02x\nDecoding...\n", bSecX, bDat );
            if(( bDat & eEDISx ) == eEDISx )
            {
                printf( "The Erase Disable bit for sector %d is set.\n", bSecX );
            }
            else
            {
                printf( "The Erase Disable bit for sector %d is clear.\n", bSecX );
            }
            if(( bDat & eSPEDISx ) == eSPEDISx )
            {
                printf( "The Sector Program Erase Disable for sector %d is set.\n", bSecX );
            }
            else
            {
                printf( "The Sector Program Erase Disable for sector %d is clear.\n", bSecX );
            }
            if(( bDat & eMOVCDISx ) == eMOVCDISx )
            {
                printf( "The MOVC Disable for sector %d is set.\n", bSecX );
            }
            else
            {
                printf( "The MOVC Disable for sector %d is clear\n", bSecX );
            }
        }
    }
    
    return( 0 );
}


static int lpc_ReadVersion( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;

    debug_printf( "Read lpc935 version number from port %s baud = %d\n", pacComPort, zBaud );

    /* Read security byte X */
    snintel_hex( acIhexStr, sizeof( acIhexStr ), READ_VERSION_ID, 0, 0, 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s - size = %d\n", acRply, bReplySize );
    if( 0 < bReplySize )
    {
        printf( "Version String returned was %s", &acRply[ strlen( acIhexStr )]);
    }
    
    return( 0 );
}

static int lpc_ReadGlobalCrc( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;

    debug_printf( "Read lpc935 global CRC from port %s baud = %d\n", pacComPort, zBaud );

    /* Read security byte X */
    snintel_hex( acIhexStr, sizeof( acIhexStr ), READ_GLOBAL_CRC, 0, 0, 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s - size = %d\n", acRply, bReplySize );
    if( 0 < bReplySize )
    {
        printf( "Global chip CRC is: 0x%08x\n", lpc_GetReplyLong( acIhexStr, acRply ));
    }
    
    return( 0 );
}


static int lpc_ReadSectorCrc( tsSerialPort *psSerPrt, unsigned short wSectorAddr )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bDat;
    unsigned char bReplySize;

    debug_printf( "Read lpc935 sector CRC from port %s baud = %d\n", pacComPort, zBaud );

    if( wSectorAddr > 0xff )
    {
        /* If usig a full address just use hi-byte */
        wSectorAddr >>= 8;
    }
    bDat = wSectorAddr & 0xff;
    
    /* Read sector CRC */
    snintel_hex( acIhexStr, sizeof( acIhexStr ), READ_SECTOR_CRC, &bDat, sizeof( bDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s - size = %d\n", acRply, bReplySize );
    if( 0 < bReplySize )
    {
        printf( "Sector 0x%02x00 CRC is: 0x%08x\n", wSectorAddr,
                lpc_GetReplyLong( acIhexStr, acRply ));
    }
    
    return( 0 );
}


static int lpc_EraseSector( tsSerialPort *psSerPrt, unsigned short wSectorAddr )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 3 ];
    
    debug_printf( "Read lpc935 security byte from port %s baud = %d\n", pacComPort, zBaud );

    /* Erase sector */
    abDat[ 0 ] = DO_SECTOR; /* Command */
    abDat[ 1 ] = ( wSectorAddr >> 8 ) & 0xff; /* Hi-byte */
    abDat[ 2 ] = wSectorAddr & 0xff; /* Lo-byte */
    
    snintel_hex( acIhexStr, sizeof( acIhexStr ), ERASE_SECTOR_PAGE, abDat, sizeof( abDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s - len = %d\n", acRply, bReplySize );

    return( 0 );
}


static int lpc_ErasePage( tsSerialPort *psSerPrt, unsigned short wPageAddr )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 3 ];

    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */
    
    debug_printf( "Read lpc935 security byte from port %s baud = %d\n", pacComPort, zBaud );

    /* Erase page */
    abDat[ 0 ] = DO_PAGE; /* Command */
    abDat[ 1 ] = ( wPageAddr >> 8 ) & 0xff; /* Hi-byte */
    abDat[ 2 ] = wPageAddr & 0xff; /* Lo-byte */

    snintel_hex( acIhexStr, sizeof( acIhexStr ), ERASE_SECTOR_PAGE, abDat, sizeof( abDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );

    return( 0 );
}


static int lpc_Reset( tsSerialPort *psSerPrt )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;

    debug_printf( "Reset the micro-controller on port %s baud = %d\n", pacComPort, zBaud );
    snintel_hex( acIhexStr, sizeof( acIhexStr ), RESET_MCU, 0, 0, 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );

    return( 0 );
}


static int lpc_WriteUcfg1( tsSerialPort *psSerPrt, unsigned char bNewCfg1 )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 2 ];

    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */

    abDat[ 0 ] = PUT_UCFG1;
    abDat[ 1 ] = bNewCfg1;

    debug_printf( "set the UCFG1 register to 0x%02x on port %s baud = %d\n",
                  bNewCfg1, pacComPort, zBaud );
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_WRITE_FN, abDat, sizeof( abDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );

    return( 0 );
}


static int lpc_WriteBootV( tsSerialPort *psSerPrt, unsigned char bNewBootV )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 2 ];

    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */

    abDat[ 0 ] = PUT_BOOTV;
    abDat[ 1 ] = bNewBootV;

    debug_printf( "set the BOOTV register to 0x%02x on port %s baud = %d\n",
                  bNewBootV, pacComPort, zBaud );
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_WRITE_FN, abDat, sizeof( abDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );

    return( 0 );
}


static int lpc_WriteStatB( tsSerialPort *psSerPrt, unsigned char bNewStatB )
{
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 2 ];

    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */

    abDat[ 0 ] = PUT_STATB;
    abDat[ 1 ] = bNewStatB;

    debug_printf( "set the STATB register to 0x%02x on port %s baud = %d\n",
                  bNewStatB, pacComPort, zBaud );
    snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_WRITE_FN, abDat, sizeof( abDat ), 0 );
    debug_printf( "Sending %s\n", acIhexStr );
    memset( acRply, 0, sizeof( acRply ));
    ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
    bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
    debug_printf( "Read %s\n", acRply );

    return( 0 );
}


static int lpc_WriteSecx( tsSerialPort *psSerPrt, unsigned char bSecxReg, unsigned char bSecxDat )
{
    unsigned char abSecCmd[] = { PUT_SECB0, PUT_SECB1, PUT_SECB2, PUT_SECB3,
                                 PUT_SECB4, PUT_SECB5, PUT_SECB6, PUT_SECB7 };
    char acIhexStr[ 20 ];
    char acRply[ 100 ];
    unsigned char bReplySize;
    unsigned char abDat[ 2 ];

    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */

    abDat[ 0 ] = abSecCmd[ bSecxReg ];
    abDat[ 1 ] = bSecxDat;

    debug_printf( "set the Sec%d register to 0x%02x on port %s baud = %d\n",
                  bSecxReg, bSecxDat, pacComPort, zBaud );
    if( bSecxReg < sizeof( abSecCmd ))
    {
        snintel_hex( acIhexStr, sizeof( acIhexStr ), MISC_WRITE_FN, abDat, sizeof( abDat ), 0 );
        debug_printf( "Sending %s\n", acIhexStr );
        memset( acRply, 0, sizeof( acRply ));
        ser_Write( psSerPrt, acIhexStr, strlen( acIhexStr ));
        bReplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 1000000, lpc_RxdPacket );
        debug_printf( "Read %s\n", acRply );
    }
    
    return( 0 );
}


static int lpc_PlaceInBootLoaderMode( tsSerialPort *psSerPrt )
{
    int zRtnv = -1;
    
    /*ser_SetDtrTo( psSerPrt, PWR_ON );
    ser_SetRtsTo( psSerPrt, RST_HI );
    udelay( 500000 );
    ser_SetDtrTo( psSerPrt, PWR_OFF );
    udelay( 500000 );
    ser_SetDtrTo( psSerPrt, PWR_ON );
    udelay( 500000 );
    zRtnv = lpc_SyncBaud( psSerPrt );*/
    
    if( 0 != zRtnv  )
    {
        /*
          Ok so hardware Activation of the bootloader is to power down the board
          wait a bit then power up the board with the reset line low and then toggling
          the reset line three times
          VDD XXX\_______/--------------------------------- DTR
          RST XXX_________/-\_/-\_/-\_/-------------------- RTS

          see section Hardware activation of the Boot Loader page 145 of the user manual
        */

        ser_SetDtrTo( psSerPrt, PWR_OFF ); /* power off */
        ser_SetRtsTo( psSerPrt, RST_LO ); /* reset low */
        udelay( 1000000 );
        
        ser_SetDtrTo( psSerPrt, PWR_ON ); /* power up */
        udelay( 100000 );
        ser_SetRtsTo( psSerPrt, RST_HI ); /* reset hi */
        
        udelay( 16 );

        ser_SetRtsTo( psSerPrt, RST_LO ); /* reset lo 1 */
        udelay( 48 );
        ser_SetRtsTo( psSerPrt, RST_HI ); /* reset hi */
        
        udelay( 16 );
        
        ser_SetRtsTo( psSerPrt, RST_LO ); /* reset lo 2 */
        udelay( 48 );
        ser_SetRtsTo( psSerPrt, RST_HI ); /* reset hi */
        
        udelay( 16 );
        
        ser_SetRtsTo( psSerPrt, RST_LO ); /* reset lo 3 */
        udelay( 48 );
        ser_SetRtsTo( psSerPrt, RST_HI ); /* reset hi */

        udelay( 100 );
        zRtnv = lpc_SyncBaud( psSerPrt );
    }
    
    return( zRtnv );
}


static int lpc_SyncBaud( tsSerialPort *psSerPrt )
{
    unsigned char cRply = 0;
    int zErrCnt = 0;
    int zRtnv = -1;

    /* Discovered by trial and error and then a big margin for error.
       It normaly takes two goes from Hyperterm so I'll try four times
       and then error out */
    while(( cRply != AUTO_BAUD_CHAR ) && ( zErrCnt < BAUD_SYNC_ERR_CNT ))
    {
        ser_Write( psSerPrt, AUTO_BAUD_STR, 1 );
        ser_Read( psSerPrt, &cRply, 1, 250000, lpc_RxdPacket );
        zErrCnt++;
    }
    
    if( AUTO_BAUD_CHAR == cRply )
    {
        zRtnv = 0;
    }
    debug_printf( " Received %c - %d - 0x%02x ErrCnt = %d\n", cRply,
                  cRply & 0xff, cRply & 0xff, zErrCnt );

    return( zRtnv );
}


#if 0
/* This is some old code that I was not happy with I have already got a
   module that reads in intel hex files and converts it to a binary
   mode so why not use it so see below */
static int lpc_Program( tsSerialPort *psSerPrt, char *pacFilename )
{
    FILE *tInFile;
    int zRtnv = -1;
    char acLineBuf[ 1024 ];
    char acRply[ 1024 ];
    int zRplySize;
    
    zShowDebug = 1; /* Switch on debug I don't know what is going to happen */
    debug_printf( "Program micro with file %s\n", pacFilename );
    if(( tInFile = fopen( pacFilename, "r" )) != NULL )
    {
        while( fgets( acLineBuf, sizeof( acLineBuf ), tInFile ) != NULL )
        {
            ser_Write( psSerPrt, acLineBuf, strlen( acLineBuf ));
            debug_printf( "Written: %s\n", acLineBuf );
            memset( acLineBuf, 0, sizeof( acLineBuf ));
            memset( acRply, 0, sizeof( acRply ));
            zRplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 2000000, lpc_RxdPacket );
            debug_printf( "Read:    %s\n", acRply );
        }
    }

    return( zRtnv );
}
#else
static int lpc_Program( tsSerialPort *psSerPrt, char *pacFilename )
{
    static unsigned char abRom[ 65536 ]; /* overkill but you never know */
    int zRtnv = -1;
    char acLineBuf[ 1024 ];
    char acRply[ 1024 ];
    int zFileSize;
    int i;
    int zRplySize;
    
    memset( abRom, 0xff, sizeof( abRom ));

    zFileSize = read_intel_hex( pacFilename, abRom, sizeof( abRom ));
    if( 0 < zFileSize )
    {
        printf( "Program chip file size is: %d - 0x%04x\n", zFileSize, zFileSize );
        for( i = 0; i < zFileSize; i += 16 )
        {
            snintel_hex( acLineBuf, sizeof( acLineBuf ), 0, &abRom[ i ], 16, i );
            ser_Write( psSerPrt, acLineBuf, strlen( acLineBuf ));
            debug_printf( "Written: %s\n", acLineBuf );
            memset( acLineBuf, 0, sizeof( acLineBuf ));
            memset( acRply, 0, sizeof( acRply ));
            
            zRplySize = ser_Read( psSerPrt, acRply, sizeof( acRply ), 2000000, lpc_RxdPacket );
            debug_printf( "Read:    %s", acRply );

            if( 0 < zRplySize )
            {
                printf( "%c", acRply[ strlen( acRply ) - 3 ]);
                fflush( stdout );
            }
        }
        printf( "\n" );
    }

    return( zRtnv );
}
#endif

#if defined(WINDOWS) || defined(WIN32) ||defined(_WIN32)
static void udelay( int uS )
{
    LARGE_INTEGER llPerfCount;
    LONGLONG llPerfCount2;
    LONGLONG llCurUsec;
    LONGLONG llDestUsec;

    static int qFirstTime = 1;
    static LARGE_INTEGER llPerfFreq;
    static LONGLONG llPerfFreq2;

    /* First time through, get frequency of high-resolution
       performance counter */
    if( 0 != qFirstTime )
    {
         QueryPerformanceFrequency( &llPerfFreq );
         llPerfFreq2 = *(LONGLONG *) &llPerfFreq;
         qFirstTime = 0;
         /*debug_printf( "Freq = %ld\n", llPerfFreq2 );*/
    }

    /* Get current value of high-resolution performance counter */
    QueryPerformanceCounter( &llPerfCount );
    llPerfCount2 = *(LONGLONG *)&llPerfCount;

    /* Calculate the finish time in uSeconds */
    llDestUsec = (( llPerfCount2 * (LONGLONG) 1000000 ) / llPerfFreq2 ) + (LONGLONG) uS;
    /*debug_printf( "uS = %d\n", uS );
    debug_printf( "llPerfCount2 = %lu\n", llPerfCount2 );*/


    do 
    {
         /* Get current value of high-resolution performance
            counter */
         QueryPerformanceCounter( &llPerfCount );
         llPerfCount2 = *(LONGLONG *) &llPerfCount;

         /* Calculate current count in uSeconds */
         llCurUsec = ( llPerfCount2 * (LONGLONG) 1000000 / llPerfFreq2 );
    } while( llCurUsec < llDestUsec );
}
#endif

#ifdef LINUX
#include <sys/time.h>
static void udelay( int uS )
{
    struct timeval sTv;
    unsigned long lCurUsec;
    unsigned long lOldUsec;

    gettimeofday( &sTv, 0 );
    lOldUsec = (sTv.tv_sec * 1000000 ) + sTv.tv_usec;
    do
    {        
        gettimeofday( &sTv, 0 );
        lCurUsec = (sTv.tv_sec * 1000000 ) + sTv.tv_usec;
    } while(( lCurUsec - lOldUsec ) < uS );
}
#endif

int lpc_RxdPacket( void *pvBuf, int zLen )
{
    int zRtnv = -1;
    char *pacBuf = pvBuf;
    
    /* from GDB acRply = ":0100000310ec15.\r\n" */
    if(( zLen > 3 ) && ( ':' == *pacBuf ) &&
       ( '\n' == *( pacBuf + zLen - 1 )) &&
       ( '\r' == *( pacBuf + zLen - 2 )))
    {
        /* These bits are the same for every packet */
        zRtnv = 0;
    }

    return( zRtnv );
}
