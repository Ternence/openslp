/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slp_net.h                                                  */
/*                                                                         */
/* Abstract:    Network utility functions                                  */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/*     Please submit patches to http://www.openslp.org                     */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/* Copyright (C) 2000 Caldera Systems, Inc                                 */
/* All rights reserved.                                                    */
/*                                                                         */
/* Redistribution and use in source and binary forms, with or without      */
/* modification, are permitted provided that the following conditions are  */
/* met:                                                                    */ 
/*                                                                         */
/*      Redistributions of source code must retain the above copyright     */
/*      notice, this list of conditions and the following disclaimer.      */
/*                                                                         */
/*      Redistributions in binary form must reproduce the above copyright  */
/*      notice, this list of conditions and the following disclaimer in    */
/*      the documentation and/or other materials provided with the         */
/*      distribution.                                                      */
/*                                                                         */
/*      Neither the name of Caldera Systems nor the names of its           */
/*      contributors may be used to endorse or promote products derived    */
/*      from this software without specific prior written permission.      */
/*                                                                         */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     */
/* `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT      */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR   */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA      */
/* SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT        */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON       */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE   */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    */
/*                                                                         */
/***************************************************************************/

#ifndef SLP_NET_H_INCLUDED
#define SLP_NET_H_INCLUDED

#ifdef _WIN32
#if(_WIN32_WINNT >= 0x0400 && _WIN32_WINNT < 0x0500) 
#include <ws2tcpip.h>
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <errno.h>
#include "slp_win32.h"
#define ETIMEDOUT 110
#define ENOTCONN  107
#else
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <netdb.h> 
#include <fcntl.h> 
#include <errno.h>
#endif

/*-------------------------------------------------------------------------*/
int SLPNetGetThisHostname(char** hostfdn, int numeric_only);
/* 
 * Description:
 *    Returns a string represting this host (the FDN) or null. Caller must    
 *    free returned string                                                    
 *
 * Parameters:
 *    hostfdn   (OUT) pointer to char pointer that is set to buffer
 *                    contining this machine's FDN.  Caller must free
 *                    returned string with call to xfree()
 *    numeric_only (IN) force return of numeric address.
 *-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
int SLPNetResolveHostToAddr(const char* host,
                            struct in_addr* addr);
/*
 * Description:
 *    Returns a string represting this host (the FDN) or null. Caller must
 *    free returned string
 *
 * Parameters:
 *    host  (IN)  pointer to hostname to resolve
 *    addr  (OUT) pointer to in_addr that will be filled with address
 *
 * Returns: zero on success, non-zero on error;
 *-------------------------------------------------------------------------*/


int SLPNetIsIPV6();
/*
 * Description:
 *    Used to determine if IPV6 was enabled in the configuration file
 *    
 *
 * Parameters:
 *
 * Returns: non-zero if IPV6 was configured, 0 if not configured
 *-------------------------------------------------------------------------*/


int SLPNetIsIPV4();
/*
 * Description:
 *    Used to determine if IPV4 was enabled in the configuration file
 *    
 *
 * Parameters:
 *
 * Returns: non-zero if IPV4 was configured, 0 if not configured
 *-------------------------------------------------------------------------*/

//char *SLPNetAddrToString(struct sockaddr_storage *addr); replaced with inet_pton
//int SLPNetStringToAddr(char *str, struct sockaddr_storage *addr); replaced with inet_ntop

int SLPNetCompareAddrs(const struct sockaddr_storage *addr1, const struct sockaddr_storage *addr2);
/*
 * Description:
 *    Used to determine if two sockaddr_storage structures are equal
 *    
 *
 * Parameters:
 *  (in) addr1  First address to be compared
 *  (in) addr2  Second address to be compared
 *
 * Returns: non-zero if not equal, 0 if equal
 *-------------------------------------------------------------------------*/
int SLPNetIsMCast(const struct sockaddr_storage *addr);
/*
 * Description:
 *    Used to determine if the specified sockaddr_storage is a multicast address
 *    
 *
 * Parameters:
 *  (in) addr  Address to be tested to see if multicast
 *
 * Returns: non-zero if address is a multicast address, 0 if not multicast
 *-------------------------------------------------------------------------*/

int SLPNetIsLocal(const struct sockaddr_storage *addr);
/*
 * Description:
 *    Used to determine if the specified sockaddr_storage is a local address
 *    
 *
 * Parameters:
 *  (in) addr  Address to be tested to see if local
 *
 * Returns: non-zero if address is a local address, 0 if not multicast
 *-------------------------------------------------------------------------*/

int SLPNetSetAddr(struct sockaddr_storage *addr, const int family, const short port, const unsigned char *address, const int addrLen);
/*
 * Description:
 *    Used to set up the relevant fields of a sockaddr_storage structure
 *    
 *
 * Parameters:
 *  (in/out) addr   Address of sockaddr_storage struct to be filled out
 *  (in) family     Protocol family (PF_INET for IPV4, PF_INET6 for IPV6)
 *  (in) port       Port for this address.  Note that appropriate host to network translations
                    will occur as part of this call.
 *  (in) address    The IP address for this sockaddr_storage struct.  If NULL this call will not
                    modify the existing address.
 *  (in) addrLen    The length of the adddress to be set.
 *
 * Returns: 0 if address set correctly, non-zero there were errors setting fields of addr
 *-------------------------------------------------------------------------*/

int SLPNetCopyAddr(struct sockaddr_storage *dst, const struct sockaddr_storage *src);
/*
 * Description:
 *    Used to copy one sockaddr_storage struct to another
 *    
 *
 * Parameters:
 *  (in/out) dst    Destination address to be filled in
 *  (in) src        Source address to be copied from
 *
 * Returns: 0 if address set correctly, non-zero there were errors setting fields of dst
 *-------------------------------------------------------------------------*/

#endif
