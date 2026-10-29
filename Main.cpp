#include "Platform.h"
#include "Server.h"
#include "AdminConsole.h"
#include "GameEngine.h"
#include "GameContext.h"
#include "ClientInput.h"
#include "ClientConnection.h"
#include "CommandRegistry.h"
#include "NameComponent.h"
#include "PermissionComponent.h"
#include "Registry.h"
#include "Platform.h"

#include <thread>
#include <iostream>
#include <string>
#include <atomic>
#include <memory>
#include <cstdlib>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include "Main.h"
#endif

#define DEFAULT_PORT "27015"

// Separate queue for console commands
ThreadSafeQueue<std::string> consoleQueue;
std::atomic<bool> consoleRunning{true};

void ConsoleInputThread() {
	std::cout << "Server console ready. Type 'help' for commands or 'quit' to shutdown." << std::endl;

	std::string line;
	while (consoleRunning) {
		std::cout << "> " << std::flush;
		if (!std::getline(std::cin, line)) {
			if (!consoleRunning) {
				break;
			}
#ifdef PLATFORM_LINUX
			if (isatty(STDIN_FILENO)) {
				consoleQueue.Push("quit");
			}
#endif
			break;
		}

		if (!line.empty()) {
			consoleQueue.Push(line);
		}
	}
}

void ProcessConsoleCommand(const std::string& command, GameEngine* engine, GameContext& ctx, Server& server) {
	if (command == "quit" || command == "shutdown" || command == "exit") {
		std::cout << "Shutting down server..." << std::endl;
		consoleRunning = false;
		engine->Quit();
	}
	else if (command == "help") {
		std::cout << "Available commands:" << std::endl;
		std::cout << "  quit/shutdown/exit - Stop the server gracefully" << std::endl;
		std::cout << "  status            - Show server status" << std::endl;
		std::cout << "  players           - List connected players" << std::endl;
		std::cout << "  save              - Force save all data" << std::endl;
		std::cout << "  help              - Show this help message" << std::endl;
	}
	else if (command == "status") {
		std::cout << "Server is running." << std::endl;
		std::cout << "Console queue size: " << consoleQueue.Size() << std::endl;
	}
	else if (command == "players") {
		std::cout << "Connected players: (feature to be implemented)" << std::endl;
	}
	else if (command == "save") {
		std::cout << "Saving all data..." << std::endl;
	}
	else {
		std::cout << "Unknown command: " << command << std::endl;
		std::cout << "Type 'help' for available commands." << std::endl;
	}
}

static std::string CollectConsoleOutput(ClientConnection* console) {
	std::string output;
	OutboundPacket pkt;
	while (console->outbound.TryPop(&pkt).has_value()) {
		output += pkt.data;
	}
	return output;
}

static std::string ToLower(const std::string& text) {
	std::string lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

static int FindPlayerEntityByName(GameEngine* engine, GameContext& ctx, const std::string& name) {
	std::string wanted = ToLower(name);
	for (auto& [clientId, client] : engine->sessions) {
		if (client->playerEntityID < 0) continue;
		auto* nameComp = ctx.registry->GetComponent<NameComponent>(client->playerEntityID);
		if (nameComp && ToLower(nameComp->displayName) == wanted) {
			return client->playerEntityID;
		}
	}
	return -1;
}

static std::string JoinWords(const std::vector<std::string>& words, size_t from) {
	std::string joined;
	for (size_t i = from; i < words.size(); ++i) {
		if (i > from) joined += " ";
		joined += words[i];
	}
	return joined;
}

static std::vector<std::string> SplitWords(const std::string& text) {
	std::vector<std::string> words;
	std::stringstream ss(text);
	std::string word;
	while (ss >> word) words.push_back(word);
	return words;
}

void ProcessAdminCommand(const AdminRequest& req, AdminConsole& adminConsole, GameEngine* engine,
						 GameContext& ctx, ClientConnection* console, unsigned long long startMs) {
	const std::string command = req.command;
	std::string output;
	bool ok = true;

	if (command == "help") {
		output =
			"Server verbs:\n"
			"  status                  - uptime, sessions, queues\n"
			"  players                 - list online players\n"
			"  save                    - force-save all players\n"
			"  broadcast <msg>         - message every online player\n"
			"  as <player> <command>   - run a command as an online player\n"
			"  restart                 - save + exit; the container comes back up\n"
			"  shutdown                - save + exit\n"
			"  help                    - this text\n"
			"Anything else runs as a registered game command with Admin permission\n"
			"(use 'as <player> ...' for commands that need a player context).";
	}
	else if (command == "status") {
		unsigned long long upMs = GetTickCountMs() - startMs;
		char buf[256];
		snprintf(buf, sizeof(buf),
			"uptime: %llud %lluh %llum %llus\nsessions: %zu\ninput queue: %zu\ngame time: %.1f",
			upMs / 86400000ULL, (upMs / 3600000ULL) % 24ULL, (upMs / 60000ULL) % 60ULL, (upMs / 1000ULL) % 60ULL,
			engine->sessions.size(), engine->inputQueue.Size(), static_cast<double>(engine->time));
		output = buf;
	}
	else if (command == "players") {
		int count = 0;
		output = "Online players:";
		for (auto& [clientId, client] : engine->sessions) {
			if (client->playerEntityID < 0) continue;
			auto* nameComp = ctx.registry->GetComponent<NameComponent>(client->playerEntityID);
			auto* permComp = ctx.registry->GetComponent<PermissionComponent>(client->playerEntityID);
			output += "\n  " + (nameComp ? nameComp->displayName : std::string("(unnamed)"))
				+ "  entity=" + std::to_string(client->playerEntityID)
				+ "  perm=" + std::to_string(permComp ? permComp->level : 0);
			count++;
		}
		if (count == 0) output += "\n  (none)";
	}
	else if (command == "save") {
		engine->SaveAllPlayers();
		output = "Saved all players.";
	}
	else if (command == "broadcast" || command.rfind("broadcast ", 0) == 0) {
		std::string message = command.size() > 10 ? command.substr(10) : "";
		if (message.empty()) {
			ok = false;
			output = "Usage: broadcast <message>";
		} else {
			std::string line = "*** " + message + "\r\n";
			int count = 0;
			for (auto& [clientId, client] : engine->sessions) {
				if (client->playerEntityID < 0) continue;
				client->QueueMessage(line);
				count++;
			}
			output = "Broadcast sent to " + std::to_string(count) + " player(s).";
		}
	}
	else if (command.rfind("as ", 0) == 0) {
		std::vector<std::string> words = SplitWords(command);
		if (words.size() < 3) {
			ok = false;
			output = "Usage: as <player> <command>";
		} else {
			int entity = FindPlayerEntityByName(engine, ctx, words[1]);
			if (entity < 0) {
				ok = false;
				output = "No online player named '" + words[1] + "'.";
			} else {
				std::string rest = JoinWords(words, 2);
				int previous = console->playerEntityID;
				console->playerEntityID = entity;
				ctx.commandRegistry->Execute(console, rest);
				console->playerEntityID = previous;
				output = CollectConsoleOutput(console);
			}
		}
	}
	else if (command == "restart") {
		output = "Saving players and exiting. The server will be brought back up automatically.";
		adminConsole.Respond({ req.connId, req.reqId, ok, output });
		engine->Quit();
		return;
	}
	else if (command == "shutdown" || command == "quit" || command == "exit") {
		output = "Saving players and shutting down. (Under Docker the restart policy brings it back up.)";
		adminConsole.Respond({ req.connId, req.reqId, ok, output });
		engine->Quit();
		return;
	}
	else {
		ctx.commandRegistry->Execute(console, command);
		output = CollectConsoleOutput(console);
		if (output.empty()) output = "(no output)";
	}

	adminConsole.Respond({ req.connId, req.reqId, ok, output });
}

int main(void) {
	GameContext ctx;
	ThreadSafeQueue<ClientInput> inputQueue;
	GameEngine engine(ctx, inputQueue);
	Server server(ctx, &engine, inputQueue);

	if (!server.Start(DEFAULT_PORT)) {
		return 1;
	}

	AdminConsole adminConsole;
	const char* adminToken = std::getenv("MUD_ADMIN_TOKEN");
	const char* adminPort = std::getenv("MUD_ADMIN_PORT");
	if (!adminConsole.Start(adminPort ? adminPort : "27016", adminToken ? adminToken : "")) {
		server.Stop();
		return 1;
	}

	// Start network thread
	std::jthread networkThread([&server](std::stop_token t) {
		server.Run(t);
	});

	std::jthread adminThread([&adminConsole](std::stop_token t) {
		adminConsole.Run(t);
	});

	auto consoleClient = std::make_shared<ClientConnection>(INVALID_SOCKET_VAL);
	consoleClient->consoleAdmin = true;
	consoleClient->SetEngine(&engine);
	consoleClient->clientID = -1;
	const unsigned long long startMs = GetTickCountMs();
	
	// Start console input thread only when stdin is a TTY (interactive).
	// In detached/backgrounded contexts (CI, docker compose up -d without -t,
	// systemd, etc.) stdin is a closed pipe, and reading EOF would push "quit"
	// and shut the server down within seconds.
#ifdef PLATFORM_LINUX
	if (isatty(STDIN_FILENO)) {
		std::thread consoleThread(ConsoleInputThread);
		consoleThread.detach();
	}

	//std::thread networkThread
#else
	// Windows: spawn console thread unconditionally; user can close the console window to shut down.
	std::thread consoleThread(ConsoleInputThread);
	consoleThread.detach();
#endif

	// 2. Run the Game Engine on the Main Thread
	// This is your "New Loop"
	const int TICKS_PER_SECOND = 30;
	const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;

	while (engine.IsRunning()) {
		auto next_game_tick = GetTickCountMs() + SKIP_TICKS;

		// Process console commands
		std::string consoleCommand;
		while (consoleQueue.TryPop(&consoleCommand).has_value()) {
			ProcessConsoleCommand(consoleCommand, &engine, ctx, server);
		}

		// Process admin console requests
		AdminRequest adminRequest;
		while (adminConsole.Requests().TryPop(&adminRequest).has_value()) {
			ProcessAdminCommand(adminRequest, adminConsole, &engine, ctx, consoleClient.get(), startMs);
		}

		// B. Update Game World
		engine.Update(0.033f);

		// C. Sleep to maintain framerate
		int sleep_time = static_cast<int>(next_game_tick - GetTickCountMs());
		if (sleep_time > 0) {
			SleepMs(sleep_time);
		}
	}
	// Graceful shutdown
	adminThread.request_stop();
	adminThread.join();
	networkThread.request_stop();
	networkThread.join(); // Blocks finishing the process until the thread is finished
	engine.SaveAllPlayers();


	server.Stop();

	return 0;
}
