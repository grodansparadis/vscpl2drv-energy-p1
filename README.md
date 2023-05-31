# vscpl2drv-energy-p1

<img src="https://vscp.org/images/logo.png" width="100">

    Available for: Linux, Windows
    Driver Linux: vscpl2drv-energy-p1.so
    Driver Windows: vscpl2drv-energy-p1.dll

A driver that read data from a [DSMR V5.0.2 P1](https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf) energy meter and translate metering to relevant VSCP events.

[Live data from this driver](https://grodansparadis.com/wordpress/wp-admin/post.php?post=5276&action=edit) using an Ellevio electric meter.

## Install the driver on Linux
You can install the driver using the debian package with

> sudo apt install ./vscpl2drv-energy-p1_x.y.z.deb

the driver will be installed to /var/lib/vscp/drivers/level2

After installing the driver you need to add it to the vscpd.conf file (/etc/vscp/vscpd.conf). Se the *configuration* section below.

You also need to set up a configuration file for the driver. If you don't need to dynamically edit the content of this file a good and safe location for this is in the */etc/vscp/* folder alongside the VSCP daemon configuration file.

If you need to do dynamic configuration we recommend that you create the file in the */var/vscp/vscpl2drv-energy-p1.so*

A sample configuration file is make available in */usr/share/vscpl2drv-energy-p1.so* after installation.

## Install the driver on Windows
tbd

## How to build the driver on Linux

- sudo git clone https://github.com/grodansparadis/vscp.git
- sudo https://github.com/grodansparadis/vscpl2drv-energy-p1.git development
- sudo apt install pandoc           (comment: optional)
- sudo apt install build-essential
- sudo apt install cmake
- sudo apt install libexpat-dev
- sudo apt install libssl-dev
- sudo apt install libcurl4-openssl-dev
- sudo apt install rpm              (comment: only if you want to create install packages)
- cd vscpl2drv-energy-p1
- mkdir build
- cd build
- cmake ..
- make
- make install
- sudo cpack ..                     (comment: only if you want to create install packages)


Install of pandoc is only needed if man pages needs to be rebuilt. This is normally already done and available in the repository.

```
git clone --recurse-submodules -j8 https://github.com/grodansparadis/vscpl2drv-energy-p1.git
cd vscpl2drv-energy-p1
./configure
make
make install
```

Default install folder when you build from source is */usr/local/lib*. You can change this with the --prefix option in the configure step. For example *--prefix /usr* to install to */usr/lib* as the debian install

You need build-essentials and git installed on your system

>sudo apt update && sudo apt -y upgrade
>sudo apt install build-essential git

## How to build the driver on Windows

### Install the vcpkg package manager

You need the vcpkg package manager on windows. Install it with

```bash
git clone https://github.com/microsoft/vcpkg.git
```

then go into the folder

```bash
cd vcpkg
```

Run the vcpkg bootstrapper command

```bash
bootstrap-vcpkg.bat
```

The process is described in detail [here](https://docs.microsoft.com/en-us/cpp/build/install-vcpkg?view=msvc-160&tabs=windows)

To [integrate with Visual Studio](https://docs.microsoft.com/en-us/cpp/build/integrate-vcpkg?view=msvc-160) run

```bash
vcpkg integrate install
```

Install the required libs

```bash
vcpkg install pthread:x64-windows
vcpkg install expat:x64-windows
vcpkg install openssl:x64-windows
```

Full usage is describe [here](https://docs.microsoft.com/en-us/cpp/build/manage-libraries-with-vcpkg?view=msvc-160&tabs=windows)

### Get the source

You need to checkout the VSCP main repository code in addition to the driver repository. You do this with

```bash
  git clone https://github.com/grodansparadis/vscp.git
  cd vscp
  git checkout development
``` 

and the vscpl2drv-energy-p1 code

```bash
git clone https://github.com/grodansparadis/vscpl2drv-energy-p1.git
```

If you check out both at the same directory level the *-DVSCP_PATH=path-vscp-repository* in next step is not needed.

### Build the driver

Build as usual but use

```bash
cd vscpl2drv-energy-p1
mkdir build
cd build
cmake .. -CMAKE_BUILD_TYPE=Release|Debug -DCMAKE_TOOLCHAIN_FILE=E:\src\vcpkg\scripts\buildsystems\vcpkg.cmake -DVSCP_PATH=path-vscp-repository
```

The **CMAKE_TOOLCHAIN_FILE** path may be different in your case

Note that *Release|Debug* should be either *Release* or *Debug*

The windows build files can now be found in the build folder and all needed files to run the project can  after build - be found in build/release or build/Debug depending on CMAKE_BUILD_TYPE setting.

Building and configuration is simplified with VS Code installed. Configure/build/run can be done (se lower toolbar). Using VS Code it ,ay be useful to add

```json
"cmake.configureSettings": {
   "CMAKE_BUILD_TYPE": "${buildType}"
}
``` 

to your settings.json file.

To build at the command prompt use

```bash
msbuild vscp-works-qt.sln
```

Note that you must have a *developer command prompt*

### Build deploy packages 

Install NSIS from [this site](https://sourceforge.net/projects/nsis/).

Run 

```bash
cpack ...
```
 
in the build folder.



## Configuration

### Linux

#### VSCP daemon driver config

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

##### enable
Set enable to "true" if the driver should be loaded.

##### name
This is the name of the driver. Used when referring to it in different interfaces.

##### path
This is the path to the driver. If you install from a Debian package this will be */usr/bin/vscpl2drv-energy-p1.so* and if you build and install the driver yourself it will be */usr/local/bin/vscpl2drv-energy-p1.so* or a custom location if you configured that.

##### guid
All level II drivers must have a unique GUID. There is many ways to obtain this GUID, Read more [here](https://grodansparadis.gitbooks.io/the-vscp-specification/vscp_globally_unique_identifiers.html).

#### vscpl2drv-energy-p1 driver config

On start up the configuration is read from the path set in the driver configuration of the VSCP daemon, usually */etc/vscp/conf-file-name* and values are set from this location. If the **write** parameter is set to "true" the above location is a bad choice as the VSCP daemon will not be able to write to it. A better location is */var/lib/vscp/drivername/configure.xml* or some other writable location.

The default configuration file available in the [resources folder](https://github.com/grodansparadis/vscpl2drv-energy-p1/blob/main/resources/linux/energyp1.json) is made for the [Ellevio electrical meters](https://grodansparadis.com/wordpress/wp-admin/post.php?post=5039&action=edit) and have the following format. You can easily adopt this file to your meters format. Live data is [here](https://grodansparadis.com/wordpress/wp-admin/post.php?post=5276&action=edit).

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

![](https://i2.wp.com/www.akehedman.se/wordpress/wp-content/uploads/2021/04/Screenshot-from-2021-04-21-21-25-34.png?w=580&ssl=1)

##### write
If write is true dynamic changes to the configuration file will be possible to save dynamically to disk. That is, settings you do at runtime can be saved and be persistent. The safest place for a configuration file is in the VSCP configuration folder */etc/vscp/* but for dynamic saves are not allowed if you don't run the VSCP daemon as root (which you should not). Next best place is to use the folder */var/lib/vscp/drivername/configure.xml*. This folder is created and a default configuration is written here when the driver is installed.

If you never intend to change driver parameters during runtime consider moving the configuration file to the VSCP daemon configuration folder.

##### debug
If debug is true the driver will output extra debug information. Normally just used during development.

##### Serial

The serial block specify the serial port to use. 

- **port**: The serial port to use. Best is to use an udev rule to create a virtual serial port here to prevent the driver from having to open the port every time it is started. But _/dev/ttyUSB0_ and similar is OK to.
- **baudrate**: The baud rate to use.
- **bits**: The number of bits per byte (7/8).
- **parity**: The parity to use (N=none, E=even, O=odd).
- **stopbits**: The stopbits to use (1/2).  
- **hwflowctrl**: Set to true to use  hardware flow control.
- **swflowctrl**: Set to true to use  software flow control.
- **dtr-on-start**: Set to true to turn on DTR on start.
##### file-log-level
Set to one of "off|critical|error|warn|info|debug|trace" for log level.

##### Logging

It is possible to log to a console or a log file or to both at the same time. Console logs will only be visible when the driver is run by a non-daemon host.
##### file-log-path" : "path to log file",
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

##### items

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

##### alarms
Alarms is specified as an array of elements. They define the alarms that will be triggered when the value of the measurement changes and a condition is true. The alarm that will be sent for an active alarm  is [CLASS1.ALARM, VSCP_TYPE_ALARM_ALARM](https://grodansparadis.github.io/vscp-doc-spec/#/./class1.alarm?id=type2) and [CLASS1.ALARM,VSCP_TYPE_ALARM_RESET](https://grodansparadis.github.io/vscp-doc-spec/#/./class1.alarm?id=type13) is sent when the alarm condition no longer is valid.

- **type**: The type of alarm. Can be _on_ or _off_.
- **variable**: The variable to use (from store in items above).
- **op**: The operator to use. Can be >, <, >=, <=, ==, !=.
- **value**: The value to use for the compare.
- **one-shot**: Set to true to make the alarm one-shot. Default is to send the alarm on every report from the p1 device.
- **alarm-byte**: The alarm byte (byte 0) to use for the alarm event.
- **zone**: The zone to use for the alarm event.
- **subzone**: The subzone to use for the alarm event.

## Using the vscpl2drv-energy-p1 driver

A video is here for metering in Belgium https://www.youtube.com/watch?v=6omi6Kms-ns that will give a good overview that is valid for other countries also. You can even use Tasmota for this https://tasmota.github.io/docs/P1-Smart-Meter/. However note there are some differences between meters.

I have a write up [here](https://grodansparadis.com/wordpress/wp-admin/post.php?post=5039&action=edit) about our setup. 

Below is a node-red flow that shows gauges and a diagrams for the active effect for the three phases L1,L2 and L3.

```
[{"id":"292e3baf010b82a6","type":"mqtt in","z":"38061fa379e39e86","name":"Active effect out L1","topic":"vscp/25:00:00:00:00:00:00:00:00:00:00:00:08:0D:00:08/1040/14/8/8","qos":"2","datatype":"json","broker":"5438645a.6577cc","nl":false,"rap":true,"rh":0,"x":170,"y":400,"wires":[["37063deaf5482572","d3e6b0d664bcc542","3d525d7c9397cf8d"]]},{"id":"0b886f663d86c6f2","type":"ui_gauge","z":"38061fa379e39e86","name":"Active effect out  L1","group":"b34740c6.f1cb58","order":9,"width":"6","height":"5","gtype":"gage","title":"Active effect out  L1","label":"kW","format":"{{value}}","min":0,"max":"6","colors":["#00b500","#e6e600","#ca3838"],"seg1":"3","seg2":"5","x":570,"y":360,"wires":[]},{"id":"37063deaf5482572","type":"function","z":"38061fa379e39e86","name":"","func":"msg.value =  parseFloat(msg.payload.measurement.value/1000).toFixed(3);\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":360,"y":360,"wires":[["0b886f663d86c6f2"]]},{"id":"328bfc97ab4e3d72","type":"mqtt in","z":"38061fa379e39e86","name":"Active effect out  L2","topic":"vscp/25:00:00:00:00:00:00:00:00:00:00:00:08:0D:00:09/1040/14/9/9","qos":"2","datatype":"json","broker":"5438645a.6577cc","nl":false,"rap":true,"rh":0,"x":170,"y":520,"wires":[["dc7e9186723fd52a","c3c14d12c8080ee6","3ad6ce749f939126"]]},{"id":"1128ef7e42860be3","type":"ui_gauge","z":"38061fa379e39e86","name":"Active effect out  L2","group":"b34740c6.f1cb58","order":10,"width":"6","height":"5","gtype":"gage","title":"Active effect out  L2","label":"kW","format":"{{value}}","min":0,"max":"6","colors":["#00b500","#e6e600","#ca3838"],"seg1":"3","seg2":"5","x":570,"y":480,"wires":[]},{"id":"dc7e9186723fd52a","type":"function","z":"38061fa379e39e86","name":"","func":"msg.value =  parseFloat(msg.payload.measurement.value/1000).toFixed(3);\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":360,"y":480,"wires":[["1128ef7e42860be3"]]},{"id":"b1d492e24341b1eb","type":"mqtt in","z":"38061fa379e39e86","name":"Active effect out  L3","topic":"vscp/25:00:00:00:00:00:00:00:00:00:00:00:08:0D:00:0A/1040/14/10/10","qos":"2","datatype":"json","broker":"5438645a.6577cc","nl":false,"rap":true,"rh":0,"x":170,"y":640,"wires":[["7980eecebf80feba","52b0bca3cdf4d636","59240d1a05ef7918"]]},{"id":"ddb538fd44202e42","type":"ui_gauge","z":"38061fa379e39e86","name":"Active effect out  L3","group":"b34740c6.f1cb58","order":11,"width":"6","height":"5","gtype":"gage","title":"Active effect out  L3","label":"kW","format":"{{value}}","min":0,"max":"6","colors":["#00b500","#e6e600","#ca3838"],"seg1":"3","seg2":"5","x":570,"y":680,"wires":[]},{"id":"7980eecebf80feba","type":"function","z":"38061fa379e39e86","name":"","func":"msg.value =  parseFloat(msg.payload.measurement.value/1000).toFixed(3);\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":340,"y":680,"wires":[["ddb538fd44202e42"]]},{"id":"06c98721a4cf06f9","type":"comment","z":"38061fa379e39e86","name":"Active effect phases","info":"","x":170,"y":340,"wires":[]},{"id":"ab50bc763ae94086","type":"ui_chart","z":"38061fa379e39e86","name":"Active effect out","group":"b34740c6.f1cb58","order":27,"width":0,"height":0,"label":"Active effect out","chartType":"line","legend":"true","xformat":"HH:mm","interpolate":"linear","nodata":"No data","dot":true,"ymin":"0","ymax":"10","removeOlder":"4","removeOlderPoints":"","removeOlderUnit":"3600","cutout":0,"useOneColor":false,"useUTC":false,"colors":["#2ca02c","#1f77b4","#ec1334","#ff7f0e","#98df8a","#68c9f3","#ff9896","#9467bd","#c5b0d5"],"outputs":1,"useDifferentColor":false,"x":560,"y":580,"wires":[[]]},{"id":"d3e6b0d664bcc542","type":"function","z":"38061fa379e39e86","name":"","func":"msg.payload =  msg.payload.measurement.value/1000;\nmsg.topic = \"Active effect out L1\";\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":360,"y":440,"wires":[["ab50bc763ae94086"]]},{"id":"c3c14d12c8080ee6","type":"function","z":"38061fa379e39e86","name":"","func":"msg.payload =  msg.payload.measurement.value/1000;\nmsg.topic = \"Active effect out L2\";\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":360,"y":560,"wires":[["ab50bc763ae94086"]]},{"id":"52b0bca3cdf4d636","type":"function","z":"38061fa379e39e86","name":"","func":"msg.payload =  msg.payload.measurement.value/1000;\nmsg.topic = \"Active effect out L3\";\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":340,"y":720,"wires":[["ab50bc763ae94086"]]},{"id":"69236a372fd34c50","type":"link in","z":"38061fa379e39e86","name":"Active effect out total","links":["8cf9f8cf50e0f63a"],"x":395,"y":600,"wires":[["ab50bc763ae94086"]]},{"id":"5438645a.6577cc","type":"mqtt-broker","name":"Local","broker":"localhost","port":"1883","clientid":"lynx","usetls":false,"compatmode":false,"protocolVersion":"4","keepalive":"60","cleansession":true,"birthTopic":"","birthQos":"0","birthPayload":"","birthMsg":{},"closeTopic":"","closeQos":"0","closePayload":"","closeMsg":{},"willTopic":"","willQos":"0","willPayload":"","willMsg":{},"sessionExpiry":""},{"id":"b34740c6.f1cb58","type":"ui_group","name":"Power Brattberg house","tab":"c70db629dd6e0ec2","order":1,"disp":true,"width":"18","collapse":false},{"id":"c70db629dd6e0ec2","type":"ui_tab","name":"Power","icon":"dashboard","disabled":false,"hidden":false}]
```

A live demo of data from our office is [here](https://demo.vscp.org/mqtt/power.html).
