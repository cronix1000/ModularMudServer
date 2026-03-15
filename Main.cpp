#include "Server.h"
#include "GameEngine.h"
#include "GameContext.h"
#include "ClientInput.h"

#include <thread>
#include <iostream>
#include <string>
#include <atomic>

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
			// EOF or error - trigger shutdown
			consoleQueue.Push("quit");
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

int main(void) {
	GameContext ctx;
	ThreadSafeQueue<ClientInput> inputQueue;
	GameEngine engine(ctx, inputQueue);
	Server server(ctx, &engine, inputQueue);
	
	if (!server.Start(DEFAULT_PORT)) {
		std::cerr << "Failed to start server!" << std::endl;
		return 1;
	}
	
	// Start network thread
	std::thread networkThread([&server]() {
		server.Run();
	});
	networkThread.detach();
	
	// Start console input thread
	std::thread consoleThread(ConsoleInputThread);
	consoleThread.detach();

	// 2. Run the Game Engine on the Main Thread
	// This is your "New Loop"
	const int TICKS_PER_SECOND = 30;
	const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;

	while (engine.IsRunning()) {
		auto next_game_tick = GetTickCountMs() + SKIP_TICKS;

		// Process client inputs from network
		engine.ProcessInputs();
		
		// Process console commands
		std::string consoleCommand;
		while (consoleQueue.TryPop(&consoleCommand).has_value()) {
			ProcessConsoleCommand(consoleCommand, &engine, ctx, server);
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
	std::cout << "Server shutting down..." << std::endl;
	server.Stop();
	
	return 0;
}
