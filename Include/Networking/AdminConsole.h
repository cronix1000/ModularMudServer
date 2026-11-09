#pragma once

#include "Platform.h"
#include "ThreadSafeQueue.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <stop_token>

struct AdminRequest {
	int connId = 0;
	long long reqId = 0;
	std::string command;
};

struct AdminResponse {
	int connId = 0;
	long long reqId = 0;
	bool ok = true;
	std::string output;
};

class AdminConsole {
public:
	bool Start(const std::string& port, const std::string& password);
	void Run(std::stop_token stopToken);
	void Stop();
	void Respond(const AdminResponse& response);
	ThreadSafeQueue<AdminRequest>& Requests() { return requests_; }
	bool IsEnabled() const { return enabled_; }

private:
	struct ConnState {
		SocketType socket = INVALID_SOCKET_VAL;
		bool authenticated = false;
		bool dead = false;
		std::string inBuffer;
		std::string outBuffer;
	};

	void AcceptConnection();
	void HandleLine(ConnState& conn, int connId, const std::string& line);
	void QueueSend(ConnState& conn, const std::string& text);
	void DrainResponses();
	void CloseAll();

	SocketType listenSocket_ = INVALID_SOCKET_VAL;
	std::string password_;
	bool enabled_ = false;
	std::atomic<int> nextConnId_{ 1 };
	std::map<int, ConnState> conns_;
	ThreadSafeQueue<AdminRequest> requests_;
	ThreadSafeQueue<AdminResponse> responses_;
};
