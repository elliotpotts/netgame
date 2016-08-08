#ifndef PEER_HPP_INCLUDED
#define PEER_HPP_INCLUDED

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <boost/endian/buffers.hpp>
#include <cstdint>
#include <google/protobuf/message_lite.h>
#include "messages.message.pb.h"
#include <sstream>
#include <utility>

namespace {
	namespace asio = boost::asio;
	namespace ip = boost::asio::ip;
	namespace en = boost::endian;
	namespace gp = google::protobuf;
}
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

		void send(gp::MessageLite& msg, ip::tcp::socket& peer, asio::yield_context yield);
		void broadcast(gp::MessageLite& msg, peer_it it, peer_it end, asio::yield_context yield);
		void broadcast(gp::MessageLite& msg, asio::yield_context yield);
		void welcome_peer(int count, asio::yield_context yield);
		void notify_peer_join(asio::yield_context yield);
		void bootstrap_net(asio::yield_context yield, int count);
		void join_net(asio::yield_context yield, ip::tcp::endpoint host_endpoint);

	public:
		peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, int peer_count);
		peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, ip::tcp::endpoint bootstrapper_endpoint);
};

#endif
