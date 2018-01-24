#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include "sock4.h"


int main(int argc, char *argv[]) {
    if (argc < 4) {
        utils::die("Usage: %s PORT DEST_CONF_FILE CLIENT_CONF_FILE\n", argv[0]);
    }
    int port = atoi(argv[1]);
    if (!utils::is_regular_file(argv[2])) {
        utils::die("Cannot open dest firewall rule file %s\n", argv[2]);
    }
    if (!utils::is_regular_file(argv[3])) {
        utils::die("Cannot open client firewall rule file %s\n", argv[3]);
    }
    int listenfd = utils::getListenSocket(port);
    printf("Sock4 server is listening to port %d\n", port);
    for (;;) {
        struct sockaddr_in cliAddr;
        int clientfd = utils::getAcceptSocket(listenfd, cliAddr);
        log("Accepted new request from [%s]\n", utils::getIPAddress(cliAddr));
        if (fork() == 0) {
            close(listenfd);
            Sock4 s4(clientfd, cliAddr, argv[2], argv[3]);
            s4.readRequest();
            if (!s4.passFirewallPolicy()) {
                s4.setAcceptRequest(false);
                s4.writeResponse();
                return 0;
            }
            if (!s4.passFirewallPolicy2()) {
                s4.setAcceptRequest(false);
                s4.writeResponse();
                return 0;
            }
            s4.setAcceptRequest(true);
            s4.startService();
            exit(0);
        }
        close(clientfd);
    }
    return 0;
}

