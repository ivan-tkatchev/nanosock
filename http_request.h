#pragma once

#include "nanosock.h"

#include <cctype>

namespace nano {

namespace http {

struct BadSend : public std::exception {
    const char* what() const noexcept {
        return "nanosock: sending when a session is in progress";
    }
};

struct BadTransfer : public std::exception {
    const char* what() const noexcept {
        return "nanosock: reading when nothing was sent";
    }
};

struct Request {
    nano::Socket sock;
    nano::Buffer buff;
    nano::Reader<> read_space;
    nano::Reader<> read_line;
    nano::Reader<nano::AnyOf<nano::Marker, nano::Marker>> read_key;
    nano::Reader<Count> read_body;

    std::string host;
    std::string version;
    std::string code;
    std::string key;
    std::string val;
    size_t content_length = 0;

    Request(const std::string& host, unsigned int port, unsigned int timeout = 0) :
        sock(host, port, timeout),
        read_space(" "),
        read_line("\r\n"),
        read_key(":", "\r\n"),
        read_body(0),
        host(host)
        {}

    enum {
        STATE_SEND,
        STATE_VERSION,
        STATE_CODE,
        STATE_FLAIR,
        STATE_KEY,
        STATE_VAL,
        STATE_BODY
    } state = STATE_SEND;

    nano::Socket& socket() { return sock; }
    bool drained() const { return buff.drained(); }

    bool valid() const { return (state == STATE_SEND); }

    void send(const std::string& method, const std::string& path, const std::string& body) {

        if (state != STATE_SEND) {
            throw BadSend();
        }

        std::string request = method;
        request += " ";
        request += path;
        request += " HTTP/1.1\r\n";
        if (body.size() > 0) {
            request += "Content-Length: ";
            request += std::to_string(body.size());
            request += "\r\n";
        }
        request += "Host: ";
        request += host;
        request += "\r\n\r\n";

        sock.send(request);
        if (body.size() > 0) {
            sock.send(body);
        }

        state = STATE_VERSION;
    }

    bool transfer(auto& responder, bool blocking = true) {
        switch (state) {

        case STATE_VERSION: {
            if (read_space(buff, sock, [this](const std::string& part) {
                version += part;
            }, blocking)) {
                responder.version(version);
                version.clear();
                state = STATE_CODE;
            }
            break;
        }

        case STATE_CODE: {
            if (read_space(buff, sock, [this](const std::string& part) {
                code += part;
            }, blocking)) {
                responder.code(code);
                code.clear();
                state = STATE_FLAIR;
            }
            break;
        }

        case STATE_FLAIR: {
            if (read_line(buff, sock, [](const std::string&) {}, blocking)) {
                state = STATE_KEY;
            }
            break;
        }

        case STATE_KEY: {
            if (read_key(buff, sock, [this](const std::string& part) {
                key += part;
            }, blocking)) {
                if (key == "\r\n") {
                    if (content_length == 0) {
                        state = STATE_SEND;
                    } else {
                        read_body.marker.n = content_length;
                        state = STATE_BODY;
                    }
                } else {
                    state = STATE_VAL;
                }
            }
            break;
        }

        case STATE_VAL: {
            if (read_line(buff, sock, [this](const std::string& part) {
                val += part;
            }, blocking)) {
                for (char& c : key) {
                    c = std::tolower(static_cast<unsigned char>(c));
                }

                if (key.starts_with("content-length")) {
                    content_length = std::stoul(val);
                }

                responder.header(key, val);
                key.clear();
                val.clear();
                state = STATE_KEY;
            }
            break;
        }

        case STATE_BODY: {
            if (read_body(buff, sock, [&responder](const std::string& body) {
                responder.body(body);
            }, blocking)) {
                state = STATE_SEND;
            }
            break;
        }

        case STATE_SEND: {
            throw BadTransfer();
        }
        }

        return (state == STATE_SEND);
    }
};
        

template <typename RES>
void send(const std::string& host, unsigned int port, unsigned int timeout, 
          const std::string& method, const std::string& path, const std::string& body,
          RES& responder) {

    Request req(host, port, timeout);

    req.send(method, path, body);

    while (!req.transfer(responder))
        ;
}

}

}

