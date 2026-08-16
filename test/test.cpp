// energy-p1-obj.h: 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP (http://www.vscp.org)
//
// Copyright (C) 2000-2026 Ake Hedman,
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

#include <iostream>
#include <string>

#include "spdlog/spdlog.h"
#include <vscphelper.h>

#include "../src/energy-p1-obj.h"

static void
clearReceiveQueue(CEnergyP1 &p1)
{
  for (auto *event : p1.m_receiveList) {
    vscp_deleteEvent(event);
  }
  p1.m_receiveList.clear();
}

int
main(int argc, char *argv[])
{
  if (3 != argc) {
    std::cerr << "Usage: " << argv[0] << " <config.json> <telegram.data>\n";
    return 1;
  }

  std::string configPath = argv[1];
  CEnergyP1 p1;
  spdlog::set_level(spdlog::level::off);
  if (!p1.doLoadConfig(configPath, false)) {
    std::cerr << "Unable to load driver configuration: " << configPath << '\n';
    return 1;
  }

  if (!p1.startWorkerThread(argv[2]) || !p1.waitWorkerThread()) {
    std::cerr << "Unable to process input file: " << argv[2] << '\n';
    return 1;
  }

  size_t count = 0;
  for (auto *item : p1.m_listItems) {
    const auto value = p1.m_lastValue.find(item->getStorageName());
    if (p1.m_lastValue.end() == value) {
      continue;
    }
    std::cout << value->first << '=' << value->second << '\n';
    count++;
  }

  clearReceiveQueue(p1);
  std::cout << count << " measurement(s)\n";
  return 0;
}