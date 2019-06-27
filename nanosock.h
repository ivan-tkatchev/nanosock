#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <stdexcept>

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
    
    Socket(const std::string& host, unsigned int port) {

        struct sockaddr_in serv_addr;
        struct hostent* server;

        fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0) 
            throw std::runtime_error("Could not socket()");

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
    
    bool recv(std::string& out) {

        ssize_t i = ::recv(fd, (void*)out.data(), out.size(), 0);

        if (i < 0)
            return false;

        out.resize(i);

        if (out.empty())
            return false;

        return true;
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

            ok = sock.recv(buff);

            if (!ok) {
                buff.clear();
            }

            begin = buff.begin();
            end = buff.end();
            pointer = begin;
        }

        std::pair<const_iterator, const_iterator> ret(pointer, end);

        pointer = end;

        return ret;
    }

    void reset_to(const_iterator p) {
        pointer = p;
    }
};

struct Reader {

    std::string marker;

    typedef std::string::const_iterator const_iterator;

    const_iterator m_b;
    const_iterator m_i;
    const_iterator m_e;

    Reader(const std::string& m) :
        marker(m),
        m_b(marker.begin()),
        m_i(m_b),
        m_e(marker.end())
        {}

    template <typename BUFF, typename SOCK, typename FUNC>
    bool operator()(BUFF& buff, SOCK& sock, FUNC func) {

        std::string data;

        if (m_i == m_e) {
            func(data);
            return true;
        }

        while (!buff.done()) {

            std::pair<const_iterator, const_iterator> range = buff.read(sock);

            const_iterator r_i = range.first;
            const_iterator r_e = range.second;

            while (r_i != r_e) {

                if (*m_i == *r_i) {
                    ++m_i;
                } else {
                    m_i = m_b;
                }

                ++r_i;

                if (m_i == m_e) {

                    data.append(range.first, r_i);
                    func(data);

                    buff.reset_to(r_i);
                    m_i = m_b;
                    return true;
                }
            }

            if (!buff.done()) {
                data.append(range.first, range.second);
            }
        }

        if (!data.empty()) {
            func(data);
        }

        return false;
    }
};

}
