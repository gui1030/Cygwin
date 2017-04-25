/*
 ******************************************************************************
 *
 *   This File Contains PROPRIETARY and CONFIDENTIAL TRADE SECRET information
 *   which is the exclusive property of VeriSolutions, LLC.  "Trade Secret"
 *   means information constituting a trade secret within the meaning of
 *   Section 10-1-761(4) of the Georgia Trade Secrets Act of 1990, including
 *   all amendments hereafter adopted.
 *
 *   (c) 2017 by VeriSolutions, LLC.                 www.VeriSolutions.co
 *
 ******************************************************************************
 *
 *      VARIOUS UTILITY FUNCTIONS -
 *
 *
 *
 *
 ******************************************************************************
 */

//------------------------------------------------------------------------------
//  
//  Emulates the Linux time() library function.  Returns UTC time in seconds
//  since begining of the epoch (midnight 01-01-1970).  If the argument is non-
//  null, also places time in the address provided.
//  
time_t   time(time_t*);

//------------------------------------------------------------------------------
//  
//  Returns a string containing the present time appropriate for debug output.
//  
char*    now_string();

//------------------------------------------------------------------------------
//  
//  Returns a cumulative checksum computed byte-by-byte over the entire object
//  provided.  Uses Fletcher's widely employed algorithm.
//  
uint16_t cksum16(const void*, size_t);

//------------------------------------------------------------------------------
//  
//  Returns a Cylic Redundancy Check value computed byte-by-byte over the entire
//  object provided.  Uses Brown's 1986 table-based algorithm.
//  
uint32_t crc32(const void*, size_t);

//------------------------------------------------------------------------------
