// vscp2drv-energy-p1.cpp : Defines the initialization routines for the DLL.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP (http://www.vscp.org)
//
// Copyright (C) 2000-2021 Ake Hedman,
// Ake Hedman, the VSCP Project, <akhe@vscp.org>
//
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this file see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//

#ifdef __GNUG__
//#pragma implementation
#endif

#ifdef WIN32
#include "StdAfx.h"
#endif

#include <map>
#include <string>

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "version.h"

#include <vscp.h>
#include <vscphelper.h>
#include <hlo.h>
#include <com_linux.h>

#include "vscpl2drv-energy-p1.h"
#include "energy-p1-obj.h"

#ifdef WIN32
#else
void
_init() __attribute__((constructor));
void
_fini() __attribute__((destructor));

void
_init() __attribute__((constructor));
void
_fini() __attribute__((destructor));
#endif

// This map holds driver handles/objects
static std::map<long, CEnergyP1*> g_ifMap;

// Mutex for the map object
static pthread_mutex_t g_mapMutex;

////////////////////////////////////////////////////////////////////////////
// DLL constructor
//

void
_init()
{
    pthread_mutex_init(&g_mapMutex, NULL);
}

////////////////////////////////////////////////////////////////////////////
// DLL destructor
//

void
_fini()
{
    // If empty - nothing to do
    if (g_ifMap.empty())
        return;

    // Remove orphan objects
    LOCK_MUTEX(g_mapMutex);

    for (std::map<long, CEnergyP1*>::iterator it = g_ifMap.begin();
         it != g_ifMap.end();
         ++it) {
        // std::cout << it->first << " => " << it->second << '\n';

        CEnergyP1* pif = it->second;
        if (NULL != pif) {
            //pif->m_srvRemoteSend.doCmdClose();
            //pif->m_srvRemoteReceive.doCmdClose();
            delete pif;
            pif = NULL;
        }
    }

    g_ifMap.clear();    // Remove all items

    UNLOCK_MUTEX(g_mapMutex);
    pthread_mutex_destroy(&g_mapMutex);
}

///////////////////////////////////////////////////////////////////////////////
// addDriverObject
//

long
addDriverObject(CEnergyP1* pif)
{
    std::map<long, CEnergyP1*>::iterator it;
    long h = 0;

    LOCK_MUTEX(g_mapMutex);

    // Find free handle
    while (true) {
        if (g_ifMap.end() == (it = g_ifMap.find(h)))
            break;
        h++;
    }

    g_ifMap[h] = pif;
    h += 1681;

    UNLOCK_MUTEX(g_mapMutex);

    return h;
}

///////////////////////////////////////////////////////////////////////////////
// getDriverObject
//

CEnergyP1*
getDriverObject(long h)
{
    std::map<long, CEnergyP1*>::iterator it;
    long idx = h - 1681;

    // Check if valid handle
    if (idx < 0) {
        return NULL;
    }

    it = g_ifMap.find(idx);
    if (it != g_ifMap.end()) {
        return it->second;
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// removeDriverObject
//

void
removeDriverObject(long h)
{
    std::map<long, CEnergyP1*>::iterator it;
    long idx = h - 1681;

    // Check if valid handle
    if (idx < 0) {
        return;
    }

    LOCK_MUTEX(g_mapMutex);
    it = g_ifMap.find(idx);
    if (it != g_ifMap.end()) {
        CEnergyP1* pObj = it->second;
        if (NULL != pObj) {
            delete pObj;
            pObj = NULL;
        }
        g_ifMap.erase(it);
    }
    UNLOCK_MUTEX(g_mapMutex);
}

///////////////////////////////////////////////////////////////////////////////
//                         V S C P   D R I V E R -  A P I
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// VSCPOpen
//

extern "C" long
VSCPOpen(const char* pPathConfig, const char* pguid)
{
    long h = 0;

    CEnergyP1* pdrvObj = new CEnergyP1();
    if (NULL != pdrvObj) {

        //cguid guid(pguid);
        std::string path = pPathConfig;
        if (path.length() && pdrvObj->open(path, (const uint8_t *)pguid)) {

            if (!(h = addDriverObject(pdrvObj))) {
                delete pdrvObj;
            }

        } else {
            delete pdrvObj;
        }
    }

    return h;
}

///////////////////////////////////////////////////////////////////////////////
//  VSCPClose
//

extern "C" int
VSCPClose(long handle)
{
    CEnergyP1* pdrvObj = getDriverObject(handle);
    if (NULL == pdrvObj) {
        return 0;
    }

    pdrvObj->close();
    removeDriverObject(handle);

    return CANAL_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//  VSCPWrite
//

extern "C" int
VSCPWrite(long handle, const vscpEvent* pEvent, unsigned long timeout)
{
    CEnergyP1* pdrvObj = getDriverObject(handle);
    if (NULL == pdrvObj) {
        return CANAL_ERROR_MEMORY;
    }

    //pdrvObj->addEvent2SendQueue(pEvent);
    //pdrvObj->sendEventAllClients(pEvent);

    return CANAL_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//  VSCPRead
//

extern "C" int
VSCPRead(long handle, vscpEvent* pEvent, unsigned long timeout)
{
    int rv = 0;

    // Check pointer
    if (NULL == pEvent) {
        return CANAL_ERROR_PARAMETER;
    }

    CEnergyP1* pdrvObj = getDriverObject(handle);
    if (NULL == pdrvObj) {
        return CANAL_ERROR_MEMORY;
    }

    if (-1 == (rv = vscp_sem_wait(&pdrvObj->m_semReceiveQueue, timeout))) {       
        if (ETIMEDOUT == errno) {
            return CANAL_ERROR_TIMEOUT;
        } else if (EINTR == errno) {
#ifndef WIN32            
            syslog(LOG_ERR, "[vscpl2drv-tcpipsrv] Interrupted by a signal handler");
#endif            
            return CANAL_ERROR_INTERNAL;
        } 
        else if (EINVAL == errno) {
#ifndef WIN32            
            syslog(LOG_ERR, "[vscpl2drv-tcpipsrv] Invalid semaphore (timout)");
#endif            
            return CANAL_ERROR_INTERNAL;
        } 
        else if (EAGAIN == errno) {
#ifndef WIN32            
            syslog(LOG_ERR, "[vscpl2drv-tcpipsrv] Blocking error");
#endif            
            return CANAL_ERROR_INTERNAL;
        } 
        else {
#ifndef WIN32            
            syslog(LOG_ERR, "[vscpl2drv-tcpipsrv] Unknown error");
#endif            
            return CANAL_ERROR_INTERNAL;
        }
    }

    pthread_mutex_lock(&pdrvObj->m_mutexReceiveQueue);
    vscpEvent* pLocalEvent = pdrvObj->m_receiveList.front();
    pdrvObj->m_receiveList.pop_front();
    pthread_mutex_unlock(&pdrvObj->m_mutexReceiveQueue);
    if (NULL == pLocalEvent) {
        return CANAL_ERROR_MEMORY;
    }

    vscp_copyEvent(pEvent, pLocalEvent);
    vscp_deleteEvent_v2(&pLocalEvent);

    return CANAL_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// VSCPGetVersion
//

extern "C" unsigned long
VSCPGetVersion(void)
{
    unsigned long ver = MAJOR_VERSION << 24 |
                        MINOR_VERSION << 16 |
                        RELEASE_VERSION << 8 |
                        BUILD_VERSION;
    return ver;
}

///////////////////////////////////////////////////////////////////////////////
// VSCPGetVendorString
//

extern "C" const char *
VSCPGetVendorString(void)
{
    return VSCP_DLL_VENDOR;
}
