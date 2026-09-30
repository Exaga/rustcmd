/*
 * rustcmd
 *
 * rustcmd C++ WebRCON tool for (RustDedicated) WebSocket interface
 *
 * Standalone C++17 WebRCON tool with no third-party dependencies.
 * Runtime configuration is read from rustcmd-cpp.conf.
 *
 * BUILD:
 *
 *   g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o rustcmd rustcmd.cpp
 *
 * USER CONFIGURATION:
 *
 *   mkdir -p ~/.config/rustcmd
 *   cp rustcmd-cpp.conf ~/.config/rustcmd/rustcmd-cpp.conf
 *   chmod 600 ~/.config/rustcmd/rustcmd-cpp.conf
 *
 * rustcmd first checks:
 *
 *   $XDG_CONFIG_HOME/rustcmd/rustcmd-cpp.conf
 *
 * If XDG_CONFIG_HOME is not set, it checks:
 *
 *   $HOME/.config/rustcmd/rustcmd-cpp.conf
 *
 * If no user configuration exists, it falls back to:
 *
 *   /etc/rustcmd/rustcmd-cpp.conf
 *
 * EXAMPLE CONFIGURATION:
 *
 *   RUST_RCON_IP=127.0.0.1
 *   RUST_RCON_PORT=28016
 *   RUST_RCON_PASSWORD=your-RCON-passwd
 *
 *   RUST_SERVICE_UNIT=/etc/systemd/system/rustserver.service
 *
 * USAGE:
 *
 *   rustcmd say "Hello World!"
 *   rustcmd status
 *
 * MIT License:
 *
 * Copyright 2026 Exaga - penthux.net
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Runtime configuration
struct RconConfig {
    std::string ip;
    std::string port;
    std::string password;
    std::string service_unit;
};

// Error details for verbose output
class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(const std::string& message, const std::string& detail)
        : std::runtime_error(message), detail_(detail) {}

    const std::string& detail() const noexcept { return detail_; }

private:
    std::string detail_;
};

static std::string errno_detail(int error_number)
{
    return "[errno " + std::to_string(error_number) + "] " +
           std::strerror(error_number);
}

static std::string trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

// Configuration file lookup
static fs::path user_config_path()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "rustcmd" / "rustcmd-cpp.conf";
    }

    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".config" / "rustcmd" / "rustcmd-cpp.conf";
    }

    return {};
}

static fs::path find_config_path()
{
    const fs::path user_path = user_config_path();

    std::error_code ec;
    if (!user_path.empty() && fs::is_regular_file(user_path, ec)) {
        return user_path;
    }

    const fs::path system_path = "/etc/rustcmd/rustcmd-cpp.conf";
    ec.clear();
    if (fs::is_regular_file(system_path, ec)) {
        return system_path;
    }

    if (!user_path.empty()) {
        throw std::runtime_error(
            "configuration file not found at " + user_path.string() +
            " or /etc/rustcmd/rustcmd-cpp.conf");
    }

    throw std::runtime_error(
        "configuration file not found at /etc/rustcmd/rustcmd-cpp.conf and no user "
        "configuration directory could be determined");
}

static RconConfig read_config(const fs::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("unable to read configuration file: " +
                                 path.string());
    }

    RconConfig config;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));

        if (key == "RUST_RCON_IP") {
            config.ip = value;
        } else if (key == "RUST_RCON_PORT") {
            config.port = value;
        } else if (key == "RUST_RCON_PASSWORD") {
            config.password = value;
        } else if (key == "RUST_SERVICE_UNIT") {
            config.service_unit = value;
        }
    }

    if (file.bad()) {
        throw std::runtime_error("error while reading configuration file: " +
                                 path.string());
    }

    if (config.ip.empty()) {
        throw std::runtime_error("RUST_RCON_IP not found in " + path.string());
    }
    if (config.port.empty()) {
        throw std::runtime_error("RUST_RCON_PORT not found in " + path.string());
    }
    if (config.password.empty()) {
        throw std::runtime_error("RUST_RCON_PASSWORD not found in " +
                                 path.string());
    }

    return config;
}

// Program information and help output
static constexpr const char* VERSION = "1.3.8";
static constexpr const char* PROJECT_URL = "https://github.com/Exaga/rustcmd";

static void print_url()
{
    std::cout << "rustcmd project (GitHub) URL: \n"
              << PROJECT_URL << '\n';
}

static void print_license()
{
    std::cout
        << "MIT License\n\n"
        << "Copyright 2026 Exaga - penthux.net\n\n"
        << "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        << "of this software and associated documentation files (the \"Software\"), to deal\n"
        << "in the Software without restriction, including without limitation the rights\n"
        << "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        << "copies of the Software, and to permit persons to whom the Software is furnished\n"
        << "to do so, subject to the following conditions:\n\n"
        << "The above copyright notice and this permission notice shall be included in all\n"
        << "copies or substantial portions of the Software.\n\n"
        << "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        << "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        << "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        << "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        << "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        << "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        << "SOFTWARE.\n";
}

static void print_version()
{
    std::cout << "rustcmd v" << VERSION << " [C++]\n"
              << "WebRCON command-line tool for (RustDedicated) WebSocket interface\n"
              << PROJECT_URL << '\n'
              << "(c) 2026 penthux.net\n\n";
}

static void print_help()
{
    std::cout
        << "rustcmd v" << VERSION << " [C++]\n"
        << "WebRCON command-line tool for (RustDedicated) WebSocket interface\n\n"
        << "Usage:\n"
        << "  rustcmd <command> [arguments]\n"
        << "  rustcmd [options]\n\n"
        << "Examples:\n"
        << "  rustcmd status\n"
        << "  rustcmd status --verbose\n"
        << "  rustcmd serverinfo\n"
        << "  rustcmd say \"Hello World!\"\n"
        << "  rustcmd server.save\n"
        << "  rustcmd server start\n"
        << "  rustcmd server stop\n"
        << "  rustcmd server restart\n"
        << "  rustcmd server status\n\n"
        << "Options:\n"
        << "  -h, -?, --help          Show this help\n"
        << "  -v, --version           Show version information\n"
        << "  -U, --url               Show official rustcmd project URL\n"
        << "  -L, --license           Show license information\n"
        << "  -c, --config <file>     Use an alternative configuration file\n"
        << "  <command> --verbose     Show detailed errors for the command\n\n";
}

// WebRCON JSON command payload
static std::string json_escape(const std::string& value)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size());

    for (const unsigned char ch : value) {
        switch (ch) {
        case '\"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20) {
                escaped += "\\u00";
                escaped += hex[(ch >> 4) & 0x0f];
                escaped += hex[ch & 0x0f];
            } else {
                escaped += static_cast<char>(ch);
            }
        }
    }

    return escaped;
}

static std::string build_web_rcon_payload(const std::string& command)
{
    return "{\"Identifier\":1,\"Message\":\"" + json_escape(command) +
           "\",\"Name\":\"WebRCON\"}";
}


// SHA-1 hashing for the WebSocket handshake
static std::uint32_t rotate_left(std::uint32_t value, unsigned int bits)
{
    return (value << bits) | (value >> (32U - bits));
}

[[maybe_unused]] static std::array<std::uint8_t, 20> sha1(const std::string& input)
{
    std::vector<std::uint8_t> message(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(message.size()) * 8U;

    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) {
        message.push_back(0x00U);
    }

    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }

    std::uint32_t h0 = 0x67452301U;
    std::uint32_t h1 = 0xefcdab89U;
    std::uint32_t h2 = 0x98badcfeU;
    std::uint32_t h3 = 0x10325476U;
    std::uint32_t h4 = 0xc3d2e1f0U;

    for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
        std::array<std::uint32_t, 80> words{};

        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t base = offset + (i * 4U);
            words[i] = (static_cast<std::uint32_t>(message[base]) << 24U) |
                       (static_cast<std::uint32_t>(message[base + 1U]) << 16U) |
                       (static_cast<std::uint32_t>(message[base + 2U]) << 8U) |
                       static_cast<std::uint32_t>(message[base + 3U]);
        }

        for (std::size_t i = 16U; i < 80U; ++i) {
            words[i] = rotate_left(words[i - 3U] ^ words[i - 8U] ^
                                   words[i - 14U] ^ words[i - 16U], 1U);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (std::size_t i = 0; i < 80U; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;

            if (i < 20U) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999U;
            } else if (i < 40U) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1U;
            } else if (i < 60U) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcU;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6U;
            }

            const std::uint32_t temp = rotate_left(a, 5U) + f + e + k + words[i];
            e = d;
            d = c;
            c = rotate_left(b, 30U);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<std::uint8_t, 20> digest{};
    const std::array<std::uint32_t, 5> hash = {h0, h1, h2, h3, h4};

    for (std::size_t i = 0; i < hash.size(); ++i) {
        digest[(i * 4U)] = static_cast<std::uint8_t>((hash[i] >> 24U) & 0xffU);
        digest[(i * 4U) + 1U] = static_cast<std::uint8_t>((hash[i] >> 16U) & 0xffU);
        digest[(i * 4U) + 2U] = static_cast<std::uint8_t>((hash[i] >> 8U) & 0xffU);
        digest[(i * 4U) + 3U] = static_cast<std::uint8_t>(hash[i] & 0xffU);
    }

    return digest;
}

// Base64 encoding for the WebSocket handshake
[[maybe_unused]] static std::string base64_encode(const std::uint8_t* data,
                                                  std::size_t length)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((length + 2U) / 3U) * 4U);

    for (std::size_t i = 0; i < length; i += 3U) {
        const std::uint32_t octet_a = data[i];
        const std::uint32_t octet_b = (i + 1U < length) ? data[i + 1U] : 0U;
        const std::uint32_t octet_c = (i + 2U < length) ? data[i + 2U] : 0U;
        const std::uint32_t triple = (octet_a << 16U) | (octet_b << 8U) | octet_c;

        output.push_back(alphabet[(triple >> 18U) & 0x3fU]);
        output.push_back(alphabet[(triple >> 12U) & 0x3fU]);
        output.push_back((i + 1U < length) ? alphabet[(triple >> 6U) & 0x3fU] : '=');
        output.push_back((i + 2U < length) ? alphabet[triple & 0x3fU] : '=');
    }

    return output;
}

[[maybe_unused]] static std::string websocket_accept_key(const std::string& client_key)
{
    static constexpr char websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto digest = sha1(client_key + websocket_guid);
    return base64_encode(digest.data(), digest.size());
}

// TCP connection to the RCON server
static int connect_tcp(const std::string& host, const std::string& port)
{
    static constexpr int connect_timeout_ms = 5000;
    static constexpr long io_timeout_seconds = 5;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const int lookup_result = getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses);
    if (lookup_result != 0) {
        throw DiagnosticError(
            "Connection failed: Unable to resolve RCON address.",
            "[getaddrinfo " + std::to_string(lookup_result) + "] " +
                gai_strerror(lookup_result));
    }

    int connected_socket = -1;
    bool timed_out = false;
    bool refused = false;
    int last_socket_error = 0;

    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        const int socket_fd = socket(address->ai_family, address->ai_socktype,
                                     address->ai_protocol);
        if (socket_fd < 0) {
            last_socket_error = errno;
            continue;
        }

        const int original_flags = fcntl(socket_fd, F_GETFL, 0);
        if (original_flags < 0 ||
            fcntl(socket_fd, F_SETFL, original_flags | O_NONBLOCK) < 0) {
            close(socket_fd);
            continue;
        }

        int result = connect(socket_fd, address->ai_addr, address->ai_addrlen);
        if (result < 0 && errno == EINPROGRESS) {
            pollfd descriptor{};
            descriptor.fd = socket_fd;
            descriptor.events = POLLOUT;

            do {
                result = poll(&descriptor, 1, connect_timeout_ms);
            } while (result < 0 && errno == EINTR);

            if (result == 0) {
                timed_out = true;
                last_socket_error = ETIMEDOUT;
                close(socket_fd);
                continue;
            }

            if (result < 0) {
                last_socket_error = errno;
                close(socket_fd);
                continue;
            }

            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);
            if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                           &socket_error, &error_length) < 0) {
                last_socket_error = errno;
                close(socket_fd);
                continue;
            }

            if (socket_error != 0) {
                last_socket_error = socket_error;
                if (socket_error == ECONNREFUSED) {
                    refused = true;
                } else if (socket_error == ETIMEDOUT) {
                    timed_out = true;
                }
                close(socket_fd);
                continue;
            }
        } else if (result < 0) {
            last_socket_error = errno;
            if (errno == ECONNREFUSED) {
                refused = true;
            } else if (errno == ETIMEDOUT) {
                timed_out = true;
            }
            close(socket_fd);
            continue;
        }

        if (fcntl(socket_fd, F_SETFL, original_flags) < 0) {
            close(socket_fd);
            continue;
        }

        const timeval io_timeout{io_timeout_seconds, 0};
        if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                       &io_timeout, sizeof(io_timeout)) < 0 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO,
                       &io_timeout, sizeof(io_timeout)) < 0) {
            close(socket_fd);
            continue;
        }

        connected_socket = socket_fd;
        break;
    }

    freeaddrinfo(addresses);

    if (connected_socket >= 0) {
        return connected_socket;
    }

    if (timed_out) {
        const int detail_error = last_socket_error != 0 ? last_socket_error : ETIMEDOUT;
        throw DiagnosticError(
            "Connection failed: RCON server timed out or is unreachable.",
            errno_detail(detail_error));
    }
    if (refused) {
        const int detail_error = last_socket_error != 0 ? last_socket_error : ECONNREFUSED;
        throw DiagnosticError(
            "Connection failed: Server is offline or RCON IP/port is misconfigured.",
            errno_detail(detail_error));
    }

    if (last_socket_error != 0) {
        throw DiagnosticError(
            "Connection failed: Unable to connect to RCON server.",
            errno_detail(last_socket_error));
    }
    throw std::runtime_error("Connection failed: Unable to connect to RCON server.");
}

// WebSocket handshake key
static std::string generate_websocket_key()
{
    std::array<std::uint8_t, 16> random_bytes{};
    std::ifstream random_source("/dev/urandom", std::ios::in | std::ios::binary);

    if (!random_source.read(reinterpret_cast<char*>(random_bytes.data()),
                            static_cast<std::streamsize>(random_bytes.size()))) {
        throw std::runtime_error("WebSocket handshake failed: unable to generate handshake key.");
    }

    return base64_encode(random_bytes.data(), random_bytes.size());
}

// Send the complete HTTP Upgrade request
static void send_all(int socket_fd, const std::string& data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        const ssize_t result = send(socket_fd, data.data() + sent,
                                    data.size() - sent, 0);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                           errno == ETIMEDOUT)) {
            throw std::runtime_error("WebSocket handshake failed: server timed out.");
        }
        throw std::runtime_error("WebSocket handshake failed: unable to send upgrade request.");
    }
}

// HTTP Upgrade response and any WebSocket data already received
struct HttpUpgradeResponse {
    std::string headers;
    std::vector<std::uint8_t> leftover;
};

static HttpUpgradeResponse receive_http_headers(int socket_fd)
{
    static constexpr std::size_t max_header_size = 16384;
    std::string response;
    std::array<char, 2048> buffer{};

    while (response.find("\r\n\r\n") == std::string::npos) {
        const ssize_t result = recv(socket_fd, buffer.data(), buffer.size(), 0);
        if (result > 0) {
            response.append(buffer.data(), static_cast<std::size_t>(result));
            if (response.size() > max_header_size) {
                throw std::runtime_error("WebSocket handshake failed: response headers are too large.");
            }
            continue;
        }
        if (result == 0) {
            throw std::runtime_error("WebSocket handshake failed: server closed the connection.");
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            throw std::runtime_error("WebSocket handshake failed: server timed out.");
        }
        throw std::runtime_error("WebSocket handshake failed: unable to read upgrade response.");
    }

    const std::size_t header_end = response.find("\r\n\r\n") + 4U;
    HttpUpgradeResponse result;
    result.headers = response.substr(0, header_end);
    result.leftover.assign(response.begin() + static_cast<std::ptrdiff_t>(header_end),
                           response.end());
    return result;
}

static std::string lowercase(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

// WebSocket HTTP Upgrade handshake
static std::vector<std::uint8_t> websocket_handshake(int socket_fd, const RconConfig& config)
{
    const std::string client_key = generate_websocket_key();
    const std::string expected_accept = websocket_accept_key(client_key);

    std::ostringstream request;
    request << "GET /" << config.password << " HTTP/1.1\r\n"
            << "Host: " << config.ip << ':' << config.port << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << client_key << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n\r\n";

    send_all(socket_fd, request.str());
    const HttpUpgradeResponse response = receive_http_headers(socket_fd);

    std::istringstream stream(response.headers);
    std::string status_line;
    if (!std::getline(stream, status_line)) {
        throw std::runtime_error("WebSocket handshake failed: invalid HTTP response.");
    }
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }

    std::istringstream status_stream(status_line);
    std::string http_version;
    int status_code = 0;
    status_stream >> http_version >> status_code;
    if (http_version.rfind("HTTP/", 0) != 0 || status_code != 101) {
        throw std::runtime_error("WebSocket handshake failed: RCON endpoint rejected the upgrade.");
    }

    bool upgrade_ok = false;
    bool connection_ok = false;
    bool accept_ok = false;
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string name = lowercase(trim(line.substr(0, colon)));
        const std::string value = trim(line.substr(colon + 1U));
        const std::string lower_value = lowercase(value);

        if (name == "upgrade" && lower_value == "websocket") {
            upgrade_ok = true;
        } else if (name == "connection") {
            std::istringstream tokens(lower_value);
            std::string token;
            while (std::getline(tokens, token, ',')) {
                if (trim(token) == "upgrade") {
                    connection_ok = true;
                    break;
                }
            }
        } else if (name == "sec-websocket-accept" && value == expected_accept) {
            accept_ok = true;
        }
    }

    if (!upgrade_ok || !connection_ok || !accept_ok) {
        throw std::runtime_error("WebSocket handshake failed: invalid upgrade response.");
    }

    return response.leftover;
}

// Send a masked WebSocket text frame
static void send_websocket_text_frame(int socket_fd, const std::string& payload)
{
    std::array<std::uint8_t, 4> mask{};
    std::ifstream random_source("/dev/urandom", std::ios::in | std::ios::binary);

    if (!random_source.read(reinterpret_cast<char*>(mask.data()),
                            static_cast<std::streamsize>(mask.size()))) {
        throw std::runtime_error("WebSocket send failed: unable to generate masking key.");
    }

    std::vector<std::uint8_t> frame;
    frame.reserve(payload.size() + 14U);

    // FIN=1, opcode=0x1 (text frame).
    frame.push_back(0x81U);

    const std::uint64_t payload_length = static_cast<std::uint64_t>(payload.size());
    if (payload_length <= 125U) {
        frame.push_back(static_cast<std::uint8_t>(0x80U | payload_length));
    } else if (payload_length <= 65535U) {
        frame.push_back(0x80U | 126U);
        frame.push_back(static_cast<std::uint8_t>((payload_length >> 8U) & 0xffU));
        frame.push_back(static_cast<std::uint8_t>(payload_length & 0xffU));
    } else {
        frame.push_back(0x80U | 127U);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<std::uint8_t>((payload_length >> shift) & 0xffU));
        }
    }

    frame.insert(frame.end(), mask.begin(), mask.end());

    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(static_cast<std::uint8_t>(payload[i]) ^ mask[i % mask.size()]);
    }

    std::size_t sent = 0;
    while (sent < frame.size()) {
        const ssize_t result = send(socket_fd, frame.data() + sent,
                                    frame.size() - sent, 0);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                           errno == ETIMEDOUT)) {
            throw std::runtime_error("WebSocket send failed: server timed out.");
        }
        throw std::runtime_error("WebSocket send failed: unable to send command frame.");
    }
}

// Read enough socket data to complete a WebSocket frame
static void receive_exact(int socket_fd, std::vector<std::uint8_t>& buffer,
                          std::size_t required)
{
    std::array<std::uint8_t, 4096> chunk{};

    while (buffer.size() < required) {
        const ssize_t result = recv(socket_fd, chunk.data(), chunk.size(), 0);
        if (result > 0) {
            buffer.insert(buffer.end(), chunk.begin(),
                          chunk.begin() + static_cast<std::ptrdiff_t>(result));
            continue;
        }
        if (result == 0) {
            throw std::runtime_error("WebSocket receive failed: server closed the connection.");
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            throw std::runtime_error("WebSocket receive failed: server timed out.");
        }
        throw std::runtime_error("WebSocket receive failed: unable to read response frame.");
    }
}

// Decoded WebSocket frame
struct WebSocketFrame {
    std::uint8_t opcode = 0U;
    std::vector<std::uint8_t> payload;
};

// Receive and decode a WebSocket frame
static WebSocketFrame receive_websocket_frame(int socket_fd,
                                               std::vector<std::uint8_t>& buffer)
{
    receive_exact(socket_fd, buffer, 2U);

    const std::uint8_t first = buffer[0];
    const std::uint8_t second = buffer[1];
    const bool final_frame = (first & 0x80U) != 0U;
    const std::uint8_t opcode = first & 0x0fU;
    const bool masked = (second & 0x80U) != 0U;

    if (!final_frame || masked || (first & 0x70U) != 0U) {
        throw std::runtime_error("WebSocket receive failed: unsupported response frame.");
    }

    std::uint64_t payload_length = second & 0x7fU;
    std::size_t header_length = 2U;

    if (payload_length == 126U) {
        receive_exact(socket_fd, buffer, 4U);
        payload_length = (static_cast<std::uint64_t>(buffer[2]) << 8U) |
                         static_cast<std::uint64_t>(buffer[3]);
        header_length = 4U;
    } else if (payload_length == 127U) {
        receive_exact(socket_fd, buffer, 10U);
        payload_length = 0U;
        for (std::size_t i = 2U; i < 10U; ++i) {
            payload_length = (payload_length << 8U) | buffer[i];
        }
        header_length = 10U;
    }

    const bool control_frame = opcode >= 0x08U;
    if (control_frame && payload_length > 125U) {
        throw std::runtime_error("WebSocket receive failed: invalid control frame.");
    }

    static constexpr std::uint64_t max_response_size = 16U * 1024U * 1024U;
    if (payload_length > max_response_size) {
        throw std::runtime_error("WebSocket receive failed: response is too large.");
    }

    const std::size_t total_length = header_length + static_cast<std::size_t>(payload_length);
    receive_exact(socket_fd, buffer, total_length);

    WebSocketFrame frame;
    frame.opcode = opcode;
    frame.payload.assign(buffer.begin() + static_cast<std::ptrdiff_t>(header_length),
                         buffer.begin() + static_cast<std::ptrdiff_t>(total_length));
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(total_length));
    return frame;
}

// Send a masked WebSocket control frame
static void send_websocket_control_frame(int socket_fd, std::uint8_t opcode,
                                         const std::vector<std::uint8_t>& payload)
{
    if (payload.size() > 125U) {
        throw std::runtime_error("WebSocket send failed: control payload is too large.");
    }

    std::array<std::uint8_t, 4> mask{};
    std::ifstream random_source("/dev/urandom", std::ios::in | std::ios::binary);
    if (!random_source.read(reinterpret_cast<char*>(mask.data()),
                            static_cast<std::streamsize>(mask.size()))) {
        throw std::runtime_error("WebSocket send failed: unable to generate masking key.");
    }

    std::vector<std::uint8_t> frame;
    frame.reserve(payload.size() + 6U);
    frame.push_back(static_cast<std::uint8_t>(0x80U | opcode));
    frame.push_back(static_cast<std::uint8_t>(0x80U | payload.size()));
    frame.insert(frame.end(), mask.begin(), mask.end());
    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(payload[i] ^ mask[i % mask.size()]);
    }

    std::size_t sent = 0U;
    while (sent < frame.size()) {
        const ssize_t result = send(socket_fd, frame.data() + sent, frame.size() - sent, 0);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) continue;
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)) {
            throw std::runtime_error("WebSocket send failed: server timed out.");
        }
        throw std::runtime_error("WebSocket send failed: unable to send control frame.");
    }
}

// Wait for the WebRCON text response and handle control frames
static std::string receive_websocket_text_frame(int socket_fd,
                                                std::vector<std::uint8_t>& buffer)
{
    for (;;) {
        WebSocketFrame frame = receive_websocket_frame(socket_fd, buffer);
        if (frame.opcode == 0x01U) {
            return std::string(frame.payload.begin(), frame.payload.end());
        }
        if (frame.opcode == 0x09U) {
            send_websocket_control_frame(socket_fd, 0x0aU, frame.payload);
            continue;
        }
        if (frame.opcode == 0x0aU) {
            continue;
        }
        if (frame.opcode == 0x08U) {
            send_websocket_control_frame(socket_fd, 0x08U, frame.payload);
            throw std::runtime_error("WebSocket receive failed: server closed before sending a response.");
        }
        throw std::runtime_error("WebSocket receive failed: unsupported response frame.");
    }
}

// Close the WebSocket without delaying command completion
static void close_websocket(int socket_fd, std::vector<std::uint8_t>& buffer)
{
    send_websocket_control_frame(socket_fd, 0x08U, {});

    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    for (;;) {
        try {
            if (buffer.size() >= 2U) {
                WebSocketFrame frame = receive_websocket_frame(socket_fd, buffer);
                if (frame.opcode == 0x08U) return;
                if (frame.opcode == 0x09U) {
                    send_websocket_control_frame(socket_fd, 0x0aU, frame.payload);
                }
                continue;
            }
        } catch (...) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

        pollfd descriptor{};
        descriptor.fd = socket_fd;
        descriptor.events = POLLIN;
        int result;
        do {
            result = poll(&descriptor, 1, static_cast<int>(remaining.count()));
        } while (result < 0 && errno == EINTR && std::chrono::steady_clock::now() < deadline);

        if (result <= 0) return;
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return;
        if ((descriptor.revents & POLLIN) == 0) return;

        try {
            WebSocketFrame frame = receive_websocket_frame(socket_fd, buffer);
            if (frame.opcode == 0x08U) return;
            if (frame.opcode == 0x09U) {
                send_websocket_control_frame(socket_fd, 0x0aU, frame.payload);
            }
        } catch (...) {
            return;
        }
    }
}

// JSON string decoding
static unsigned int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return static_cast<unsigned int>(ch - '0');
    if (ch >= 'a' && ch <= 'f') return 10U + static_cast<unsigned int>(ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10U + static_cast<unsigned int>(ch - 'A');
    throw std::runtime_error("WebRCON response failed: invalid JSON escape.");
}

static void append_utf8(std::string& output, std::uint32_t codepoint)
{
    if (codepoint <= 0x7fU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ffU) {
        output.push_back(static_cast<char>(0xc0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    } else if (codepoint <= 0xffffU) {
        output.push_back(static_cast<char>(0xe0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    } else {
        output.push_back(static_cast<char>(0xf0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
}

static std::string parse_json_string(const std::string& json, std::size_t& position)
{
    if (position >= json.size() || json[position] != '"') {
        throw std::runtime_error("WebRCON response failed: malformed JSON.");
    }
    ++position;

    std::string output;
    while (position < json.size()) {
        const char ch = json[position++];
        if (ch == '"') {
            return output;
        }
        if (ch != '\\') {
            output.push_back(ch);
            continue;
        }
        if (position >= json.size()) {
            break;
        }

        const char escaped = json[position++];
        switch (escaped) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u': {
            if (position + 4U > json.size()) {
                throw std::runtime_error("WebRCON response failed: invalid JSON escape.");
            }
            std::uint32_t codepoint = 0U;
            for (int i = 0; i < 4; ++i) {
                codepoint = (codepoint << 4U) | hex_value(json[position++]);
            }
            append_utf8(output, codepoint);
            break;
        }
        default:
            throw std::runtime_error("WebRCON response failed: invalid JSON escape.");
        }
    }

    throw std::runtime_error("WebRCON response failed: unterminated JSON string.");
}

// Extract the Message field from the WebRCON response
static std::string extract_web_rcon_message(const std::string& json)
{
    std::size_t position = 0U;
    while (position < json.size()) {
        if (json[position] != '"') {
            ++position;
            continue;
        }

        const std::string key = parse_json_string(json, position);
        std::size_t cursor = position;
        while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
            ++cursor;
        }
        if (cursor >= json.size() || json[cursor] != ':') {
            position = cursor;
            continue;
        }
        ++cursor;
        while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
            ++cursor;
        }

        if (key == "Message") {
            if (cursor >= json.size() || json[cursor] != '"') {
                throw std::runtime_error("WebRCON response failed: Message is not a JSON string.");
            }
            return parse_json_string(json, cursor);
        }
        position = cursor;
    }

    throw std::runtime_error("WebRCON response failed: Message field not found.");
}

// Local Rust server service management
enum class ServiceBackend {
    Systemd,
    Rc
};

static ServiceBackend service_backend(const std::string& unit)
{
    const fs::path path(unit);
    const std::string normalized = path.lexically_normal().string();

    if (normalized.find("/systemd/") != std::string::npos) {
        return ServiceBackend::Systemd;
    }
    if (normalized.rfind("/etc/rc.d/", 0) == 0) {
        return ServiceBackend::Rc;
    }

    throw std::runtime_error("Unsupported Rust service unit: " + unit);
}

// Run a local service command and return its exit status
static int run_process(const std::vector<std::string>& arguments)
{
    if (arguments.empty()) {
        throw std::runtime_error("Service command is empty.");
    }

    const pid_t pid = fork();
    if (pid < 0) {
        const int saved_errno = errno;
        throw DiagnosticError("Unable to execute Rust service command.",
                              errno_detail(saved_errno));
    }

    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1U);
        for (const std::string& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        const int saved_errno = errno;
        throw DiagnosticError("Unable to wait for Rust service command.",
                              errno_detail(saved_errno));
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static std::string executable_path(const std::string& executable)
{
    if (executable.find('/') != std::string::npos) {
        return executable;
    }

    const char* path_env = std::getenv("PATH");
    if (path_env != nullptr) {
        std::istringstream paths(path_env);
        std::string directory;
        while (std::getline(paths, directory, ':')) {
            if (directory.empty()) directory = ".";
            const fs::path candidate = fs::path(directory) / executable;
            if (access(candidate.c_str(), X_OK) == 0) {
                return candidate.string();
            }
        }
    }

    return executable;
}

static void manage_rust_service(const RconConfig& config,
                                const std::string& action)
{
    if (config.service_unit.empty()) {
        throw std::runtime_error("RUST_SERVICE_UNIT not found in configuration.");
    }

    const ServiceBackend backend = service_backend(config.service_unit);
    std::vector<std::string> arguments;

    if (backend == ServiceBackend::Systemd) {
        const std::string unit_name = fs::path(config.service_unit).filename().string();
        if (unit_name.empty()) {
            throw std::runtime_error("Invalid RUST_SERVICE_UNIT: " + config.service_unit);
        }
        const std::string systemctl = executable_path("systemctl");
        const std::string systemd_action = (action == "status") ? "is-active" : action;
        std::cout << systemctl << ' ' << systemd_action << ' ' << unit_name << '\n';
        arguments = {"sudo", "systemctl", systemd_action, unit_name};
    } else {
        std::cout << config.service_unit << ' ' << action << '\n';
        arguments = {config.service_unit, action};
    }

    if (action == "start") {
        std::cout << "Starting Rust server ...\n";
    } else if (action == "restart") {
        std::cout << "Restarting Rust server ...\n";
    } else if (action == "stop") {
        std::cout << "Shutting down Rust server ...\n";
    }

    const int exit_code = run_process(arguments);
    if (exit_code != 0) {
        throw DiagnosticError("Rust service operation failed.",
                              "service command exited with status " +
                                  std::to_string(exit_code));
    }
}

// Command-line processing and command dispatch
int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: rustcmd <command>\n";
        return 1;
    }

    const std::string first = argv[1];

    if (first == "-h" || first == "-?" || first == "--help") {
        print_help();
        return 0;
    }

    if (first == "-v" || first == "--version") {
        print_version();
        return 0;
    }

    if (first == "-U" || first == "--url") {
        print_url();
        return 0;
    }

    if (first == "-L" || first == "--license") {
        print_license();
        return 0;
    }

    bool verbose = false;
    fs::path config_path;
    std::vector<std::string> command_parts;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--verbose") {
            verbose = true;
            continue;
        }
        if (argument == "-c" || argument == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "Usage: rustcmd --config <file> <command>\n";
                return 1;
            }
            config_path = argv[++i];
            continue;
        }
        command_parts.push_back(argument);
    }

    if (command_parts.empty()) {
        print_help();
        return 0;
    }

    std::ostringstream command_stream;
    for (std::size_t i = 0; i < command_parts.size(); ++i) {
        if (i != 0U) command_stream << ' ';
        command_stream << command_parts[i];
    }

    try {
        if (config_path.empty()) {
            config_path = find_config_path();
        }

        const RconConfig config = read_config(config_path);

        if (command_parts.size() == 2U && command_parts[0] == "server" &&
            (command_parts[1] == "start" || command_parts[1] == "stop" ||
             command_parts[1] == "restart" || command_parts[1] == "status")) {
            manage_rust_service(config, command_parts[1]);
            return 0;
        }

        // Send normal commands to RustDedicated through WebRCON
        const std::string payload = build_web_rcon_payload(command_stream.str());

        const int socket_fd = connect_tcp(config.ip, config.port);
        try {
            std::vector<std::uint8_t> buffered = websocket_handshake(socket_fd, config);
            send_websocket_text_frame(socket_fd, payload);
            const std::string response = receive_websocket_text_frame(socket_fd, buffered);
            const std::string message = trim(extract_web_rcon_message(response));
            if (!message.empty()) {
                std::cout << message << '\n';
            }
            close_websocket(socket_fd, buffered);
        } catch (...) {
            close(socket_fd);
            throw;
        }
        close(socket_fd);
    } catch (const DiagnosticError& error) {
        std::cerr << error.what() << '\n';
        if (verbose) {
            std::cerr << "Detail: " << error.detail() << '\n';
        }
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}

//#EOF<*>
