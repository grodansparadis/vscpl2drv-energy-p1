// energy-p1-obj.h: 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP (http://www.vscp.org)
//
// Copyright (C) 2000-2023 Ake Hedman,
// the VSCP Project, <akhe@vscp.org>
//
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this file see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//

#if !defined(VSCPENERGYP1_TEST_H__202105112227__INCLUDED_)
#define VSCPENERGYP1_TEST_H__202105112227__INCLUDED_

#define _POSIX

#ifdef WIN32
#include "StdAfx.h"
#endif

#include <map>
#include <string>

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#if WIN32
#else
#include <syslog.h>
#include <unistd.h>
#endif
#include <time.h>

#include <canal.h>
#include <canal_macro.h>
#include <dllist.h>
#include <guid.h>
#include <vscp.h>

#include <nlohmann/json.hpp>  // Needs C++11  -std=c++11

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "../src/alarm.h"
#include "../src/p1item.h"
#include "../src/energy-p1-obj.h"

int main()
{ 
  CEnergyP1 p1;
  
  
  return 0;
}


#endif  // VSCPENERGYP1_TEST_H__202105112227__INCLUDED_