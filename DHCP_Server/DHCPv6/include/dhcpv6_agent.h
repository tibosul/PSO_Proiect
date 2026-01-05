#ifndef DHCPV6_AGENT_H
#define DHCPV6_AGENT_H

/**
 * @brief Entry point for the DHCPv6 Server Agent.
 * 
 * This function initializes the server, loads configuration, 
 * starts worker threads, and enters the main event loop.
 * It is intended to be run in a separate thread if part of a unified server.
 * 
 * @param arg Unused for now (can pass config path later).
 * @return void* Returns NULL on exit.
 */
void* dhcpv6_agent_start(void* arg);

/**
 * @brief Signals the DHCPv6 Agent to stop.
 */
void dhcpv6_agent_stop();

#endif // DHCPV6_AGENT_H
