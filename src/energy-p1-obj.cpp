// energy-p1-obj.cpp:
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP Project (http://www.vscp.org)
//
// Copyright (C) 2000-2021 Ake Hedman,
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

#ifdef WIN32
#include "StdAfx.h"
#endif

#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

#include <sys/types.h>

#ifdef WIN32
#else
#include <libgen.h>
#include <net/if.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <sys/types.h>
#include <time.h>

#include <expat.h>

#include "alarm.h"
#include "energy-p1-obj.h"

#include <com.h>
#include <hlo.h>
#include <remotevariablecodes.h>
#include <vscp.h>
#include <vscp_class.h>
#include <vscp_type.h>
#include <vscpdatetime.h>
#include <vscphelper.h>

#include <json.hpp> // Needs C++11  -std=c++11
#include <mustache.hpp>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <string>

// https://github.com/nlohmann/json
using json = nlohmann::json;

using namespace kainjow::mustache;

// Forward declaration
void *
workerThread(void *pData);

//////////////////////////////////////////////////////////////////////
// CEnergyP1
//

CEnergyP1::CEnergyP1()
{
  m_bQuit = false;

  // Init seral data
  m_serialDevice        = "/dev/ttyUSB0";
  m_serialBaudrate      = "115200";
  m_serialParity        = "N";
  m_serialCountDataBits = "8";
  m_serialCountStopbits = 1;
  m_bSerialHwFlowCtrl   = false;
  m_bSerialSwFlowCtrl   = false;
  m_bDtrOnStart         = true;

  vscp_clearVSCPFilter(&m_rxfilter); // Accept all events
  vscp_clearVSCPFilter(&m_txfilter); // Send all events

  sem_init(&m_semSendQueue, 0, 0);
  sem_init(&m_semReceiveQueue, 0, 0);

  pthread_mutex_init(&m_mutexSendQueue, NULL);
  pthread_mutex_init(&m_mutexReceiveQueue, NULL);

  // Init pool
  spdlog::init_thread_pool(8192, 1);

  // Flush log every five seconds
  spdlog::flush_every(std::chrono::seconds(5));

  auto console = spdlog::stdout_color_mt("console");
  // Start out with level=info. Config may change this
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[vscpl2drv-energy-p1] [%^%l%$] %v");
  spdlog::set_default_logger(console);

  spdlog::debug("Starting the vscpl2drv-energy-p1 driver...");

  m_bConsoleLogEnable = true;
  m_consoleLogLevel   = spdlog::level::debug;
  m_consoleLogPattern = "[vscpl2drv-energy.p1 %c] [%^%l%$] %v";

  m_bFileLogEnable   = true;
  m_fileLogLevel     = spdlog::level::debug;
  m_fileLogPattern   = "[vscpl2drv-energy-p1 %c] [%^%l%$] %v";
  m_path_to_log_file = "/var/log/vscp/vscpl2drv-energy-p1.log";
  m_max_log_size     = 5242880;
  m_max_log_files    = 7;
}

//////////////////////////////////////////////////////////////////////
// ~CEnergyP1
//

CEnergyP1::~CEnergyP1()
{
  close();

  m_bQuit = true;
  pthread_join(m_workerThread, NULL);

  sem_destroy(&m_semSendQueue);
  sem_destroy(&m_semReceiveQueue);

  pthread_mutex_destroy(&m_mutexSendQueue);
  pthread_mutex_destroy(&m_mutexReceiveQueue);

  // Deallocate ON alarms
  for (auto const &alarm : m_mapAlarmOn) {
    delete alarm.second;
  }

  m_mapAlarmOn.clear();

  // Deallocate OFF alarms
  for (auto const &alarm : m_mapAlarmOff) {
    delete alarm.second;
  }

  m_mapAlarmOff.clear();

  // Deallocate measurement items
  for (auto const &item : m_listItems) {
    delete item;
  }
  m_listItems.clear();

  // Shutdown logger in a nice way
  spdlog::drop_all();
  spdlog::shutdown();
}

//////////////////////////////////////////////////////////////////////
// open
//

bool
CEnergyP1::open(std::string &path, const uint8_t *pguid)
{
  if (NULL == pguid) {
    return false;
  }

  // Set GUID
  m_guid.getFromArray(pguid);

  // Save path to config file
  m_path = path;

  spdlog::debug("About to read configurationfile {}.", path.c_str());

  // Read configuration file
  if (!doLoadConfig(path)) {
    spdlog::error("Failed to load configuration file [{}]", path.c_str());
    spdlog::drop_all();
    return false;
  }

  // Set up logger
  // if (m_path_to_log_file.length()) {
  //   auto rotating_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_path_to_log_file.c_str(),
  //                                                                                    m_max_log_size,
  //                                                                                    m_max_log_files);
  //   if (m_bFileLogEnable) {
  //     rotating_file_sink->set_level(m_fileLogLevel);
  //     rotating_file_sink->set_pattern(m_fileLogPattern);
  //   }
  //   else {
  //     // If disabled set to off
  //     rotating_file_sink->set_level(spdlog::level::off);
  //   }

  //   std::vector<spdlog::sink_ptr> sinks{ rotating_file_sink };
  //   auto logger = std::make_shared<spdlog::async_logger>("logger",
  //                                                        sinks.begin(),
  //                                                        sinks.end(),
  //                                                        spdlog::thread_pool(),
  //                                                        spdlog::async_overflow_policy::block);
  //   // The separate sub loggers will handle trace levels
  //   logger->set_level(spdlog::level::trace);
  //   logger->flush_on(spdlog::level::debug);
  //   spdlog::flush_every(std::chrono::seconds(5));
  //   spdlog::register_logger(logger);
  //   spdlog::set_default_logger(logger);
  // }

  spdlog::debug("Logging starts here after config file is read.");

  if (!startWorkerThread()) {
    spdlog::error("Failed to start worker thread.");
    spdlog::drop_all();
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////
// close
//

void
CEnergyP1::close(void)
{
  // Do nothing if already terminated
  if (m_bQuit) {
    spdlog::drop_all();
    return;
  }

  m_bQuit = true; // terminate the thread
#ifndef WIN32
  sleep(1); // Give the thread some time to terminate
#else
  Sleep(1000);
#endif

  pthread_join(m_workerThread, NULL);

  spdlog::drop_all();
  spdlog::shutdown();
}

///////////////////////////////////////////////////////////////////////////////
// loadConfiguration
//

bool
CEnergyP1::doLoadConfig(std::string &path)
{
  try {
    std::ifstream in(m_path, std::ifstream::in);
    in >> m_j_config;
    spdlog::debug("doLoadConfig: JSON loaded.");
  }
  catch (json::parse_error) {
    spdlog::critical("Failed to load/parse JSON configuration.");
    return false;
  }

  // write
  if (m_j_config.contains("write")) {
    try {
      m_bWriteEnable = m_j_config["write"].get<bool>();
      spdlog::debug("doLoadConfig: Write enable {}", m_bWriteEnable);
    }
    catch (const std::exception &ex) {
      spdlog::error("Failed to read 'write' Error='{}'", ex.what());
    }
    catch (...) {
      spdlog::error("Failed to read 'write' due to unknown error.");
    }
  }
  else {
    spdlog::error("ReadConfig: Failed to read LOGGING 'write' Defaults will be used.");
  }

  // VSCP key file
  if (m_j_config.contains("key-file") && m_j_config["key-file"].is_string()) {
    if (!readEncryptionKey(m_j_config["key-file"].get<std::string>())) {
      spdlog::warn("WARNING!!! Failed to load encryption key. Default key will be used.");
    }
  }

  // * * * Logging * * *

  if (m_j_config.contains("logging") && m_j_config["logging"].is_object()) {

    json j = m_j_config["logging"];

    // * * *  CONSOLE  * * *

    // Logging: console-log-enable
    if (j.contains("console-enable")) {
      try {
        m_bConsoleLogEnable = j["console-enable"].get<bool>();
        spdlog::debug("doLoadConfig: 'console-enable' {}", m_bConsoleLogEnable);
      }
      catch (const std::exception &ex) {
        spdlog::error("Failed to read 'console-enable' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("Failed to read 'console-enable' due to unknown error.");
      }
    }
    else {
      spdlog::debug("Failed to read LOGGING 'console-enable' Defaults will be used.");
    }

    // Logging: console-log-level
    if (j.contains("console-level")) {
      std::string str;
      try {
        str = j["console-level"].get<std::string>();
        spdlog::debug("doLoadConfig: 'console-level' {}", str);
      }
      catch (const std::exception &ex) {
        spdlog::error("Failed to read 'console-level' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("Failed to read 'console-level' due to unknown error.");
      }
      vscp_makeLower(str);
      if (std::string::npos != str.find("off")) {
        m_consoleLogLevel = spdlog::level::off;
      }
      else if (std::string::npos != str.find("critical")) {
        m_consoleLogLevel = spdlog::level::critical;
      }
      else if (std::string::npos != str.find("err")) {
        m_consoleLogLevel = spdlog::level::err;
      }
      else if (std::string::npos != str.find("warn")) {
        m_consoleLogLevel = spdlog::level::warn;
      }
      else if (std::string::npos != str.find("info")) {
        m_consoleLogLevel = spdlog::level::info;
      }
      else if (std::string::npos != str.find("debug")) {
        m_consoleLogLevel = spdlog::level::debug;
      }
      else if (std::string::npos != str.find("trace")) {
        m_consoleLogLevel = spdlog::level::trace;
      }
      else {
        spdlog::error("Failed to read LOGGING 'console-level' has invalid "
                      "value [{}]. Default value used.",
                      str);
      }
    }
    else {
      spdlog::error("Failed to read LOGGING 'console-level' Defaults will be used.");
    }

    // Logging: console-log-pattern
    if (j.contains("console-pattern")) {
      try {
        m_consoleLogPattern = j["console-pattern"].get<std::string>();
        spdlog::debug("doLoadConfig: 'console-pattern' {}", m_consoleLogPattern);
      }
      catch (const std::exception &ex) {
        spdlog::error("Failed to read 'console-pattern' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("Failed to read 'console-pattern' due to unknown error.");
      }
    }
    else {
      spdlog::debug("Failed to read LOGGING 'console-pattern' Defaults will be used.");
    }

    // * * *  FILE  * * *

    // Logging: file-enable-log
    if (j.contains("file-log-enable") && j["file-log-enable"].is_boolean()) {
      try {
        m_bFileLogEnable = j["file-log-enable"].get<bool>();
        spdlog::debug("doLoadConfig: 'file-log-enable' {}", m_bFileLogEnable);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-log-enable' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-log-enable' due to unknown error.");
      }
    }
    else {
      spdlog::debug("ReadConfig: Failed to read LOGGING 'file-log-enable' Defaults will be used.");
    }

    // Logging: file-log-level
    if (j.contains("file-log-level")) {
      std::string str;
      try {
        str = j["file-log-level"].get<std::string>();
        spdlog::debug("doLoadConfig: 'file-log-level' {}", str);
      }
      catch (const std::exception &ex) {
        spdlog::error("[vscpl2drv-energyp1] Failed to read 'file-log-level' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("[vscpl2drv-energyp1] Failed to read 'file-log-level' due to unknown error.");
      }
      vscp_makeLower(str);
      if (std::string::npos != str.find("off")) {
        m_fileLogLevel = spdlog::level::off;
      }
      else if (std::string::npos != str.find("critical")) {
        m_fileLogLevel = spdlog::level::critical;
      }
      else if (std::string::npos != str.find("err")) {
        m_fileLogLevel = spdlog::level::err;
      }
      else if (std::string::npos != str.find("warn")) {
        m_fileLogLevel = spdlog::level::warn;
      }
      else if (std::string::npos != str.find("info")) {
        m_fileLogLevel = spdlog::level::info;
      }
      else if (std::string::npos != str.find("debug")) {
        m_fileLogLevel = spdlog::level::debug;
      }
      else if (std::string::npos != str.find("trace")) {
        m_fileLogLevel = spdlog::level::trace;
      }
      else {
        spdlog::error("ReadConfig: LOGGING 'file-log-level' has invalid value [{}]. Default value used.", str);
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-log-level' Defaults will be used.");
    }

    // Logging: file-pattern
    if (j.contains("file-log-pattern")) {
      try {
        m_fileLogPattern = j["file-log-pattern"].get<std::string>();
        spdlog::debug("doLoadConfig: 'file-log-pattern' {}", m_fileLogPattern);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-log-pattern' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-log-pattern' due to unknown error.");
      }
    }
    else {
      spdlog::debug("ReadConfig: Failed to read LOGGING 'file-log-pattern' Defaults will be used.");
    }

    // Logging: file-path
    if (j.contains("file-log-path")) {
      try {
        m_path_to_log_file = j["file-log-path"].get<std::string>();
        spdlog::debug("doLoadConfig: 'file-log-path' {}", m_path_to_log_file);
      }
      catch (const std::exception &ex) {
        spdlog::error("Failed to read 'file-log-path' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-log-path' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-path' Defaults will be used.");
    }

    // Logging: file-max-size
    if (j.contains("file-log-max-size")) {
      try {
        m_max_log_size = j["file-log-max-size"].get<uint32_t>();
        spdlog::debug("doLoadConfig: 'file-log-max-size' {}", m_max_log_size);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-log-max-size' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-log-max-size' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-log-max-size' Defaults will be used.");
    }

    // Logging: file-max-files
    if (j.contains("file-log-max-files")) {
      try {
        m_max_log_files = j["file-log-max-files"].get<uint16_t>();
        spdlog::debug("doLoadConfig: 'file-log-max-files' {}", m_max_log_files);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-log-max-files' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-log-max-files' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-log-max-files' Defaults will be used.");
    }

  } // Logging
  else {
    spdlog::error("ReadConfig: No logging has been setup.");
  }

  ///////////////////////////////////////////////////////////////////////////
  //                          Setup logger
  ///////////////////////////////////////////////////////////////////////////

  // Console log
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  if (m_bConsoleLogEnable) {
    spdlog::debug("doLoadConfig: Enable console log");
    console_sink->set_level(m_consoleLogLevel);
    console_sink->set_pattern(m_consoleLogPattern);
  }
  else {
    // If disabled set to off
    console_sink->set_level(spdlog::level::off);
    spdlog::debug("doLoadConfig: 'Disable console log");
  }

  // auto rotating =
  // std::make_shared<spdlog::sinks::rotating_file_sink_mt>("log_filename",
  // 1024*1024, 5, false);
  auto rotating_file_sink =
    std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_path_to_log_file.c_str(), m_max_log_size, m_max_log_files);

  if (m_bFileLogEnable) {
    rotating_file_sink->set_level(m_fileLogLevel);
    rotating_file_sink->set_pattern(m_fileLogPattern);
    spdlog::debug("doLoadConfig: 'Enable file log");
  }
  else {
    // If disabled set to off
    rotating_file_sink->set_level(spdlog::level::off);
    spdlog::debug("doLoadConfig: 'Disable file log");
  }

  std::vector<spdlog::sink_ptr> sinks{ console_sink, rotating_file_sink };
  auto logger = std::make_shared<spdlog::async_logger>("logger",
                                                       sinks.begin(),
                                                       sinks.end(),
                                                       spdlog::thread_pool(),
                                                       spdlog::async_overflow_policy::block);
  // The separate sub loggers will handle trace levels
  logger->set_level(spdlog::level::trace);
  spdlog::register_logger(logger);

  spdlog::debug("doLoadConfig: Logging has been set up");

  // ------------------------------------------------------------------------

  // * * * Serial settings * * *

  if (m_j_config.contains("serial") && m_j_config["serial"].is_object()) {

    json j = m_j_config["serial"];

    // Serial interface
    if (j.contains("port") && j["port"].is_string()) {
      try {
        m_serialDevice = j["port"].get<std::string>();
        spdlog::debug("doLoadConfig: 'port' {}", m_serialDevice);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'port' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'port' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'port' Defaults will be used.");
    }

    // baudrate
    if (j.contains("baudrate") && j["baudrate"].is_number()) {
      try {
        int baudrate     = 115200;
        baudrate         = j["baudrate"].get<int>();
        m_serialBaudrate = vscp_str_format("%d", baudrate);
        spdlog::debug("doLoadConfig: 'baudrate' {}", m_serialBaudrate);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'baudrate' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'baudrate' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'baudrate' Defaults will be used.");
    }

    // Parity
    if (j.contains("parity") && j["parity"].is_string()) {
      try {
        m_serialParity = j["parity"].get<std::string>();
        spdlog::debug("doLoadConfig: 'parity' {}", m_serialParity);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'parity' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'parity' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'parity' Defaults will be used.");
    }

    // Data bits
    if (j.contains("bits") && j["bits"].is_number()) {
      try {
        int bits              = 8;
        bits                  = j["bits"].get<int>();
        m_serialCountDataBits = vscp_str_format("%d", bits);
        spdlog::debug("doLoadConfig: 'bits' {}", m_serialCountDataBits);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'bits' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'bits' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'bits' Defaults will be used.");
    }

    // Stop bits
    if (j.contains("stopbits") && j["stopbits"].is_number()) {
      try {
        m_serialCountStopbits = j["stopbits"].get<int>();
        spdlog::debug("doLoadConfig: 'stopbits' {}", m_serialCountStopbits);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'stopbits' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'stopbits' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'stopbits' Defaults will be used.");
    }

    // HW Flow control
    if (j.contains("hwflowctrl") && j["hwflowctrl"].is_boolean()) {
      try {
        m_bSerialHwFlowCtrl = j["hwflowctrl"].get<bool>();
        spdlog::debug("doLoadConfig: 'hwflowctrl' {}", m_bSerialHwFlowCtrl);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'hwflowctrl' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'hwflowctrl' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'hwflowctrl' Defaults will be used.");
    }

    // SW Flow control
    if (j.contains("swflowctrl") && j["swflowctrl"].is_boolean()) {
      try {
        m_bSerialSwFlowCtrl = j["swflowctrl"].get<bool>();
        spdlog::debug("doLoadConfig: 'swflowctrl' {}", m_bSerialSwFlowCtrl);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'swflowctrl' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'swflowctrl' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'swflowctrl' Defaults will be used.");
    }

    // DTR control
    if (j.contains("dtr-on-start") && j["dtr-on-start"].is_boolean()) {
      try {
        m_bDtrOnStart = j["dtr-on-start"].get<bool>();
        spdlog::debug("doLoadConfig: 'dtr-on-start' {}", m_bDtrOnStart);
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig: Failed to read 'dtr-on-start' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig: Failed to read 'dtr-on-start' due to unknown error.");
      }
    }
    else {
      spdlog::warn("ReadConfig: Failed to read 'dtr-on-start' Defaults will be used.");
    }

  } // Serial config

  // * * * Items * * *

  if (m_j_config.contains("items") && m_j_config["items"].is_array()) {

    for (auto it : m_j_config["items"]) {

      CP1Item *pItem = new CP1Item;
      if (nullptr == pItem) {
        spdlog::critical("ReadConfig: Unable to allocate data for p1 measurement item.");
        return false;
      }

      // token
      if (it.contains("token") && it["token"].is_string()) {
        try {
          pItem->setToken(it["token"].get<std::string>());
          spdlog::debug("doLoadConfig: 'token' {}", it["token"].get<std::string>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'token' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'token' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'token' Defaults will be used.");
      }

      // description
      if (it.contains("description") && it["description"].is_string()) {
        try {
          pItem->setDescription(it["description"].get<std::string>());
          spdlog::debug("doLoadConfig: 'description' {}", it["description"].get<std::string>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'description' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'description' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'description' Defaults will be used.");
      }

      // vscp_class
      if (it.contains("vscp-class") && it["vscp-class"].is_number_unsigned()) {
        try {
          pItem->setVscpClass(it["vscp-class"].get<uint16_t>());
          spdlog::debug("doLoadConfig: 'vscp-class' {}", it["vscp-class"].get<uint16_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'vscp-class' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'vscp-class' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'vscp-class' Defaults will be used.");
      }

      // vscp_type
      if (it.contains("vscp-type") && it["vscp-type"].is_number_unsigned()) {
        try {
          pItem->setVscpType(it["vscp-type"].get<uint16_t>());
          spdlog::debug("doLoadConfig: 'vscp-type' {}", it["vscp-type"].get<uint16_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'vscp-type' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'vscp-type' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'vscp-type' Defaults will be used.");
      }

      // sensorindex
      if (it.contains("sensorindex") && it["sensorindex"].is_number_unsigned()) {
        try {
          pItem->setSensorIndex(it["sensorindex"].get<uint8_t>());
          spdlog::debug("doLoadConfig: 'sensorindex' {}", it["sensorindex"].get<uint8_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'sensorindex' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'sensorindex' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'sensorindex' Defaults will be used.");
      }

      // guid-lsb
      if (it.contains("guid-lsb") && it["guid-lsb"].is_number_unsigned()) {
        try {
          pItem->setGuidLsb(it["guid-lsb"].get<uint8_t>());
          spdlog::debug("doLoadConfig: 'guid-lsb' {}", it["guid-lsb"].get<uint8_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'guid-lsb' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'guid-lsb' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'guid-lsb' Defaults will be used.");
      }

      // zone
      if (it.contains("zone") && it["zone"].is_number_unsigned()) {
        try {
          pItem->setZone(it["zone"].get<uint8_t>());
          spdlog::debug("doLoadConfig: 'zone' {}", it["zone"].get<uint8_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'zone' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'zone' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'zone' Defaults will be used.");
      }

      // subzone
      if (it.contains("subzone") && it["subzone"].is_number_unsigned()) {
        try {
          pItem->setSubZone(it["subzone"].get<uint8_t>());
          spdlog::debug("doLoadConfig: 'subzone' {}", it["subzone"].get<uint8_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'subzone' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'subzone' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'subzone' Defaults will be used.");
      }

      // factor
      if (it.contains("factor") && it["factor"].is_number_unsigned()) {
        try {
          pItem->setFactor(it["factor"].get<double>());
          spdlog::debug("doLoadConfig: 'factor' {}", it["factor"].get<double>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'factor' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'factor' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'factor' Defaults will be used.");
      }

      // storage-name
      if (it.contains("store") && it["store"].is_string()) {
        try {
          pItem->setStorageName(it["store"].get<std::string>());
          spdlog::debug("doLoadConfig: 'store' {}", it["store"].get<std::string>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'store' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'store' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'store' Defaults will be used.");
      }

      // units
      if (it.contains("units") && it["units"].is_object()) {

        for (auto &itt : it["units"].items()) {
          spdlog::debug("doLoadConfig: 'units' {} {}", itt.key(), itt.value().get<int>());
          pItem->addUnit(itt.key(), itt.value().get<int>());
        }
      }

      m_listItems.push_back(pItem);

    } // iterator items

    // * * * alarms * * *

    if (m_j_config.contains("alarms") && m_j_config["alarms"].is_array()) {

      for (auto it : m_j_config["alarms"]) {

        std::string strType;
        CAlarm *pAlarm = new CAlarm;
        if (nullptr == pAlarm) {
          spdlog::critical("ReadConfig: Unable to allocate data for p1 measurement item.");
          return false;
        }

        // Type
        if (it.contains("type") && it["type"].is_string()) {
          try {
            strType = it["type"].get<std::string>();
            vscp_makeLower(strType);
            spdlog::debug("doLoadConfig: 'type' {}", strType);
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'op' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'op' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'op' Defaults will be used.");
        }

        // stored variable to work on
        if (it.contains("variable") && it["variable"].is_string()) {
          try {
            pAlarm->setVariable(it["variable"].get<std::string>());
            spdlog::debug("doLoadConfig: 'variable' {}", it["variable"].get<std::string>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'variable' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'variable' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'variable' Defaults will be used.");
        }

        // Operation
        if (it.contains("op") && it["op"].is_string()) {
          try {
            pAlarm->setOperation(it["op"].get<std::string>());
            spdlog::debug("doLoadConfig: 'op' {}", it["op"].get<std::string>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'op' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'op' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'op' Defaults will be used.");
        }

        // Value
        if (it.contains("value") && it["value"].is_number()) {
          try {
            pAlarm->setValue(it["value"].get<double>());
            spdlog::debug("doLoadConfig: 'value' {}", it["value"].get<double>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'value' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'value' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'value' Defaults will be used.");
        }

        // one-shot
        if (it.contains("one-shot") && it["one-shot"].is_boolean()) {
          try {
            pAlarm->setOneShot(it["one-shot"].get<bool>());
            spdlog::debug("doLoadConfig: 'one-shot' {}", it["one-shot"].get<bool>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'one-shot' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'one-shot' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'one-shot' Defaults will be used.");
        }

        // Alarm byte
        if (it.contains("alarm-byte") && it["alarm-byte"].is_number_unsigned()) {
          try {
            pAlarm->setAlarmByte(it["alarm-byte"].get<uint8_t>());
            spdlog::debug("doLoadConfig: 'alarm-byte' {}", it["alarm-byte"].get<uint8_t>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'alarm-byte' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'alarm-byte' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'alarm-byte' Defaults will be used.");
        }

        // Zone
        if (it.contains("zone") && it["zone"].is_number_unsigned()) {
          try {
            pAlarm->setZone(it["zone"].get<uint8_t>());
            spdlog::debug("doLoadConfig: 'zone' {}", it["zone"].get<uint8_t>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'zone' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'zone' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'zone' Defaults will be used.");
        }

        // Subzone
        if (it.contains("subzone") && it["subzone"].is_number_unsigned()) {
          try {
            pAlarm->setSubZone(it["subzone"].get<uint8_t>());
            spdlog::debug("doLoadConfig: 'subzone' {}", it["subzone"].get<uint8_t>());
          }
          catch (const std::exception &ex) {
            spdlog::error("ReadConfig: Failed to read 'subzone' Error='{}'", ex.what());
          }
          catch (...) {
            spdlog::error("ReadConfig: Failed to read 'subzone' due to unknown error.");
          }
        }
        else {
          spdlog::warn("ReadConfig: Failed to read 'sunzone' Defaults will be used.");
        }

        if ("on" == strType) {
          if (nullptr == m_mapAlarmOn[pAlarm->getVariable()]) {
            m_mapAlarmOn[pAlarm->getVariable()] = pAlarm;
            spdlog::debug("doLoadConfig: 'ON'");
          }
          else {
            spdlog::warn("ReadConfig: Doublet of ON alarms detected [{}]", pAlarm->getVariable());
            delete pAlarm;
            pAlarm = nullptr;
          }
        }
        else if ("off" == strType) {
          if (nullptr == m_mapAlarmOn[pAlarm->getVariable()]) {
            m_mapAlarmOff[pAlarm->getVariable()] = pAlarm;
            spdlog::debug("doLoadConfig: 'OFF'");
          }
          else {
            spdlog::warn("ReadConfig: Doublet of OFF alarms detected [{}]", pAlarm->getVariable());
            delete pAlarm;
            pAlarm = nullptr;
          }
        }
        else {
          spdlog::error("ReadConfig: Invalid type = {0} for alarm [{1}}.", strType, pAlarm->getVariable());
          delete pAlarm;
          pAlarm = nullptr;
        }

      } // iterator alarms

    } // Alarms

  } // config

  spdlog::debug("doLoadConfig: done");

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// saveConfiguration
//

bool
CEnergyP1::doSaveConfig(void)
{
  if (m_j_config.value("write", false)) {}
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// handleHLO
//

bool
CEnergyP1::handleHLO(vscpEvent *pEvent)
{
  vscpEventEx ex;

  // Check pointers
  if (NULL == pEvent || (NULL == pEvent->pdata)) {
    spdlog::error("HLO handler: NULL event pointer.");
    return false;
  }

  // > 18  pos 0-15 = GUID, 16 type >> 4, encryption & 0x0f
  // JSON if type = 2
  // JSON from 17 onwards

  // CHLO hlo;
  // if (!hlo.parseHLO(j, pEvent )) {
  spdlog::error("Failed to parse HLO.");
  //     return false;
  // }

  // Must be HLO command event
  if ((pEvent->vscp_class != VSCP_CLASS2_HLO) && (pEvent->vscp_type != VSCP2_TYPE_HLO_COMMAND)) {
    return false;
  }

  // Get GUID / encryption / type
  cguid hlo_guid(pEvent->pdata);
  uint8_t hlo_encryption = pEvent->pdata[16] & 0x0f;
  uint8_t hlo_type       = (pEvent->pdata[16] >> 4) & 0x0f;

  char buf[512];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, (pEvent->pdata + 17), pEvent->sizeData);
  auto j = json::parse(buf);
  printf("%s\n", j.dump().c_str());

  if (m_j_config["users"].is_array()) {
    printf("Yes it's an array %zu - %s\n",
           m_j_config["users"].size(),
           m_j_config["users"][0]["name"].get<std::string>().c_str());
  }

  // Must be an operation
  if (!j["op"].is_string() || j["op"].is_null()) {
    spdlog::error("HLO-command: Missing op [{}]", j.dump().c_str());
    return false;
  }

  if (j["arg"].is_object() && !j["arg"].is_null()) {
    printf("Argument is object\n");
  }

  if (j["arg"].is_string() && !j["arg"].is_null()) {
    printf("Argument is string\n");
  }

  if (j["arg"].is_number() && !j["arg"].is_null()) {
    printf("Argument is number\n");
  }

  // Make HLO response event
  ex.obid      = 0;
  ex.head      = 0;
  ex.timestamp = vscp_makeTimeStamp();
  vscp_setEventExToNow(&ex); // Set time to current time
  ex.vscp_class = VSCP_CLASS2_PROTOCOL;
  ex.vscp_type  = VSCP2_TYPE_HLO_RESPONSE;
  m_guid.writeGUID(ex.GUID);

  json j_response;

  if (j.value("op", "") == "noop") {
    // Send positive response
    j_response["op"]          = "vscp-reply";
    j_response["name"]        = "noop";
    j_response["result"]      = "OK";
    j_response["description"] = "NOOP commaned executed correctly.";

    memset(ex.data, 0, sizeof(ex.data));
    ex.sizeData = (uint16_t) strlen(buf);
    memcpy(ex.data, buf, ex.sizeData);
  }
  else if (j.value("op", "") == "readvar") {
    readVariable(ex, j);
  }
  else if (j.value("op", "") == "writevar") {
    writeVariable(ex, j);
  }
  else if (j.value("op", "") == "delvar") {
    deleteVariable(ex, j);
  }
  else if (j.value("op", "") == "load") {
    std::string path = "";
    doLoadConfig(path);
  }
  else if (j.value("op", "") == "save") {
    doSaveConfig();
  }
  else if (j.value("op", "") == "stop") {
    stop();
  }
  else if (j.value("op", "") == "start") {
    start();
  }
  else if (j.value("op", "") == "restart") {
    restart();
  }

  // Put event in receive queue
  return eventExToReceiveQueue(ex);
}

///////////////////////////////////////////////////////////////////////////////
// readVariable
//

bool
CEnergyP1::readVariable(vscpEventEx &ex, const json &json_req)
{
  json j;

  j["op"]          = "readvar";
  j["result"]      = VSCP_ERROR_SUCCESS;
  j["arg"]["name"] = j.value("name", "");

  if ("debug" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("debug", false);
  }
  else if ("write" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("write", false);
  }
  else if ("interface" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("interface", ""));
  }
  else if ("vscp-key-file" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("vscp-key-file", ""));
  }
  else if ("max-out-queue" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("max-out-queue", 0);
  }
  else if ("max-in-queue" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("max-in-queue", 0);
  }
  else if ("encryption" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("encryption", ""));
  }
  else if ("ssl-certificate" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-certificate", ""));
  }
  else if ("ssl-certificate-chain" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-certificate-chain", ""));
  }
  else if ("ssl-ca-path" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-ca-path", ""));
  }
  else if ("ssl-ca-file" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-ca-file", ""));
  }
  else if ("ssl-verify-depth" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("ssl-verify-depth", 9);
  }
  else if ("ssl-default-verify-paths" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-default-verify-paths", ""));
  }
  else if ("ssl-cipher-list" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-cipher-list", ""));
  }
  else if ("ssl-protocol-version" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("ssl-protocol-version", 3);
  }
  else if ("ssl-short-trust" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("ssl-short-trust", false);
  }
  else if ("user-count" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = 9;
  }
  else if ("users" == j.value("name", "")) {

    if (!m_j_config["users"].is_array()) {
      spdlog::warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_SUCCESS;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_JSON;
    j["arg"]["value"] = m_j_config["users"][index].dump();
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::error("Variable [] is unknown.");
  }

abort:

  memset(ex.data, 0, sizeof(ex.data));
  ex.sizeData = (uint16_t) j.dump().length();
  memcpy(ex.data, j.dump().c_str(), ex.sizeData);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// writeVariable
//

bool
CEnergyP1::writeVariable(vscpEventEx &ex, const json &json_req)
{
  json j;

  j["op"]          = "writevar";
  j["result"]      = VSCP_ERROR_SUCCESS;
  j["arg"]["name"] = j.value("name", "");

  if ("debug" == j.value("name", "")) {

    // arg should be boolean
    if (!j["arg"].is_boolean() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    // set new value
    m_j_config["debug"] = j["arg"].get<bool>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("debug", false);
  }
  else if ("write" == j.value("name", "")) {

    // arg should be boolean
    if (!j["arg"].is_boolean() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["write"] = j["arg"].get<bool>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("write", false);
  }
  else if ("interface" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["interface"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config["interface"]);
  }
  else if ("vscp-key-file" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["vscp-key-file"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("vscp-key-file", ""));
  }
  else if ("max-out-queue" == j.value("name", "")) {

    // arg should be number
    if (!j["arg"].is_number() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["max-out-queue"] = j["arg"].get<int>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("max-out-queue", 0);
  }
  else if ("max-in-queue" == j.value("name", "")) {

    // arg should be number
    if (!j["arg"].is_number() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["max-in-queue"] = j["arg"].get<int>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("max-in-queue", 0);
  }
  else if ("encryption" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["encryption"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("encryption", ""));
  }
  else if ("ssl-certificate" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-certificate"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-certificate", ""));
  }
  else if ("ssl-certificate-chain" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-certificate-chain"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-certificate-chain", ""));
  }
  else if ("ssl-ca-path" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-ca-path"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-ca-path", ""));
  }
  else if ("ssl-ca-file" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-ca-file"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-ca-file", ""));
  }
  else if ("ssl-verify-depth" == j.value("name", "")) {
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("ssl-verify-depth", 9);
  }
  else if ("ssl-default-verify-paths" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-default-verify-paths"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-default-verify-paths", ""));
  }
  else if ("ssl-cipher-list" == j.value("name", "")) {

    // arg should be string
    if (!j["arg"].is_string() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-cipher-list"] = j["arg"].get<std::string>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_STRING;
    j["arg"]["value"] = vscp_convertToBase64(m_j_config.value("ssl-cipher-list", ""));
  }
  else if ("ssl-protocol-version" == j.value("name", "")) {

    // arg should be number
    if (!j["arg"].is_number() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-protocol-version"] = j["arg"].get<bool>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_INTEGER;
    j["arg"]["value"] = m_j_config.value("ssl-protocol-version", 3);
  }
  else if ("ssl-short-trust" == j.value("name", "")) {

    // arg should be boolean
    if (!j["arg"].is_boolean() || j["arg"].is_null()) {
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }
    // set new value
    m_j_config["ssl-short-trust"] = j["arg"].get<bool>();

    // report back
    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_BOOLEAN;
    j["arg"]["value"] = m_j_config.value("ssl-short-trust", false);
  }
  else if ("users" == j.value("name", "")) {

    // users must be array
    if (!m_j_config["users"].is_array()) {
      spdlog::warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    // Must be object
    if (!m_j_config["args"].is_object()) {
      spdlog::warn("The user info must be an object.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    m_j_config["users"][index] = j["args"];

    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_JSON;
    j["arg"]["value"] = m_j_config["users"][index].dump();
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::error("Variable [] is unknown.");
  }

abort:

  memset(ex.data, 0, sizeof(ex.data));
  ex.sizeData = (uint16_t) j.dump().length();
  memcpy(ex.data, j.dump().c_str(), ex.sizeData);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// deleteVariable
//

bool
CEnergyP1::deleteVariable(vscpEventEx &ex, const json &json_reg)
{
  json j;

  j["op"]          = "deletevar";
  j["result"]      = VSCP_ERROR_SUCCESS;
  j["arg"]["name"] = j.value("name", "");

  if ("users" == j.value("name", "")) {

    // users must be array
    if (!m_j_config["users"].is_array()) {
      spdlog::warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    // Must be object
    if (!m_j_config["args"].is_object()) {
      spdlog::warn("The user info must be an object.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    m_j_config["users"].erase(index);
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::warn("Variable [{}] is unknown.", j.value("name", "").c_str());
  }

abort:

  memset(ex.data, 0, sizeof(ex.data));
  ex.sizeData = (uint16_t) j.dump().length();
  memcpy(ex.data, j.dump().c_str(), ex.sizeData);

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// stop
//

bool
CEnergyP1::stop(void)
{
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// start
//

bool
CEnergyP1::start(void)
{
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// restart
//

bool
CEnergyP1::restart(void)
{
  if (!stop()) {
    spdlog::warn("Failed to stop VSCP worker loop.");
  }

  if (!start()) {
    spdlog::warn("Failed to start VSCP worker loop.");
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// doWork
//

bool
CEnergyP1::doWork(std::string &strbuf)
{
  // Check for a full line of input
  size_t pos_find;
  std::string exstr;
  std::string valstr;

  if (std::string::npos != (pos_find = strbuf.find("("))) {
    spdlog::debug("Working thread: Line {}", strbuf);
    exstr  = strbuf.substr(0, pos_find);
    valstr = strbuf.substr(pos_find + 2);
  }
  else {
    return false;
  }

  spdlog::debug("exstr={0} valstr={1} strbuf={2}", exstr, valstr, strbuf);

  for (auto const &pItem : m_listItems) {

    if (exstr.rfind(pItem->getToken(), 0) == 0) {

      // Initialize new event
      vscpEventEx ex;
      ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
      double value = pItem->getValue(strbuf);

      spdlog::debug("MATCH! - Found token={0} value={1} unit={2}",
                    pItem->getToken(),
                    pItem->getValue(strbuf),
                    pItem->getUnit(strbuf));

      if (m_bDebug) {
        ;
      }

      // Save measurement value
      m_lastValue[pItem->getStorageName()] = value;

      switch (pItem->getVscpClass()) {

        case VSCP_CLASS1_MEASUREMENT: {

          switch (pItem->getLevel1Coding()) {

            case VSCP_DATACODING_STRING: {
              if (!vscp_makeStringMeasurementEventEx(&ex,
                                                     (float) value,
                                                     pItem->getSensorIndex(),
                                                     pItem->getUnit(strbuf))) {
                break;
              }
            } break;

            case VSCP_DATACODING_INTEGER: {
              uint64_t val64 = value;
              if (!vscp_convertIntegerToNormalizedEventData(ex.data,
                                                            &ex.sizeData,
                                                            val64,
                                                            pItem->getUnit(strbuf),
                                                            pItem->getSensorIndex())) {
                break;
              }
            } break;

            case VSCP_DATACODING_NORMALIZED: {
              uint64_t val64 = value;
              if (!vscp_convertIntegerToNormalizedEventData(ex.data,
                                                            &ex.sizeData,
                                                            val64,
                                                            pItem->getUnit(strbuf),
                                                            pItem->getSensorIndex())) {
                break;
              }
            } break;

            case VSCP_DATACODING_SINGLE:
              if (!vscp_makeFloatMeasurementEventEx(&ex,
                                                    (float) value,
                                                    pItem->getSensorIndex(),
                                                    pItem->getUnit(strbuf))) {
                break;
              }
              break;

            case VSCP_DATACODING_DOUBLE:
              break;
          }

        } break;

        case VSCP_CLASS1_MEASUREMENT64: {
          if (!vscp_makeFloatMeasurementEventEx(&ex, (float) value, pItem->getSensorIndex(), pItem->getUnit(strbuf))) {
            break;
          }
        } break;

        case VSCP_CLASS1_MEASUREZONE: {
        } break;

        case VSCP_CLASS1_MEASUREMENT32: {
        } break;

        case VSCP_CLASS1_SETVALUEZONE: {
        } break;

        case VSCP_CLASS2_MEASUREMENT_STR: {
          if (vscp_makeLevel2StringMeasurementEventEx(&ex,
                                                      pItem->getVscpType(),
                                                      value,
                                                      pItem->getUnit(strbuf),
                                                      pItem->getSensorIndex(),
                                                      pItem->getZone(),
                                                      pItem->getSubZone())) {

            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = pItem->getVscpClass();
            ex.vscp_type  = pItem->getVscpType();
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15] = pItem->getGuidLsb();

            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Event added to i receive queue class={0} type={1}", ex.vscp_class, ex.vscp_type);
              }
            }
            else {
              spdlog::error("Failed to allocate memory for event.");
            }
          }
          else {
            spdlog::error("Failed to build level II string measurement event.");
          }
        } break;

        case VSCP_CLASS2_MEASUREMENT_FLOAT: {

          if (!vscp_makeLevel2FloatMeasurementEventEx(&ex,
                                                      pItem->getVscpType(),
                                                      value,
                                                      pItem->getUnit(strbuf),
                                                      pItem->getSensorIndex(),
                                                      pItem->getZone(),
                                                      pItem->getSubZone())) {

            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = pItem->getVscpClass();
            ex.vscp_type  = pItem->getVscpType();
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15] = pItem->getGuidLsb();

            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Event added to receive queue class={0} type={1}", ex.vscp_class, ex.vscp_type);
              }
            }
            else {
              spdlog::error("Failed to allocate memory for event.");
            }
          }
          else {
            spdlog::error("Failed to build level II string measurement event.");
          }
        } break;
      }

      // Check Alarm ON
      CAlarm *pAlarm;
      if (nullptr != (pAlarm = m_mapAlarmOn[pItem->getStorageName()])) {

        if (alarm_op::gt == pAlarm->getOp()) {
          if ((pAlarm->getValue() > value) && !(pAlarm->isSent() && pAlarm->isOneShot())) {

            // Send alarm
            vscpEventEx ex;
            ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = VSCP_CLASS1_ALARM;
            ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15] = pItem->getGuidLsb();
            ex.sizeData = 3;
            ex.data[0]  = pAlarm->getAlarmByte();
            ex.data[1]  = pAlarm->getZone();
            ex.data[2]  = pAlarm->getSubZone();

            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("AlarmOn: Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Sent ON alarm [{}]", pAlarm->getVariable());
                pAlarm->setSentFlag();
              }
            }
            else {
              spdlog::error("AlarmOn: Failed to allocate memory for event.");
            }
          }
        }
        else if (alarm_op::lt == pAlarm->getOp()) {
          
          if (pAlarm->getValue() < value) {
            
            // send alarm
            vscpEventEx ex;
            ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = VSCP_CLASS1_ALARM;
            ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15]       = pItem->getGuidLsb();
            ex.sizeData       = 3;
            ex.data[0]        = pAlarm->getAlarmByte();
            ex.data[1]        = pAlarm->getZone();
            ex.data[2]        = pAlarm->getSubZone();

            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("AlarmOff: Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Sent OFF alarm [{}]", pAlarm->getVariable());
                pAlarm->setSentFlag();
                // Reset Possible OFF flag
              }
            }
            else {
              spdlog::error("AlarmOn: Failed to allocate memory for event.");
            }
          }
        }
      }

      // Check Alarm OFF
      if (nullptr != (pAlarm = m_mapAlarmOff[pItem->getStorageName()])) {

        if (alarm_op::gt == pAlarm->getOp()) {

          if ((pAlarm->getValue() > value) && !(pAlarm->isSent() && pAlarm->isOneShot())) {
          
            // Send alarm
            vscpEventEx ex;
            ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = VSCP_CLASS1_ALARM;
            ex.vscp_type  = VSCP_TYPE_ALARM_RESET;
            // memcpy(ex.GUID, pObj->m_guid.m_id, 16);
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15]       = pItem->getGuidLsb();
            ex.sizeData       = 3;
            ex.data[0]        = pAlarm->getAlarmByte();
            ex.data[1]        = pAlarm->getZone();
            ex.data[2]        = pAlarm->getSubZone();
          
            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("AlarmOn: Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Sent ON alarm [{}]", pAlarm->getVariable());
                pAlarm->setSentFlag();
              }
            }
            else {
              spdlog::error("AlarmOn: Failed to allocate memory for event.");
            }
          }
        }
        else if (alarm_op::lt == pAlarm->getOp()) {
          
          if (pAlarm->getValue() < value) {
          
            // send alarm
            vscpEventEx ex;
            ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
            ex.timestamp = vscp_makeTimeStamp();
            vscp_setEventExDateTimeBlockToNow(&ex);
            ex.vscp_class = VSCP_CLASS1_ALARM;
            ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
            // memcpy(ex.GUID, pObj->m_guid.m_id, 16);
            m_guid.writeGUID(ex.GUID);
            ex.GUID[15]       = pItem->getGuidLsb();
            ex.sizeData       = 3;
            ex.data[0]        = pAlarm->getAlarmByte();
            ex.data[1]        = pAlarm->getZone();
            ex.data[2]        = pAlarm->getSubZone();
          
            vscpEvent *pEvent = new vscpEvent;
            if (nullptr != pEvent) {
              pEvent->pdata    = nullptr;
              pEvent->sizeData = 0;
              vscp_convertEventExToEvent(pEvent, &ex);
              if (!addEvent2ReceiveQueue(pEvent)) {
                spdlog::error("AlarmOff: Failed to add event to receive queue.");
              }
              else {
                spdlog::debug("Sent OFF alarm [{}]", pAlarm->getVariable());
                pAlarm->setSentFlag();
                // Reset Possible ON flag
              }
            }
            else {
              spdlog::error("AlarmOn: Failed to allocate memory for event.");
            }
          }
        }
      }
    } // if match
  }   // Iterate

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// eventExToReceiveQueue
//

bool
CEnergyP1::eventExToReceiveQueue(vscpEventEx &ex)
{
  vscpEvent *pev = new vscpEvent();
  if (!vscp_convertEventExToEvent(pev, &ex)) {
    spdlog::error("Failed to convert event from ex to ev.");
    vscp_deleteEvent(pev);
    return false;
  }

  if (NULL != pev) {
    if (vscp_doLevel2Filter(pev, &m_rxfilter)) {
      pthread_mutex_lock(&m_mutexReceiveQueue);
      m_receiveList.push_back(pev);
      pthread_mutex_unlock(&m_mutexReceiveQueue);
      sem_post(&m_semReceiveQueue);
    }
    else {
      vscp_deleteEvent(pev);
    }
  }
  else {
    spdlog::error("Unable to allocate event storage.");
  }
  return true;
}

//////////////////////////////////////////////////////////////////////
// addEvent2SendQueue
//
// Send event to device
//

bool
CEnergyP1::addEvent2SendQueue(const vscpEvent *pEvent)
{
  pthread_mutex_lock(&m_mutexSendQueue);
  m_sendList.push_back((vscpEvent *) pEvent);
  sem_post(&m_semSendQueue);
  pthread_mutex_unlock(&m_mutexSendQueue);
  return true;
}

//////////////////////////////////////////////////////////////////////
// addEvent2ReceiveQueue
//
//  Send event to host
//

bool
CEnergyP1::addEvent2ReceiveQueue(const vscpEvent *pEvent)
{
  pthread_mutex_lock(&m_mutexReceiveQueue);
  m_receiveList.push_back((vscpEvent *) pEvent);
  pthread_mutex_unlock(&m_mutexReceiveQueue);
  sem_post(&m_semReceiveQueue);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// startWorkerThread
//

bool
CEnergyP1::startWorkerThread(void)
{
  if (m_bDebug) {
    spdlog::debug("Starting P1 energy meter interface...");
  }

  if (pthread_create(&m_workerThread, NULL, workerThread, this)) {
    spdlog::error("Unable to start the workerthread.");
    return false;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////
// stopWorkerThread
//

bool
CEnergyP1::stopWorkerThread(void)
{
  // // Tell the thread it's time to quit
  // CEnergyP1->m_nStopTcpIpSrv = VSCP_TCPIP_SRV_STOP;

  // if (__VSCP_DEBUG_TCP) {
  //     spdlog::debug("Terminating TCP thread.");
  // }

  // pthread_join(m_tcpipListenThread, NULL);
  // delete CEnergyP1;
  // CEnergyP1 = NULL;

  // if (__VSCP_DEBUG_TCP) {
  //     spdlog::debug("Terminated TCP thread.");
  // }

  return true;
}

/////////////////////////////////////////////////////////////////////////////
// readEncryptionKey
//

bool
CEnergyP1::readEncryptionKey(const std::string &path)
{
  try {
    std::ifstream in(path, std::ifstream::in);
    std::stringstream strStream;
    strStream << in.rdbuf();
    return vscp_hexStr2ByteArray(m_vscp_key, 32, strStream.str().c_str());
  }
  catch (...) {
    spdlog::error("Failed to read encryption key file [{}]", m_path.c_str());
    return false;
  }

  return true;
}

// ----------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// workerThread
//

void *
workerThread(void *pData)
{
  char buf[1024];
  std::string strbuf;
  uint16_t pos = 0;

  // Linux serial port
  Comm com;

  CEnergyP1 *pObj = (CEnergyP1 *) pData;
  if (nullptr == pData) {
    return NULL;
  }

  spdlog::debug("Working thread: Starting Worker loop GUID = {}", pObj->m_guid.getAsString());

  // Open the serial port
  if (!com.open((const char *) pObj->m_serialDevice.c_str())) {
    spdlog::debug("Working thread: Failed to open serial port");
    return NULL;
  }

  // Set serial parameters
  com.setParam(pObj->m_serialBaudrate.c_str(),
               pObj->m_serialParity.c_str(),
               pObj->m_serialCountDataBits.c_str(),
               pObj->m_bSerialHwFlowCtrl ? 1 : 0,
               pObj->m_bSerialSwFlowCtrl ? 1 : 0);

  // Set DTR if requested to do so
  if (pObj->m_bDtrOnStart) {
    spdlog::debug("Working thread: DTR ON");
    com.DtrOn();
  }

  // Work on
  while (!pObj->m_bQuit) {

    pos  = 0;
    *buf = 0;

    if (com.isCharReady()) {
      int read;
      while (com.isCharReady()) {
        char c = com.readChar(&read);
        if (read) {
          buf[pos++] = c;
          if (pos > sizeof(buf) - 1) {
            spdlog::debug("Working thread: Serial Buffer overlow");
            continue;
          }
          // printf("%c", c);
          if (0x0a == c) {
            buf[pos] = 0;           // Add terminating zero
            strbuf   = buf;         // Add to the string buffer
            pObj->doWork(strbuf);   // Do work
          }
        }
      } // while
    }
    else {
      // If no data we sleep for a second  - no rush here...
      sleep(1);
      continue;
    }

  // dowork:

  //   buf[pos] = 0;   // Add terminating zero
  //   strbuf   = buf; // Add to the string buffer

  //   // Check for a full line of input
  //   size_t pos_find;
  //   std::string exstr;
  //   std::string valstr;
  //   if (std::string::npos != (pos_find = strbuf.find("("))) {
  //     // spdlog::debug("Working thread: Line {}", strbuf);
  //     exstr  = strbuf.substr(0, pos_find);
  //     valstr = strbuf.substr(pos_find + 2);
  //     // spdlog::debug("exstr={0} valstr={1}", exstr, valstr);
  //   }
  //   else {
  //     continue;
  //   }

  //   spdlog::debug("exstr={0} valstr={1} strbuf={2}", exstr, valstr, strbuf);

  //   for (auto const &pItem : pObj->m_listItems) {

  //     if (exstr.rfind(pItem->getToken(), 0) == 0) {

  //       // Initialize new event
  //       vscpEventEx ex;
  //       ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
  //       double value = pItem->getValue(strbuf);

  //       spdlog::debug("MATCH! - Found token={0} value={1} unit={2}",
  //                     pItem->getToken(),
  //                     pItem->getValue(strbuf),
  //                     pItem->getUnit(strbuf));

  //       if (pObj->m_bDebug) {
  //         ;
  //       }

  //       // Save measurement value
  //       pObj->m_lastValue[pItem->getStorageName()] = value;

  //       switch (pItem->getVscpClass()) {

  //         case VSCP_CLASS1_MEASUREMENT: {

  //           switch (pItem->getLevel1Coding()) {

  //             case VSCP_DATACODING_STRING: {
  //               if (!vscp_makeStringMeasurementEventEx(&ex,
  //                                                      (float) value,
  //                                                      pItem->getSensorIndex(),
  //                                                      pItem->getUnit(strbuf))) {
  //                 break;
  //               }
  //             } break;

  //             case VSCP_DATACODING_INTEGER: {
  //               uint64_t val64 = value;
  //               if (!vscp_convertIntegerToNormalizedEventData(ex.data,
  //                                                             &ex.sizeData,
  //                                                             val64,
  //                                                             pItem->getUnit(strbuf),
  //                                                             pItem->getSensorIndex())) {
  //                 break;
  //               }
  //             } break;

  //             case VSCP_DATACODING_NORMALIZED: {
  //               uint64_t val64 = value;
  //               if (!vscp_convertIntegerToNormalizedEventData(ex.data,
  //                                                             &ex.sizeData,
  //                                                             val64,
  //                                                             pItem->getUnit(strbuf),
  //                                                             pItem->getSensorIndex())) {
  //                 break;
  //               }
  //             } break;

  //             case VSCP_DATACODING_SINGLE:
  //               if (!vscp_makeFloatMeasurementEventEx(&ex,
  //                                                     (float) value,
  //                                                     pItem->getSensorIndex(),
  //                                                     pItem->getUnit(strbuf))) {
  //                 break;
  //               }
  //               break;

  //             case VSCP_DATACODING_DOUBLE:
  //               break;
  //           }

  //         } break;

  //         case VSCP_CLASS1_MEASUREMENT64: {
  //           if (!vscp_makeFloatMeasurementEventEx(&ex,
  //                                                 (float) value,
  //                                                 pItem->getSensorIndex(),
  //                                                 pItem->getUnit(strbuf))) {
  //             break;
  //           }
  //         } break;

  //         case VSCP_CLASS1_MEASUREZONE: {
  //         } break;

  //         case VSCP_CLASS1_MEASUREMENT32: {
  //         } break;

  //         case VSCP_CLASS1_SETVALUEZONE: {
  //         } break;

  //         case VSCP_CLASS2_MEASUREMENT_STR: {
  //           if (vscp_makeLevel2StringMeasurementEventEx(&ex,
  //                                                       pItem->getVscpType(),
  //                                                       value,
  //                                                       pItem->getUnit(strbuf),
  //                                                       pItem->getSensorIndex(),
  //                                                       pItem->getZone(),
  //                                                       pItem->getSubZone())) {

  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = pItem->getVscpClass();
  //             ex.vscp_type  = pItem->getVscpType();
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15] = pItem->getGuidLsb();

  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Event added to i receive queue class={0} type={1}", ex.vscp_class, ex.vscp_type);
  //               }
  //             }
  //             else {
  //               spdlog::error("Failed to allocate memory for event.");
  //             }
  //           }
  //           else {
  //             spdlog::error("Failed to build level II string measurement event.");
  //           }
  //         } break;

  //         case VSCP_CLASS2_MEASUREMENT_FLOAT: {

  //           if (!vscp_makeLevel2FloatMeasurementEventEx(&ex,
  //                                                       pItem->getVscpType(),
  //                                                       value,
  //                                                       pItem->getUnit(strbuf),
  //                                                       pItem->getSensorIndex(),
  //                                                       pItem->getZone(),
  //                                                       pItem->getSubZone())) {

  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = pItem->getVscpClass();
  //             ex.vscp_type  = pItem->getVscpType();
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15] = pItem->getGuidLsb();

  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Event added to receive queue class={0} type={1}", ex.vscp_class, ex.vscp_type);
  //               }
  //             }
  //             else {
  //               spdlog::error("Failed to allocate memory for event.");
  //             }
  //           }
  //           else {
  //             spdlog::error("Failed to build level II string measurement event.");
  //           }
  //         } break;
  //       }

  //       // Check Alarm ON
  //       CAlarm *pAlarm;
  //       if (nullptr != (pAlarm = pObj->m_mapAlarmOn[pItem->getStorageName()])) {

  //         if (alarm_op::gt == pAlarm->getOp()) {
  //           if ((pAlarm->getValue() > value) && !(pAlarm->isSent() && pAlarm->isOneShot())) {

  //             // Send alarm
  //             vscpEventEx ex;
  //             ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = VSCP_CLASS1_ALARM;
  //             ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15] = pItem->getGuidLsb();
  //             ex.sizeData = 3;
  //             ex.data[0]  = pAlarm->getAlarmByte();
  //             ex.data[1]  = pAlarm->getZone();
  //             ex.data[2]  = pAlarm->getSubZone();

  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("AlarmOn: Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Sent ON alarm [{}]", pAlarm->getVariable());
  //                 pAlarm->setSentFlag();
  //               }
  //             }
  //             else {
  //               spdlog::error("AlarmOn: Failed to allocate memory for event.");
  //             }
  //           }
  //         }
  //         else if (alarm_op::lt == pAlarm->getOp()) {
  //           if (pAlarm->getValue() < value) {
  //             // send alarm
  //             vscpEventEx ex;
  //             ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = VSCP_CLASS1_ALARM;
  //             ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15]       = pItem->getGuidLsb();
  //             ex.sizeData       = 3;
  //             ex.data[0]        = pAlarm->getAlarmByte();
  //             ex.data[1]        = pAlarm->getZone();
  //             ex.data[2]        = pAlarm->getSubZone();
  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("AlarmOff: Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Sent OFF alarm [{}]", pAlarm->getVariable());
  //                 pAlarm->setSentFlag();
  //                 // Reset Possible OFF flag
  //               }
  //             }
  //             else {
  //               spdlog::error("AlarmOn: Failed to allocate memory for event.");
  //             }
  //           }
  //         }
  //       }

  //       // Check Alarm OFF
  //       if (nullptr != (pAlarm = pObj->m_mapAlarmOff[pItem->getStorageName()])) {

  //         if (alarm_op::gt == pAlarm->getOp()) {
  //           if ((pAlarm->getValue() > value) && !(pAlarm->isSent() && pAlarm->isOneShot())) {
  //             // Send alarm
  //             vscpEventEx ex;
  //             ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = VSCP_CLASS1_ALARM;
  //             ex.vscp_type  = VSCP_TYPE_ALARM_RESET;
  //             // memcpy(ex.GUID, pObj->m_guid.m_id, 16);
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15]       = pItem->getGuidLsb();
  //             ex.sizeData       = 3;
  //             ex.data[0]        = pAlarm->getAlarmByte();
  //             ex.data[1]        = pAlarm->getZone();
  //             ex.data[2]        = pAlarm->getSubZone();
  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("AlarmOn: Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Sent ON alarm [{}]", pAlarm->getVariable());
  //                 pAlarm->setSentFlag();
  //               }
  //             }
  //             else {
  //               spdlog::error("AlarmOn: Failed to allocate memory for event.");
  //             }
  //           }
  //         }
  //         else if (alarm_op::lt == pAlarm->getOp()) {
  //           if (pAlarm->getValue() < value) {
  //             // send alarm
  //             vscpEventEx ex;
  //             ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL | VSCP_HEADER16_DUMB;
  //             ex.timestamp = vscp_makeTimeStamp();
  //             vscp_setEventExDateTimeBlockToNow(&ex);
  //             ex.vscp_class = VSCP_CLASS1_ALARM;
  //             ex.vscp_type  = VSCP_TYPE_ALARM_ALARM;
  //             // memcpy(ex.GUID, pObj->m_guid.m_id, 16);
  //             pObj->m_guid.writeGUID(ex.GUID);
  //             ex.GUID[15]       = pItem->getGuidLsb();
  //             ex.sizeData       = 3;
  //             ex.data[0]        = pAlarm->getAlarmByte();
  //             ex.data[1]        = pAlarm->getZone();
  //             ex.data[2]        = pAlarm->getSubZone();
  //             vscpEvent *pEvent = new vscpEvent;
  //             if (nullptr != pEvent) {
  //               pEvent->pdata    = nullptr;
  //               pEvent->sizeData = 0;
  //               vscp_convertEventExToEvent(pEvent, &ex);
  //               if (!pObj->addEvent2ReceiveQueue(pEvent)) {
  //                 spdlog::error("AlarmOff: Failed to add event to receive queue.");
  //               }
  //               else {
  //                 spdlog::debug("Sent OFF alarm [{}]", pAlarm->getVariable());
  //                 pAlarm->setSentFlag();
  //                 // Reset Possible ON flag
  //               }
  //             }
  //             else {
  //               spdlog::error("AlarmOn: Failed to allocate memory for event.");
  //             }
  //           }
  //         }
  //       }
  //     } // if match
  //   }   // Iterate

  } // Main loop

  // Set DTR if requested to do so
  if (pObj->m_bDtrOnStart) {
    spdlog::debug("Working thread: DTR OFF");
    com.DtrOff();
  }

  // Close the serial port
  spdlog::debug("Working thread: Closing serial port");
  com.close();

  spdlog::debug("Working thread: Ending Worker loop");

  return NULL;
}
