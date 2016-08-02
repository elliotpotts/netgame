#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

const auto localhost = ip::address::from_string("127.0.0.1");
const auto hosting_endpoint = ip::tcp::endpoint(localhost, 3692);

class client
{
	private:
		asio::io_service& io;
		ip::tcp::acceptor acceptor;
		std::vector<ip::tcp::socket> peers;

		void host_peers(asio::yield_context yield, int count) {
			for(int i = 0; i < count; i++) {
				peers.emplace_back(io);
				std::cout << "Awaiting peer...\n";
				acceptor.async_accept(peers.back(), yield);
				std::cout << "Peer #" << i + 1 << " of " << count << " connected from " << peers.back().remote_endpoint() << "\n";
			}
		}

		void join_peers(asio::yield_context yield, ip::tcp::endpoint host) {
		}

	public:
		client(asio::io_service& io, int peer_count) :
			io(io),
			acceptor(io, hosting_endpoint) {
			asio::spawn(io, [&io,peer_count,this](auto yc) { host_peers(yc, peer_count); });
		}

		client(asio::io_service& io, ip::tcp::endpoint host) :
			io(io),
			acceptor(io) {
			asio::spawn(io, [&io,host,this](auto yc) { join_peers(yc, host); });
		}
};

int main() {
	asio::io_service io;

	client c(io, 3);

	io.run();
}
