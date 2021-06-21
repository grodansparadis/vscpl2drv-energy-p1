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
}

//////////////////////////////////////////////////////////////////////
// ~CEnergyP1
//

CEnergyP1::~CEnergyP1()
{
  close();

  sem_destroy(&m_semSendQueue);
  sem_destroy(&m_semReceiveQueue);

  pthread_mutex_destroy(&m_mutexSendQueue);
  pthread_mutex_destroy(&m_mutexReceiveQueue);
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

  // Init pool
  spdlog::init_thread_pool(8192, 1);

  // Flush log every five seconds
  spdlog::flush_every(std::chrono::seconds(5));

  auto console = spdlog::stdout_color_mt("console");
  // Start out with level=info. Config may change this
  console->set_level(spdlog::level::info);
  console->set_pattern("[vscp] [%^%l%$] %v");
  spdlog::set_default_logger(console);

  // Read configuration file
  if (!doLoadConfig(path)) {
    console->error("Failed to load configuration file [{}]", path.c_str());
    spdlog::drop_all();
    return false;
  }

  // Set up logger
  if (m_path_to_log_file.length()) {
    auto rotating_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_path_to_log_file.c_str(),
                                                                                     m_max_log_size,
                                                                                     m_max_log_files);
    if (m_bEnableFileLog) {
      rotating_file_sink->set_level(m_fileLogLevel);
      rotating_file_sink->set_pattern(m_fileLogPattern);
    }
    else {
      // If disabled set to off
      rotating_file_sink->set_level(spdlog::level::off);
    }

    std::vector<spdlog::sink_ptr> sinks{ rotating_file_sink };
    auto logger = std::make_shared<spdlog::async_logger>("logger",
                                                         sinks.begin(),
                                                         sinks.end(),
                                                         spdlog::thread_pool(),
                                                         spdlog::async_overflow_policy::block);
    // The separate sub loggers will handle trace levels
    logger->set_level(spdlog::level::trace);
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
  }

  if (!startWorkerThread()) {
    console->error("Failed to start worker thread.");
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
  }
  catch (json::parse_error) {
    spdlog::critical("Failed to load/parse JSON configuration.");
    return false;
  }

  // write
  if (m_j_config.contains("write")) {
    try {
      m_bWriteEnable = m_j_config["write"].get<bool>();
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
  if (m_j_config.contains("key-file") && m_j_config["logging"].is_string()) {
    if (!readEncryptionKey(m_j_config["key-file"].get<std::string>())) {
      spdlog::warn("WARNING!!! Default key will be used.");
    }
  }
  else {
    spdlog::warn("WARNING!!! Default key will be used.");
  }

  // * * * Logging * * *

  if (m_j_config.contains("logging") && m_j_config["logging"].is_object()) {

    json j = m_j_config["logging"];

    // Logging: file-log-level
    if (j.contains("file-log-level")) {
      std::string str;
      try {
        str = j["file-log-level"].get<std::string>();
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
    if (j.contains("file-pattern")) {
      try {
        m_fileLogPattern = j["file-pattern"].get<std::string>();
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-pattern' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-pattern' due to unknown error.");
      }
    }
    else {
      spdlog::debug("ReadConfig: Failed to read LOGGING 'file-pattern' Defaults will be used.");
    }

    // Logging: file-path
    if (j.contains("file-path")) {
      try {
        m_path_to_log_file = j["file-path"].get<std::string>();
      }
      catch (const std::exception &ex) {
        spdlog::error("Failed to read 'file-path' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-path' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-path' Defaults will be used.");
    }

    // Logging: file-max-size
    if (j.contains("file-max-size")) {
      try {
        m_max_log_size = j["file-max-size"].get<uint32_t>();
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-max-size' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-max-size' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-max-size' Defaults will be used.");
    }

    // Logging: file-max-files
    if (j.contains("file-max-files")) {
      try {
        m_max_log_files = j["file-max-files"].get<uint16_t>();
      }
      catch (const std::exception &ex) {
        spdlog::error("ReadConfig:Failed to read 'file-max-files' Error='{}'", ex.what());
      }
      catch (...) {
        spdlog::error("ReadConfig:Failed to read 'file-max-files' due to unknown error.");
      }
    }
    else {
      spdlog::error("ReadConfig: Failed to read LOGGING 'file-max-files' Defaults will be used.");
    }

  } // Logging
  else {
    spdlog::error("ReadConfig: No logging has been setup.");
  }

  // * * * Serial settings * * *

  if (m_j_config.contains("serial") && m_j_config["serial"].is_object()) {

    json j = m_j_config["logging"];

    // Serial interface
    if (j.contains("port") && j["port"].is_string()) {
      try {
        m_serialDevice = m_j_config["port"].get<std::string>();
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
        baudrate         = m_j_config["baudrate"].get<int>();
        m_serialBaudrate = vscp_str_format("%d", baudrate);
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
        m_serialParity = m_j_config["baudrate"].get<std::string>();
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
        bits                  = m_j_config["bits"].get<int>();
        m_serialCountDataBits = vscp_str_format("%d", bits);
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
        m_serialCountStopbits = m_j_config["stopbits"].get<int>();
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
        m_bSerialHwFlowCtrl = m_j_config["hwflowctrl"].get<bool>();
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
        m_bSerialSwFlowCtrl = m_j_config["swflowctrl"].get<bool>();
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
        m_bDtrOnStart = m_j_config["dtr-on-start"].get<bool>();
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
      if (nullptr != pItem) {
        continue;
      }

      // token
      if (it.contains("token") && it["token"].is_string()) {
        try {
          pItem->setToken(it.get<std::string>());
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
          pItem->setDescription(it.get<std::string>());
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
      if (it.contains("vscp_class") && it["vscp_class"].is_number_unsigned()) {
        try {
          pItem->setVscpClass(it.get<uint16_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'vscp_class' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'vscp_class' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'vscp_class' Defaults will be used.");
      }

      // vscp_type
      if (it.contains("vscp_type") && it["vscp_type"].is_number_unsigned()) {
        try {
          pItem->setVscpType(it.get<uint16_t>());
        }
        catch (const std::exception &ex) {
          spdlog::error("ReadConfig: Failed to read 'vscp_type' Error='{}'", ex.what());
        }
        catch (...) {
          spdlog::error("ReadConfig: Failed to read 'vscp_type' due to unknown error.");
        }
      }
      else {
        spdlog::warn("ReadConfig: Failed to read 'vscp_type' Defaults will be used.");
      }

      // sensorindex
      if (it.contains("sensorindex") && it["sensorindex"].is_number_unsigned()) {
        try {
          pItem->setSensorIndex(it.get<uint8_t>());
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
          pItem->setGuidLsb(it.get<uint8_t>());
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
          pItem->setZone(it.get<uint8_t>());
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
          pItem->setSubZone(it.get<uint8_t>());
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

      // units
      if (m_j_config["items"].contains("units") && m_j_config["items"]["units"].is_object()) {

        for (auto &itt : m_j_config["items"]["units"].items()) {
          pItem->addUnit(itt.key(), itt.value());
        }
      }

    } // items
  }   // config

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
    spdlog::get("logger")->error("HLO handler: NULL event pointer.");
    return false;
  }

  // > 18  pos 0-15 = GUID, 16 type >> 4, encryption & 0x0f
  // JSON if type = 2
  // JSON from 17 onwards

  // CHLO hlo;
  // if (!hlo.parseHLO(j, pEvent )) {
  spdlog::get("logger")->error("Failed to parse HLO.");
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
    spdlog::get("logger")->error("HLO-command: Missing op [%s]", j.dump().c_str());
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
      spdlog::get("logger")->warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_SUCCESS;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::get("logger")->warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_JSON;
    j["arg"]["value"] = m_j_config["users"][index].dump();
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::get("logger")->error("Variable [] is unknown.");
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
      spdlog::get("logger")->warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    // Must be object
    if (!m_j_config["args"].is_object()) {
      spdlog::get("logger")->warn("The user info must be an object.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::get("logger")->warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    m_j_config["users"][index] = j["args"];

    j["arg"]["type"]  = VSCP_REMOTE_VARIABLE_CODE_JSON;
    j["arg"]["value"] = m_j_config["users"][index].dump();
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::get("logger")->error("Variable [] is unknown.");
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
      spdlog::get("logger")->warn("'users' must be of type array.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    // Must be object
    if (!m_j_config["args"].is_object()) {
      spdlog::get("logger")->warn("The user info must be an object.");
      j["result"] = VSCP_ERROR_INVALID_TYPE;
      goto abort;
    }

    int index = j.value("index", 0); // get index
    if (index >= m_j_config["users"].size()) {
      // Index to large
      spdlog::get("logger")->warn("index of array is to large [%u].", index >= m_j_config["users"].size());
      j["result"] = VSCP_ERROR_INDEX_OOB;
      goto abort;
    }

    m_j_config["users"].erase(index);
  }
  else {
    j["result"] = VSCP_ERROR_MISSING;
    spdlog::get("logger")->warn("Variable [%s] is unknown.", j.value("name", "").c_str());
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
    spdlog::get("logger")->warn("Failed to stop VSCP tcp/ip server.");
  }

  if (!start()) {
    spdlog::get("logger")->warn("Failed to start VSCP tcp/ip server.");
  }

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
    spdlog::get("logger")->error("Failed to convert event from ex to ev.");
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
    spdlog::get("logger")->error("Unable to allocate event storage.");
  }
  return true;
}

//////////////////////////////////////////////////////////////////////
// addEvent2SendQueue
//
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

///////////////////////////////////////////////////////////////////////////////
// sendEventToClient
//

// bool
// CEnergyP1::sendEventToClient(const vscpEvent* pEvent)
// {
//     // Must be valid pointers
//     if (NULL == pClientItem) {
//         spdlog::get("logger")->error("sendEventToClient - Pointer to clientitem is null");
//         return false;
//     }
//     if (NULL == pEvent) {
//         spdlog::get("logger")->error("sendEventToClient - Pointer to event is null");
//         return false;
//     }

//     // Check if filtered out - if so do nothing here
//     if (!vscp_doLevel2Filter(pEvent, &pClientItem->m_filter)) {
//         if (m_j_config.contains("debug") && m_j_config["debug"].get<bool>()) {
//             spdlog::get("logger")->debug("sendEventToClient - Filtered out");
//         }
//         return false;
//     }

//     // If the client queue is full for this client then the
//     // client will not receive the message
//     if (pClientItem->m_clientInputQueue.size() > m_j_config.value("max-out-queue", MAX_ITEMS_IN_QUEUE)) {
//         if (m_j_config.contains("debug") && m_j_config["debug"].get<bool>()) {
//             spdlog::get("logger")->debug("sendEventToClient - overrun");
//         }
//         // Overrun
//         pClientItem->m_statistics.cntOverruns++;
//         return false;
//     }

//     // Create a new event
//     vscpEvent* pnewvscpEvent = new vscpEvent;
//     if (NULL != pnewvscpEvent) {

//         // Copy in the new event
//         if (!vscp_copyEvent(pnewvscpEvent, pEvent)) {
//             vscp_deleteEvent_v2(&pnewvscpEvent);
//             return false;
//         }

//         // Add the new event to the input queue
//         pthread_mutex_lock(&pClientItem->m_mutexClientInputQueue);
//         pClientItem->m_clientInputQueue.push_back(pnewvscpEvent);
//         pthread_mutex_unlock(&pClientItem->m_mutexClientInputQueue);
//         sem_post(&pClientItem->m_semClientInputQueue);
//     }

//     return true;
// }

///////////////////////////////////////////////////////////////////////////////
// sendEventAllClients
//

// bool
// CEnergyP1::sendEventAllClients(const vscpEvent* pEvent)
// {
//     CClientItem* pClientItem;
//     std::deque<CClientItem*>::iterator it;

//     if (NULL == pEvent) {
//         spdlog::get("logger")->error("sendEventAllClients - No event to send");
//         return false;
//     }

//     pthread_mutex_lock(&m_clientList.m_mutexItemList);
//     for (it = m_clientList.m_itemList.begin();
//          it != m_clientList.m_itemList.end();
//          ++it) {
//         pClientItem = *it;

//         if (NULL != pClientItem) {
//             if (m_j_config.contains("debug") && m_j_config["debug"].get<bool>()) {
//                 spdlog::get("logger")->debug("Send event to client [%s]",
//                                                 pClientItem->m_strDeviceName.c_str());
//             }
//             if (!sendEventToClient(pClientItem, pEvent)) {
//                 spdlog::get("logger")->error("sendEventAllClients - Failed to send event");
//             }
//         }
//     }

//     pthread_mutex_unlock(&m_clientList.m_mutexItemList);

//     return true;
// }

/////////////////////////////////////////////////////////////////////////////
// startWorkerThread
//

bool
CEnergyP1::startWorkerThread(void)
{
  if (m_bDebug) {
    spdlog::get("logger")->debug("Starting P1 energy meter interface...");
  }

  if (pthread_create(&m_workerThread, NULL, workerThread, this)) {
    spdlog::get("logger")->error("Unable to start the workerthread.");
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
  //     spdlog::get("logger")->debug("Controlobject: Terminating TCP thread.");
  // }

  // pthread_join(m_tcpipListenThread, NULL);
  // delete CEnergyP1;
  // CEnergyP1 = NULL;

  // if (__VSCP_DEBUG_TCP) {
  //     spdlog::get("logger")->debug("Controlobject: Terminated TCP thread.");
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
    spdlog::get("logger")->error("Failed to read encryption key file [%s]", m_path.c_str());
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
  if (nullptr == pData)
    return NULL;

  // Open the serial port
  if (!com.open((const char *) pObj->m_serialDevice.c_str())) {
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
    com.DtrOn();
  }

  // Work on
  while (!pObj->m_bQuit) {

    if (com.isCharReady()) {
      int read;
      while (com.isCharReady() && (pos < (sizeof(buf) - 1))) {
        char c = com.readChar(&read);
        if (read) {
          buf[pos++] = c;
        }
      }
    }
    else {
      // If no data we sleep for a second  - no rush here...
      sleep(1);
      continue;
    }

    buf[pos] = 0;  // Add terminating zero
    strbuf += buf; // Add to the string buffer

    // Check for a full line of input
    size_t pos_cr;
    std::string exstr;
    if (std::string::npos != (pos_cr = strbuf.find("\n"))) {
      exstr  = strbuf.substr(0, pos_cr);
      strbuf = strbuf.substr(pos_cr + 1);
    }

    for (auto const &pItem : pObj->m_listItems) {

      vscpEventEx ex;
      ex.head      = VSCP_HEADER16_GUID_TYPE_STANDARD | VSCP_PRIORITY_NORMAL;
      ex.timestamp = vscp_makeTimeStamp();
      vscp_setEventExDateTimeBlockToNow(&ex);
      ex.vscp_class = pItem->getVscpClass();
      ex.vscp_class = pItem->getVscpType();
      ex.GUID[15]   = pItem->getGuidLsb();
      double value  = pItem->getValue(exstr);

      if (exstr.rfind(pItem->getToken(), 0) == 0) {
        if (pObj->m_bDebug) {
          std::cout << ">>> Found " << pItem->getToken() << " value = " << pItem->getValue(exstr)
                    << " unit = " << pItem->getUnit(exstr) << std::endl;
        }
        switch (pItem->getVscpClass()) {
          case VSCP_CLASS1_MEASUREMENT: {
            switch (pItem->getLevel1Coding()) {
              case VSCP_DATACODING_STRING: {
                if (!vscp_makeStringMeasurementEventEx(&ex,
                                                       (float) value,
                                                       pItem->getSensorIndex(),
                                                       pItem->getUnit(exstr))) {
                  break;
                }
              } break;
              case VSCP_DATACODING_INTEGER: {
                uint64_t val64 = value;
                if (!vscp_convertIntegerToNormalizedEventData(ex.data,
                                                              &ex.sizeData,
                                                              val64,
                                                              pItem->getUnit(exstr),
                                                              pItem->getSensorIndex())) {
                  break;
                }
              } break;
              case VSCP_DATACODING_NORMALIZED: {
                uint64_t val64 = value;
                if (!vscp_convertIntegerToNormalizedEventData(ex.data,
                                                              &ex.sizeData,
                                                              val64,
                                                              pItem->getUnit(exstr),
                                                              pItem->getSensorIndex())) {
                  break;
                }
              } break;
              case VSCP_DATACODING_SINGLE:
                if (!vscp_makeFloatMeasurementEventEx(&ex,
                                                      (float) value,
                                                      pItem->getSensorIndex(),
                                                      pItem->getUnit(exstr))) {
                  break;
                }
                break;
              case VSCP_DATACODING_DOUBLE:
                break;
            }

          } break;

          case VSCP_CLASS1_MEASUREMENT64: {
            if (!vscp_makeFloatMeasurementEventEx(&ex, (float) value, pItem->getSensorIndex(), pItem->getUnit(exstr))) {
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
            if (!vscp_makeLevel2StringMeasurementEventEx(&ex,
                                                         pItem->getVscpType(),
                                                         value,
                                                         pItem->getUnit(exstr),
                                                         pItem->getSensorIndex(),
                                                         pItem->getZone(),
                                                         pItem->getSubZone())) {
              break;
            }
          } break;

          case VSCP_CLASS2_MEASUREMENT_FLOAT: {
            if (!vscp_makeLevel2FloatMeasurementEventEx(&ex,
                                                        pItem->getVscpType(),
                                                        value,
                                                        pItem->getUnit(exstr),
                                                        pItem->getSensorIndex(),
                                                        pItem->getZone(),
                                                        pItem->getSubZone())) {
              break;
            }
          } break;
        }
      }
    }

  } // Main loop

  // Set DTR if requested to do so
  if (pObj->m_bDtrOnStart) {
    com.DtrOff();
  }

  // Close the serial port
  com.close();

  return NULL;
}