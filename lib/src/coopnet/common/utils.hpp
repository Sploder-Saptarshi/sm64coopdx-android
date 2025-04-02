#pragma once
#include <string>
#include <ctype.h>
#include "socket.hpp"

typedef struct {
	std::string host;
	std::string username;
	std::string password;
	uint16_t port;
} StunTurnServer;

in_addr_t GetAddrFromDomain(const std::string& domain);
float clock_elapsed(void);

std::string getExecutablePath();
std::size_t hashFile(const std::string &filepath = getExecutablePath());
