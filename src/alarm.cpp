// p1item.h
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

#include <vscp.h>

#include "alarm.h"

///////////////////////////////////////////////////////////////////////////////
// CTOR
//

CAlarm::CAlarm()
{
  m_name    = "";
  m_zone    = 0;
  m_subzone = 0;
}

///////////////////////////////////////////////////////////////////////////////
// CTOR
//

CAlarm::CAlarm(const std::string name,
               alarm_op op,
               double value,
               uint8_t b,
               uint8_t zone,
               uint8_t subzone,
               bool bOneShoot)
{
  init(name, op, value, b, zone, subzone, bOneShoot);
}

///////////////////////////////////////////////////////////////////////////////
// DTOR
//

CAlarm::~CAlarm()
{
  m_bSent  = false;
  m_name = "";
  m_op   = alarm_op::gt;

  m_alarmByte = 0;
  m_zone      = 0;
  m_subzone   = 0;
}

///////////////////////////////////////////////////////////////////////////////
// initItem
//

bool
CAlarm::init(const std::string name,
             alarm_op op,
             double value,
             uint8_t b,
             uint8_t zone,
             uint8_t subzone,
             bool bOneShoot)
{
  m_bSent = false;

  // Must be a token
  if (!name.length()) {
    return false;
  }

  m_name = name;
  m_op   = op;

  m_alarmByte = b;
  m_zone      = zone;
  m_subzone   = subzone;

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// setOperation
//

bool
CAlarm::setOperation(const std::string &strop)
{
  if (">" == strop) {
    m_op = alarm_op::gt;
  }
  else if ("<" == strop) {
    m_op = alarm_op::lt;
  }
  return true;
}