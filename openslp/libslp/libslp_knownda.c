/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slplib_knownda.c                                           */
/*                                                                         */
/* Abstract:    Internal implementation for generating unique XIDs.        */
/*              Provides functions that are supposed to generate 16-bit    */
/*              values that won't be generated for a long time in this     */
/*              process and hopefully won't be generated by other process  */ 
/*              for a long time.                                           */
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


#include "slp.h"
#include "libslp.h"

/*=========================================================================*/
SLPDatabase G_KnownDACache ={0,0,0};
/* The cache DAAdvert messages from known DAs.                             */
/*=========================================================================*/

/*=========================================================================*/
int   G_KnownDAScopesLen = 0;
char* G_KnownDAScopes   = 0;
/* Cached known scope list                                                 */
/*=========================================================================*/


/*=========================================================================*/
time_t G_KnownDALastCacheRefresh = 0;
/* The time of the last Multicast for known DAs                            */
/*=========================================================================*/


/*-------------------------------------------------------------------------*/
SLPBoolean KnownDAListFind(int scopelistlen,
                           const char* scopelist,
                           int spistrlen,
                           const char* spistr,
                           struct in_addr* daaddr)
/* Returns: non-zero on success, zero if DA can not be found               */
/*-------------------------------------------------------------------------*/
{
    SLPDatabaseHandle   dh;
    SLPDatabaseEntry*   entry;
    int result = SLP_FALSE;
   
    dh = SLPDatabaseOpen(&G_KnownDACache);
    if(dh)
    {
        /*----------------------------------------*/
        /* Check to see if there a matching entry */
        /*----------------------------------------*/
        while(1)
        {
            entry = SLPDatabaseEnum(dh);
            if(entry == NULL) break;
            
            /* Check scopes */
            if(SLPSubsetStringList(entry->msg->body.daadvert.scopelistlen,
                                   entry->msg->body.daadvert.scopelist,
                                   scopelistlen,
                                   scopelist))
            {
#ifdef ENABLE_SECURITY
                if(SLPCompareString(entry->msg->body.daadvert.spilistlen,
                                    entry->msg->body.daadvert.spilist,
                                    spistrlen,
                                    spistr) == 0)
#endif
                {
                    
                    memcpy(daaddr, 
                           &(entry->msg->peer.sin_addr),
                           sizeof(struct in_addr));
    
                    result = SLP_TRUE;
                }
            }
        }
        SLPDatabaseClose(dh);
    }

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDAAdd(SLPMessage msg, SLPBuffer buf)
/* Add an entry to the KnownDA cache                                       */
/*                                                                         */
/* Returns: zero on success, non-zero on error                             */
/*-------------------------------------------------------------------------*/
{
    SLPDatabaseHandle   dh;
    SLPDatabaseEntry*   entry;
    SLPDAAdvert*        entrydaadvert;
    SLPDAAdvert*        daadvert;
    int                 result;

    result = 0;

    dh = SLPDatabaseOpen(&G_KnownDACache);
    if(dh)
    {
        /* daadvert is the DAAdvert message being added */
        daadvert = &(msg->body.daadvert);
    
        /*-----------------------------------------------------*/
        /* Check to see if there is already an identical entry */
        /*-----------------------------------------------------*/
        while(1)
        {
            entry = SLPDatabaseEnum(dh);
            if(entry == NULL) break;
            
            /* entrydaadvert is the DAAdvert message from the database */
            entrydaadvert = &(entry->msg->body.daadvert);

            /* Assume DAs are identical if their URLs match */
            if(SLPCompareString(entrydaadvert->urllen,
                                entrydaadvert->url,
                                daadvert->urllen,
                                daadvert->url) == 0)
            {
                SLPDatabaseRemove(dh,entry);
                break;
            }
        }

        /* Create and link in a new entry */
        entry = SLPDatabaseEntryCreate(msg,buf);
        if(entry)
        {
            SLPDatabaseAdd(dh, entry);
        }
        else
        {
            result = SLP_MEMORY_ALLOC_FAILED;
        }
        
        SLPDatabaseClose(dh);
    }
        
    return result;
}

/*-------------------------------------------------------------------------*/
SLPBoolean KnownDADiscoveryCallback(SLPError errorcode,
                                    struct sockaddr_in* peerinfo,
                                    SLPBuffer rplybuf, 
                                    void* cookie)
/*-------------------------------------------------------------------------*/
{
    SLPMessage      replymsg;
    SLPBuffer       dupbuf;
    struct hostent* he;
    SLPSrvURL*      srvurl;
    int*            count;
    SLPBoolean      result = SLP_TRUE;
    
    count = (int*)cookie;

    if(errorcode == 0)
    {
        dupbuf = SLPBufferDup(rplybuf);
        if(dupbuf)
        {
            replymsg = SLPMessageAlloc();
            if(replymsg)
            {
                 if(SLPMessageParseBuffer(peerinfo,dupbuf,replymsg) == 0 &&
                    replymsg->header.functionid == SLP_FUNCT_DAADVERT)
                 {
                    if(replymsg->body.daadvert.errorcode == 0)
                    {
                        /* TRICKY: NULL terminate the DA url */
                        ((char*)(replymsg->body.daadvert.url))[replymsg->body.daadvert.urllen] = 0;
                        if(SLPParseSrvURL(replymsg->body.daadvert.url, &srvurl) == 0)
                        {
                            he = gethostbyname(srvurl->s_pcHost);
                            SLPFree(srvurl);
			    if(he)
                            {
                                /* Reset the peer to the one in the URL */
                                replymsg->peer.sin_addr.s_addr = *((unsigned int*)(he->h_addr_list[0]));
                                
                                (*count) += 1;
                         
                                KnownDAAdd(replymsg,dupbuf);
                                 if(replymsg->header.flags & SLP_FLAG_MCAST)
                                 {
                                     return SLP_FALSE;
                                 }
                                 
                                 return SLP_TRUE;
                            }                            
                        }
                         
                     }
                     else if(replymsg->body.daadvert.errorcode == SLP_ERROR_INTERNAL_ERROR)
                     {
                        /* SLP_ERROR_INTERNAL_ERROR is a "end of stream" */
                        /* marker for looppack IPC                       */
                        result = SLP_FALSE;
                     }
                 }
                 
                 SLPMessageFree(replymsg);
            }

            SLPBufferFree(dupbuf);
        }
    }
            
    return result;
}
               

/*-------------------------------------------------------------------------*/
int KnownDADiscoveryRqstRply(int sock, 
                             struct sockaddr_in* peeraddr,
                             int scopelistlen,
                             const char* scopelist)
/* Returns: number of *new* DAEntries found                                */
/*-------------------------------------------------------------------------*/
{
    char*   buf;
    char*   curpos;
    int     bufsize;
    int     result = 0;

    /*-------------------------------------------------------------------*/
    /* determine the size of the fixed portion of the SRVRQST            */
    /*-------------------------------------------------------------------*/
    bufsize  = 31; /*  2 bytes for the srvtype length */
                   /* 23 bytes for "service:directory-agent" srvtype */
                   /*  2 bytes for scopelistlen */
                   /*  2 bytes for predicatelen */
                   /*  2 bytes for sprstrlen */
    bufsize += scopelistlen;

    /* TODO: make sure that we don't exceed the MTU */
    buf = curpos = (char*)xmalloc(bufsize);
    if(buf == 0)
    {
        return 0;
    }
    memset(buf,0,bufsize);

    /*------------------------------------------------------------*/
    /* Build a buffer containing the fixed portion of the SRVRQST */
    /*------------------------------------------------------------*/
    /* service type */
    ToUINT16(curpos,23);
    curpos = curpos + 2;
    /* 23 is the length of SLP_DA_SERVICE_TYPE */
    memcpy(curpos,SLP_DA_SERVICE_TYPE,23);
    curpos += 23;
    /* scope list */
    ToUINT16(curpos,scopelistlen);
    curpos = curpos + 2;
    memcpy(curpos,scopelist,scopelistlen);
    /* predicate zero length */
    /* spi list zero length */

    NetworkRqstRply(sock,
                    peeraddr,
                    "en",
                    buf,
                    SLP_FUNCT_DASRVRQST,
                    bufsize,
                    KnownDADiscoveryCallback,
                    &result);

    xfree(buf);

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromMulticast(int scopelistlen, const char* scopelist)
/* Locates  DAs via multicast convergence                                  */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    struct sockaddr_in peeraddr;
    int sockfd; 
    int result = 0;

    sockfd = NetworkConnectToMulticast(&peeraddr);
    if(sockfd >= 0)
    {
        result = KnownDADiscoveryRqstRply(sockfd,
                                          &peeraddr,
                                          scopelistlen,
                                          scopelist);
        close(sockfd);
    }

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromDHCP()
/* Locates  DAs via DHCP                                                   */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    return 0;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromProperties()
/* Locates DAs from a list of DA hostnames                                 */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    char*               temp;
    char*               tempend;
    char*               slider1;
    char*               slider2;
    int                 sockfd;
    struct hostent*     he;
    struct sockaddr_in  peeraddr;
    struct timeval      timeout;
    int                 result      = 0;

    memset(&peeraddr,0,sizeof(peeraddr));
    peeraddr.sin_family = AF_INET;
    peeraddr.sin_port = htons(SLP_RESERVED_PORT);

    slider1 = slider2 = temp = xstrdup(SLPGetProperty("net.slp.DAAddresses"));
    if(temp)
    {
        tempend = temp + strlen(temp);
        while(slider1 != tempend)
        {
            timeout.tv_sec = SLPPropertyAsInteger(SLPGetProperty("net.slp.DADiscoveryMaximumWait"));
            timeout.tv_usec = (timeout.tv_sec % 1000) * 1000;
            timeout.tv_sec = timeout.tv_sec / 1000;

            while(*slider2 && *slider2 != ',') slider2++;
            *slider2 = 0;

            he = gethostbyname(slider1);
            if(he)
            {
                peeraddr.sin_addr.s_addr = *((unsigned int*)(he->h_addr_list[0]));
                sockfd = SLPNetworkConnectStream(&peeraddr,&timeout);
                if(sockfd >= 0)
                {
                    result = KnownDADiscoveryRqstRply(sockfd,&peeraddr,0,"");
                    close(sockfd);
                    if(result)
                    {
                        break;
                    }
                }
            }

            slider1 = slider2;
            slider2++;
        }

        xfree(temp);
    }

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromIPC()
/* Ask Slpd if it knows about a DA                                         */ 
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    struct sockaddr_in peeraddr;
    int sockfd; 
    int result = 0;

    sockfd = NetworkConnectToSlpd(&peeraddr);
    if(sockfd >= 0)
    {
        result = KnownDADiscoveryRqstRply(sockfd, &peeraddr, 0, "");
        close(sockfd);
    }

    return result;
}

/*-------------------------------------------------------------------------*/
SLPBoolean KnownDAFromCache(int scopelistlen,
                            const char* scopelist,
                            int spistrlen,
                            const char* spistr,
                            struct in_addr* daaddr)
/* Ask Slpd if it knows about a DA                                         */ 
/*                                                                         */
/* Returns: non-zero on success, zero if DA can not be found               */
/*-------------------------------------------------------------------------*/
{
    time_t          curtime;
    
    if(KnownDAListFind(scopelistlen,
                       scopelist,
                       spistrlen,
                       spistr,
                       daaddr) == SLP_FALSE)
    {
        curtime = time(&curtime);
        if(G_KnownDALastCacheRefresh == 0 ||
           curtime - G_KnownDALastCacheRefresh > MINIMUM_DISCOVERY_INTERVAL)
        {
            G_KnownDALastCacheRefresh = curtime;

            /* discover DAs */

            if(KnownDADiscoverFromIPC() == 0)
                if(KnownDADiscoverFromProperties() == 0)
                    if(KnownDADiscoverFromDHCP() == 0)
                        KnownDADiscoverFromMulticast(scopelistlen,
                                                     scopelist);
        }

        return KnownDAListFind(scopelistlen,
                       scopelist,
                       spistrlen,
                       spistr,
                       daaddr);
    }

    return SLP_TRUE; 
}


/*=========================================================================*/
int KnownDAConnect(PSLPHandleInfo handle,
                   int scopelistlen,
                   const char* scopelist,
                   struct sockaddr_in* peeraddr)
/* Get a connected socket to a DA that supports the specified scope        */
/*                                                                         */
/* scopelistlen (IN) stringlen of the scopelist                            */
/*                                                                         */
/* scopelist (IN) DA must support this scope                               */
/*                                                                         */
/* peeraddr (OUT) the peer that was connected to                           */
/*                                                                         */
/*                                                                         */
/* returns: valid socket file descriptor or -1 if no DA is found           */
/*=========================================================================*/
{
    struct timeval  timeout;
    int             sock = -1;
    int                 spistrlen   = 0;
    char*               spistr      = 0;
#ifdef ENABLE_SECURITY
    if(SLPPropertyAsBoolean(SLPGetProperty("net.slp.securityEnabled")))
    {
        SLPSpiGetDefaultSPI(handle->hspi,
                            SLPSPI_KEY_TYPE_PUBLIC,
                            &spistrlen,
                            &spistr);
    }
#endif

    /* Set up connect timeout */
    timeout.tv_sec = SLPPropertyAsInteger(SLPGetProperty("net.slp.DADiscoveryMaximumWait"));
    timeout.tv_usec = (timeout.tv_sec % 1000) * 1000;
    timeout.tv_sec = timeout.tv_sec / 1000;

    while(1)
    {
        memset(peeraddr,0,sizeof(peeraddr));
        
        if(KnownDAFromCache(scopelistlen,
                            scopelist,
                            spistrlen,
                            spistr,
                            &(peeraddr->sin_addr)) == 0)
        {
            break;
        }
        peeraddr->sin_family = PF_INET;
        peeraddr->sin_port = htons(SLP_RESERVED_PORT);
        
        sock = SLPNetworkConnectStream(peeraddr,&timeout);
        if(sock >= 0)
        {
            break;
        }

        KnownDABadDA(&(peeraddr->sin_addr));
    }


#ifdef ENABLE_SECURITY
    if(spistr) xfree(spistr);
#endif

    return sock;
}

/*=========================================================================*/
void KnownDABadDA(struct in_addr* daaddr)
/* Mark a KnownDA as a Bad DA.                                             */
/*                                                                         */
/* daaddr (IN) address of the bad DA                                       */
/*                                                                         */
/* Returns: none                                                           */
/*=========================================================================*/
{
    SLPDatabaseHandle   dh;
    SLPDatabaseEntry*   entry;
    
    dh = SLPDatabaseOpen(&G_KnownDACache);
    if(dh)
    {
        /*-----------------------------------*/
        /* Check to find the requested entry */
        /*-----------------------------------*/
        while(1)
        {
            entry = SLPDatabaseEnum(dh);
            if(entry == NULL) break;
            
            /* Assume DAs are identical if their in_addrs match */
            if(memcmp(daaddr,&(entry->msg->peer.sin_addr),sizeof(struct in_addr)) == 0)
            {
                SLPDatabaseRemove(dh,entry);
                break;            
            }
        }

        SLPDatabaseClose(dh);
    }
}


/*=========================================================================*/
int KnownDAGetScopes(int* scopelistlen,
                     char** scopelist)
/* Gets a list of scopes from the known DA list                            */
/*                                                                         */
/* scopelistlen (OUT) stringlen of the scopelist                           */
/*                                                                         */
/* scopelist (OUT) NULL terminated list of scopes                          */
/*                                                                         */
/* returns: zero on success, non-zero on failure                           */
/*=========================================================================*/
{
    int                 newlen;
    time_t              curtime;
    SLPDatabaseHandle   dh;
    SLPDatabaseEntry*   entry;
    
    /* Refresh the cache if necessary */
    curtime = time(&curtime);
    if(G_KnownDALastCacheRefresh == 0 ||
       curtime - G_KnownDALastCacheRefresh > MINIMUM_DISCOVERY_INTERVAL)
    {
        G_KnownDALastCacheRefresh = curtime;

        /* discover DAs */
        if(KnownDADiscoverFromIPC() == 0)
        {
            KnownDADiscoverFromDHCP();
            KnownDADiscoverFromProperties();
            KnownDADiscoverFromMulticast(0,"");
        }
    }

    /* enumerate through all the knownda entries and generate a */
    /* scopelist                                                */
    dh = SLPDatabaseOpen(&G_KnownDACache);
    if(dh)
    {
        /*-----------------------------------*/
        /* Check to find the requested entry */
        /*-----------------------------------*/
        while(1)
        {
            entry = SLPDatabaseEnum(dh);
            if(entry == NULL) break;
            newlen = G_KnownDAScopesLen;
            while(SLPUnionStringList(G_KnownDAScopesLen,
                                     G_KnownDAScopes,
                                     entry->msg->body.daadvert.scopelistlen,
                                     entry->msg->body.daadvert.scopelist,
                                     &newlen,
                                     G_KnownDAScopes) < 0)
            {
                G_KnownDAScopes = xrealloc(G_KnownDAScopes,newlen);
                if(G_KnownDAScopes == 0)
                {
                    G_KnownDAScopesLen = 0;
                    break;
                }
            }
            G_KnownDAScopesLen = newlen;

        }

        SLPDatabaseClose(dh);
    }

    /* Explicitly add in the useScopes property */
    newlen = G_KnownDAScopesLen;
    while(SLPUnionStringList(G_KnownDAScopesLen,
                             G_KnownDAScopes,
                             strlen(SLPPropertyGet("net.slp.useScopes")),
                             SLPPropertyGet("net.slp.useScopes"),
                             &newlen,
                             G_KnownDAScopes) < 0)
    {
        G_KnownDAScopes = xrealloc(G_KnownDAScopes,newlen);
        if(G_KnownDAScopes == 0)
        {
            G_KnownDAScopesLen = 0;
            break;
        }
    }
    G_KnownDAScopesLen = newlen;


    if(G_KnownDAScopesLen)
    {
        *scopelist = xmalloc(G_KnownDAScopesLen + 1);
        if(*scopelist == 0)
        {
            return -1;
        }
        memcpy(*scopelist,G_KnownDAScopes, G_KnownDAScopesLen);
        (*scopelist)[G_KnownDAScopesLen] = 0; 
        *scopelistlen = G_KnownDAScopesLen;
    }
    else
    {
        *scopelist = xstrdup("");
        if(*scopelist == 0)
        {
            return -1;
        }
        *scopelistlen = 0; 
    }

    return 0;
}

#ifdef DEBUG
/*=========================================================================*/
void KnownDAFreeAll()
/* Frees all (cached) resources associated with known DAs                  */
/*                                                                         */
/* returns: none                                                           */
/*=========================================================================*/
{
    SLPDatabaseHandle   dh;
    SLPDatabaseEntry*   entry;
    dh = SLPDatabaseOpen(&G_KnownDACache);
    if(dh)
    {
        while(1)
        {
            entry = SLPDatabaseEnum(dh);
            if(entry == NULL) break;
            
            SLPDatabaseRemove(dh,entry);
        }

        SLPDatabaseClose(dh);
    }
    G_KnownDAScopesLen = 0;
    
    if(G_KnownDAScopes) xfree(G_KnownDAScopes);
}

#endif
