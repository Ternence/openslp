/*-------------------------------------------------------------------------
 * Copyright (C) 2000 Caldera Systems, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    Neither the name of Caldera Systems nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA
 * SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-------------------------------------------------------------------------*/

/** Open and close handle.
 *
 * Implementation for functions open, close, and associate ifc.
 *
 * @file       libslp_handle.c
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    LibSLPCode
 */

#include "slp.h"
#include "libslp.h"
#include "slp_net.h"

int G_OpenSLPHandleCount = 0;
/*!< Global variable that keeps track of the number of handles 
 * that are open.
 */

/** Open an OpenSLP session handle.
 *
 * Returns a SLPHandle handle in the phSLP parameter for the language
 * locale passed in as the pcLang parameter. The client indicates if
 * operations on the handle are to be synchronous or asynchronous
 * through the isAsync parameter. The handle encapsulates the language
 * locale for SLP requests issued through the handle, and any other
 * resources required by the implementation. However, SLP properties
 * are not encapsulated by the handle; they are global. The return
 * value of the function is an SLPError code indicating the status of
 * the operation. Upon failure, the phSLP parameter is NULL.
 *
 * @par
 * An SLPHandle can only be used for one SLP API operation at a time.
 * If the original operation was started asynchronously, any attempt
 * to start an additional operation on the handle while the original
 * operation is pending results in the return of an SLP_HANDLE_IN_USE
 * error from the API function. The SLPClose() API function terminates
 * any outstanding calls on the handle. If an implementation is unable
 * to support a asynchronous (resp. synchronous) operation, due to
 * memory constraints or lack of threading support, the
 * SLP_NOT_IMPLEMENTED flag may be returned when the isAsync flag
 * is SLP_TRUE (resp. SLP_FALSE).
 *
 * @param[in] pcLang -  A pointer to an array of characters containing 
 *    the [RFC 1766] Language Tag for the natural language locale of 
 *    requests and registrations issued on the handle. (Pass NULL or
 *    the empty string to use the default locale.)
 *
 * @param[in] isAsync - An SLPBoolean indicating whether the SLPHandle 
 *    should be opened for asynchronous operation or not.
 *
 * @param[out] phSLP - A pointer to an SLPHandle, in which the open  
 *    SLPHandle is returned. If an error occurs, the value upon return 
 *    is NULL.
 *
 * @return An SLPError code; SLP_OK(0) on success, SLP_PARAMETER_BAD,
 *    SLP_NOT_IMPLEMENTED, SLP_MEMORY_ALLOC_FAILED, 
 *    SLP_NETWORK_INIT_FAILED, SLP_INTERNAL_SYSTEM_ERROR
 */
SLPError SLPAPI SLPOpen(const char *pcLang, SLPBoolean isAsync, SLPHandle *phSLP)
{
    SLPError        result = SLP_OK;
    PSLPHandleInfo  handle = 0;

    /*------------------------------*/
    /* check for invalid parameters */
    /*------------------------------*/
    if(phSLP == 0)
    {
        result =  SLP_PARAMETER_BAD;
        goto FINISHED;
    }

    /* assign out param to zero in just for paranoia */
    *phSLP = 0;


#ifndef ENABLE_ASYNC_API   
    if(isAsync == SLP_TRUE)
    {
        result =  SLP_NOT_IMPLEMENTED;
        goto FINISHED;
    }
/*-------------------------------------------------------*/
/* TODO: remove the #else when we implement async calls  */
/*-------------------------------------------------------*/
#else
    if(isAsync == SLP_TRUE)
    {
        result =  SLP_NOT_IMPLEMENTED;
        goto FINISHED;
    }
#endif 

    /*------------------------------------*/
    /* allocate a SLPHandleInfo structure */
    /*------------------------------------*/
    handle = (PSLPHandleInfo)xmalloc(sizeof(SLPHandleInfo));
    if(handle == 0)
    {
        result =  SLP_PARAMETER_BAD;
        goto FINISHED;
    }
    memset(handle,0,sizeof(SLPHandleInfo));

    /*-------------------------------*/
    /* Set the language tag          */
    /*-------------------------------*/
    if(pcLang && *pcLang)
    {
        handle->langtaglen = strlen(pcLang);
        handle->langtag = (char*)xmalloc(handle->langtaglen + 1);
        if(handle->langtag == 0)
        {
            xfree(handle);
            result =  SLP_PARAMETER_BAD;
            goto FINISHED;
        }
        memcpy(handle->langtag,pcLang,handle->langtaglen + 1); 
    }
    else
    {
        handle->langtaglen = strlen(SLPGetProperty("net.slp.locale"));
        handle->langtag = (char*)xmalloc(handle->langtaglen + 1);
        if(handle->langtag == 0)
        {
            xfree(handle);
            result =  SLP_PARAMETER_BAD;
            goto FINISHED;
        }
        memcpy(handle->langtag,SLPGetProperty("net.slp.locale"),handle->langtaglen + 1);       
    }

    /*---------------------------------------------------------*/
    /* Seed the XID generator if this is the first open handle */
    /*---------------------------------------------------------*/
    if(G_OpenSLPHandleCount == 0)
    {
#ifdef _WIN32
        WSADATA wsaData; 
        WORD    wVersionRequested = MAKEWORD(1,1); 
        if(0 != WSAStartup(wVersionRequested, &wsaData))
        {
            result = SLP_NETWORK_INIT_FAILED;
            goto FINISHED;
        }
#endif

#ifdef DEBUG
        xmalloc_init("/tmp/libslp_xmalloc.log",0);
#endif
        
        SLPXidSeed();
    }

#ifdef ENABLE_SLPv2_SECURITY
    handle->hspi = SLPSpiOpen(LIBSLP_SPIFILE,0);
#endif

    handle->sig = SLP_HANDLE_SIG;
    handle->inUse = SLP_FALSE;
    handle->isAsync = isAsync;
    handle->dasock = -1;
    handle->sasock = -1;
#ifndef UNICAST_NOT_SUPPORTED
    handle->unicastsock = -1;
#endif
		
    G_OpenSLPHandleCount ++;  

    *phSLP = (SLPHandle)handle; 

    FINISHED:                  

    if(result)
    {
        *phSLP = 0;
    }

    return result;
}

/** Close an SLP handle.
 *
 * @param[in] hSLP - The handle to be closed.
 */
void SLPAPI SLPClose(SLPHandle hSLP)                                             
{
    PSLPHandleInfo   handle;

    /*------------------------------*/
    /* check for invalid parameters */
    /*------------------------------*/
    if(hSLP == 0 || *(unsigned int*)hSLP != SLP_HANDLE_SIG)
    {
        return;
    }

    handle = (PSLPHandleInfo)hSLP;

    if(handle->isAsync)
    {
        /* TODO: stop the usage of this handle (kill threads, etc) */
    }

    if(handle->langtag)
    {
        xfree(handle->langtag);
    }

    if(handle->dasock >=0)
    {
#ifdef _WIN32
        closesocket(handle->dasock);
#else
        close(handle->dasock);
#endif
    }

    if(handle->dascope)
    {
        xfree(handle->dascope);
    }

    if(handle->sasock >=0)
    {
#ifdef _WIN32
        closesocket(handle->sasock);
#else
        close(handle->sasock);
#endif
    }

    if(handle->sascope)
    {
        xfree(handle->sascope);
    }

#ifdef ENABLE_SLPv2_SECURITY
    if(handle->hspi) SLPSpiClose(handle->hspi);
#endif

    handle->sig = 0;  /* If they use the handle again, it won't be valid */

    xfree(hSLP);

    G_OpenSLPHandleCount --;
    

#if DEBUG
    /* Free additional resources if this is the last handle open */
    if(G_OpenSLPHandleCount <= 0)
    {
        G_OpenSLPHandleCount = 0;

        SLPPropertyFreeAll();
        
        KnownDAFreeAll();
        
        xmalloc_deinit();
    }
#endif

}

#ifndef MI_NOT_SUPPORTED
/** Associates an interface list with an SLP handle.
 *
 * Associates a list of interfaces McastIFList on which multicast needs
 * to be done with a particular SLPHandle hSLP. McastIFList is a comma 
 * separated list of host interface IP addresses.
 *
 * @param[in] hSLP - The SLPHandle with which the interface list is to be 
 *    associated with.
 *
 * @param[in] McastIFList - A comma separated list of host interface IP 
 *    addresses on which multicast needs to be done.
 *
 * @return An SLPError code.
 */
SLPError SLPAssociateIFList( SLPHandle hSLP, const char* McastIFList)
{

    PSLPHandleInfo      handle;

    /*------------------------------*/
    /* check for invalid parameters */
    /*------------------------------*/
    if(hSLP            == 0 ||
       *(unsigned int*)hSLP != SLP_HANDLE_SIG ||
       McastIFList == 0 ||
       *McastIFList == 0)  /* interface list can't be empty string */
    {
        return SLP_PARAMETER_BAD;
    }

    handle = (PSLPHandleInfo)hSLP;

#ifdef DEBUG
    fprintf(stderr, "SLPAssociateIFList(): McastIFList = %s\n", McastIFList);
#endif

        handle->McastIFList = McastIFList;

    return SLP_OK;
}
#endif /* MI_NOT_SUPPORTED */


#ifndef UNICAST_NOT_SUPPORTED
/** Associates a unicast IP address with an open SLP handle.
 *
 * Associates an IP address unicast_ip with a particular SLPHandle hSLP.
 * unicast_ip is the IP address of the SA/DA from which service is 
 * requested.
 *
 * @param[in] hSLP - The SLPHandle with which the unicast_ip address is 
 *    to be associated with.
 *
 * @param[in] unicast_ip - IP address of the SA/DA from which service 
 *    is requested.
 *
 * @return An SLPError code.
 */
SLPError SLPAssociateIP( SLPHandle hSLP, const char* unicast_ip)
{

    PSLPHandleInfo				handle;
	int							result=-1;

    /*------------------------------*/
    /* check for invalid parameters */
    /*------------------------------*/
    if(hSLP            == 0 ||
       *(unsigned int*)hSLP != SLP_HANDLE_SIG ||
       unicast_ip == 0 ||
       *unicast_ip == 0)  /* unicast address not specified */
    {
        return SLP_PARAMETER_BAD;
    }

    handle = (PSLPHandleInfo)hSLP;

#ifdef DEBUG
    fprintf(stderr, "SLPAssociateIP(): unicast_ip = %s\n", unicast_ip);
#endif
    handle->dounicast = 1;

   /** @todo Verify error conditions in associate ip address. */
	result = SLPNetResolveHostToAddr(unicast_ip, &handle->unicastaddr);
	if (SLPNetSetPort(&handle->unicastaddr, SLP_RESERVED_PORT) != 0)
		return SLP_PARAMETER_BAD;
    return SLP_OK;
}
#endif

/*=========================================================================*/
