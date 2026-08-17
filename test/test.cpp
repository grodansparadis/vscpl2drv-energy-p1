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

#include <chrono>
#include <iomanip>
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

  const auto parseStart = std::chrono::steady_clock::now();
  if (!p1.startWorkerThread(argv[2]) || !p1.waitWorkerThread()) {
    std::cerr << "Unable to process input file: " << argv[2] << '\n';
    return 1;
  }
  const auto parseEnd = std::chrono::steady_clock::now();
  const std::chrono::duration<double, std::milli> parseTime = parseEnd - parseStart;

  size_t currentFrame = 0;
  for (const auto &measurement : p1.m_workerMeasurements) {
    if (measurement.frame != currentFrame) {
      currentFrame = measurement.frame;
      std::cout << "--- telegram " << currentFrame << " ---\n";
    }
    std::cout << measurement.name << '=' << measurement.value << '\n';
  }

  clearReceiveQueue(p1);
  std::cout << p1.m_workerMeasurements.size() << " measurement(s) in "
            << p1.m_workerFrameCount << " frame(s)\n";
  std::cout << "Parsing time: " << std::fixed << std::setprecision(3) << parseTime.count() << " ms\n";

  if (p1.m_workerCrcErrorCount) {
    for (const auto &failure : p1.m_workerCrcFailures) {
      std::cerr << "CRC FAILURE: " << failure << '\n';
    }
    std::cerr << "CRC FAILURE: " << p1.m_workerCrcErrorCount << " frame(s) rejected\n";
    return 2;
  }

  return 0;
}