#ifndef __SOCK4_H__
#define __SOCK4_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <fstream>

#include "utils.h"
#include "sock4-common.h"

void log(const char *fmt, ...);

struct FirewallRule {
    std::string permit;
    char mode;
    std::string ip;
};

class Sock4 {
public:
    Sock4(int clientfd, struct sockaddr_in &cliAddr,
          const std::string &rulefile, const std::string &rulefile2);
    void setConnfd(int clientfd);
    void readRequest();
    bool passFirewallPolicy();
    bool passFirewallPolicy2();
    void setAcceptRequest(bool accepted);
    void writeResponse(uint16_t reply_port = 0, struct in_addr *reply_ip = NULL);
    void showMessage(const std::string &content = "");
    void startService();
    void connectMode();
    void bindMode();
    void redirectTraffic(int destfd);
private:
    int clientfd = -1;
    struct sockaddr_in &cliAddr;
    pid_t pid = -1;
    std::string rulefile;
    std::string rulefile2;
    char buffer[utils::MAX_BUFFER_SIZE] = {0};
    Sock4Request sock4request;
    Sock4Reply sock4reply;
    int cmdCode;
    std::string cmdName = "UNKNOWN";
    uint16_t dest_port;
    char *dest_ip;
    uint16_t source_port;
    char *source_ip;
    bool accepted = false;
};

#endif
