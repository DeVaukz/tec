#include "server/client_connection.hpp"
#include "server/server.hpp"
#include "simulation.hpp"
#include <iostream>
#include <thread>

namespace tec {
	namespace networking {
		std::mutex ClientConnection::write_msg_mutex;
		void ClientConnection::StartRead() {
			read_header();
		}

		void ClientConnection::QueueWrite(const ServerMessage& msg) {
			bool write_in_progress;
			{
				std::lock_guard<std::mutex> lock(write_msg_mutex);
				write_in_progress = !write_msgs_.empty();
				write_msgs_.push_back(msg);
			}
			if (!write_in_progress) {
				do_write();
			}
		}

		void ClientConnection::read_header() {
			auto self(shared_from_this());
			asio::async_read(socket,
				asio::buffer(current_read_msg.GetDataPTR(), ServerMessage::header_length),
				[this, self] (std::error_code error, std::size_t /*length*/) {
				if (!error && current_read_msg.decode_header()) {
					read_body();
				}
				else {
					server->Leave(shared_from_this());
				}
			});
		}

		void ClientConnection::read_body() {
			auto self(shared_from_this());
			asio::async_read(socket,
				asio::buffer(current_read_msg.GetBodyPTR(), current_read_msg.GetBodyLength()),
				[this, self] (std::error_code error, std::size_t /*length*/) {
				if (!error) {
					if (current_read_msg.GetMessageType() == CHAT_MESSAGE) {
						server->Deliver(current_read_msg);
						std::cout.write(current_read_msg.GetBodyPTR(),
							current_read_msg.GetBodyLength());
						std::cout << std::endl;
					}
					else if (current_read_msg.GetMessageType() == SYNC) {
						QueueWrite(current_read_msg);
					}
					read_header();
				}
				else {
					server->Leave(shared_from_this());
				}
			});
		}

		void ClientConnection::do_write() {
			auto self(shared_from_this());
			std::lock_guard<std::mutex> lock(write_msg_mutex);
			asio::async_write(socket,
				asio::buffer(write_msgs_.front().GetDataPTR(),
				write_msgs_.front().length()),
				[this, self] (std::error_code error, std::size_t /*length*/) {
				if (!error) {
					bool more_to_write = false;
					{
						std::lock_guard<std::mutex> lock(write_msg_mutex);
						write_msgs_.pop_front();
						more_to_write = !write_msgs_.empty();
					}
					if (more_to_write) {
						do_write();
					}
				}
				else {
					server->Leave(shared_from_this());
				}
			});
		}
	}
}
