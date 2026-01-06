#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "dhcpv6_agent.h"

void handle_signal(int sig) {
    (void)sig;
    // Notify module to stop
    dhcpv6_agent_stop();
}

int main(int argc, char** argv) {
   (void)argc; (void)argv;
   
   // Setup signals in the main process thread
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = handle_signal;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
   
   printf("Starting DHCPv6 Standalone Agent...\n");
   dhcpv6_agent_start(NULL);
   printf("DHCPv6 Agent exited.\n");
   
   return 0;
}
