#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <stdexcept>
#include <tuple>

#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

namespace nano {

struct Socket {

    int fd;

    void teardown(const std::string& msg) {

        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
        fd = -1;

        throw std::runtime_error(msg);
    }
    
    Socket(const std::string& host, unsigned int port, unsigned int rcv_timeout = 0) {

        struct sockaddr_in serv_addr;
        struct hostent* server;

        fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0) 
            throw std::runtime_error("Could not socket()");

	if (rcv_timeout) {

	    struct timeval tv;
	    tv.tv_sec = rcv_timeout / 1000;
	    tv.tv_usec = (rcv_timeout % 1000) * 1000;

	    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0) 
		throw std::runtime_error("Could not setsockopt(SO_RCVTIMEO)");
	}

        server = ::gethostbyname(host.c_str());

        if (server == NULL) 
            teardown("Invalid host: " + host);

        ::memset((void*)&serv_addr, 0, sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;

        ::memcpy((void*)&serv_addr.sin_addr.s_addr, (void*)server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        if (::connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            teardown("Could not connect to " + host);
    }

    ~Socket() {
        close();
    }

    void close() {

        if (fd >= 0) {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
    }
    
    size_t recv(std::string& out) {

        ssize_t i = ::recv(fd, (void*)out.data(), out.size(), 0);

        if (i < 0)
            return 0;

        return i;
    }

    void send(const std::string& in) {

        ssize_t tmp = ::send(fd, (void*)in.data(), in.size(), MSG_NOSIGNAL);

        if (tmp != (ssize_t)in.size())
            throw std::runtime_error("Error sending data");
    }
};

struct Buffer {

    std::string buff;

    typedef std::string::const_iterator const_iterator;

    const_iterator begin;
    const_iterator pointer;
    const_iterator end;

    bool ok;

    Buffer(size_t len = 64 * 1024) :
        buff(len, ' '),
        begin(buff.begin()),
        pointer(buff.end()),
        end(buff.end()),
        ok(true)
        {}

    bool done() const { return !ok; }

    template <typename SOCK>
    std::pair<const_iterator, const_iterator> read(SOCK& sock) {

        if (ok && pointer == end) {

            size_t n = sock.recv(buff);

            begin = buff.begin();
            end = begin + n;
            pointer = begin;

            ok = (n > 0);
        }

        std::pair<const_iterator, const_iterator> ret(pointer, end);

        pointer = end;

        return ret;
    }

    void reset_to(const_iterator p) {
        pointer = p;
    }
};

struct Marker {
    std::string marker;

    typedef std::string::const_iterator const_iterator;

    const_iterator m_b;
    const_iterator m_i;
    const_iterator m_e;

    Marker(const std::string& m) :
        marker(m),
        m_b(marker.begin()),
        m_i(m_b),
        m_e(marker.end())
        {}

    void check_and_advance(auto c) {
        if (*m_i == c) {
            ++m_i;
        } else {
            m_i = m_b;
            if (*m_i == c) {
                ++m_i;
            }
        }
    }

    bool matched() {
        bool ret = (m_i == m_e);

        if (ret) {
            m_i = m_b;
        }

        return ret;
    }
};

struct Count {
    size_t n;
    size_t i;

    Count(size_t n) : n(n), i(0) {}

    void check_and_advance(auto c) {
        ++i;
    }

    bool matched() {
        bool ret = (i >= n);

        if (ret) {
            i = 0;
        }

        return ret;
    }
};

template <typename... MARKERS>
struct AnyOf {

    std::tuple<MARKERS...> markers;

    template <typename... ARG>
    AnyOf(ARG&& ... arg) : markers(std::forward<ARG>(arg)...) {}

    void check_and_advance(auto c) {
        std::apply([c](auto&& ... marker) {
            (marker.check_and_advance(c), ...);
        }, markers);
    }

    bool matched() {
        return std::apply([](auto&& ... marker) {
            return (marker.matched() || ...);
        }, markers);
    }
};

struct EndOfBuffer : public std::exception {
    const char* what() const noexcept {
        return "nanosock: reading from a closed socket";
    }
};

template <typename MARKER>
struct Reader_ {

    MARKER marker;

    typedef std::string::const_iterator const_iterator;

    template <typename... ARG>
    Reader_(ARG&& ... arg) : marker(std::forward<ARG>(arg)...) {}

    template <typename BUFF, typename SOCK, typename FUNC>
    bool operator()(BUFF& buff, SOCK& sock, FUNC func) {

        if (buff.done()) {
            throw EndOfBuffer();
        }

        std::string data;

        if (marker.matched()) {
            func(data);
            return true;
        }

        std::pair<const_iterator, const_iterator> range = buff.read(sock);

        const_iterator r_i = range.first;
        const_iterator r_e = range.second;

        while (r_i != r_e) {

            marker.check_and_advance(*r_i);

            ++r_i;

            if (marker.matched()) {
                data.append(range.first, r_i);
                func(data);
                buff.reset_to(r_i);
                return true;
            }
        }

        data.append(range.first, range.second);

        if (!data.empty()) {
            func(data);
        }

        return false;
    }
};

typedef Reader_<Marker> Reader;

}
