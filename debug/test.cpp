
#include <iostream>
#include <sstream>
#include <string>
#include <deque>

int main() {

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
  for(std::string s; iss>>s ;) {
    inp_array.push_back(s);
    std::cout << s << std::endl;
    if (s.rfind("1-0:51.7.0", 0) == 0) {
      std::cout << "\t" << s << " - " << std::stod(s.substr(10 + 1)) << std::endl;
    }
  }

  return 0;
}