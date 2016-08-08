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

class peer
{
	private:
		asio::io_service& io;
		ip::tcp::acceptor acceptor;
		std::vector<ip::tcp::socket> peers;
		using peer_it = decltype(peers)::iterator;

		template<class T>
		T recieve(ip::tcp::socket& peer, asio::yield_context yield) {
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
			return msg;
		}

		void send(gp::MessageLite& msg, ip::tcp::socket& peer, asio::yield_context yield) {
			en::big_int32_buf_t length {msg.ByteSize()};
			std::vector<char> encoded_msg(length.value());
			msg.SerializeToArray(encoded_msg.data(), encoded_msg.size());
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
			msg.set_index(peers.size() + 1);
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
			std::cout << "Bootstrapper is peer #1 of " << count << "\n";
			// We are the 0th peer
			for(int i = 1; i < count; i++) {
				std::cout << "Awaiting peer #" << i + 1 << " of " << count << "\n";
				peers.emplace_back(io);
				auto& next_peer = peers.back();
				acceptor.async_accept(next_peer, yield);
				std::cout << "Peer #" << i + 1<< " of " << count << " connected from " << next_peer.remote_endpoint() << "\n";
				welcome_peer(count, yield);
				notify_peer_join(yield);
			}
		}

		void join_net(asio::yield_context yield, ip::tcp::endpoint host_endpoint) {
			peers.emplace_back(io);
			ip::tcp::socket& host = peers.back();
			host.async_connect(host_endpoint, yield);

			auto welcome_msg = recieve<class welcome_peer>(host, yield);
			int me_index = welcome_msg.index();
			int index = me_index;
			int count = welcome_msg.count();
			std::cout << index << "/" << count << "\n";
			for(; index < count - me_index; index++) {
				auto peer_join_msg = recieve<class notify_peer_join>(host, yield);
				peers.emplace_back(io);
				peers.back().async_connect({
					ip::address::from_string(peer_join_msg.address()),
					port
				}, yield);
			}
		}

	public:
		peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, int peer_count) :
			io(io),
			acceptor(io, local_endpoint) {
			asio::spawn(io, [peer_count,this](auto yc) { bootstrap_net(yc, peer_count); });
		}

		peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, ip::tcp::endpoint bootstrapper_endpoint) :
			io(io),
			acceptor(io, local_endpoint) {
			asio::spawn(io, [bootstrapper_endpoint, this](auto yc) { join_net(yc, bootstrapper_endpoint); });
		}
};

int main() {
	try {
		asio::io_service io;

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
