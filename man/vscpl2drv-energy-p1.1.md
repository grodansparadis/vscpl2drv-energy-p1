% VSCPL2DRV-ENERGY-P1(1) VSCP Level II driver
% Åke Hedman, Grodans Paradis AB
% November 2026

# NAME

vscpl2drv-energy-p1 - read a smart electricity meter's P1 port

# SYNOPSIS

**libvscpl2drv-energy-p1.so** (Linux), **vscpl2drv-energy-p1.dll** (Windows), **libvscpl2drv-energy-p1.dylib** (macOS)

A VSCP Level II driver. It is loaded by the VSCP daemon (**vscpd**(8)), VSCP Works or any other program that uses the VSCP Level II driver API, and is configured with the JSON configuration file described below. The path of the configuration file is given to the driver when it is opened (**path-config** in the daemon's driver configuration).

# DESCRIPTION

A driver that read data from a [DSMR V5.0.2 P1](https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf) energy meter and translate metering to relevant VSCP events.

# CONFIGURATION

## Linux

### VSCP daemon driver config

The VSCP daemon configuration is (normally) located at */etc/vscp/vscpd.conf*. To use the vscpl2drv-energy-p1.so driver there must be an entry in the

```
> <level2driver enable="true">
```

section on the following format

```xml
<!-- Level II TCP/IP Server -->
<driver enable="true"
  name="vscp-tcpip-srv"
  path-driver="/usr/lib/vscp/drivers/level2/vscpl2drv-energy-p1.so"
  path-config="/etc/vscp/vscpl2drv-energy-p1.conf"
  guid="FF:FF:FF:FF:FF:FF:FF:FC:88:99:AA:BB:CC:DD:EE:FF"
</driver>
```

#### enable
Set enable to "true" if the driver should be loaded.

#### name
This is the name of the driver. Used when referring to it in different interfaces.

#### path
This is the path to the driver. If you install from a Debian package this will be */usr/bin/vscpl2drv-energy-p1.so* and if you build and install the driver yourself it will be */usr/local/bin/vscpl2drv-energy-p1.so* or a custom location if you configured that.

#### guid
All level II drivers must have a unique GUID. There is many ways to obtain this GUID, Read more [here](https://grodansparadis.gitbooks.io/the-vscp-specification/vscp_globally_unique_identifiers.html).

### vscpl2drv-energy-p1 driver config

On start up the configuration is read from the path set in the driver configuration of the VSCP daemon, usually */etc/vscp/conf-file-name* and values are set from this location. If the **write** parameter is set to "true" the above location is a bad choice as the VSCP daemon will not be able to write to it. A better location is */var/lib/vscp/drivername/configure.xml* or some other writable location.

The default configuration file available in the [resources folder](https://github.com/grodansparadis/vscpl2drv-energy-p1/blob/main/resources/linux/energyp1.json) is made for the [Ellevio electrical meters](https://grodansparadis.com/wordpress/wp-admin/post.php?post=5039&action=edit) and have the following format. You can easily adopt this file to your meters format. Live data is [here](https://demo.vscp.org/power.html).

```json
{
  "write" : false,
  "debug" : true,       
  "serial": {
    "port": "/dev/electric_meter",
    "baudrate": 115200,
    "bits": 8,
    "parity": "N",
    "stopbits": 1,
    "hwflowctrl": false,
    "swflowctrl": false,
    "dtr-on-start": true
  },
  "logging": { 
    "console-enable": true,
    "console-level": "trace",    
    "console-pattern": "[vcpl2drv-energyp1 %c] [%^%l%$] %v",
    "file-log-enable": true,
    "file-log-level": "trace",
    "file-log-path" : "/var/log/vscp/vscpl2drv-energyp1.log",
    "file-log-pattern": "[vscpl2drv-energyp1: %c] [%^%l%$] %v", 
    "file-log-max-size": 50000,
    "file-log-max-files": 7
  }, 
  "items": [
    {
      "token": "1-0:1.8.0",
      "description": "Energy out",
      "vscp-class": 1040,
      "vscp-type": 13,
      "sensorindex": 0,
      "guid-lsb": 0,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
        "kWh": 1
      },
      "store": "energy_out"
    },
    {
      "token": "1-0:2.8.0",
      "description": "Energy in",
      "vscp-class": 1040,
      "vscp-type": 13,
      "sensorindex": 1,
      "guid-lsb": 1,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
        "kWh": 1
      },
      "store": "energy_in"
    },
    {
      "token": "1-0:3.8.0",
      "description": "Reactive energy out",
      "vscp-class": 1040,
      "vscp-type": 65,
      "sensorindex": 2,
      "guid-lsb": 2,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
        "kvarh": 0
      },
      "store": "reactive_energy_out"
    },
    {
      "token": "1-0:4.8.0",
      "description": "Reactive energy in",
      "vscp-class": 1040,
      "vscp-type": 65,
      "sensorindex": 3,
      "guid-lsb": 3,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
        "kvarh": 0
      },
      "store": "reactive_energy_in"
    },
    {
      "token": "1-0:1.7.0",
      "description": "Active effect out",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 4,
      "guid-lsb": 4,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_out"
    },
    {
      "token": "1-0:2.7.0",
      "description": "Active effect in",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 5,
      "guid-lsb": 5,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_in"
    },
    {
      "token": "1-0:3.7.0",
      "description": "Reactive effect out",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 6,
      "guid-lsb": 6,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_out"
    },
    {
      "token": "1-0:4.7.0",
      "description": "Reactive effect in",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 7,
      "guid-lsb": 7,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_in"
    },
    {
      "token": "1-0:21.7.0",
      "description": "Active effect out L1",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 8,
      "guid-lsb": 8,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_out_l1"
    },
    {
      "token": "1-0:41.7.0",
      "description": "Active effect out L2",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 9,
      "guid-lsb": 9,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_out_l2"
    },
    {
      "token": "1-0:61.7.0",
      "description": "Active effect out L3",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 10,
      "guid-lsb": 10,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_out_l3"
    },
    {
      "token": "1-0:22.7.0",
      "description": "Active effect in L1",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 11,
      "guid-lsb": 11,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_in_l1"
    },
    {
      "token": "1-0:42.7.0",
      "description": "Active effect in L2",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 12,
      "guid-lsb": 12,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_in_l1"
    },
    {
      "token": "1-0:62.7.0",
      "description": "Active effect in L3",
      "vscp-class": 1040,
      "vscp-type": 14,
      "sensorindex": 13,
      "guid-lsb": 13,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kW": 0
      },
      "store": "active_effect_in_l3"
    },
    {
      "token": "1-0:23.7.0",
      "description": "Reactive effect out L1",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 14,
      "guid-lsb": 14,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_out_l1"
    },
    {
      "token": "1-0:43.7.0",
      "description": "Reactive effect out L2",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 15,
      "guid-lsb": 15,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_out_l2"
    },
    {
      "token": "1-0:63.7.0",
      "description": "Reactive effect out L3",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 16,
      "guid-lsb": 16,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_out_l3"
    },
    {
      "token": "1-0:24.7.0",
      "description": "Reactive effect in L1",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 17,
      "guid-lsb": 17,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_in_l1"
    },
    {
      "token": "1-0:44.7.0",
      "description": "Reactive effect in L2",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 18,
      "guid-lsb": 18,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_in_l2"
    },
    {
      "token": "1-0:64.7.0",
      "description": "Reactive effect in L3",
      "vscp-class": 1040,
      "vscp-type": 64,
      "sensorindex": 19,
      "guid-lsb": 19,
      "zone": 0,
      "subzone": 0,
      "factor": 1000,
      "units": {
        "kvar": 0
      },
      "store": "reactive_effect_in_l3"
    },
    {
      "token": "1-0:32.7.0",
      "description": "Voltage L1",
      "vscp-class": 1040,
      "vscp-type": 16,
      "sensorindex": 20,
      "guid-lsb": 20,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
          "V": 0 
      },
      "store": "voltage_l1"
    },
    {
      "token": "1-0:52.7.0",
      "description": "Voltage L2",
      "vscp-class": 1040,
      "vscp-type": 16,
      "sensorindex": 21,
      "guid-lsb": 21,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
          "V": 0 
      },
      "store": "voltage_l2"
    },
    {
      "token": "1-0:72.7.0",
      "description": "Voltage L3",
      "vscp-class": 1040,
      "vscp-type": 16,
      "sensorindex": 22,
      "guid-lsb": 22,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
          "V": 0 
      },
      "store": "voltage_l3"
    },
    {
      "token": "1-0:31.7.0",
      "description": "Current L1",
      "vscp-class": 1040,
      "vscp-type": 5,
      "sensorindex": 23,
      "guid-lsb": 23,
      "zone": 0,
      "subzone": 5,
      "factor": 1,
      "units": {
          "A": 0
      },
      "store": "current_l1"
    },
    {
      "token": "1-0:51.7.0",
      "description": "Current L2",
      "vscp-class": 1040,
      "vscp-type": 5,
      "sensorindex": 24,
      "guid-lsb": 24,
      "izone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
          "A": 0
      },
      "store": "current_l2"
    },
    {
      "token": "1-0:71.7.0",
      "description": "Current L3",
      "vscp-class": 1040,
      "vscp-type": 5,
      "sensorindex": 25,
      "guid-lsb": 25,
      "zone": 0,
      "subzone": 0,
      "factor": 1,
      "units": {
          "A": 0
      },
      "store": "current_l3"
    }
  ],
  "alarms": [
    {
      "type":"on",
      "variable": "current_l3",
      "op": ">",
      "value": 1234,
      "one-shoot": false,
      "alarm-byte":1234,
      "zone": 1234,
      "subzone": 1234
    },
    {
      "type":"off",
      "variable": "current_l3",
      "op": "<",
      "value": 1234,
      "one-shoot": false,
      "alarm-byte":1234,
      "zone": 1234,
      "subzone": 1234
    }
  ],
  "alarms": [
    {
      "type":"on",
      "variable": "current_l1",
      "op": ">",
      "value": 5.0,
      "one-shot": false,
      "alarm-byte":99,
      "zone": 1,
      "subzone": 2
    },
    {
      "type":"off",
      "variable": "current_l1",
      "op": "<",
      "value": 4.9,
      "one-shot": false,
      "alarm-byte":88,
      "zone": 1,
      "subzone": 2
    }
  ]
}

```

Actual output data from the meter is like this

#### write
If write is true dynamic changes to the configuration file will be possible to save dynamically to disk. That is, settings you do at runtime can be saved and be persistent. The safest place for a configuration file is in the VSCP configuration folder */etc/vscp/* but for dynamic saves are not allowed if you don't run the VSCP daemon as root (which you should not). Next best place is to use the folder */var/lib/vscp/drivername/configure.xml*. This folder is created and a default configuration is written here when the driver is installed.

If you never intend to change driver parameters during runtime consider moving the configuration file to the VSCP daemon configuration folder.

#### debug
If debug is true the driver will output extra debug information. Normally just used during development.

#### Serial

The serial block specify the serial port to use. 

- **port**: The serial port to use. Best is to use an udev rule to create a virtual serial port here to prevent the driver from having to open the port every time it is started. But _/dev/ttyUSB0_ and similar is OK to.
- **baudrate**: The baud rate to use.
- **bits**: The number of bits per byte (7/8).
- **parity**: The parity to use (N=none, E=even, O=odd).
- **stopbits**: The stopbits to use (1/2).  
- **hwflowctrl**: Set to true to use  hardware flow control.
- **swflowctrl**: Set to true to use  software flow control.
- **dtr-on-start**: Set to true to turn on DTR on start.
#### file-log-level
Set to one of "off|critical|error|warn|info|debug|trace" for log level.

#### Logging

It is possible to log to a console or a log file or to both at the same time. Console logs will only be visible when the driver is run by a non-daemon host.
#### file-log-path" : "path to log file",
Set a writable path to a file that will get log information written to that file. This can be a valuable help if things does not behave as expected.

- "console-enable": true|false - Enable console logging.
- "console-log-level": "off|critical|error|warn|info|debug|trace" - Set log level for console logging.
- "console-pattern": "pattern" - Set log pattern for console logging.

- "file-log-enable": true|false - Enable file logging.
- "file-log-level": "off|critical|error|warn|info|debug|trace" - Set log level for file logging.
- "file-log-path": "path to log file" - Set a writable path to a file that will get log information written to that file. This can be a valuable help if things does not behave as expected.
- "file-log-pattern": "pattern" - Set log pattern for file logging.
- "file-log-max-size": "size" - Set max size for log file.
- "file-log-max-files": "number" - Set max number of log files.

#### items

Items is an array of elements. They specify the translation from the P1 protocol to the VSCP event data format.

Each element is a dictionary with the following keys:

- **token**: The token to look for in the P1 protocol.
- **description**: The description of the measurement.
- **vscp-class**: The VSCP class to use for the event that will be sent.
- **vscp-type**: The VSCP type to use for the event taht will be sent.
- **sensorindex**: The sensor index to use. This is used to identify the sensor in the VSCP event. One can use one common GUID for all measurements and then use the sensor index to identify the sensor in the VSCP event. Or change the lsb byte of the GUID for each measurement value or both.
- **guid-lsb**: The GUID LSB to use. See sensorindex above.
- **izone**: The zone to use.
- **subzone**: The subzone to use.
- **factor**: The factor to use. This is used to convert the value from the P1 protocol to the VSCP event data format.
- **units**: The units to use. This is used to convert the value from the P1 protocol to the VSCP event data format. For example a power measurement is in Watts in VSCP but often given in kilowatts. In that case the factor would be 1000.
- **units**: The units to use.
- **store**: The is a name of a variable to store the value in. This is used to store the value in a variable for later use (alarms).

#### alarms
Alarms is specified as an array of elements. They define the alarms that will be triggered when the value of the measurement changes and a condition is true. The alarm that will be sent for an active alarm  is [CLASS1.ALARM, VSCP_TYPE_ALARM_ALARM](https://grodansparadis.github.io/vscp-doc-spec/#/./class1.alarm?id=type2) and [CLASS1.ALARM,VSCP_TYPE_ALARM_RESET](https://grodansparadis.github.io/vscp-doc-spec/#/./class1.alarm?id=type13) is sent when the alarm condition no longer is valid.

- **type**: The type of alarm. Can be _on_ or _off_.
- **variable**: The variable to use (from store in items above).
- **op**: The operator to use. Can be >, <, >=, <=, ==, !=.
- **value**: The value to use for the compare.
- **one-shot**: Set to true to make the alarm one-shot. Default is to send the alarm on every report from the p1 device.
- **alarm-byte**: The alarm byte (byte 0) to use for the alarm event.
- **zone**: The zone to use for the alarm event.
- **subzone**: The subzone to use for the alarm event.

# FILES

*/var/lib/vscp/drivers/level2/libvscpl2drv-energy-p1.so*
:   The driver (Linux).

*resources/linux/energyp1.json*
:   Sample configuration in the source repository (installed with the driver).

# SEE ALSO

**vscpd**(8), **vscp-dump**(1), **vscp-send**(1), **vscpl2drv-socketcan**(1), **vscpl2drv-tcpipsrv**(1)

The source code and the full documentation: <https://github.com/grodansparadis/vscpl2drv-energy-p1>

The VSCP specification: <https://grodansparadis.github.io/vscp-doc-spec>

# COPYRIGHT

Copyright 2000-2026 Åke Hedman, Grodans Paradis AB. See the LICENSE file of the driver.
