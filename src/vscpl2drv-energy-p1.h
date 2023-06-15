// vscp2drv-energy-p1.h : main header file for the canallogger.dll
// Linux version
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
// 
// This file is part of the VSCP (http://www.vscp.org) 
//
// Copyright (C) 2000-2023 Ake Hedman, 
// Ake Hedman, the VSCP Project, <akhe@vscp.org>
// 
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU Lesser  General Public License
// along with this file see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//


#if !defined(VSCPL2_TCPIPLINK_H__A388C093_AD35_4672_8BF7_DBC702C6B0C8__INCLUDED_)
#define VSCPL2_TCPIPLINK_H__A388C093_AD35_4672_8BF7_DBC702C6B0C8__INCLUDED_

#ifdef WIN32
#include "StdAfx.h"
#endif

#define _POSIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#else
#include <unistd.h>
#include <syslog.h>
#endif
#include <pthread.h>
#include <canal.h>
#include <vscpremotetcpif.h>
#include <canal_macro.h>
#include <vscp.h>
#include <hlo.h>

#include "energy-p1-obj.h"

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// This is the vendor string - Change to your own value
#define VSCP_DLL_VENDOR "the VSCP Project, Sweden, https://www.vscp.org"


/*!
    Add a driver object

    @parm plog Object to add
    @return handle or 0 for error
*/
long
addDriverObject(CEnergyP1 *pif);

/*!
    Get a driver object from its handle

    @param handle for object
    @return pointer to object or NULL if invalid
            handle.
*/
CEnergyP1 *
getDriverObject(long handle);

/*!
    Remove a driver object
    @param handle for object.
*/
void
removeDriverObject(long handle);


#endif // !defined(VSCPL2_TCPIPLINK_H__A388C093_AD35_4672_8BF7_DBC702C6B0C8__INCLUDED_)
