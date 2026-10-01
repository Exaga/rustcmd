# rustcmd

WebRCON command-line tool for the RustDedicated WebSocket interface.

## Contents

-   [What is rustcmd](#what-is-rustcmd)
-   [Download the source](#download-the-source)
-   [Build rustcmd](#build-rustcmd)
-   [Installation](#installation)
-   [Configuration](#configuration)
-   [Usage](#usage)
-   [Verbose error output](#verbose-error-output)
-   [Local service management](#local-service-management)
-   [Command reference](#command-reference)

###################### 

## What is `rustcmd`?

### TL;DR version

From this repository, users download, compile, install and configure 
`rustcmd` to run on their local or remotely hosted RustDedicated 
server(s). Easy-to-follow [README](README.md) documentation included.

### RTFA version

`rustcmd` is for owners and administrators of [Rust](https://rust.facepunch.com/) 
servers running locally or remotely. Its purpose is to offer an easy and 
convenient solution for managing a Rust server via the shell. 

**NOTE**: [Rust](https://rust.facepunch.com) is a multiplayer survival video 
game by [Facepunch Studios](https://facepunch.com/). Not to be confused with 
the Rust programming language. 

`rustcmd` is specifically designed around RustDedicated and its WebRCON 
interface. It's a standalone C++17 implementation using the standard 
library and POSIX sockets, with no third-party WebSocket or JSON library. 
The TCP connection, RFC 6455 handshake, SHA-1/Base64, WebSocket framing, 
masking and control-frame handling are implemented directly in `rustcmd` 
itself.

This is technically `rustcmd` \[MkII\], the C++ successor to the
original `rustcmd` that never saw a public release. The original tool
used the legacy Valve Source Engine RCON protocol. `rustcmd` uses
Facepunch's WebRCON protocol over standard WebSockets (RFC 6455).
Because, according to Facepunch, ["Websocket RCON is the
future."](https://rust.facepunch.com/news/devblog-99)

- `rustcmd` was designed to be Linux distribution-agnostic and operate
    on any Linux system capable of hosting a Rust server.

- `rustcmd` is released under the [MIT License](LICENSE).

## Why does `rustcmd` exist?

`rustcmd` was created after existing solutions were found to be unsuitable, 
relied on external dependencies, or lacked overall functionality. Something 
exceptionally fast, lightweight, and entirely command-line driven was needed. 
C++ was chosen as a high-performance native solution without any additional 
scripting runtimes or heavy external dependencies. After proving itself to be 
an impressive and effective standalone tool for managing a Rust server, 
additional features were added and `rustcmd` was made publicly available for 
other server owners and admins who might find it useful.

## What does `rustcmd` do?

`rustcmd` runs from the command-line and sends RustDedicated console
commands to the server through its WebRCON WebSocket interface and
prints the returned response. It's a standalone C++17 program using the
C++ standard library and POSIX sockets, and purposely avoids heavy,
external third-party network libraries. `rustcmd` is self-contained and
self-sufficient. It's very quick in operation, offering instant command
execution and immediate output.

-   `rustcmd` also provides four local service-management commands for
    starting, stopping, restarting, and checking the status of the Rust
    server service.
-   `rustcmd` can also be used for automating Rust server commands and
    tasks via `cron`.

## Download the source

Clone the `rustcmd` repository and enter the new directory:

``` bash
git clone https://github.com/Exaga/rustcmd
cd rustcmd
```

## Build rustcmd

`rustcmd` is distributed as C++ source and is intended to be compiled on the
Linux system where it will be used. Building locally produces a native 
executable against the target system's own C++ runtime and system libraries, 
avoiding the cross-distribution compatibility issues that can occur with 
pre-compiled Linux binaries.

A C++17 compiler is required. Build `rustcmd` with:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o rustcmd rustcmd.cpp
```

Before configuring it for installation, you can test the executable to
ensure it's working:

``` bash
./rustcmd --help
./rustcmd --version
./rustcmd --url
./rustcmd --license
```

These informational options do not require a configuration file, or a 
running Rust server with an active WebRCON interface, in order to work.

## Installation

Choose either a [user installation](#user-installation) or a
[system-wide installation](#system-wide-installation).

- If you are not sure which option is best for you, the
[user installation](#user-installation) is advised.

### User installation

A user installation keeps both the executable and configuration under
your home directory and does not require root access.

Create the local executable directory:

``` bash
mkdir -p ~/.local/bin
```

Copy `rustcmd` into it:

``` bash
cp rustcmd ~/.local/bin/rustcmd
chmod 755 ~/.local/bin/rustcmd
```

Create the configuration directory:

``` bash
mkdir -p ~/.config/rustcmd
```

Copy the supplied configuration file:

``` bash
cp rustcmd-cpp.conf ~/.config/rustcmd/rustcmd-cpp.conf
chmod 600 ~/.config/rustcmd/rustcmd-cpp.conf
```

Edit the configuration file:

``` bash
nano ~/.config/rustcmd/rustcmd-cpp.conf
```

and set the correct RCON parameters for your RustDedicated WebRCON
interface and the PATH to your Rust server service unit. For example:

``` ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

If `~/.local/bin` is already in your `$PATH`, the `rustcmd` installation
can now be checked with:

``` bash
rustcmd --version
```

If your shell cannot find `rustcmd`, check your current `$PATH`:

``` bash
echo "$PATH"
```

If `~/.local/bin` is not included, add it to your shell configuration:

``` bash
export PATH="$HOME/.local/bin:$PATH"
```

For Bash, add this line to `~/.bashrc` (or `~/.profile` for login
shells), then start a new shell or re-log in:

``` bash
source ~/.bashrc
```

### System-wide installation

A system-wide installation places the executable in `/usr/local/bin` and
the configuration in `/etc/rustcmd`.

Install the executable:

``` bash
sudo cp rustcmd /usr/local/bin/rustcmd
sudo chmod 755 /usr/local/bin/rustcmd
```

Create the configuration directory and install the supplied
configuration:

``` bash
sudo mkdir -p /etc/rustcmd
sudo cp rustcmd-cpp.conf /etc/rustcmd/rustcmd-cpp.conf
sudo chown root:root /etc/rustcmd/rustcmd-cpp.conf
sudo chmod 600 /etc/rustcmd/rustcmd-cpp.conf
```

Edit the configuration file:

``` bash
sudo nano /etc/rustcmd/rustcmd-cpp.conf
```

Set the RCON connection details and the PATH to your Rust server service
unit:

``` ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

Check the installed executable:

``` bash
rustcmd --version
```

With root:root ownership and mode 0600, the system-wide configuration is
readable only by root. If ordinary users need to use the machine-wide
configuration, assign an appropriate group and use mode 0640.

## Configuration

The following settings are used by `rustcmd`:

``` ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

`RUST_RCON_IP`, `RUST_RCON_PORT` and `RUST_RCON_PASSWORD` are required
for WebRCON commands.

`RUST_SERVICE_UNIT` is used by the local `rustcmd server ...` commands.
Normal WebRCON commands do not depend on service management.

The configuration contains the RCON password and should not be readable
by unauthorised users. Use mode 0600 for a root-only configuration, or
0640 when access is required by members of an appropriate group.

### Configuration lookup

An explicit configuration file takes priority:

``` bash
rustcmd --config /path/to/rustcmd-cpp.conf status
```

or:

``` bash
rustcmd -c /path/to/rustcmd-cpp.conf status
```

Without `-c` or `--config`, `rustcmd` checks for one configuration file
in this order:

1.  `$XDG_CONFIG_HOME/rustcmd/rustcmd-cpp.conf`
2.  `$HOME/.config/rustcmd/rustcmd-cpp.conf` when `XDG_CONFIG_HOME` is
    not set
3.  `/etc/rustcmd/rustcmd-cpp.conf`

Configuration files are not merged. One file only is selected and used.

### Multiple RustDedicated instances

If more than one RustDedicated server instance is running on the same
system, each instance should have its own `rustcmd` configuration file
containing the appropriate RCON connection details and service
configuration.

Only one `rustcmd` executable is required. The default
`rustcmd-cpp.conf` filename can be retained for a single-server
installation or the default server. For additional server instances, a
recommended naming convention is:

``` text
rustcmd-<identity>.conf
```

where `<identity>` corresponds to the RustDedicated `server.identity`
where practical. For example:

``` text
~/.config/rustcmd/
├── rustcmd-cpp.conf
├── rustcmd-vanilla.conf
├── rustcmd-modded.conf
└── rustcmd-staging.conf
```

Select the configuration for the RustDedicated instance you intend to
administer with `-c` or `--config`:

``` bash
rustcmd -c ~/.config/rustcmd/rustcmd-vanilla.conf status
rustcmd -c ~/.config/rustcmd/rustcmd-modded.conf status
```

The same applies to local service-management commands:

``` bash
rustcmd -c ~/.config/rustcmd/rustcmd-vanilla.conf server status
rustcmd -c ~/.config/rustcmd/rustcmd-modded.conf server status
```

The `rustcmd-<identity>.conf` filename is a recommended convention only;
`-c` and `--config` accept any valid configuration pathname.

`rustcmd` does not maintain named server profiles or automatically
select between multiple RustDedicated instances. Without `-c` or
`--config`, the normal configuration lookup order described above is
used.

## Usage

Once installed and configured, commands can be run simply as `rustcmd`.

Check the Rust server:

``` bash
rustcmd status
```

Request server information:

``` bash
rustcmd serverinfo
```

Structured JSON returned by `serverinfo` is printed in a readable
format.

Send a message to players:

``` bash
rustcmd say "Hello World!"
```

Save the server:

``` bash
rustcmd server.save
```

Other RustDedicated console commands can be supplied in the same way:

``` text
rustcmd <command> [arguments]
```

The command and its arguments are passed through WebRCON.

## Verbose error output

Normal connection failure error outputs are deliberately suppressed. Add
`--verbose` when you need the underlying socket or address-resolution
detail:

``` bash
rustcmd status --verbose
```

`--verbose` does not add output to a successful command.

## Local service management

Four commands are reserved for management of the local Rust server
service unit:

``` bash
rustcmd server start
rustcmd server stop
rustcmd server restart
rustcmd server status
```

These commands operate on the local service and are not sent through
WebRCON.

NB: The distinction between the two uses of status
**`<u>`{=html}is`</u>`{=html}** important:

``` bash
rustcmd status
```

-   checks and outputs the RustDedicated `status` through WebRCON.

``` bash
rustcmd server status
```

-   checks and outputs the state of the local operating-system service.

### systemd

For a systemd installation, configure the full service unit PATH:

``` ini
RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

`rustcmd` uses `systemctl` for start, stop and restart, and
`systemctl is-active` for `server status`.

systemd service operations are executed through `sudo`. The user running
`rustcmd` must already have the appropriate sudo permission.

### BSD-style and SysV init scripts

For a rc-style installation, configure the full service-script PATH:

``` ini
RUST_SERVICE_UNIT=/etc/rc.d/rc.rustserver
```

`rustcmd` executes the configured script directly with `start`, `stop`,
`restart` or `status`.

It does not use `sudo`, change permissions, or attempt to obtain
privileges for an `/etc/rc.d/` script. Service-script permissions remain
the responsibility of the system administrator (i.e. *YOU*).

NB: A configured service PATH that is neither recognised as a systemd
unit PATH nor an `/etc/rc.d/` script is rejected rather than guessed.

## Command reference

The commands listed in this section are common examples along with the 
options and local service-management commands provided by `rustcmd` itself. 
`rustcmd` is not limited to the commands shown below. Any RCON command 
supported by the Rust server can be used with `rustcmd`.

General command syntax:

``` text
rustcmd <command> [arguments]
rustcmd [options]
```

Common WebRCON commands:

``` bash
rustcmd status
rustcmd status --verbose
rustcmd serverinfo
rustcmd say "Hello World!"
rustcmd server.save
```

`rustcmd` is not limited to those examples. Other RustDedicated console
commands are passed through WebRCON.

Local service commands:

``` bash
rustcmd server start
rustcmd server stop
rustcmd server restart
rustcmd server status
```

Options:

``` text
-h, -?, --help          Show this help
-v, --version           Show version information
-U, --url               Show official rustcmd project URL
-L, --license           Show license information
-c, --config <file>     Use an alternative configuration file
<command> --verbose     Show detailed errors for the command
```

`-v` means version. Verbose command diagnostics use `--verbose`.

Running `rustcmd --verbose` without a command displays the help text.

## Project URL

The official project URL is compiled into `rustcmd` and can be displayed
without a configuration file or network connection:

``` bash
rustcmd --url
```

The project is maintained at:

`https://github.com/Exaga/rustcmd`

## Changelog

The development history and version changes are recorded in
[CHANGELOG.md](CHANGELOG.md).

## License

`rustcmd` is released under the [MIT License](LICENSE). The complete
license text is included in the source and can also be displayed with:

``` bash
rustcmd --license
```

Copyright © 2026 Exaga - penthux.net

\#EOF\<\*\>
