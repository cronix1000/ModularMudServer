#include "AdminConsole.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdio>
#include <cstring>

using json = nlohmann::json;

namespace {
	constexpr size_t MAX_LINE_BYTES = 64 * 1024;
	constexpr unsigned int SELECT_TIMEOUT_US = 50 * 1000;

	bool Listen(const char* port, SocketType& outSocket) {
		struct addrinfo* result = nullptr;
		struct addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		if (getaddrinfo(NULL, port, &hints, &result) != 0) {
			printf("Admin console: getaddrinfo failed: %d\n", GetSocketError());
			return false;
		}

		SocketType sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (sock == INVALID_SOCKET_VAL) {
			printf("Admin console: socket failed: %d\n", GetSocketError());
			freeaddrinfo(result);
			return false;
		}

		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
#ifdef PLATFORM_WINDOWS
			(const char*)
#else
			(const void*)
#endif
			&opt, sizeof(opt));

		if (bind(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR_VAL ||
			listen(sock, SOMAXCONN) == SOCKET_ERROR_VAL) {
			printf("Admin console: bind/listen on port %s failed: %d\n", port, GetSocketError());
			freeaddrinfo(result);
			CloseSocket(sock);
			return false;
		}

		freeaddrinfo(result);
		outSocket = sock;
		return true;
	}
}

bool AdminConsole::Start(const std::string& port, const std::string& password) {
	password_ = password;
	enabled_ = !password_.empty();
	if (!enabled_) {
		std::cout << "Admin console disabled (MUD_ADMIN_TOKEN not set)" << std::endl;
		return true;
	}

#ifdef PLATFORM_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "Admin console: WSAStartup failed" << std::endl;
		enabled_ = false;
		return false;
	}
#endif

	if (!Listen(port.c_str(), listenSocket_)) {
#ifdef PLATFORM_WINDOWS
		WSACleanup();
#endif
		enabled_ = false;
		return false;
	}

	std::cout << "Admin console listening on port " << port << std::endl;
	return true;
}

void AdminConsole::Run(std::stop_token stopToken) {
	while (!stopToken.stop_requested()) {
		DrainResponses();

		fd_set readFd;
		fd_set writeFd;
		FD_ZERO(&readFd);
		FD_ZERO(&writeFd);

		SocketType maxFd = INVALID_SOCKET_VAL;
		if (listenSocket_ != INVALID_SOCKET_VAL) {
			FD_SET(listenSocket_, &readFd);
			maxFd = listenSocket_;
		}
		for (auto& [id, conn] : conns_) {
			FD_SET(conn.socket, &readFd);
			if (!conn.outBuffer.empty()) {
				FD_SET(conn.socket, &writeFd);
			}
			if (conn.socket > maxFd) {
				maxFd = conn.socket;
			}
		}
		if (maxFd == INVALID_SOCKET_VAL) {
			SleepMs(50);
			continue;
		}

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = SELECT_TIMEOUT_US;

		int ready = select(static_cast<int>(maxFd) + 1, &readFd, &writeFd, NULL, &timeout);
		if (ready == SOCKET_ERROR_VAL) {
			printf("Admin console: select failed: %d\n", GetSocketError());
			break;
		}

		if (listenSocket_ != INVALID_SOCKET_VAL && FD_ISSET(listenSocket_, &readFd)) {
			AcceptConnection();
		}

		for (auto it = conns_.begin(); it != conns_.end();) {
			ConnState& conn = it->second;

			if (!conn.dead && FD_ISSET(conn.socket, &readFd)) {
				char buf[4096];
				int bytes = recv(conn.socket, buf, sizeof(buf), 0);
				if (bytes <= 0) {
					conn.dead = true;
				} else {
					conn.inBuffer.append(buf, static_cast<size_t>(bytes));
					size_t pos;
					while ((pos = conn.inBuffer.find('\n')) != std::string::npos) {
						std::string line = conn.inBuffer.substr(0, pos);
						conn.inBuffer.erase(0, pos + 1);
						if (!line.empty() && line.back() == '\r') line.pop_back();
						if (!line.empty()) HandleLine(conn, it->first, line);
						if (conn.dead) break;
					}
					if (conn.inBuffer.size() > MAX_LINE_BYTES) {
						conn.dead = true;
					}
				}
			}

			if (!conn.dead && FD_ISSET(conn.socket, &writeFd) && !conn.outBuffer.empty()) {
				int sent = send(conn.socket, conn.outBuffer.c_str(),
					static_cast<int>(conn.outBuffer.size()), 0);
				if (sent < 0) {
					conn.dead = true;
				} else {
					conn.outBuffer.erase(0, static_cast<size_t>(sent));
				}
			}

			if (conn.dead) {
				CloseSocket(conn.socket);
				it = conns_.erase(it);
			} else {
				++it;
			}
		}
	}

	DrainResponses();
	CloseAll();
}

void AdminConsole::Stop() {
	if (listenSocket_ != INVALID_SOCKET_VAL) {
		CloseSocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET_VAL;
	}
}

void AdminConsole::Respond(const AdminResponse& response) {
	responses_.Push(response);
}

void AdminConsole::AcceptConnection() {
	SocketType sock = accept(listenSocket_, NULL, NULL);
	if (sock == INVALID_SOCKET_VAL) {
		return;
	}
	int connId = nextConnId_.fetch_add(1);
	ConnState& conn = conns_[connId];
	conn.socket = sock;
	QueueSend(conn, json{ {"ok", true}, {"proto", "mud-admin"}, {"version", 1} }.dump() + "\n");
}

void AdminConsole::HandleLine(ConnState& conn, int connId, const std::string& line) {
	json parsed = json::parse(line, nullptr, false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		QueueSend(conn, json{ {"ok", false}, {"error", "malformed request"} }.dump() + "\n");
		return;
	}

	if (!conn.authenticated) {
		if (parsed.value("auth", std::string()) == password_) {
			conn.authenticated = true;
			QueueSend(conn, json{ {"ok", true}, {"authenticated", true} }.dump() + "\n");
		} else {
			QueueSend(conn, json{ {"ok", false}, {"error", "invalid token"} }.dump() + "\n");
			conn.dead = true;
		}
		return;
	}

	AdminRequest req;
	req.connId = connId;
	req.reqId = parsed.value("id", 0LL);
	req.command = parsed.value("cmd", std::string());
	if (req.command.empty()) {
		QueueSend(conn, json{ {"id", req.reqId}, {"ok", true}, {"output", ""} }.dump() + "\n");
		return;
	}
	requests_.Push(req);
}

void AdminConsole::QueueSend(ConnState& conn, const std::string& text) {
	if (conn.outBuffer.size() + text.size() > MAX_LINE_BYTES) {
		conn.dead = true;
		return;
	}
	conn.outBuffer += text;
}

void AdminConsole::DrainResponses() {
	AdminResponse response;
	while (responses_.TryPop(&response).has_value()) {
		auto it = conns_.find(response.connId);
		if (it == conns_.end()) {
			continue;
		}
		json body{ {"id", response.reqId}, {"ok", response.ok}, {"output", response.output} };
		QueueSend(it->second, body.dump() + "\n");
	}
}

void AdminConsole::CloseAll() {
	for (auto& [id, conn] : conns_) {
		if (!conn.outBuffer.empty()) {
			send(conn.socket, conn.outBuffer.c_str(), static_cast<int>(conn.outBuffer.size()), 0);
		}
		CloseSocket(conn.socket);
	}
	conns_.clear();

	if (listenSocket_ != INVALID_SOCKET_VAL) {
		CloseSocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET_VAL;
	}
}
