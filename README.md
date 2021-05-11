# vscpl2drv-energy-p1

<img src="https://vscp.org/images/logo.png" width="100">

    Available for: Linux, Windows
    Driver Linux: vscpl2drv-energy-p1.so
    Driver Windows: vscpl2drv-energy-p1.dll

A driver that read data from a P1 energy meter and send relevant VSCP events.

## Install the driver on Linux
You can install the driver using the debian package with

> sudo apt install ./vscpl2drv-energy-p1_x.y.z.deb

the driver will be installed to /var/lib/vscp/drivers/level2

After installing the driver you need to add it to the vscpd.conf file (/etc/vscp/vscpd.conf). Se the *configuration* section below.

You also need to set up a configuration file for the driver. If you don't need to dynamically edit the content of this file a good and safe location for this is in the */etc/vscp/* folder alongside the VSCP daemon configuration file.

If you need to do dynamic configuration we recommend that you create the file in the */var/vscp/vscpl2drv-tcpipsrv.so*

A sample configuration file is make available in */usr/share/vscpl2drv-tcpipsrv.so* after installation.

## Install the driver on Windows
tbd

## How to build the driver on Linux

- sudo git clone https://github.com/grodansparadis/vscp.git
- sudo https://github.com/grodansparadis/vscpl2drv-tcpipsrv.git development
- sudo apt install pandoc           (comment: optional)
- sudo apt install build-essential
- sudo apt install cmake
- sudo apt install libexpat-dev
- sudo apt install libssl-dev
- sudo apt install rpm              (comment: only if you want to create install packages)
- cd vscpl2drv-tcpipsrv
- mkdir build
- cd build
- cmake ..
- make
- make install
- sudo cpack ..                     (comment: only if you want to create install packages)


Install of pandoc is only needed if man pages needs to be rebuilt. This is normally already done and available in the repository.




```
git clone --recurse-submodules -j8 https://github.com/grodansparadis/vscpl2drv-tcpipsrv.so.git
cd vscpl2drv-tcpipsrv
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

and the vscpl2drv-tcpipsrv code

```bash
git clone https://github.com/grodansparadis/vscpl2drv-tcpipsrv.git
```

If you check out both at the same directory level the *-DVSCP_PATH=path-vscp-repository* in next step is not needed.

### Build the driver

Build as usual but use

```bash
cd vscpl2drv-tcpipsrv
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

The VSCP daemon configuration is (normally) located at */etc/vscp/vscpd.conf*. To use the vscpl2drv-tcpipsrv.so driver there must be an entry in the

```
> <level2driver enable="true">
```

section on the following format

```xml
<!-- Level II TCP/IP Server -->
<driver enable="true"
    name="vscp-tcpip-srv"
    path-driver="/usr/lib/vscp/drivers/level2/vscpl2drv-tcpipsrv.so"
    path-config="/etc/vscp/vscpl2drv-tcpipsrv.conf"
    guid="FF:FF:FF:FF:FF:FF:FF:FC:88:99:AA:BB:CC:DD:EE:FF"
</driver>
```

##### enable
Set enable to "true" if the driver should be loaded.

##### name
This is the name of the driver. Used when referring to it in different interfaces.

##### path
This is the path to the driver. If you install from a Debian package this will be */usr/bin/vscpl2drv-tcpipsrv.so* and if you build and install the driver yourself it will be */usr/local/bin/vscpl2drv-tcpipsrv.so* or a custom location if you configured that.

##### guid
All level II drivers must have a unique GUID. There is many ways to obtain this GUID, Read more [here](https://grodansparadis.gitbooks.io/the-vscp-specification/vscp_globally_unique_identifiers.html).

#### vscpl2drv-tcpipsrv driver config

On start up the configuration is read from the path set in the driver configuration of the VSCP daemon, usually */etc/vscp/conf-file-name* and values are set from this location. If the **write** parameter is set to "true" the above location is a bad choice as the VSCP daemon will not be able to write to it. A better location is */var/lib/vscp/drivername/configure.xml* or some other writable location.

The configuration file have the following format

```json
{
    "write" : false,
    "interface": "[s]tcp://ip-address:port",
    "logging": { 
        "file-log-level": "off|critical|error|warn|info|debug|trace",
        "file-log-path" : "path to log file",
        "file-log-pattern": "Pattern for log file",
        "file-log-max-size": 50000,
        "file-log-max-files": 7,
    },    
    "auth-domain": "mydomain.com",
    "key-file": "/var/vscp/.vscp.key"
    "path-users" : "/etc/vscp/tcpip_srv_users.json",
    "response-timeout" : 0,
    "filter" : {
        "in-filter" : "incoming filter on string form",
        "in-mask" : "incoming mask on string form",
        "out-filter" : "outgoing filter on string form",
        "out-mask" : "outgoing mask on string form",
    },
    "tls": {
        "certificate" : "/srv/vscp/certs/tcpip_server.pem",
        "certificate_chain" : "",
        "verify_peer" : false,
        "ca-path" : "",
        "ca-file" : "",
        "verify_depth" : 9,
        "default-verify-paths" : true,
        "cipher-list" : "DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256",
        "protocol-version" : 3,
        "short-trust" : false
    }
}
```

##### file-log-level
Set to one of "off|critical|error|warn|info|debug|trace" for log level.

##### file-log-path" : "path to log file",
Set a writable path to a file that will get log information written to that file. This can be a valuable help if things does not behave as expected.

##### file-log-pattern
Pattern for log file rows.

##### file-log-max-size
Max size for log file before it will rotate and a new file is created. Default is 5 Mb.

##### file-log-max-files
Max number of log files to keep. Default is 7

##### write
If write is true dynamic changes to the configuration file will be possible to save dynamically to disk. That is, settings you do at runtime can be saved and be persistent. The safest place for a configuration file is in the VSCP configuration folder */etc/vscp/* but for dynamic saves are not allowed if you don't run the VSCP daemon as root (which you should not). Next best place is to use the folder */var/lib/vscp/drivername/configure.xml*. This folder is created and a default configuration is written here when the driver is installed.

If you never intend to change driver parameters during runtime consider moving the configuration file to the VSCP daemon configuration folder.

##### interface
Set the interface to listen on. Default is: *tcp://localhost:9598*. The interface is either secure (TLS) or insecure. It is not possible to define interfaces that accept connections of both types.

if "tcp:// part is omitted the content is treated as it was present.

If port is omitted, default 9598 is used.

For TLS/SSL use prefix "stcp://"

##### auth-domain
The authentication domain. In reality this is an arbitrary string that is used when calulating md5 checksums. In this case the checksum is calulated over "user:auth-domain-password"

##### path-users
The user database is separated from the configuration file for security reasons and should be stored in a folder that is only readable by the user of the host, usually the VSCP daemon.

The format for the user file is specified below.

##### response-timeout
Response timeout in milliseconds. Connection will be restarted if this expires.

##### encryption
Response and commands from/to the tcp/ip link server can be encrypted using AES-128, AES-192 or AES-256. Set here as

"none|aes128|aes192|aes256"

**Default**: is no encryption.

##### filter
Filter and mask is a way to select which events is received by the driver. A filter have the following format

> priority,vscpclass,vscptype,guid

All values can be give in decimal or hexadecimal (preceded number with '0x'). GUID is always given i hexadecimal (without preceded '0x').

**Default**: setting is

> 0,0,0,00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00

Read the [vscpd manual](http://grodansparadis.github.io/vscp/#/) for more information about how filter/masks work.

The default filter/mask pair means that all events are received by the driver.

##### mask
Filter and mask is a way to select which events is received by the driver. A mask have the following format

> priority,vscpclass,vscptype,guid

All values can be give in decimal or hexadecimal (preceded number with '0x'). GUID is always given i hexadecimal (without preceded '0x').

The mask have a binary one ('1') in the but position of the filter that should have a specific value and zero ('0') for a don't care bit.

Default setting is

> 0,0,0,00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00

Read the vscpd manual for more information about how filter/masks work.

The default filter/mask pair means that all events are received by the driver.

##### ssl_certificate
Path to SSL certificate file. This option is only required when at least one of the listening_ports is SSL The file must be in PEM format, and it must have both private key and certificate, see for example ssl_cert.pem. If this option is set, then the webserver serves SSL connections on the port set up to listen on.

**Default**: /srv/vscp/certs/server.pem

##### ssl_certificate_chain
T.B.D.

##### ssl_verify_peer
Enable client's certificate verification by the server.

**Default**: false

##### ssl_ca_path
Name of a directory containing trusted CA certificates for peers. Each file in the directory must contain only a single CA certificate. The files must be named by the subject name’s hash and an extension of “.0”. If there is more than one certificate with the same subject name they should have extensions ".0", ".1", ".2" and so on respectively.

##### ssl_ca_file"
Path to a .pem file containing trusted certificates for peers. The file may contain more than one certificate.

##### ssl_verify_depth
Sets maximum depth of certificate chain. If client's certificate chain is longer than the depth set here connection is refused.

**Default**: 9

##### ssl_default_verify_paths
Loads default trusted certificates locations set at openssl compile time.

**Default**: true

##### ssl_cipher_list
List of ciphers to present to the client. Entries should be separated by colons, commas or spaces.

| Selection	| Description |
| ========= | =========== |
| ALL |	All available ciphers |
| ALL:!eNULL | All ciphers excluding NULL ciphers |
| AES128:!MD5 | AES 128 with digests other than MD5 |

See [this entry in OpenSSL documentation](https://www.openssl.org/docs/manmaster/apps/ciphers.html) for full list of options and additional examples.

**Default**: "DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256",

##### ssl_protocol_version
Sets the minimal accepted version of SSL/TLS protocol according to the table:

| Selected protocols | setting |
| ================== | ======= |
| SSL2+SSL3+TLS1.0+TLS1.1+TLS1.2 | 0 |
| SSL3+TLS1.0+TLS1.1+TLS1.2 | 1 |
| TLS1.0+TLS1.1+TLS1.2 | 2 |
| TLS1.1+TLS1.2	| 3 |
| TLS1.2 | 4 |

**Default**: 4.

##### ssl_short_trust
Enables the use of short lived certificates. This will allow for the certificates and keys specified in ssl_certificate, ssl_ca_file and ssl_ca_path to be exchanged and reloaded while the server is running.

In an automated environment it is advised to first write the new pem file to a different filename and then to rename it to the configured pem file name to increase performance while swapping the certificate.

Disk IO performance can be improved when keeping the certificates and keys stored on a tmpfs (linux) on a system with very high throughput.

**Default**: false

 #### Format for user database

 ```json
 {
	"users" : [
		{
			"user" : "admin",
            "fullname" : "Full name",
            "note" : "note about user-item",
			"credentials"  : "fastpbkdf2 over 'user:password' using 256 bit system key (stored: item:iv)",
            "filter" : "outgoing filter",
			"rights" : "comma separated list of rights",
            "remotes" : "comma separated list of hosts First char: +=allow -deny",
			"events" : "comma separated list of events "TX|RX|BOTH;vscp-class;vscp.type;priority"
		}
	]
}
```

Any number of users can be specified

##### user
The login user name

##### credentials
fastpbkdf2 over "user:auth-domain:password" stored as 'pw:salt'

credentials = encrypted_pw:iv where the encrypted_pw
is calculated over md5(user:password)
encrypted_pw = aes256(user,password,iv) encrypted with key 
key = 256 bit, 32 byte number.
user send "password" (no :) or encrypted_pw:iv (: present)

##### rights
Rights for this user as a 32-bit rights number.

##### name
Full name for user.

 ##### events
 This is a list with events the user is allowed to send and/or receive. If empty all events can be sent and received by the users.

 ###### class
 VSCP class. Can be set to -1 to allow all classes.

 ###### type
 VSCP type. Can be set to -1 to allow all types. 

 ###### dir
 The direction the user is allowed to handle. Set to "rx" to allow only receive. Set to "tx" to allow only transmit. Set to "both" or empty to allow sending and receiving.

###### max-priority
Max priority (0-7) this user can use for send events. Trying to send an event with a higher priority will replace the event value with the value set here. Note that 0 is the hightst priority.

### Windows
See information from Linux. The only difference is the disk location from where configuration data is fetched.

## Using the vscpl2drv-tcpipsrv driver

