// alarm.h
//
// This file is part of the VSCP (https://www.vscp.org)
//
// The MIT License (MIT)
//
// Copyright © 2000-2023 Ake Hedman, the VSCP Project
// <akhe@vscp.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#if !defined(VSCP_ALARM_H__INCLUDED_)
#define VSCP_ALARM_H__INCLUDED_

#include <deque>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

enum class alarm_op { gt, lt };

class CAlarm {

public:
  /// CTOR
  CAlarm();

  CAlarm(const std::string name,
         alarm_op op,
         double value,
         uint8_t b       = 0,
         uint8_t zone    = 0,
         uint8_t subzone = 0,
         bool bOneShoot  = false);

  /// DTOR
  ~CAlarm();

  /*!
    Set up P1 item
    @param name Variable name to check
    @param op Compare operation
    @param value Value to compare named variable with.
    @param bOneShoot Alarm sent once if false, need alarm reset before
      another alarm will be sent.
    @return true on success
  */
  bool init(const std::string name,
            alarm_op op,
            double value,
            uint8_t b       = 0,
            uint8_t zone    = 0,
            uint8_t subzone = 0,
            bool bOneShoot  = false);

  // Getters / Setters

  /*
    Name = "variable" in config
  */
  std::string getVariable(void) { return m_name; };
  void setVariable(const std::string &name) { m_name = name; };

  /*!
    Handle sent flag
  */
  void setSentFlag(bool sent = true) {m_bSent = sent; };
  bool isSent(void)  {return m_bSent; };

  /*
    op
  */
  alarm_op getOp(void) { return m_op; };
  void setOp(alarm_op op) { m_op = op; };

  /*!
    Set operation from string
    @param strop Operation in string format
            "<" - Less than
            ">" - Greate than
    @return true on success        
  */
  bool setOperation(const std::string& strop);

  /*
    Value
  */
  double getValue(void) { return m_value; };
  void setValue(double value) { m_value = value; };

  /*
    Alarm byte
  */
  uint8_t getAlarmByte(void) { return m_alarmByte; };
  void setAlarmByte(uint8_t b) { m_alarmByte = b; };

  /*
    zone
  */
  uint8_t getZone(void) { return m_zone; };
  void setZone(uint8_t zone) { m_zone = zone; };

  /*
    subzone
  */
  uint8_t getSubZone(void) { return m_subzone; };
  void setSubZone(uint8_t subzone) { m_subzone = subzone; };

  /*
    One Shot
  */
  bool isOneShot(void) { return m_bOneShot; };
  void setOneShot(bool bOneShot = true) { m_bOneShot = bOneShot; };

private:
  /*!
    Name on variable to test
  */
  std::string m_name;

  /*!
    True if alarm has been sent
  */
  bool m_bSent;

  /*!
    Compare operation to perform
  */
  alarm_op m_op;

  /*!
    Value to compare with
  */
  double m_value;

  /*!
    True for on-shoot alarm. If false alarm will be
    sent on every compare.
  */
  bool m_bOneShot;

  /*!
    Alarm byte (byte 0)
  */
  uint8_t m_alarmByte;

  /*!
    Zone to use for event
  */
  uint8_t m_zone;

  /*!
    Subzone to use for event
  */
  uint8_t m_subzone;
};

#endif // VSCP_ALARM_H__INCLUDED_