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

/** Win32 service code.
 *
 * @file       slpd_win32.c
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    SlpdCode
 */

#include "slpd.h"
#include "slpd_cmdline.h"
#include "slpd_log.h"
#include "slpd_property.h"
#include "slpd_database.h"
#include "slpd_socket.h"
#include "slpd_incoming.h"
#include "slpd_outgoing.h"
#include "slpd_knownda.h"

#include "slp_linkedlist.h"
#include "slp_xid.h"

SERVICE_STATUS ssStatus;       /* current status of the service  */
SERVICE_STATUS_HANDLE sshStatusHandle; 
BOOL bDebug = FALSE; 
TCHAR szErr[256];

/* externals (variables) from slpd_main.c */
extern int G_SIGTERM;

/* externals (functions) from slpd_main.c */
void LoadFdSets(SLPList* socklist, 
                int* highfd, 
                fd_set* readfds, 
                fd_set* writefds);
void HandleSigTerm();
void HandleSigAlrm();

/** Reports the current status of the service to the SCM.
 *
 * @param[in] dwCurrentState - The state of the service.
 * @param[in] dwWin32ExitCode - The error code to report.
 * @param[in] dwWaitHint - Worst case estimate to next checkpoint.
 *
 * @return A boolean value; TRUE on success, FALSE on failure.
 *
 * @internal
 */
BOOL ReportStatusToSCMgr(DWORD dwCurrentState, 
                         DWORD dwWin32ExitCode, 
                         DWORD dwWaitHint) 
{
    static DWORD dwCheckPoint = 1; 
    BOOL fResult = TRUE; 

    /* when debugging we don't report to the SCM    */
    if(G_SlpdCommandLine.action != SLPD_DEBUG)
    {
        if(dwCurrentState == SERVICE_START_PENDING)
        {
            ssStatus.dwControlsAccepted = 0;
        }
        else
        {
            ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP; 
        }

        ssStatus.dwCurrentState = dwCurrentState; 
        ssStatus.dwWin32ExitCode = dwWin32ExitCode; 
        ssStatus.dwWaitHint = dwWaitHint; 

        if(( dwCurrentState == SERVICE_RUNNING ) || 
           ( dwCurrentState == SERVICE_STOPPED ))
        {
            ssStatus.dwCheckPoint = 0;
        }
        else
        {
            ssStatus.dwCheckPoint = dwCheckPoint++; 
        }   

        /* Report the status of the service to the service control manager.*/

        if(!(fResult = SetServiceStatus( sshStatusHandle, &ssStatus)))
        {
            SLPDLog("SetServiceStatus failed"); 
        }
    }
    return fResult; 
} 

/** Copies error message text to a string.
 *
 * @param[out] lpszBuf - A destination buffer.
 * @param[in] dwSize - The size of @p lpszBuf in bytes.
 *
 * @return A pointer to the destination buffer (for convenience). 
 *
 * @internal
 */
LPTSTR GetLastErrorText( LPTSTR lpszBuf, DWORD dwSize ) 
{
    DWORD dwRet; 
    LPTSTR lpszTemp = NULL; 

    dwRet = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                           FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_ARGUMENT_ARRAY, 
                           NULL, 
                           GetLastError(), 
                           LANG_NEUTRAL, 
                           (LPTSTR)&lpszTemp, 
                           0, 
                           NULL ); 

    /* supplied buffer is not long enough    */
    if(!dwRet || ( (long)dwSize < (long)dwRet+14 ))
    {
        lpszBuf[0] = TEXT('\0');
    }
    else
    {
        lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  
        sprintf( lpszBuf, "%s (0x%x)", lpszTemp, GetLastError() ); 
    } 

    if(lpszTemp)
    {
        LocalFree((HLOCAL) lpszTemp );
    }

    return lpszBuf; 
} 

/** Signal the service to stop, and then report it.
 */
void ServiceStop() 
{
    G_SIGTERM = 1;
    ReportStatusToSCMgr(SERVICE_STOPPED,       /* service state */
                        NO_ERROR,              /* exit code    */
                        3000);                 /* wait hint    */
} 

/** Start the service and report it.
 *
 * @param[in] argc - The number of arguments in @p argv.
 * @param[in] argv - An array of argument string pointers.
 *
 * @internal
 */
void ServiceStart (int argc, char **argv) 
{
    fd_set          readfds;
    fd_set          writefds;
    int             highfd;
    int             fdcount         = 0;
    time_t          curtime;
    time_t          alarmtime;
    struct timeval  timeout;
    WSADATA         wsaData; 
    WORD            wVersionRequested = MAKEWORD(1,1); 

    /*------------------------*/
    /* Service initialization */
    /*------------------------*/
    if(!ReportStatusToSCMgr(SERVICE_START_PENDING, /* service state*/
                            NO_ERROR,              /* exit code    */
                            3000))                 /* wait hint    */
    {
        goto cleanup;
    }

    if(WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        (void)ReportStatusToSCMgr(SERVICE_STOP_PENDING, 
                                  NO_ERROR, 
                                  0); 
        goto cleanup;
    }

    /*------------------------*/
    /* Parse the command line */
    /*------------------------*/
    if(SLPDParseCommandLine(argc,argv))
    {
        ReportStatusToSCMgr(SERVICE_STOP_PENDING, /* service state    */
                            NO_ERROR,             /* exit code    */
                            0);                   /* wait hint    */
        goto cleanup_winsock;
    }
    if(!ReportStatusToSCMgr(SERVICE_START_PENDING, /* service state    */
                            NO_ERROR,              /* exit code    */
                            3000))                 /* wait hint    */
    {
        goto cleanup_winsock;
    }

    /*------------------------------*/
    /* Initialize the log file      */
    /*------------------------------*/
    if(SLPDLogFileOpen(G_SlpdCommandLine.logfile, 1))
    {
        SLPDLog("Could not open logfile %s\n",G_SlpdCommandLine.logfile);
        goto cleanup_winsock;
    }

    /*------------------------*/
    /* Seed the XID generator */
    /*------------------------*/
    SLPXidSeed();

    /*---------------------*/
    /* Log startup message */
    /*---------------------*/
    SLPDLog("****************************************\n");
    SLPDLogTime();
    SLPDLog("SLPD daemon started\n");
    SLPDLog("****************************************\n");
    SLPDLog("Command line = %s\n",argv[0]);
    SLPDLog("Using configuration file = %s\n",G_SlpdCommandLine.cfgfile);
    SLPDLog("Using registration file = %s\n",G_SlpdCommandLine.regfile);
    if(!ReportStatusToSCMgr(SERVICE_START_PENDING, /* service state    */
                            NO_ERROR,              /* exit code    */
                            3000))                 /* wait hint    */
    {
        goto cleanup_winsock;
    }


    /*--------------------------------------------------*/
    /* Initialize for the first time                    */
    /*--------------------------------------------------*/
    if(SLPDPropertyInit(G_SlpdCommandLine.cfgfile) ||
       SLPDDatabaseInit(G_SlpdCommandLine.regfile) ||
       SLPDIncomingInit() ||
       SLPDOutgoingInit() ||
       SLPDKnownDAInit())
    {
        SLPDLog("slpd initialization failed\n");
        goto cleanup_winsock;
    }
    SLPDLog("Agent Interfaces = %s\n",G_SlpdProperty.interfaces);
    SLPDLog("Agent URL = %s\n",G_SlpdProperty.myUrl);

    /* Service is now running, perform work until shutdown    */

    if(!ReportStatusToSCMgr(SERVICE_RUNNING,       /* service state    */
                            NO_ERROR,              /* exit code    */
                            0))                    /* wait hint    */
    {
        goto cleanup_winsock;
    }


    /*-----------*/
    /* Main loop */
    /*-----------*/
    SLPDLog("Startup complete entering main run loop ...\n\n");
    G_SIGTERM   = 0;
    curtime = time(&alarmtime);
    alarmtime = curtime + SLPD_AGE_INTERVAL;
    while(G_SIGTERM == 0)
    {
        /*--------------------------------------------------------*/
        /* Load the fdsets up with all valid sockets in the list  */
        /*--------------------------------------------------------*/
        highfd = 0;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        LoadFdSets(&G_IncomingSocketList, &highfd, &readfds,&writefds);
        LoadFdSets(&G_OutgoingSocketList, &highfd, &readfds,&writefds);

        /*--------------------------------------------------*/
        /* Before select(), check to see if we got a signal */
        /*--------------------------------------------------*/
        if(G_SIGALRM)
        {
            goto HANDLE_SIGNAL;
        }

        /*-------------*/
        /* Main select */
        /*-------------*/
        timeout.tv_sec = SLPD_AGE_INTERVAL;
        timeout.tv_usec = 0;
        fdcount = select(highfd+1,&readfds,&writefds,0,&timeout);
        if(fdcount > 0) /* fdcount will be < 0 when timed out */
        {
            SLPDIncomingHandler(&fdcount,&readfds,&writefds);
            SLPDOutgoingHandler(&fdcount,&readfds,&writefds);
        }

        /*----------------*/
        /* Handle signals */
        /*----------------*/
        HANDLE_SIGNAL:
        curtime = time(&curtime);
        if(curtime >= alarmtime)
        {
            HandleSigAlrm();
            alarmtime = curtime + SLPD_AGE_INTERVAL;
        }

    } /* End of main loop */

    /* Got SIGTERM */
    HandleSigTerm();

    cleanup_winsock:
    WSACleanup();

    cleanup: 
    ;
} 

/** Handles console control events.
 *
 * @param[in] dwCtrlType - The type of control event.
 *
 * @return A boolean value; TRUE if the event was handled, FALSE if not.
 *
 * @internal
 */
BOOL WINAPI ControlHandler ( DWORD dwCtrlType ) 
{
    switch(dwCtrlType)
    {
    case CTRL_BREAK_EVENT:  /* use Ctrl+C or Ctrl+Break to simulate    */
    case CTRL_C_EVENT:      /* SERVICE_CONTROL_STOP in debug mode    */
        printf("Stopping %s.\n", G_SERVICEDISPLAYNAME); 
        ServiceStop(); 
        return TRUE; 
        break; 

    } 
    return FALSE; 
} 

/** Called by the SCM whenever ControlService is called on this service.
 *
 * @param[in] dwCtrlCode - The type of control requested.
 *
 * @internal
 */
VOID WINAPI ServiceCtrl(DWORD dwCtrlCode) 
{
    /* Handle the requested control code.    */
    /*    */

    switch(dwCtrlCode)
    {
    /* Stop the service.    */
    case SERVICE_CONTROL_STOP: 
        ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0); 
        ServiceStop(); 
        return; 

        /* Update the service status.    */
    case SERVICE_CONTROL_INTERROGATE: 
        break; 

        /* invalid control code    */
    default: 
        break; 

    } 

    ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0); 
} 

/** Win32 service main entry point.
 *
 * @param[in] argc - The number of arguments in @p argv.
 *
 * @param[in] argv - An array of argument string pointers.
 *
 * @internal
 */
void WINAPI SLPDServiceMain(DWORD argc, LPTSTR *argv) 
{

    /* register our service control handler:    */
    sshStatusHandle = RegisterServiceCtrlHandler( G_SERVICENAME, ServiceCtrl); 

    if(sshStatusHandle != 0)
    {
        /* SERVICE_STATUS members that don't change    */
        ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
        ssStatus.dwServiceSpecificExitCode = 0; 


        /* report the status to the service control manager.    */
        if(ReportStatusToSCMgr(SERVICE_START_PENDING, /* service state    */
                               NO_ERROR,              /* exit code    */
                               3000))                 /* wait hint    */
        {
            ServiceStart(argc, argv); 
        }
    }


    /* try to report the stopped status to the service control manager.    */

    if(sshStatusHandle)
        (void)ReportStatusToSCMgr(SERVICE_STOPPED, 
                                  0, 
                                  0);
} 

/** Install the service.
 *
 * @param[in] automatic - A flag indicating whether or not the service
 *    should be installed to start automatically at boot time.
 *
 * @internal
 */
void SLPDCmdInstallService(int automatic) 
{
    SC_HANDLE   schService; 
    SC_HANDLE   schSCManager; 

    DWORD start_type;
    TCHAR szPath[512]; 

    if(GetModuleFileName( NULL, szPath, 512 ) == 0)
    {
        printf("Unable to install %s - %s\n", 
               G_SERVICEDISPLAYNAME, 
               GetLastErrorText(szErr, 256)); 
        return; 
    }

    if(automatic)
    {
      start_type = SERVICE_AUTO_START;
    }
    else
    {
      start_type = SERVICE_DEMAND_START;
    } 

    schSCManager = OpenSCManager(
                                NULL,                   /* machine (NULL == local)    */
                                NULL,                   /* database (NULL == default)    */
                                SC_MANAGER_ALL_ACCESS); /* access required    */

    if(schSCManager)
    {
        schService = CreateService(
                                  schSCManager,               /* SCManager database    */
                                  G_SERVICENAME,        /* name of service    */
                                  G_SERVICEDISPLAYNAME, /* name to display    */
                                  SERVICE_ALL_ACCESS,         /* desired access    */
                                  SERVICE_WIN32_OWN_PROCESS,  /* service type    */
                                  start_type,       			  /* start type    */
                                  SERVICE_ERROR_NORMAL,       /* error control type    */
                                  szPath,                     /* service's binary    */
                                  NULL,                       /* no load ordering group    */
                                  NULL,                       /* no tag identifier    */
                                  "",       /* dependencies    */
                                  NULL,                       /* LocalSystem account    */
                                  NULL);                      /* no password    */

        if(schService)
        {
            printf("%s installed.\n", G_SERVICEDISPLAYNAME ); 
            CloseServiceHandle(schService); 
        }
        else
        {
            printf("CreateService failed - %s\n", GetLastErrorText(szErr, 256)); 
        } 

        CloseServiceHandle(schSCManager); 
    }
    else
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr,256)); 
} 

/** Stop a service by service handle.
 *
 * @param[in] schService - A handle to the service to be stopped.
 *
 * @internal
 */
static void SLPDHlpStopService(SC_HANDLE schService)
{
	/* try to stop the service    */
	if(ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus))
	{
		printf("Stopping %s.", G_SERVICEDISPLAYNAME);
		Sleep(1000);
      
		while(QueryServiceStatus(schService, &ssStatus))
		{
			if(ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
			{
				printf(".");
				Sleep(1000);
			}
			else
				break; 
		} 
      
		if(ssStatus.dwCurrentState == SERVICE_STOPPED)
			printf("\n%s stopped.\n", G_SERVICEDISPLAYNAME);
		else
			printf("\n%s failed to stop.\n", G_SERVICEDISPLAYNAME); 
	}
}
 
/** Uninstall the service.
 *
 * @internal
 */
void SLPDCmdRemoveService() 
{
    SC_HANDLE schService; 
    SC_HANDLE schSCManager; 

    schSCManager = OpenSCManager(
                                NULL,                   /* machine (NULL == local)    */
                                NULL,                   /* database (NULL == default)    */
                                SC_MANAGER_ALL_ACCESS); /* access required    */
    if(schSCManager)
    {
        schService = OpenService(schSCManager, G_SERVICENAME, SERVICE_ALL_ACCESS); 

        if(schService)
        {
				SLPDHlpStopService(schService);

				/* now remove the service    */
            if(DeleteService(schService))
                printf("%s removed.\n", G_SERVICEDISPLAYNAME );
            else
                printf("DeleteService failed - %s\n", GetLastErrorText(szErr,256)); 


            CloseServiceHandle(schService); 
        }
        else
            printf("OpenService failed - %s\n", GetLastErrorText(szErr,256)); 

        CloseServiceHandle(schSCManager); 
    }
    else
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr,256)); 
} 

/** Start the service.
 *
 * @internal
 */
void SLPDCmdStartService()
{
	 SC_HANDLE schService; 
	 SC_HANDLE schSCManager; 

	 schSCManager = OpenSCManager(
	 									  NULL,                   /* machine (NULL == local)    */
	 									  NULL,                   /* database (NULL == default) */
	 									  SC_MANAGER_ALL_ACCESS); /* access required    		  */
	 if(schSCManager)
	 {
	 	  schService = OpenService(schSCManager, G_SERVICENAME, SERVICE_ALL_ACCESS); 

	 	  if(schService)
	 	  {
	 			if( !StartService(schService, 0, NULL))
	 			{
	 				 printf("OpenService failed - %s\n", GetLastErrorText(szErr,256)); 
	 			}
	 			CloseServiceHandle(schService); 
	 	  }
	 	  else
	 	  {
	 	  		printf("OpenService failed - %s\n", GetLastErrorText(szErr,256)); 
	 	  }
	 	  CloseServiceHandle(schSCManager); 
	 }
	 else
	 {
	 	  printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr,256)); 
	 }
}

/** Stop the service.
 *
 * @internal
 */
void SLPDCmdStopService()
{
	 SC_HANDLE schService; 
	 SC_HANDLE schSCManager; 

	 schSCManager = OpenSCManager(
	 									  NULL,                   /* machine (NULL == local)    */
	 									  NULL,                   /* database (NULL == default) */
	 									  SC_MANAGER_ALL_ACCESS); /* access required    		  */
	 if(schSCManager)
	 {
	 	  schService = OpenService(schSCManager, G_SERVICENAME, SERVICE_ALL_ACCESS); 

	 	  if(schService)
	 	  {
	 			SLPDHlpStopService(schService);
	 			CloseServiceHandle(schService); 
	 	  }
	 	  else
	 	  {
	 			printf("OpenService failed - %s\n", GetLastErrorText(szErr,256)); 
	 	  }
	 	  CloseServiceHandle(schSCManager); 
	 }
	 else
	 {
	 	  printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr,256)); 
	 }
}

/** Debug the service.
 *
 * @param[in] argc - The number of arguments in @p argv.
 *
 * @param[in] argv - An array of argument string pointers.
 *
 * @internal
 */
void SLPDCmdDebugService(int argc, char ** argv) 
{
    printf("Debugging %s.\n", G_SERVICEDISPLAYNAME); 

    SetConsoleCtrlHandler( ControlHandler, TRUE ); 
    ServiceStart( argc, argv ); 
}

/** The main program entry point (when not executed as a service). 
 *
 * @param[in] argc - The number of arguments in @p argv.
 *
 * @param[in] argv - An array of argument string pointers.
 */
void __cdecl main(int argc, char **argv) 
{
    SERVICE_TABLE_ENTRY dispatchTable[] = 
    { 
        { G_SERVICENAME, (LPSERVICE_MAIN_FUNCTION)SLPDServiceMain}, 
        { NULL, NULL} 
    }; 

    /*------------------------*/
    /* Parse the command line */
    /*------------------------*/
    if(SLPDParseCommandLine(argc,argv))
    {
        SLPDFatal("Invalid command line\n");
    }

    switch(G_SlpdCommandLine.action)
    {
    case SLPD_DEBUG:
        SLPDCmdDebugService(argc, argv);
        break;
    case SLPD_INSTALL:
        SLPDCmdInstallService(G_SlpdCommandLine.autostart);
        break;
    case SLPD_REMOVE:
        SLPDCmdRemoveService();
        break;
    case SLPD_START:
        SLPDCmdStartService();
		  break;
    case SLPD_STOP:
        SLPDCmdStopService();
        break;
    default:
        SLPDPrintUsage();
        StartServiceCtrlDispatcher(dispatchTable);

        break;
    } 
} 

/*=========================================================================*/
