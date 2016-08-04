#include <iostream>
#include <thread>
#include <vector>
#include <string>
//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <boost/endian/buffers.hpp>
#include <cstdint>
#include <google/protobuf/message_lite.h>
#include "messages.message.pb.h"
#include <sstream>
#include <utility>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace en = boost::endian;
namespace gp = google::protobuf;

const int port = 3692;
const auto accepting_endpoint = ip::tcp::endpoint(ip::address_v6::loopback(), port);

class client
{
	private:
		asio::io_service& io;
		ip::tcp::acceptor acceptor;
		std::vector<ip::tcp::socket> peers;
		using peer_it = decltype(peers)::iterator;

		template<class T>
		T&& recieve(ip::tcp::socket& peer, asio::yield_context yield) {
			en::big_int32_buf_t length;
			asio::async_read(peer, asio::buffer(&length, sizeof(length)), yield);
			std::vector<char> encoded_msg(length.value());
			asio::async_read(peer, asio::buffer(encoded_msg.data(), encoded_msg.size()), yield);
			T msg;
			if(!msg.ParseFromArray(encoded_msg.data(), encoded_msg.size())) {
				std::ostringstream err_sstream;
				err_sstream << "Expected ";
				err_sstream << T::descriptor()->full_name();
				err_sstream << " but got ";
				err_sstream << msg.GetTypeName();
				throw std::runtime_error(err_sstream.str());
			}
			return std::move(msg);
		}

		void send(gp::MessageLite& msg, ip::tcp::socket& peer, asio::yield_context yield) {
			std::string encoded_msg = msg.SerializeAsString();
			en::big_int32_buf_t length {msg.ByteSize()};
			asio::async_write(peer, asio::buffer(&length, sizeof(length)), yield);
			asio::async_write(peer, asio::buffer(encoded_msg.data(), msg.ByteSize()), yield);
		}

		void broadcast(gp::MessageLite& msg, peer_it it, peer_it end, asio::yield_context yield) {
			for(; it != end; it++) {
				send(msg, *it, yield);
			}
		}

		void broadcast(gp::MessageLite& msg, asio::yield_context yield) {
			broadcast(msg, peers.begin(), peers.end(), yield);
		}

		// Send message actions

		void welcome_peer(int count, asio::yield_context yield) {
			class welcome_peer msg;
			msg.set_index(peers.size());
			msg.set_count(count);
			send(msg, peers.back(), yield);
		}

		void notify_peer_join(asio::yield_context yield) {
			class notify_peer_join msg;
			msg.set_address(peers.back().remote_endpoint().address().to_string());
			broadcast(msg, peers.begin(), peers.end() - 1, yield);
		}

		//

		void bootstrap_net(asio::yield_context yield, int count) {
			for(int i = 1; i < count + 1; i++) {
				std::cout << "Awaiting peer...\n";
				peers.emplace_back(io);
				auto& next_peer = peers.back();
				acceptor.async_accept(next_peer, yield);
				std::cout << "Peer #" << i << " of " << count << " connected from " << next_peer.remote_endpoint() << "\n";
				welcome_peer(count, yield);
				notify_peer_join(yield);
			}
		}

		void join_net(asio::yield_context yield, ip::tcp::endpoint host_endpoint) {
			peers.emplace_back(io);
			ip::tcp::socket& host = peers.back();
			host.async_connect(host_endpoint, yield);

			auto welcome_msg = recieve<class welcome_peer>(host, yield);
			int index = welcome_msg.index();
			int count = welcome_msg.count();

			for(; index < count; index++) {
				auto peer_join_msg = recieve<class notify_peer_join>(host, yield);
				peers.emplace_back(io);
				peers.back().async_connect({
					ip::address::from_string(peer_join_msg.address()),
					port
				}, yield);
			}
		}

	public:
		client(asio::io_service& io, int peer_count) :
			io(io),
			acceptor(io, accepting_endpoint) {
			asio::spawn(io, [&io,peer_count,this](auto yc) { bootstrap_net(yc, peer_count); });
		}

		client(asio::io_service& io, ip::tcp::endpoint host) :
			io(io),
			acceptor(io, accepting_endpoint) {
			asio::spawn(io, [&io,host,this](auto yc) { join_net(yc, host); });
		}
};

int main() {
	asio::io_service io;

	client a(io, 4);
	client b(io, accepting_endpoint);

	io.run();
}
