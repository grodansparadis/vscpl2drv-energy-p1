// p1item.h
//
// This file is part of the VSCP (https://www.vscp.org)
//
// The MIT License (MIT)
//
// Copyright © 2000-2021 Ake Hedman, the VSCP Project
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

#include "p1item.h"


///////////////////////////////////////////////////////////////////////////////
// CTOR
//

CP1Item::CP1Item() {
  m_token = "";
  m_description = "Undefined";
  m_vscp_class = 1040;
  m_vscp_type = 0;
  m_guid_lsb = 0;
  m_sensorindex = 0;
  m_zone = 0;
  m_subzone = 0;
  m_level1Coding = VSCP_DATACODING_STRING;
}

///////////////////////////////////////////////////////////////////////////////
// CTOR
//

CP1Item::CP1Item(const std::string token,
                    const std::string description,
                    uint16_t vscp_class,
                    uint16_t vscp_type,
                    uint8_t sensorindex,
                    uint8_t zone,
                    uint8_t subzone,
                    uint8_t level1Coding) {
  initItem(token,
            description,
            vscp_class,
            vscp_type,
            sensorindex,
            zone,
            subzone,
            level1Coding);
}

///////////////////////////////////////////////////////////////////////////////
// DTOR
//

CP1Item::~CP1Item() {
}

///////////////////////////////////////////////////////////////////////////////
// initItem
//

bool CP1Item::initItem(const std::string& token,
                    const std::string& description,
                    uint16_t vscp_class,
                    uint16_t vscp_type,
                    uint8_t sensorindex,
                    uint8_t zone,
                    uint8_t subzone,
                    uint8_t level1Coding)
{
  // Must be a token
  if (!token.length()) {
    return false;
  }

  m_token = token;
  m_description = description;

  // Must be a valid VSCP measurement class
  if ( (0 != vscp_class) &&
       (60 != vscp_class) &&
       (65 != vscp_class) &&
       (70 != vscp_class) &&
       (75 != vscp_class) && 
       (85 != vscp_class)) {
    return false;       
  }
  
  m_vscp_class = vscp_class;
  m_vscp_type = vscp_type;

  m_sensorindex = sensorindex;
  m_zone = zone;
  m_subzone = subzone;

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// getUnit
//

int CP1Item::getUnit(const std::string& line)
{
  size_t pos1_unit = line.find("*");
  size_t pos2_unit = line.find(")");
  int diff = pos2_unit - pos1_unit - 1;

  for (auto const& x : m_map_unit) {
    if (x.first == line.substr(pos1_unit + 1, diff)) {
      return x.second;
    }
  }
  return -1;
}
