#ifndef VSCP_ENERGY_P1_SERIALPORT_H
#define VSCP_ENERGY_P1_SERIALPORT_H

#ifdef WIN32

#include <com-win32.h>

#include <cstdlib>
#include <string>

class SerialPort {
public:
  bool open(const char *device)
  {
    if (nullptr == device) {
      return false;
    }

    std::string port(device);
    const std::string::size_type marker = port.find("COM");
    if (std::string::npos == marker) {
      return false;
    }

    const long portNumber = std::strtol(port.c_str() + marker + 3, nullptr, 10);
    if ((portNumber < 1) || (portNumber > 255)) {
      return false;
    }

    m_portNumber = static_cast<unsigned char>(portNumber);
    return true;
  }

  void setParam(const char *baud,
                const char *parity,
                const char *bits,
                int hardwareFlow,
                int softwareFlow)
  {
    const unsigned char parityMode = ('E' == parity[0]) ? EVENPARITY : ('O' == parity[0]) ? ODDPARITY : NOPARITY;
    const unsigned char handshake = hardwareFlow ? HANDSHAKE_HARDWARE
                                                 : softwareFlow ? HANDSHAKE_SOFTWARE : HANDSHAKE_NONE;
    m_comm.init(m_portNumber,
                static_cast<DWORD>(std::strtoul(baud, nullptr, 10)),
                static_cast<unsigned char>(std::strtoul(bits, nullptr, 10)),
                parityMode,
                ONESTOPBIT,
                handshake);
  }

  void DtrOn()
  {
    if (nullptr != m_comm.getHandle()) {
      EscapeCommFunction(m_comm.getHandle(), SETDTR);
    }
  }

  void DtrOff()
  {
    if (nullptr != m_comm.getHandle()) {
      EscapeCommFunction(m_comm.getHandle(), CLRDTR);
    }
  }

  void close()
  {
    m_comm.close();
  }

  int isCharReady()
  {
    COMSTAT status = {};
    DWORD errors = 0;
    return (nullptr != m_comm.getHandle()) && ClearCommError(m_comm.getHandle(), &errors, &status)
             ? static_cast<int>(status.cbInQue)
             : 0;
  }

  char readChar(int *count)
  {
    if (nullptr == m_comm.getHandle()) {
      *count = 0;
      return 0;
    }
    return m_comm.readChar(count);
  }

private:
  unsigned char m_portNumber = 0;
  CComm m_comm;
};

#else

#include <com-linux.h>

using SerialPort = Comm;

#endif

#endif