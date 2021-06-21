
#include <iostream>
#include <sstream>
#include <string>
#include <deque>
#include <map>


class CP1Item {

public:

  /// CTOR
  CP1Item();

  CP1Item(const std::string token,
            const std::string description,
            uint16_t vscp_class,
            uint16_t vscp_type,
            uint8_t sensorindex = 0,
            uint8_t zone = 0,
            uint8_t subzone = 0);

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
    @return true on success
  */
  bool initItem(const std::string& token,
                    const std::string& description,
                    uint16_t vscp_class,
                    uint16_t vscp_type,
                    uint8_t sensorindex = 0,
                    uint8_t zone = 0,
                    uint8_t subzone = 0 );

  /*!
    Add unit to unit map
    @param p1unit Unit string for P1 unit (such as "kW", "V" and "A")
    @param vscp_unit VSCP unit code.
  */
  void addUnit(const std::string& p1unit, uint8_t vscp_unit) 
        { m_map_unit[p1unit] = vscp_unit; };

  /*!
    Get measurement value
    @param line Meter reding line
    @return Measurement value as a double
  */
  double getValue(const std::string& line) 
    { return std::stod(line.substr(10 + 1)); };   

  /*!
    Get VSCP numerical unit code from textual unit
    @param line Meter reding line
    @return Unit as integer. -1 if not found
  */
  int getUnit(const std::string& line);     

  // Getters / Setters

  /*
    Token
  */
  std::string getToken(void) { return m_token; };
  void setToken(const std::string& token) { m_token = token; };

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
    Maps P1 unit to VSCP unit code
  */
  std::map<std::string, uint8_t> m_map_unit;

};

///////////////////////////////////////////////////////////////////////////////
// CTOR
//

CP1Item::CP1Item() {
  m_token = "";
  m_description = "Undefined";
  m_vscp_class = 0;
  m_vscp_type = 0;
  m_sensorindex = 0;
  m_zone = 0;
  m_subzone = 0;
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
                    uint8_t subzone) {
  initItem(token,
            description,
            vscp_class,
            vscp_type,
            sensorindex,
            zone,
            subzone);
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
                    uint8_t subzone)
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

// ----------------------------------------------------------------------------

int main() 
{
  CP1Item *pItem;
  std::deque<CP1Item *> listp1;

  // Active energy out
  pItem = new CP1Item("1-0:1.8.0",
                        "Active energy out",
                        1040,
                        13,
                        1,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("kWh",13);

  // Active energy in
  pItem = new CP1Item("1-0:2.8.0",
                        "Active energy in",
                        1040,
                        13,
                        2,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("kWh",13);                             

  // Voltage L1
  pItem = new CP1Item("1-0:32.7.0",
                        "Voltage L1",
                        1040,
                        13,
                        21,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("V",16);

  // Voltage L2
  pItem = new CP1Item("1-0:52.7.0",
                        "Voltage L3",
                        1040,
                        13,
                        22,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("V",16);

  // Voltage L3
  pItem = new CP1Item("1-0:72.7.0",
                        "Voltage L3",
                        1040,
                        13,
                        23,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("V",16);


  // Current L1
  pItem = new CP1Item("1-0:31.7.0",
                        "Current L1",
                        1040,
                        13,
                        24,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("A",5);

  // Current L2
  pItem = new CP1Item("1-0:51.7.0",
                        "Current L2",
                        1040,
                        13,
                        25,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("A",5);

  // Current L3
  pItem = new CP1Item("1-0:71.7.0",
                        "Current L3",
                        1040,
                        13,
                        26,0,0);
  listp1.push_back(pItem);
  pItem->addUnit("A",5);
  
  const std::string inputstr = "0-0:1.0.0(210511210508W)\n"\
                "1-0:1.8.0(00001576.782*kWh)\n"\
                "1-0:2.8.0(00000000.001*kWh)\n"\
                "1-0:3.8.0(00000009.258*kvarh)\n"\
                "1-0:4.8.0(00000072.421*kvarh)\n"\
                "1-0:1.7.0(0007.171*kW)\n"\
                "1-0:2.7.0(0000.000*kW)\n"\
                "1-0:3.7.0(0000.063*kvar)\n"\
                "1-0:4.7.0(0000.523*kvar)\n"\
                "1-0:21.7.0(0001.180*kW)\n"\
                "1-0:41.7.0(0004.253*kW)"\
                "1-0:61.7.0(0001.736*kW)\n"\
                "1-0:22.7.0(0000.000*kW)\n"\
                "1-0:42.7.0(0000.000*kW)\n"\
                "1-0:62.7.0(0000.000*kW)\n"\
                "1-0:23.7.0(0000.000*kvar)\n"\
                "1-0:43.7.0(0000.000*kvar)\n"\
                "1-0:63.7.0(0000.063*kvar)\n"\
                "1-0:24.7.0(0000.200*kvar)\n"\
                "1-0:44.7.0(0000.323*kvar)\n"\
                "1-0:64.7.0(0000.000*kvar)\n"\
                "1-0:32.7.0(236.2*V)\n"\
                "1-0:52.7.0(231.1*V)\n"\
                "1-0:72.7.0(236.2*V)\n"\
                "1-0:31.7.0(005.5*A)\n"\
                "1-0:51.7.0(018.4*A)\n"\
                "1-0:71.7.0(007.3*A)\n"\
                "!A0B1\n";
  std::cout << "Test program for P1 electric meter data" << std::endl;

  std::deque<std::string> inp_array; // = {7, 5, 16, 8};
  std::istringstream iss(inputstr);
  for(std::string s; iss >> s;) {
    inp_array.push_back(s);
    std::cout << s << std::endl;
    size_t pos1_unit = s.find("*");
    size_t pos2_unit = s.find(")");
    int diff = pos2_unit - pos1_unit - 1;
    if (s.rfind("1-0:62.7.0", 0) == 0) {
      std::cout << "\t" << s << " - value = " 
                << std::stod(s.substr(10 + 1)) 
                << "\t" << pos1_unit << " " << pos2_unit << " - " + s.substr(pos1_unit + 1, diff)
                << std::endl;
    }

    for (auto const& pItem : listp1) {
      if (s.rfind(pItem->getToken(), 0) == 0) {
        std::cout << ">>> Found " 
                  << pItem->getToken() 
                  << " value = "
                  << pItem->getValue(s)
                  << " unit = "
                  << pItem->getUnit(s)
                  << std::endl;
      }
    }

  }

  // Clean up
  for (auto const& x : listp1) {
    delete x;
  }

  std::string str = "This is line 1\nThis is line2\nThis is line3\n";
  size_t pos_cr;
  if (std::string::npos != (pos_cr = str.find("\n"))) {
    std::string exstr = str.substr(0,pos_cr);
    std::string rest = str.substr(pos_cr+1);
    std::cout << "[" << exstr << "] - [" << rest << "]\n";
  }

  return 0;
}