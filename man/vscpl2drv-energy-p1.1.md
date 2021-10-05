% vscpl2drv-energy-p1(1) VSCP Level II energy p1 Driver
% Åke Hedmann, the VSCP Project
% April 07, 2021

# NAME

vscpl2drv-energy-p1 - VSCP Level II energy p1 driver

# SYNOPSIS

vscpl2drv-energy-p1

# DESCRIPTION

A driver that read data from a [DSMR V5.0.2 P1](https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf) energy meter and translate metering to relevant VSCP events.

# CONFIGURATION

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

See _/usr/local/share/vscpl2drv-energy-p1_ for a sample configuration file. Also available on github at https://github.com/grodansparadis/vscpl2drv-energy-p1/blob/main/resources/linux/energyp1.json
# SEE ALSO

`vscpd` (8).
`uvscpd` (8).
`vscpworks` (1).
`vscpcmd` (1).
`vscp-makepassword` (1).
`vscphelperlib` (1).

The VSCP project homepage is here <https://www.vscp.org>.

The [manual](https://grodansparadis.gitbooks.io/the-vscp-daemon) for vscpd contains full documentation. Other documentation can be found here <https://grodansparadis.gitbooks.io>.

The vscpd source code may be downloaded from <https://github.com/grodansparadis/vscp>. Source code for other system components of VSCP & Friends are here <https://github.com/grodansparadis>

# COPYRIGHT
Copyright 2000-2021 Åke Hedman, the VSCP Project - MIT license.




