/*  
  File:         ihex.h
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
#ifndef IHEX_H
#define IHEX_H

unsigned int read_intel_hex( char filename[], unsigned char data_ptr[], unsigned int length);
unsigned int write_intel_hex( unsigned char data_ptr[], unsigned int length,
                              unsigned int line_length, char filename[]);

unsigned int snintel_hex( char abString[], unsigned int lStrLen, unsigned char bRecId,
                          unsigned char *pbBuff, unsigned char bBufLen,
                          unsigned short wStartAddr );
int nibble( char c );

#endif

