#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>   // waitpid
#include <signal.h>     // signal, SIGCHLD
#include <fcntl.h>      // O_WRONLY
#include <errno.h>

#include <string>
#include <vector>
#include <map>

#include "lib/utility.hpp"
#include "lib/user-shell.hpp"

int DEBUG = 0;

ClientInfo clientList[MAX_CLIENT_NUMBER];
ClientInfo *currentClient;

void broadcast(const char *format, ...) {
    if (DEBUG) printf("[broadcast] From Client #%d\n", currentClient->id);
    for (int id = 0; id < MAX_CLIENT_NUMBER; ++id) {
        if (!clientList[id].online) {
            continue;
        }
        if (DEBUG) printf("[sigusr1-handler] Server is further broadcasting to Client #%d\n", clientList[id].id);
        char message[MAX_BUFFER_SIZE];
        va_list args;
        va_start(args, format);
        vsnprintf(message, MAX_BUFFER_SIZE, format, args);
        va_end(args);
        message[strlen(message)] = '\0';
        clientList[id].write(message);
    }
    if (DEBUG) printf("[broadcast] From Client #%d completed\n", currentClient->id);
}

void unicast(int userID, const char *format, ...) {
    if (DEBUG) printf("[unicast] From Client #%d\n", currentClient->id);
    int id = userID - 1;
    if (id < 0 || id > 30) {
        writeFD(currentClient->fd, "*** Error: user #%d is invalid (should between 1-30). ***\n", userID);
    }
    if (!clientList[id].online) {
        writeFD(currentClient->fd, "*** Error: user #%d does not exist yet. ***\n", userID);
        return;
    }
    char message[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    message[strlen(message)] = '\0';
    clientList[id].write(message);
    if (DEBUG) printf("[unicast]  From Client #%d completed\n", currentClient->id);
}

void waitChildTerminationHandler(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        printf("[sigchild-handler] Child (pid %d) terminated\n", pid);
    }
}

void registerPerChildSignalHandlers() {
    //signal(SIGCHLD, SIG_IGN);
    signal(SIGCHLD, waitChildTerminationHandler);
}

void registerGlobalSignalHandlers() {
    signal(SIGCHLD, waitChildTerminationHandler);
}

int main(int argc, char const *argv[]) {
    if (argc == 3) {
        DEBUG = atoi(argv[2]);
    }
    if (argc < 2) {
        die("Usage: %s PORT\n", argv[0]);
    }
    int port = strtoul(argv[1], NULL, 10);
    if (port == 0 || port > 65535) {
        die("Error: invalid PORT format, should be a number between 0-65535\n");
    }

    registerGlobalSignalHandlers();
    printf("[main] chdir to %s returns %d\n", RAS_ROOT, chdir(RAS_ROOT));

    pid_t pid = getpid();
    int listenfd = getListenSocket(port);
    printf("[main] Server (pid %d) is listening port %d\n", pid, port);

    int maxfd = listenfd;
    int maxClientID = -1;
    int nready = -1;
    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    struct sockaddr_in cliAddr;
    for (size_t seq=1;; seq++) {
        rset = allset;
        printf("[main] select is probing readFDset (seq=%lu)\n", seq);
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)) {
            int connfd = getAcceptSocket(listenfd, cliAddr);
            printf("[main] connfd %d probed\n", connfd);
            int id = 0;
            for (id=0; id<FD_SETSIZE; ++id) {
                if (!clientList[id].isOnline()) {
                    clientList[id].login(id+1, cliAddr, connfd, pid, clientList);
                    clientList[id].dump();
                    break;
                }
            }
            if (id >= MAX_CLIENT_NUMBER) {
                writeFD(connfd, "Too many clients (>= 30)\n");
                continue;
            }
            FD_SET(connfd, &allset);
            if (connfd > maxfd) {
                maxfd = connfd;
            }
            if (id > maxClientID) {
                maxClientID = id;
            }
            if (--nready <= 0) {
                continue;
            }
        }
        for (int i=0; i<=maxClientID; ++i) {
            if (!clientList[i].isOnline()) {
                continue;
            }
            if (!FD_ISSET(clientList[i].fd, &rset)) {
                continue;
            }
            currentClient = &clientList[i];
            if (!clientList[i].shell->processCommandline()) {
                printf("[rwg] Client #%d (pid %d) left\n", i+1, pid);
                int fd = clientList[i].fd;
                clientList[i].logout();
                FD_CLR(fd, &allset);
                if (DEBUG) printf("[rwg] Client #%d logged out\n", i+1);
                if (--nready <= 0) {
                    break;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

