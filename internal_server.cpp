#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

void handle_request(tcp::socket& socket) {
    try {
        for(int i=0;i<10000000;i++);
        boost::asio::streambuf request_buffer;
        boost::asio::read_until(socket, request_buffer, "\r\n\r\n");

        string response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
        
        boost::asio::write(socket, boost::asio::buffer(response));
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

int main() {
    try {
        boost::asio::io_service io_service;

        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 3000));

        cout << "Server running on port 3000..." << endl;

        while (true) {
            tcp::socket socket(io_service);
            acceptor.accept(socket);
            handle_request(socket);
        }
    } catch (const std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
