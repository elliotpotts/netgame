#include <iostream>
#include <boost/asio.hpp>
#include "peer.hpp"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

int main() {
	try {
		asio::io_service io;

		const int port = 3692;
		auto bootstrapper_endpoint = ip::tcp::endpoint{ip::address::from_string("192.168.1.84"), port};

		peer a(io, bootstrapper_endpoint, 4);
		peer b(io, ip::tcp::endpoint{ip::address::from_string("192.168.1.101"), port}, bootstrapper_endpoint);
		peer c(io, ip::tcp::endpoint{ip::address::from_string("192.168.1.102"), port}, bootstrapper_endpoint);
		peer d(io, ip::tcp::endpoint{ip::address::from_string("192.168.1.103"), port}, bootstrapper_endpoint);

		io.run();
	} catch (boost::exception& e) {
		std::cout << boost::diagnostic_information(e, true) << "\n";
	}
}
