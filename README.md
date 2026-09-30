# rustcmd

`rustcmd` C++ WebRCON command-line tool for (RustDedicated) WebSocket interface.

## What is `rustcmd`?

`rustcmd` is a command-line (CLI) tool for Rust (survival game) servers. Its purpose is to be an easy and convenient solution for managing a Rust server via the shell. `rustcmd` can handle server starts, stops, restarts, output the server status, and use all the commands you'd generally expect to be able to use on a RCON GUI browser interface in connection with an active Rust game server.

This is technically `rustcmd` [MkII], the C++ successor to the original `rustcmd`. The original tool used the legacy Valve Source Engine RCON protocol. `rustcmd` now uses the newer WebSocket method. Because, according to Facepunch, ["Websocket RCON is the future."](https://rust.facepunch.com/news/devblog-99)

`rustcmd` was designed to be Linux-agnostic and operate on any Linux distribution capable of hosting a Rust server. Not to be confused with the Rust programming language, this tool is for use with [Rust survival game.](https://rust.facepunch.com)

## What does `rustcmd` do?

`rustcmd` runs from the command-line and sends RustDedicated console commands to the server through its WebRCON WebSocket interface and prints the returned response. It's a standalone C++17 program using the C++ standard library and POSIX sockets, with no Python runtime or third-party WebSocket library required. `rustcmd` is self-contained and self-sufficient. It's very quick in operation offering instant returns on command responses and returned output. 

- `rustcmd` also provides four local service-management commands for starting, stopping, restarting, and checking the status of the Rust server service. 
-`rustcmd` can also be used for automating Rust server commands and tasks via `cron`.

## Download the source

Clone the repository and enter the new directory:

```bash
git clone https://github.com/Exaga/rustcmd
cd rustcmd
```

## Build rustcmd

A C++17 compiler is required. Build `rustcmd` with:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o rustcmd rustcmd.cpp
```

Before installing it, you can check the executable directly from the repository:

```bash
./rustcmd --help
./rustcmd --version
./rustcmd --url
./rustcmd --license
```

These informational options do not require a configuration file or a running Rust server.

## Installation

Choose either a user installation or a system-wide installation.

### User installation

A user installation keeps both the executable and configuration under your home directory and does not require root access.

Create the local executable directory:

```bash
mkdir -p ~/.local/bin
```

Copy `rustcmd` into it:

```bash
cp rustcmd ~/.local/bin/rustcmd
chmod 755 ~/.local/bin/rustcmd
```

Create the configuration directory:

```bash
mkdir -p ~/.config/rustcmd
```

Copy the supplied configuration file:

```bash
cp rustcmd-cpp.conf ~/.config/rustcmd/rustcmd-cpp.conf
chmod 600 ~/.config/rustcmd/rustcmd-cpp.conf
```

Edit:

```text
~/.config/rustcmd/rustcmd-cpp.conf
```

and set the connection details for your RustDedicated WebRCON interface:

```ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

If `~/.local/bin` is already in your `PATH`, the installation can now be checked with:

```bash
rustcmd --version
```

If your shell cannot find `rustcmd`, check your current `PATH`:

```bash
echo "$PATH"
```

and add `~/.local/bin` using the normal method for your shell or Linux distribution.

### System-wide installation

A system-wide installation places the executable in `/usr/local/bin` and the configuration in `/etc/rustcmd`.

Install the executable:

```bash
sudo cp rustcmd /usr/local/bin/rustcmd
sudo chmod 755 /usr/local/bin/rustcmd
```

Create the configuration directory and install the supplied configuration:

```bash
sudo mkdir -p /etc/rustcmd
sudo cp rustcmd-cpp.conf /etc/rustcmd/rustcmd-cpp.conf
sudo chown root:root /etc/rustcmd/rustcmd-cpp.conf
sudo chmod 600 /etc/rustcmd/rustcmd-cpp.conf
```

Edit:

```text
/etc/rustcmd/rustcmd-cpp.conf
```

and set the connection details:

```ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

Check the installed executable:

```bash
rustcmd --version
```

With `root:root` ownership and mode `0600`, the system-wide configuration is readable only by root. If ordinary users need to use the machine-wide configuration, the system administrator must choose suitable group ownership and permissions.

## Configuration

The following settings are used by `rustcmd`:

```ini
RUST_RCON_IP=127.0.0.1
RUST_RCON_PORT=28016
RUST_RCON_PASSWORD=your-RCON-password

RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

`RUST_RCON_IP`, `RUST_RCON_PORT` and `RUST_RCON_PASSWORD` are required for WebRCON commands.

`RUST_SERVICE_UNIT` is used by the local `rustcmd server ...` commands. Normal WebRCON commands do not depend on service management.

Protect any configuration file containing the RCON password appropriately.

### Configuration lookup

An explicit configuration file takes priority:

```bash
rustcmd --config /path/to/rustcmd-cpp.conf status
```

or:

```bash
rustcmd -c /path/to/rustcmd-cpp.conf status
```

Without `-c` or `--config`, `rustcmd` checks for one configuration file in this order:

1. `$XDG_CONFIG_HOME/rustcmd/rustcmd-cpp.conf`
2. `$HOME/.config/rustcmd/rustcmd-cpp.conf` when `XDG_CONFIG_HOME` is not set
3. `/etc/rustcmd/rustcmd-cpp.conf`

Configuration files are not merged. One file is selected and used.

## Using rustcmd

Once installed and configured, commands can be run simply as `rustcmd`.

Check the Rust server:

```bash
rustcmd status
```

Request server information:

```bash
rustcmd serverinfo
```

Structured JSON returned by `serverinfo` is printed in a readable format.

Send a message to players:

```bash
rustcmd say "Hello World!"
```

Save the server:

```bash
rustcmd server.save
```

Other RustDedicated console commands can be supplied in the same way:

```text
rustcmd <command> [arguments]
```

The command and its arguments are passed through WebRCON.

## Verbose errors

Normal connection failures are deliberately concise. Add `--verbose` when you need the underlying socket or address-resolution detail:

```bash
rustcmd status --verbose
```

`--verbose` does not add output to a successful command.

## Local service management

Four commands are reserved for management of the local Rust server service:

```bash
rustcmd server start
rustcmd server stop
rustcmd server restart
rustcmd server status
```

These commands operate on the local service and are not sent through WebRCON.

The distinction between the two status commands is important:

```bash
rustcmd status
```

sends the RustDedicated `status` command through WebRCON.

```bash
rustcmd server status
```

checks the state of the local operating-system service.

### systemd

For a systemd installation, configure the full service-unit path:

```ini
RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
```

`rustcmd` uses `systemctl` for start, stop and restart, and `systemctl is-active` for `server status`.

systemd service operations are executed through `sudo`. The user running `rustcmd` must already have the appropriate sudo permission.

### `/etc/rc.d` service scripts

For an rc-style installation, configure the full service-script path:

```ini
RUST_SERVICE_UNIT=/etc/rc.d/rc.rustserver
```

`rustcmd` executes the configured script directly with `start`, `stop`, `restart` or `status`.

It does not use `sudo`, change permissions or attempt to obtain privileges for an `/etc/rc.d/` script. Service-script permissions remain the responsibility of the system administrator.

A configured service path that is neither recognised as a systemd unit path nor an `/etc/rc.d/` script is rejected rather than guessed.

## Command reference

General command forms:

```text
rustcmd <command> [arguments]
rustcmd [options]
```

Common WebRCON commands:

```text
rustcmd status
rustcmd status --verbose
rustcmd serverinfo
rustcmd say "Hello World!"
rustcmd server.save
```

`rustcmd` is not limited to those examples. Other RustDedicated console commands are passed through WebRCON.

Local service commands:

```text
rustcmd server start
rustcmd server stop
rustcmd server restart
rustcmd server status
```

Options:

```text
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

The official project URL is compiled into `rustcmd` and can be displayed without a configuration file or network connection:

```bash
rustcmd --url
```

The project is maintained at:

`https://github.com/Exaga/rustcmd`

## Changelog

The development history and version changes are recorded in `CHANGELOG.md`.

## License

`rustcmd` is released under the MIT License. The complete license text is included in the source and can also be displayed with:

```bash
rustcmd --license
```

Copyright © 2026 Exaga - penthux.net

\#EOF<*>
