
#include <iostream>

#include "nanosock.h"


int main(int argc, char** argv) {

    try {

        nano::Socket sock(argv[1], std::stoul(argv[2]));
        nano::Buffer buff;

        std::string body = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";

        sock.send(body);

        auto printer = [](const std::string& s) {
                           std::cout << s;
                       };

        nano::Reader read_status("\r\n");
        nano::Reader read_headers("\r\n\r\n");
        nano::Reader read_lines("\n");

        std::cout << "STATUS:" << std::endl;

        if (!read_status(buff, sock, printer)) {
            throw std::runtime_error("HTTP server returned no status line.");
        }

        std::cout << std::endl << "HEADERS:" << std::endl;

        if (!read_headers(buff, sock, printer)) {
            throw std::runtime_error("HTTP server returned no headers.");
        }

        std::cout << "BODY:" << std::endl;

        while (read_lines(buff, sock, printer)) {
            //
        }

    } catch (std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
    }

    return 0;
}
