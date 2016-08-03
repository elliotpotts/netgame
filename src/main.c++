#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <boost/endian/buffers.hpp>
#include <cstdint>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

const int port = 3692;
const auto accepting_endpoint = ip::tcp::endpoint(ip::address_v6::loopback(), port);

using i32nb = boost::endian::big_int32_buf_t;

class client
{
	private:
		asio::io_service& io;
		ip::tcp::acceptor acceptor;
		std::vector<ip::tcp::socket> peers;

		void bootstrap_net(asio::yield_context yield, int count) {
			i32nb count_buf {count};
			for(int i = 1; i < count + 1; i++) {
				std::cout << "Awaiting peer...\n";

				peers.emplace_back(io);
				auto& next_peer = peers.back();
				acceptor.async_accept(next_peer, yield);
				std::cout << "Peer #" << i << " of " << count << " connected from " << next_peer.remote_endpoint() << "\n";

				i32nb i_buf {i};
				asio::async_write(next_peer, asio::buffer(i_buf.data(), sizeof(i_buf)), yield);
				asio::async_write(next_peer, asio::buffer(count_buf.data(), sizeof(count_buf)), yield);

				for(auto it = peers.begin(); it != peers.end() - 1; it++) {
					std::string encoded_endpoint = next_peer.remote_endpoint().address().to_string();
					i32nb encoded_endpoint_size {static_cast<std::int32_t>(encoded_endpoint.size())};
					asio::async_write(*it, asio::buffer(encoded_endpoint_size.data(), sizeof(encoded_endpoint_size)), yield);
					asio::async_write(*it, asio::buffer(encoded_endpoint.data(), encoded_endpoint.size()), yield);
				}
			}
		}

		void join_net(asio::yield_context yield, ip::tcp::endpoint host) {
		}

	public:
		client(asio::io_service& io, int peer_count) :
			io(io),
			acceptor(io, accepting_endpoint) {
			asio::spawn(io, [&io,peer_count,this](auto yc) { bootstrap_net(yc, peer_count); });
		}

		client(asio::io_service& io, ip::tcp::endpoint host) :
			io(io),
			acceptor(io) {
			asio::spawn(io, [&io,host,this](auto yc) { join_net(yc, host); });
		}
};

int main() {
	asio::io_service io;

	client c(io, 3);

	io.run();
}
