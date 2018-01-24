#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include "sock4.h"

void log(const char *fmt, ...) {
    static FILE *fp = NULL;
    if (fp == NULL) {
        fp = fopen("debug-sock.log", "w");
    }
    va_list args;
    va_start(args, fmt);
    fprintf(fp, "[sock4] ");
    vfprintf(fp, fmt, args);
    fflush(fp);
    va_end(args);
}

Sock4::Sock4(int clientfd, struct sockaddr_in &cliAddr,
             const std::string &rulefile,
             const std::string &rulefile2)
    : clientfd(clientfd), cliAddr(cliAddr), pid(getpid()), rulefile(rulefile), rulefile2(rulefile2) {
    memset(&sock4request, 0, sizeof(sock4request));
    memset(&sock4reply, 0, sizeof(sock4reply));
    source_ip = utils::getIPAddress(cliAddr);
    source_port = cliAddr.sin_port;
}

void Sock4::setConnfd(int clientfd) {
    this->clientfd = clientfd;
}

void Sock4::readRequest() {
    ssize_t n = utils::readFD(clientfd, buffer);
    log("read %d bytes from clientfd %d (pid: %d)\n", n, clientfd, pid);
    if (n < 0) {
        close(clientfd);
        utils::die("Error: read bad file descriptor (sock4 pid=%d)\n", pid);
    } else if (n == 0) {
        log("read EOF (sock4 pid=%d)\n", pid);
        close(clientfd);
        return;
    }
    memcpy(&sock4request, buffer, SOCK4_MIN_PKT_SIZE);
    dest_port = ntohs(sock4request.DST_PORT);
    dest_ip = utils::getIPAddress(sock4request.DST_IP);
	cmdCode= sock4request.CD;
    switch (cmdCode) {
        case CONNECT:
            cmdName = "CONNECT";
            break;
        case BIND:
            cmdName = "BIND";
            break;
    }
    printf("[%d] Request = { VN: %d, CD: %d, DST_PORT: %d, DST_IP: %s }\n",
            pid,
            sock4request.VN,
            sock4request.CD,
            dest_port,
            dest_ip);
}

bool Sock4::passFirewallPolicy() {
    log("checking firewall policy (support runtime re-reading)...\n");
    std::ifstream ifs(rulefile);
    if (!ifs.good() || ifs.fail() || ifs.eof() || ifs.bad()) {
        utils::die("Failed to open sock conf file %s\n", rulefile.c_str());
    }
    FirewallRule rule;
    std::vector<FirewallRule> rules;
    while (!ifs.eof()) {
        ifs >> rule.permit >> rule.mode >> rule.ip >> std::ws;
        printf("[firewall][conf] %s %c %s\n", rule.permit.c_str(), rule.mode, rule.ip.c_str());
        if (!rule.permit.empty() && rule.permit[0] == '#') {
            continue;
        }
        rules.push_back(rule);
    }
    if (rules.size() == 0) {
        return true;
    }
    for (size_t i = 0; i < rules.size(); ++i) {
        FirewallRule &rule = rules[i];
        if (rule.mode == 'c' && cmdCode == CONNECT) {
            bool fullyMatched = true;
            for (size_t j = 0; j < rule.ip.size(); j++) {
                printf("[firewall][match][connect] rules[%lu].ip[%lu]=%c", i, j, rule.ip[j]);
                fflush(stdout);
                if (rule.ip[j] == '*') {
                    printf(" => wildcard matched!\n");
                    return true;
                }
                if (dest_ip[j] != rule.ip[j]) {
                    printf(" != %c => not match\n", dest_ip[j]);
                    fullyMatched = false;
                    break;
                }
                printf(" == %c\n", dest_ip[j]);
            }
            if (fullyMatched) {
                printf(" => fully matched!\n");
                return true;
            }
        }
        else if (rule.mode == 'b' && cmdCode == BIND) {
            bool fullyMatched = true;
            for (size_t j = 0; j < rule.ip.size(); j++) {
                printf("[firewall][match][bind] rules[%lu].ip[%lu]=%c", i, j, rule.ip[j]);
                fflush(stdout);
                if (rule.ip[j] == '*') {
                    printf(" => wildcard matched!\n");
                    return true;
                }
                if (dest_ip[j] != rule.ip[j]) {
                    printf(" != %c => not match\n", dest_ip[j]);
                    fullyMatched = false;
                    break;
                }
                printf(" == %c\n", dest_ip[j]);
            }
            if (fullyMatched) {
                printf(" => fully matched\n");
                return true;
            }
        }
    }
    printf("[firewall][match] => no rule matched\n");
    return false;
}

bool Sock4::passFirewallPolicy2() {
    log("checking firewall client policy (support runtime re-reading)...\n");
    std::ifstream ifs(rulefile2);
    if (!ifs.good() || ifs.fail() || ifs.eof() || ifs.bad()) {
        utils::die("Failed to open client's sock conf file %s\n", rulefile2.c_str());
    }
    FirewallRule rule;
    std::vector<FirewallRule> rules;
    while (!ifs.eof()) {
        ifs >> rule.permit >> rule.mode >> rule.ip >> std::ws;
        printf("[firewall][client][conf] %s %c %s\n", rule.permit.c_str(), rule.mode, rule.ip.c_str());
        if (!rule.permit.empty() && rule.permit[0] == '#') {
            continue;
        }
        rules.push_back(rule);
    }
    if (rules.size() == 0) {
        return true;
    }
    for (size_t i = 0; i < rules.size(); ++i) {
        FirewallRule &rule = rules[i];
        if (rule.mode == 'c' && cmdCode == CONNECT) {
            bool fullyMatched = true;
            for (size_t j = 0; j < rule.ip.size(); j++) {
                printf("[firewall][client][match][connect] rules[%lu].ip[%lu]=%c", i, j, rule.ip[j]);
                fflush(stdout);
                if (rule.ip[j] == '*') {
                    printf(" => wildcard matched!\n");
                    return true;
                }
                if (source_ip[j] != rule.ip[j]) {
                    printf(" != %c => not match\n", source_ip[j]);
                    fullyMatched = false;
                    break;
                }
                printf(" == %c\n", source_ip[j]);
            }
            if (fullyMatched) {
                printf(" => fully matched!\n");
                return true;
            }
        }
        else if (rule.mode == 'b' && cmdCode == BIND) {
            bool fullyMatched = true;
            for (size_t j = 0; j < rule.ip.size(); j++) {
                printf("[firewall][client][match][bind] rules[%lu].ip[%lu]=%c", i, j, rule.ip[j]);
                fflush(stdout);
                if (rule.ip[j] == '*') {
                    printf(" => wildcard matched!\n");
                    return true;
                }
                if (source_ip[j] != rule.ip[j]) {
                    printf(" != %c => not match\n", source_ip[j]);
                    fullyMatched = false;
                    break;
                }
                printf(" == %c\n", source_ip[j]);
            }
            if (fullyMatched) {
                printf(" => fully matched\n");
                return true;
            }
        }
    }
    printf("[firewall][client][match] => no rule matched\n");
    return false;
}

void Sock4::setAcceptRequest(bool accepted) {
    this->accepted = accepted;
}

void Sock4::writeResponse(uint16_t reply_port, struct in_addr *reply_ip) {
    sock4reply.VN = 0;
    sock4reply.CD = this->accepted? 90: 91;
    sock4reply.DST_PORT = reply_port? reply_port: sock4request.DST_PORT;
    memcpy(&sock4reply.DST_IP, reply_ip == NULL? &sock4request.DST_IP: reply_ip, IP_BYTE_SIZE);
    printf("[%d] Reply = { VN: %d, CD: %d, DST_PORT: %d, DST_IP: %s }\n",
            pid, sock4reply.VN, sock4reply.CD, ntohs(sock4reply.DST_PORT), utils::getIPAddress(sock4reply.DST_IP));
    memcpy(buffer, &sock4reply, SOCK4_MIN_PKT_SIZE);
    ssize_t n = write(clientfd, buffer, SOCK4_MIN_PKT_SIZE);
    if (n < 0) {
        close(clientfd);
        utils::die("Error: write bad file descriptor (sock4 pid=%d)\n", pid);
    }
    log("write %d bytes to clientfd %d (pid: %d)\n", n, clientfd, pid);
    showMessage();
}

void Sock4::showMessage(const std::string &content) {
    printf("<S_IP>    : %s\n", source_ip);
    printf("<S_PORT>  : %d\n", source_port);
    printf("<D_IP>    : %s\n", dest_ip);
    printf("<D_PORT>  : %d\n", dest_port);
    printf("<Command> : %s\n", cmdName.c_str());
    printf("<Reply>   : %s\n", accepted? "Accepted": "Reject");
    if (content.size() > 0) {
        printf("<Content> : %s\n", content.c_str());
    }
    printf("\n");
}

void Sock4::startService() {
    switch (cmdCode) {
        case CONNECT:
            return connectMode();
        case BIND:
            return bindMode();
    }
}

void Sock4::connectMode() {
    log("CONNECT mode detected\n");
    writeResponse();
    struct sockaddr_in cliAddr;
    int destfd = utils::getClientSocket(cliAddr, dest_ip, dest_port, false);
    int status = connect(destfd, (sockaddr*)&cliAddr, sizeof(cliAddr));
    if (status == -1) {
        log("%s\n", strerror(errno));
        utils::die("Error: failed to connect to destination host (ip: %s, port: %d)\n", dest_ip, dest_port);
    }
    log("connected to dest host status: %d, fd: %d ip: %s, port: %d\n", status, destfd, dest_ip, dest_port);
    redirectTraffic(destfd);
}

void Sock4::bindMode() {
    log("BIND mode detected\n");
    int listenfd = utils::getListenSocket(0);
    if (listenfd == -1) {
        utils::die("Error: failed to create passive TCP socket for BIND operation\n");
    }
    struct in_addr* ip;
    uint16_t port;
    std::tie(ip, port) = utils::getIPAndPort(listenfd);
    printf("listenfd = %d, port = %d, ip = %s\n", listenfd, ntohs(port), inet_ntoa(*ip));
    writeResponse(port, ip);
    struct sockaddr_in bindDestAddr;
    int bindfd = utils::getAcceptSocket(listenfd, bindDestAddr);
    if (bindfd == -1) {
        utils::die("Error: failed to accept from dest host for BIND operation\n");
    }
    std::tie(ip, port) = utils::getIPAndPort(bindfd);
    writeResponse(port, ip);
    redirectTraffic(bindfd);
}

void Sock4::redirectTraffic(int destfd) {
    int nfds = FD_SETSIZE;
    fd_set rfds;
    fd_set wfds;
    fd_set rs;
    fd_set ws;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_SET(destfd, &rs);
    FD_SET(destfd, &ws);
    FD_SET(clientfd, &rs);
    FD_SET(clientfd, &ws);
    std::string requestBuffer;
    std::string responseBuffer;
    for (;;) {
        memcpy(&rfds, &rs, sizeof(rfds));
        memcpy(&wfds, &ws, sizeof(wfds));
        if (select(nfds, &rfds, &wfds, NULL, NULL) == 0) {
            log("Nothing to read\n");
            break;
        }
        if (FD_ISSET(clientfd, &rfds) && requestBuffer.empty()) {
            log("1. read from client fd: %d\n", clientfd);
            ssize_t n = utils::readFD(clientfd, buffer);
            log("requestBuffer[%d]=[%s]\n", n, buffer);
            if (n == -1) {
                perror("read error: clientfd");
                break;
            }
            if (n == 0) {
                break;
            }
            requestBuffer.assign(buffer, n);
        }
        if (FD_ISSET(destfd, &wfds) && requestBuffer.size() > 0) {
            log("2. write to dest fd: %d\n", destfd);
            ssize_t n = write(destfd, requestBuffer.c_str(), requestBuffer.size());
            log("written %d bytes - requestBuffer[%d]=[%s]\n", n, requestBuffer.size(), requestBuffer.c_str());
            requestBuffer.clear();
        }
        if (FD_ISSET(destfd, &rfds) && responseBuffer.empty()) {
            log("3. read from dest fd: %d\n", destfd);
            ssize_t n = utils::readFD(destfd, buffer);
            log("requestBuffer[%d]=[%s]\n", n, buffer);
            if (n == -1) {
                perror("read error: destfd");
                break;
            }
            if (n == 0) {
                break;
            }
            responseBuffer.assign(buffer, n);
        }
        if (FD_ISSET(clientfd, &wfds) && responseBuffer.size() > 0) {
            log("4. write to client fd: %d\n", clientfd);
            ssize_t n = write(clientfd, responseBuffer.c_str(), responseBuffer.size());
            log("written %d bytes - responseBuffer[%d]=[%s]\n", n, requestBuffer.size(), requestBuffer.c_str());
            if (responseBuffer.size() > 0) {
                showMessage(responseBuffer.substr(0, responseBuffer.size() > 30? 30: responseBuffer.size()));
            }
            responseBuffer.clear();
        }
    }
}
