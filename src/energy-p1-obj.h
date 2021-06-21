// energy-p1-obj.h: 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP (http://www.vscp.org)
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

#if !defined(VSCPENERGYP1_H__202105112227__INCLUDED_)
#define VSCPENERGYP1_H__202105112227__INCLUDED_

#define _POSIX

#ifdef WIN32
#include "StdAfx.h"
#endif

#include <list>
#include <string>

#include <pthread.h>
#include <stdio.h>
#include <string.h>
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

#include "p1item.h"

#include <json.hpp>  // Needs C++11  -std=c++11

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

// https://github.com/nlohmann/json
using json = nlohmann::json;

const uint16_t MAX_ITEMS_IN_QUEUE = 32000;

#define DRIVER_COPYRIGHT "Copyright © 2000-2021 Ake Hedman, the VSCP Project, https://www.vscp.org"

// Seconds before trying to reconnect to a broken connection
#define VSCP_ENERGYP1_DEFAULT_RECONNECT_TIME 30

#define VSCP_ENERGYP1_SYSLOG_DRIVER_ID "[vscpl2drv-energyp1] "
#define VSCP_LEVEL2_DLL_LOGGER_OBJ_MUTEX                                       \
    "___VSCP__DLL_L2TCPIPLINK_OBJ_MUTEX____"
#define VSCP_ENERGYP1_LIST_MAX_MSG 2048

// Module Local HLO op's
#define HLO_OP_LOCAL_CONNECT      HLO_OP_USER_DEFINED + 0
#define HLO_OP_LOCAL_DISCONNECT   HLO_OP_USER_DEFINED + 1

// Forward declarations
class CHLO;

class CEnergyP1
{
  public:
    /// Constructor
    CEnergyP1();

    /// Destructor
    virtual ~CEnergyP1();

    /*!
        Open
        @return True on success.
     */
    bool open(std::string& path, const uint8_t* pguid);

    /*!
        Flush and close the log file
     */
    void close(void);

    /*!
      Parse HLO object
    */
    bool parseHLO(uint16_t size, uint8_t* inbuf, CHLO* phlo);

    /*!
      Handle high level object
    */
    bool handleHLO(vscpEvent* pEvent);

    /*!
      Load configuration if allowed to do so
      @param path Oath to configuration file
      @return True on success, false on failure.
    */
    bool doLoadConfig(std::string& path);

    /*!
      Save configuration if allowed to do so
    */
    bool doSaveConfig(void);

    bool readVariable(vscpEventEx& ex, const json& json_req);

    bool writeVariable(vscpEventEx& ex, const json& json_req);

    bool deleteVariable(vscpEventEx& ex, const json& json_req);

    bool stop(void);

    bool start(void);

    bool restart(void);  

    bool startWorkerThread(void);

    bool stopWorkerThread(void);

    /*!
        Put event on receive queue and signal
        that a new event is available

        @param ex Event to send
        @return true on success, false on failure
    */
    bool eventExToReceiveQueue(vscpEventEx& ex);

    /*!
      Add event to send queue
     */
    bool addEvent2SendQueue(const vscpEvent* pEvent);

    /*!
      Add event to receive queue
    */
    bool addEvent2ReceiveQueue(const vscpEvent* pEvent);

    // Send event to host
    bool sendEvent(vscpEvent *pEvent);

    /*!
      Read encryption key
      @param path Path to file holding encryption key.
      @return True if read OK.
    */
    bool readEncryptionKey(const std::string& path);

  public:

    /// Parsed Config file
    json m_j_config;

    // ------------------------------------------------------------------------

    // * * * Configuration    

    /// Path to configuration file
    std::string m_path;

    /*! 
      True if config is remote writable
    */
    bool m_bWriteEnable;

    /*!
      True to enable debug mode
    */
    bool m_bDebug;
    
    /*! 
      Serial device
    */
    std::string m_serialDevice;

    /*! 
      Serial baudrate
    */
    std::string m_serialBaudrate;

    /*! 
      Serial parity
    */
    std::string m_serialParity;

    /*! 
      Serial number of data bits
    */
    std::string m_serialCountDataBits;

    /*! 
      Serial number of stopbits
    */
    uint8_t m_serialCountStopbits;

    /*! 
      Enable hardware flow control
    */
    bool m_bSerialHwFlowCtrl;

    /*! 
      Enable software flow control
    */
    bool m_bSerialSwFlowCtrl;

    /*!
      Enable DTR on start, disable on close.
    */
    bool m_bDtrOnStart;


    /////////////////////////////////////////////////////////
    //                      Logging
    /////////////////////////////////////////////////////////
    
    bool m_bEnableFileLog;                    // True to enable logging
    spdlog::level::level_enum m_fileLogLevel; // log level
    std::string m_fileLogPattern;             // log file pattern
    std::string m_path_to_log_file;           // Path to logfile      
    uint32_t m_max_log_size;                  // Max size for logfile before rotating occures 
    uint16_t m_max_log_files;                 // Max log files to keep

    // ------------------------------------------------------------------------

    /// Run flag
    bool m_bQuit;

    /// Our GUID
    cguid m_guid;

    /// Filter for receive
    vscpEventFilter m_rxfilter;

    /// Filter for transmitt
    vscpEventFilter m_txfilter;

    // The default random encryption key
    uint8_t m_vscp_key[32] = {
        0x2d, 0xbb, 0x07, 0x9a, 0x38, 0x98, 0x5a, 0xf0, 0x0e, 0xbe, 0xef, 0xe2, 0x2f, 0x9f, 0xfa, 0x0e,
        0x7f, 0x72, 0xdf, 0x06, 0xeb, 0xe4, 0x45, 0x63, 0xed, 0xf4, 0xa1, 0x07, 0x3c, 0xab, 0xc7, 0xd4
    };

    /*!
      List with P1 meaurement item defines
    */
    std::deque<CP1Item *> m_listItems;
 
    // ------------------------------------------------------------------------

    /// Send queue
    std::list<vscpEvent*> m_sendList;

    /// Receive queue
    std::list<vscpEvent*> m_receiveList;

    // Maximum number of events in the outgoing queue
    uint16_t m_maxItemsInClientReceiveQueue;

    /*!
        Event object to indicate that there is an event in the output queue
     */
    sem_t m_semSendQueue;

    /*!
      Event object to indicate that there is an event in the input queue
    */
    sem_t m_semReceiveQueue;

    /// Mutex to protect the output queue
    pthread_mutex_t m_mutexSendQueue;

    /// Mutex to protet the input queue
    pthread_mutex_t m_mutexReceiveQueue;

    /*!
      Serial worker thread
    */
    pthread_t m_workerThread;
};

#endif  // !defined(VSCPENERGYP1_H__202105112227__INCLUDED_)
