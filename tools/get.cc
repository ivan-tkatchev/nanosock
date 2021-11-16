#include <iostream>
#include "../http_request.h"

struct Response {

    void version(const std::string& version) {
        std::cout << "VERSION: " << version << std::endl;
    }

    void code(const std::string& code) {
        std::cout << "CODE: " << code << std::endl;
    }

    void header(const std::string& key, const std::string& val) {
        std::cout << "HEADER: " << key << "\t|\t" << val << std::endl;
    }

    void body(const std::string& body) {
        std::cout << "BODY: " << body << std::endl;
    }
};

int main(int argc, char** argv) {
    try {
        Response res;
        nano::http::send(argv[1], std::stoul(argv[2]), 1000, "GET", argv[3], argv[4], res);

    } catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
