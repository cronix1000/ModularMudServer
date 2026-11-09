#pragma once
#include <string>
#include "ClientConnection.h"
class MenuManager
{
public:
	MenuManager();
	~MenuManager();

	// load menus from io

	// Builder that builds menus based on stats
	void BuildMenu(ClientConnection* client, std::string& replacements) {

	}

private:

};

MenuManager::MenuManager()
{
}

MenuManager::~MenuManager()
{
}