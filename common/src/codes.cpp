// SPDX-License-Identifier: GPL-3.0-or-later
#include "tvcnet/codes.hpp"

namespace tvcnet {

const char* command_name(std::uint8_t code) {
    switch (code) {
    case cmd::kPing: return "PING";
    case cmd::kEcho: return "ECHO";
    case cmd::kStatus: return "STATUS";
    case cmd::kWifi: return "WIFI";
    case cmd::kGet: return "GET";
    case cmd::kNext: return "NEXT";
    case cmd::kClose: return "CLOSE";
    case cmd::kOpen: return "OPEN";
    case cmd::kSend: return "SEND";
    default: return "?";
    }
}

const char* status_name(std::uint8_t code) {
    switch (code) {
    case status::kOk: return "OK";
    case status::kNone: return "NONE";
    case status::kBusy: return "BUSY";
    case status::kData: return "DATA";
    case status::kEnd: return "END";
    case status::kErrUnknown: return "ERR_UNKNOWN";
    case status::kErrParam: return "ERR_PARAM";
    case status::kErrNoWifi: return "ERR_NOWIFI";
    case status::kErrDns: return "ERR_DNS";
    case status::kErrConnect: return "ERR_CONNECT";
    case status::kErrHttp: return "ERR_HTTP";
    case status::kErrLost: return "ERR_LOST";
    default: return "?";
    }
}

}  // namespace tvcnet
