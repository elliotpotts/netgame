#include "peer.hpp"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace en = boost::endian;
namespace gp = google::protobuf;

void peer::send(gp::MessageLite& msg, ip::tcp::socket& peer, asio::yield_context yield) {
	en::big_int32_buf_t length {msg.ByteSize()};
	std::vector<char> encoded_msg(length.value());
	msg.SerializeToArray(encoded_msg.data(), encoded_msg.size());
	asio::async_write(peer, asio::buffer(&length, sizeof(length)), yield);
	asio::async_write(peer, asio::buffer(encoded_msg.data(), msg.ByteSize()), yield);
}

void peer::broadcast(gp::MessageLite& msg, peer_it it, peer_it end, asio::yield_context yield) {
	for(; it != end; it++) {
		send(msg, *it, yield);
	}
}

void peer::broadcast(gp::MessageLite& msg, asio::yield_context yield) {
	broadcast(msg, peers.begin(), peers.end(), yield);
}

// Send message actions

void peer::welcome_peer(int count, asio::yield_context yield) {
	class welcome_peer msg;
	msg.set_index(peers.size() + 1);
	msg.set_count(count);
	send(msg, peers.back(), yield);
}

void peer::notify_peer_join(asio::yield_context yield) {
	class notify_peer_join msg;
	msg.set_address(peers.back().remote_endpoint().address().to_string());
	broadcast(msg, peers.begin(), peers.end() - 1, yield);
}

//

void peer::bootstrap_net(asio::yield_context yield, int count) {
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

void peer::join_net(asio::yield_context yield, ip::tcp::endpoint host_endpoint) {
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
		peers.back().async_connect(
			{ip::address::from_string(peer_join_msg.address()),
					acceptor.local_endpoint().port()},
			yield);
	}
}

peer::peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, int peer_count) :
	io(io),
	acceptor(io, local_endpoint) {
	asio::spawn(io, [peer_count,this](auto yc) { bootstrap_net(yc, peer_count); });
}

peer::peer(asio::io_service& io, ip::tcp::endpoint local_endpoint, ip::tcp::endpoint bootstrapper_endpoint) :
	io(io),
	acceptor(io, local_endpoint) {
	asio::spawn(io, [bootstrapper_endpoint, this](auto yc) { join_net(yc, bootstrapper_endpoint); });
}
