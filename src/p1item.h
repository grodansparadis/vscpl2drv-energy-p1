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

#if !defined(VSCP_P1ITEM_H__INCLUDED_)
#define VSCP_P1ITEM_H__INCLUDED_

#include <deque>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

class CP1Item {

public:
  /// CTOR
  CP1Item();

  CP1Item(const std::string token,
          const std::string description,
          uint16_t vscp_class,
          uint16_t vscp_type,
          uint8_t sensorindex  = 0,
          uint8_t zone         = 0,
          uint8_t subzone      = 0,
          uint8_t level1Coding = 0);

  /// DTOR
  ~CP1Item();

  /*!
    Set up P1 item
    @param token The P1 string token to search for.
    @param description The description of the token.
    @param vscp_class The VSCP class for the event to constuct. Only
                      10,60.65,80,85 are valid values.
    @param vscp_type The VSCP type for the event to constuct.
    @param vscp_sensorindex The VSCP class for the event to constuct.
    @param vscp_zone The VSCP class for the event to constuct.
    @param vscp_subzone The VSCP class for the event to constuct.
    @param level1Coding How Level 1 measurements should be coded. Can be
                          normalize integer VSCP_DATACODING_NORMALIZED (0x80),
                          integer           VSCP_DATACODING_INTEGER (0x60),
                          single            VSCP_DATACODING_SINGLE (0xa0),
                          double            VSCP_DATACODING_DOUBLE (0xc0),
                          string            VSCP_DATACODING_STRING (0x40)
    @return true on success
  */
  bool initItem(const std::string &token,
                const std::string &description,
                uint16_t vscp_class,
                uint16_t vscp_type,
                uint8_t sensorindex  = 0,
                uint8_t zone         = 0,
                uint8_t subzone      = 0,
                uint8_t level1Coding = 0);

  /*!
    Add unit to unit map
    @param p1unit Unit string for P1 unit (such as "kW", "V" and "A")
    @param vscp_unit VSCP unit code.
  */
  void addUnit(const std::string &p1unit, uint8_t vscp_unit) { m_map_unit[p1unit] = vscp_unit; };

  /*!
    Get measurement value
    @param line Meter reding line
    @return Measurement value as a double
  */
  double getValue(const std::string &line) { return (m_factor * std::stod(line.substr(10 + 1))); };

  /*!
    Get VSCP numerical unit code from textual unit
    @param line Meter reding line
    @return Unit as integer. -1 if not found
  */
  int getUnit(const std::string &line);


  // Getters / Setters


  /*
    Token
  */
  std::string getToken(void) { return m_token; };
  void setToken(const std::string &token) { m_token = token; };

  /*
    Description
  */
  std::string getDescription(void) { return m_description; };
  void setDescription(const std::string &description) { m_description = description; };

  /*
    vscp_class
  */
  uint16_t getVscpClass(void) { return m_vscp_class; };
  void setVscpClass(const uint16_t vscp_class) { m_vscp_class = vscp_class; };

  /*
    vscp_type
  */
  uint16_t getVscpType(void) { return m_vscp_type; };
  void setVscpType(uint16_t vscp_type) { m_vscp_type = vscp_type; };

  /*
    guid_lsb
  */
  uint8_t getGuidLsb(void) { return m_guid_lsb; };
  void setGuidLsb(uint8_t guid_lsb) { m_guid_lsb = guid_lsb; };

  /*
    sensorindex
  */
  uint8_t getSensorIndex(void) { return m_sensorindex; };
  void setSensorIndex(uint8_t sensorindex) { m_sensorindex = sensorindex; };

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
    Level I coding
  */
  uint8_t getLevel1Coding(void) { return m_level1Coding; };
  void setLevel1Coding(uint8_t coding) { m_level1Coding = coding; };

  /*
    Factor
  */
  double getFactor(void) { return m_factor; };
  void setFactor(double factor) { m_factor = factor; };

  /*
    Storage name
  */
  std::string getStorageName(void) { return m_storageName; };
  void setStorageName(const std::string& storage) { m_storageName = storage; };

private:
  /*!
    Measurement value id such as "1-0:1.8.0"
  */
  std::string m_token;

  /*!
    P1 item description such as "Voltage L1"
  */
  std::string m_description;

  /*!
    VSCP class the P1 value should translated to
    Allowed classes
    10   - Level I measurement            CLASS1.MEASUREMENT
    60   - Level I float 64 measurement   CLASS1.MEASUREMENT64
    65   - Level I measurement zone       CLASS1.MEASUREZONE
    70   - Level I measurement float 32   CLASS1.MEASUREMENT32
    85   - Level I set value              CLASS1.SETVALUEZONE
    1040 - Level II measurement string    CLASS2.MEASUREMENT_STR
    1060 - Level II measurement float     CLASS2.MEASUREMENT_FLOAT
  */
  uint16_t m_vscp_class;

  /*!
    VSCP type the P1 value should translated to
  */
  uint16_t m_vscp_type;

  /*!
    GUID least significant byte
  */
  uint8_t m_guid_lsb;

  /*!
    Sensor index to use for event
  */
  uint8_t m_sensorindex;

  /*!
    Zone to use for event
  */
  uint8_t m_zone;

  /*!
    Subzone to use for event
  */
  uint8_t m_subzone;

  /*!
    Level I coding
  */
  uint8_t m_level1Coding;

  /*
    Factor
    Multiply read value with factor to 
    get value in VSCP unit.
  */
  double m_factor;

  /*
    Name that value will be stored as
  */
  std::string m_storageName;

  /*!
    Maps P1 unit to VSCP unit code
  */
  std::map<std::string, uint8_t> m_map_unit;

};

#endif // VSCP_P1ITEM_H__INCLUDED_