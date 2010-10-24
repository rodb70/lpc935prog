/*  
  File:         ihex.c
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "ihex.h"

/* Define Intel record types */
#define DATA_RECORD            0x00
#define END_OF_FILE            0x01
#define EXTENDED_LINER_ADDRESS 0x04

/* Define record offsets */
#define REC_LEN_OFFSET  1
#define ADDRESS_OFFSET  3
#define REC_TYPE_OFFSET 7
#define DATA_OFFSET     9

/* Maximum size of line being read */
#define TEXT_BUFFER 524
#define START_INTEL_DATA_SECTION 9

static int do_chechsum( const char *record);
static unsigned char get_checksum( unsigned char sum );

static unsigned int strword( char *buff);
static void nibble_to_char( char *ch, int num);
static void byte_to_str( char ch[], int num);
static void word_to_str( char pos[], unsigned int num );


/**
 Function will given the file name and the data pointer to the location
 in memory will read an Intel hex file into memory.
 Parameters:
    pacFilename - char pointer to the filename to open and read.  It is
                   assumed at this point that the file exists and is an
                   Intel hex file.
    pabData - a pointer to a location in memory that will hold the binary
              version of the Intel hex file
    lLen - that length of the data block.
 Returns
    The number of bytes read from the hex file.  A negative number
    indicates an error.
 */
unsigned int read_intel_hex( char *pacFilename, unsigned char pabData[], unsigned int lLen)
{
    FILE *in;
    char buff[ TEXT_BUFFER ];
    unsigned int rec_type;
    unsigned int taddr;
    unsigned int max_addr = 0;
    unsigned int rec_len;
    unsigned int i;
    unsigned int dbyte;

    if( NULL == ( in = fopen( pacFilename, "rt")))
    {
        return( -1 );
    }

    while((fgets( buff, TEXT_BUFFER - 2, in)) != NULL)
    {
        /* if not intel hex record continue */
        if(buff[0] != ':')
        {
            continue;
        }

        if( do_chechsum( buff ))
        {
            fclose( in );
            return( -2 );
        }

        rec_type = (nibble( buff[ REC_TYPE_OFFSET ]) << 4) +
            nibble( buff[ REC_TYPE_OFFSET + 1]);
        /* We don't need to re-checksum the line as it is already done when we
           got the address boundaries */
        switch( rec_type )
        {
          case DATA_RECORD:
              rec_len = (nibble( buff[ REC_LEN_OFFSET ]) << 4) +
                  nibble( buff[ REC_LEN_OFFSET + 1]);

              taddr = strword( &buff[ ADDRESS_OFFSET ]);

              for( i = 0; i < rec_len; i++)
              {
                  if(( taddr + i ) >= lLen )
                  {
                      fclose( in );
                      return( -3 );
                  }

                  dbyte = ( nibble( buff[ DATA_OFFSET + ( i * 2 )]) << 4) +
                      nibble( buff[ DATA_OFFSET + ( i * 2 ) + 1 ]);

                  if( taddr + i > max_addr)   /* Find the highest address */
                  {
                      max_addr = taddr + i;
                  }

                  pabData[ taddr + i ] = dbyte;
              }/*for*/
              break;
              
          case EXTENDED_LINER_ADDRESS:
              break;
              
          case END_OF_FILE:
              break;
              
          default :
              printf("Unknown record type %02X\n", rec_type);
              fclose( in );
              return( -4 );
        } /* switch */
    } /* while */

    fclose( in );
    
    return( max_addr );
}


/**
 * Write out an Intel hex file
 *
 * The following function will given a pointer to a block of data, the size of
 * the data block, length of each hex line, and the filename will output an
 * Intel format hex file
 *
 * @param pabData - Pointer to the block of data that contains the data to
 *                  output
 * @param lLen - The size of the data block to output to the hex file.
 * @param lLineLen - The number of characters to output in each line MUST be
 *                   greater than the minimum length ( 11 chars)
 * @param pacFilename - The filename to write the Intel hex file to.
 * @return A positive value or 0 for all OK.  A negative value indicates an error.
 */
unsigned int write_intel_hex( unsigned char pabData[], unsigned int lLen,
                              unsigned int lLineLen, char *pacFilename)
{
   FILE *out;
   char buff[ TEXT_BUFFER ];
   int sum;
   int lpos;
   int max_chars;
   int num_chars;
   int datapos = 0;

   if( NULL == ( out = fopen( pacFilename, "wt")))
   {
      return( -1 );
   }

   /* Decide how many characters per line we want */
   max_chars = lLineLen;

   while( lLen > datapos )
   {
      memset( buff, 0, sizeof( buff ));

      /* do header */
      sum = 0;
      buff[0] = ':';
      byte_to_str( &buff[ REC_LEN_OFFSET ], 0);
      sum += (datapos >> 8);
      sum += ( datapos & 0xFF);
      word_to_str( &buff[ ADDRESS_OFFSET ], datapos);
      byte_to_str( &buff[ REC_TYPE_OFFSET ], DATA_RECORD);
      sum += DATA_RECORD;

      /* do data */
      lpos = START_INTEL_DATA_SECTION;
      num_chars = 0;
      while(( lLen > datapos ) && ( num_chars < max_chars))
      {
         byte_to_str( &buff[ lpos ], pabData[ datapos ]);
         sum += pabData[ datapos ];
         lpos += 2;
         num_chars++;
         datapos++;
      }
      
      byte_to_str( &buff[ REC_LEN_OFFSET ], num_chars);
      sum += num_chars;
      sum = get_checksum( sum );
      byte_to_str( &buff[ lpos ], sum);
      fprintf( out, "%s\n", buff );
   }
   
   /* print out the end of file header */
   fprintf( out, ":00000001FF\n" );

   fclose( out );

   return( 0 );
}

/**
 * This function will return a string in Intel hex format that is a conversion
 * of a data buffer.
 * Parameters
 *   abString - pointer to the return string
 *   lStrLen - the maximum size of the return string including the terminating null
 *   bRecId - the record ID to assign to this buffer
 *   pbBuff - pointer to the data buffer to convert to Intel string
 *   bBufLen - the size of the buffer
 *   wStartAddr - The start address to put into the Intel hex record
 * Returns
 *    A positive value or 0 for all OK.  A negative value indicates an error.
 */
unsigned int snintel_hex( char abString[], unsigned int lStrLen, unsigned char bRecId,
                          unsigned char *pbBuff, unsigned char bBufLen,
                          unsigned short wStartAddr )
{
    unsigned int lCurStrPos;
    unsigned char bSum;
    int i;

    /* Set up start of record and length */
    memset( abString, 0, lStrLen );
    abString[ 0 ] = ':';
    bSum = 0;
    lCurStrPos = strlen( abString );
    snprintf( &abString[ lCurStrPos ], lStrLen - lCurStrPos, "%02x", bBufLen );
    lCurStrPos = strlen( abString );
    bSum += bBufLen;

    /* Do address */
    snprintf( &abString[ lCurStrPos ], lStrLen - lCurStrPos, "%04x", wStartAddr );
    lCurStrPos = strlen( abString );
    bSum += ( wStartAddr >> 8 );
    bSum += ( wStartAddr & 0xff );

    /* Do record Id */
    snprintf( &abString[ lCurStrPos ], lStrLen - lCurStrPos, "%02x", bRecId );
    lCurStrPos = strlen( abString );
    bSum += bRecId;

    /* Do Data */
    for( i = 0; i < bBufLen; i++ )
    {
        snprintf( &abString[ lCurStrPos ], lStrLen - lCurStrPos, "%02x", *( pbBuff + i ));
        lCurStrPos = strlen( abString );
        bSum += *( pbBuff + i );
    }

    /* Do checksum */
    bSum = get_checksum( bSum );
    snprintf( &abString[ lCurStrPos ], lStrLen - lCurStrPos, "%02x", bSum );
    lCurStrPos = strlen( abString );

    return( lCurStrPos );
}


/*
   Convert ASCII nibble to binary
 */
int nibble( char c )
{
   if( isascii( c ) && isxdigit( c ))
   {
       if( isdigit( c ))
       {
           return( c - '0' );
       }
       
       if( isupper( c ))
       {
           return( c - 'A' + 10 );
       }
       
       if( islower( c ))
       {
           return( c - 'a' + 10 );
       }
   }
   
   printf( "Illegal Hex data in record\n" );
   return( -1 );
}


/*
   Private functions
 */
/*
  Converts num to a hex character and returns it in ch
 */
static void nibble_to_char( char *ch, int num)
{
   num &= 0xF;

   if( num > 9 )
   {
       *ch = num + ('a' - 10);
   }
   else
   {
       *ch = num + '0';
   }
}


/*
   converts the byte in num to hex characters.  Assumes array ch is
   at least 2 in size.
 */
static void byte_to_str( char ch[], int num)
{
   int tnum;

   tnum = num >> 4;
   nibble_to_char( ch, tnum );
   tnum = num & 0xF;
   nibble_to_char( &ch[ 1 ], tnum );
}


/*
   converts the word passed to hex characters.  Assumes aray pos is
   at least 4 in size.
 */
static void word_to_str( char pos[], unsigned int num )
{
   unsigned int tnum;

   tnum = num >> 8;
   byte_to_str( pos, tnum );
   tnum = num & 0xFF;
   byte_to_str( &pos[ 2 ], tnum );
}


/*
  Convert a string of hex chars to a word
 */
static unsigned int strword( char *buff)
{
   return((nibble( buff[ 0 ]) << 12) + (nibble( buff[ 1 ]) << 8) +
          (nibble( buff[ 2 ]) << 4) + nibble( buff[ 3 ]));
}


/*
  return the checksum of the sum
 */
static unsigned char get_checksum( unsigned char sum )
{
   unsigned char rtnv;

   rtnv = 0 - sum % 256;

   return( rtnv );
}


/*
  check the checksum of the hex record
 */
static int do_chechsum( const char *record )
{
   int i, rec_len, csum = 0;

   rec_len = strlen( record ) - 2;
   for( i = 1; i < rec_len; i = i + 2)
   {
       csum += (nibble( record[ i ]) << 4) + nibble( record[ i + 1]);
   }
   csum &= 0xFF;
   
   return( csum );
}

