#pragma once
#include "GameState.h"
#include "ClientConnection.h"
#include "GameEngine.h"
#include "GameContext.h"
#include "SQLiteDatabase.h"
#include "picosha2.h"
#include <random>

class PasswordResetState : public GameState
{
public:
	enum class ResetStep {
		USERNAME,
		OLD_PASSWORD,
		NEW_PASSWORD,
		CONFIRM_PASSWORD
	};

	PasswordResetState() : step(ResetStep::USERNAME) {}

	void OnEnter(ClientConnection* client) override {
		client->QueueMessage("\r\n=== PASSWORD RESET ===\r\n");
		client->QueueMessage("Enter your username:\r\n");
	}

	void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
		if (p.empty()) return;

		GameEngine* engine = client->GetEngine();

		switch (step) {
		case ResetStep::USERNAME:
			tempUsername = p[0];
			
			if (!engine->gameContext.db->PlayerExists(tempUsername)) {
				client->QueueMessage("User not found. Returning to main menu...\r\n");
				client->PopState();
				return;
			}

			step = ResetStep::OLD_PASSWORD;
			client->QueueMessage("Enter your current password:\r\n");
			break;

		case ResetStep::OLD_PASSWORD:
			if (!engine->gameContext.db->VerifyPassword(tempUsername, p[0])) {
				client->QueueMessage("Incorrect password. Returning to main menu...\r\n");
				client->PopState();
				return;
			}

			step = ResetStep::NEW_PASSWORD;
			client->QueueMessage("Enter your new password:\r\n");
			break;

		case ResetStep::NEW_PASSWORD:
			if (p[0].length() < 4) {
				client->QueueMessage("Password must be at least 4 characters. Try again:\r\n");
				return;
			}

			newPassword = p[0];
			step = ResetStep::CONFIRM_PASSWORD;
			client->QueueMessage("Confirm your new password:\r\n");
			break;

		case ResetStep::CONFIRM_PASSWORD:
			if (p[0] != newPassword) {
				client->QueueMessage("Passwords do not match. Password reset cancelled.\r\n");
				client->PopState();
				return;
			}

			// Generate new salt and hash
			std::string salt = GenerateSalt();
			std::string combined = newPassword + salt;
			std::string hash = picosha2::hash256_hex_string(combined);

			if (engine->gameContext.db->UpdatePassword(tempUsername, hash, salt)) {
				client->QueueMessage("Password updated successfully! Returning to main menu...\r\n");
			} else {
				client->QueueMessage("Failed to update password. Please try again later.\r\n");
			}
			client->PopState();
			break;
		}
	}

private:
	ResetStep step;
	std::string tempUsername;
	std::string newPassword;

	std::string GenerateSalt(int length = 16) {
		const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*";
		std::default_random_engine rng(std::random_device{}());
		std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

		std::string salt;
		for (int i = 0; i < length; ++i) {
			salt += charset[dist(rng)];
		}
		return salt;
	}
};
