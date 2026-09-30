# Changelog

Development history on `rustcmd` C++ WebRCON tool for (RustDedicated) WebSocket interface

This is `rustcmd` MkII, following in the footsteps of the former `rustcmd` tool
that used the legacy Valve Source Engine RCON protocol. This `rustcmd` tool is
written in C++ and uses the newer `WebSocket` method. Because,  according to 
Facepunch ["Websocket RCON is the future."](https://rust.facepunch.com/news/devblog-99)

Version numbering used by this project:

-   `MAJOR` - fundamental redesign or substantial change in the purpose of the
    program.
-   `MINOR` - new functionality or a new functional subsystem.
-   `REVISION` - an individual command, option, output change, protocol step, 
    correction or incremental addition.
-   A fourth sub-revision is reserved for trivial corrections such as typos or
    rewording text and is not used in the development history below.

Dates record the development day. Release times can be added later.

## 2026-09-15 22:22:06 - v0.0.1

-   Created the initial C++17 `rustcmd` source.
-   Established the standalone C++ replacement for the original Python
    WebRCON tool.
-   Added basic command-line handling and configuration parsing.

## 2026-09-16 08:16:22 - v1.0.0

-   First complete C++ development baseline compiled successfully with:
    `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o rustcmd rustcmd.cpp`
-   Added command assembly from command-line arguments.
-   Added XDG user configuration discovery.
-   Added `$HOME/.config/rustcmd/rustcmd-cpp.conf` fallback.
-   Added `/etc/rustcmd/rustcmd-cpp.conf` system configuration fallback.
-   Added `-c` / `--config` for an explicit configuration file.
-   Added validation of the required RCON IP address, port and password
    settings.

## 2026-09-16 08:58:14 - v1.1.0

-   Added WebRCON JSON functionality.
-   Added WebRCON JSON payload construction.
-   Added JSON string escaping for commands sent to RustDedicated.
-   Preserved the WebRCON payload format used by the original Python
    tool.

## 2026-09-16 16:48:54 - v1.2.0

-   Began the native RFC 6455 WebSocket implementation.
-   Added native SHA-1 support for the WebSocket handshake.
-   Added native Base64 encoding.
-   Verified SHA-1 and Base64 against known WebSocket test vectors.
-   No external cryptography library required.

## 2026-09-17 07:16:37 - v1.2.1

-   Added POSIX TCP connection handling.
-   Added non-blocking connection establishment with a bounded
    connection timeout.
-   Added socket send and receive timeouts.
-   Added concise connection failure reporting.

## 2026-09-17 09:44:12 - v1.2.2

-   Added the RFC 6455 HTTP WebSocket Upgrade handshake.
-   Added random `Sec-WebSocket-Key` generation.
-   Added `Sec-WebSocket-Accept` calculation and validation.
-   Added validation of HTTP 101, `Upgrade` and `Connection` response
    headers.
-   Successfully completed a WebSocket handshake against a live
    RustDedicated server.

## 2026-09-17 11:18:57 - v1.2.3

-   Added masked WebSocket text framing.
-   Added normal, 16-bit and 64-bit WebSocket payload lengths.
-   Successfully transmitted WebRCON command frames to RustDedicated.

## 2026-09-18 15:04:34 - v1.2.4

-   Added WebSocket response-frame reception.
-   Preserved response bytes received immediately after the HTTP Upgrade
    headers.
-   Added normal, 16-bit and 64-bit incoming payload lengths.

## 2026-09-19 08:05:51 - v1.2.5

-   Added WebRCON JSON `Message` extraction.
-   Added JSON string unescaping.
-   Added whitespace trimming and output of non-empty RustDedicated
    responses.
-   Completed the first end-to-end `rustcmd status` command/response
    test against a live server.

## 2026-09-19 13:15:17 - v1.2.6

-   Added WebSocket ping handling.
-   Added masked WebSocket pong replies.
-   Improved protocol handling while waiting for the WebRCON response.

## 2026-09-19 17:27:39 - v1.2.7

-   Added masked WebSocket close frames.
-   Added a bounded WebSocket close handshake.
-   Limited the peer-close wait to approximately one second so the
    command returns promptly after receiving its response.

## 2026-09-19 17:54:21 - v1.2.8

-   Refined WebRCON connection and protocol error handling.
-   Kept normal command failures concise and suitable for command-line
    use.
-   Completed live testing of the native C++ WebSocket path without
    Python or third-party WebSocket libraries.

## 2026-09-20 11:31:44 - v1.2.9

-   Completed the native C++ WebSocket and JSON/WebRCON implementation.
-   Confirmed `status`, `serverinfo`, `say`, `server.save` and other
    RustDedicated console commands can be passed through WebRCON.
-   Confirmed successful command execution and response output against a
    live RustDedicated server.
-   Retained separate bounded connection, receive and close timeouts.

## 2026-09-21 21:38:19 - v1.2.10

-   Added `-?` as an additional help command.
-   Updated command help and examples.

## 2026-09-23 06:59:16- v1.2.11

-   Added `--verbose` command diagnostics.
-   Normal failures remain concise.
-   Verbose failures include the underlying socket or address-resolution
    detail.
-   Successful commands produce the same output with or without
    `--verbose`.

## 2026-09-23 19:27:34 - v1.3.0

-   Added local RustDedicated service-management functionality.
-   Added the `RUST_SERVICE_UNIT` configuration setting.
-   Added automatic classification of configured systemd unit paths.
-   Added automatic classification of `/etc/rc.d/` service scripts.
-   Kept the local `server` namespace separate from normal WebRCON
    commands.
-   systemd operations use `sudo systemctl`.
-   `/etc/rc.d/` scripts are executed directly.
-   Service permissions remain under the control of the system
    administrator.

## 2026-09-24 20:12:43 - v1.3.1

-   Added `rustcmd server start`.
-   systemd service units are started through `systemctl`.
-   rc-style service scripts are executed directly with `start`.

## 2026-09-25 09:38:22 - v1.3.2

-   Added `rustcmd server stop`.
-   systemd service units are stopped through `systemctl`.
-   rc-style service scripts are executed directly with `stop`.

## 2026-09-25 16:13:55 - v1.3.3

-   Added `rustcmd server restart`.
-   systemd service units are restarted through `systemctl`.
-   rc-style service scripts are executed directly with `restart`.

## 2026-09-26 21:04:37 - v1.3.4

-   Added `-U` / `--url`.
-   Compiled the canonical project URL directly into `rustcmd`:
    `https://github.com/Exaga/rustcmd`
-   Removed the project URL from runtime configuration.
-   Updated version output to use the canonical project identity.

## 2026-09-27 08:31:15 - v1.3.5

-   Added `-L` / `--license`.
-   Added the complete MIT licence text to the executable.
-   License output does not require configuration or network access.
-   Kept the MIT notice prominently in the C++ source.

## 2026-09-28 10:32:50 - v1.3.6

-   Service-management commands now display the underlying service
    command.
-   Added consistent progress messages for server start, stop and
    restart.
-   systemd output shows the resolved `systemctl` path and configured
    unit name.
-   rc-style output shows the configured `/etc/rc.d/` script and action.

## 2026-09-29 15:56:41 - v1.3.7

-   Added `rustcmd server status`.
-   systemd service status uses `systemctl is-active`.
-   rc-style service status invokes the configured service script with
    `status`.
-   Kept `rustcmd server status` separate from `rustcmd status`; the
    latter remains the RustDedicated WebRCON `status` command.
-   Live-tested systemd start, restart, status and stop operations.
-   Confirmed WebRCON operation after a service restart.

## 2026-09-30 13:24:05 - v1.3.8

-   Added readable pretty-printing of structured JSON returned by
    `serverinfo`.
-   Confirmed formatted `serverinfo` output including hostname, player
    counts, entity count, game time, uptime, map, framerate, memory,
    save time, version and protocol information.
-   Completed the development pass with native WebRCON and local
    service-management functionality working together.


\#EOF<*>
