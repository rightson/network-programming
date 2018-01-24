#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>   // waitpid
#include <signal.h>     // signal, SIGCHLD
#include <errno.h>

#include <string>
#include <vector>
#include <map>

#include "lib/utility.hpp"
#include "lib/user-shell.hpp"

int DEBUG = 0;

ClientInfo *clientList;
ClientInfo *currentClient;

void broadcast(const char *format, ...) {
    if (DEBUG) printf("[broadcast] From Client #%d\n", currentClient->id);
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    kill(getppid(), SIGUSR1);

    char message[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    message[strlen(message)] = '\0';

    if (writeFIFO(BROADCAST_FIFO, message) == 0) {
        printf("Error: Failed to write message to broadcast FIFO\n");
    }
    if (currentClient->online) { // for client exit command, it shoudn't be broadcasted and wll never receive SIGURS1
        sigsuspend(&oldmask);
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
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
    kill(clientList[id].pid, SIGUSR1);

    char message[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    message[strlen(message)] = '\0';

    clientList[id].fifosize++;
    writeFIFO(clientList[id].fifopath, message);
    if (DEBUG) printf("[unicast]  From Client #%d completed\n", currentClient->id);
}

void childBroadcastHandler(int signo) {
    if (DEBUG) printf("[sigusr1-handler-child] Client #%d broadcast/unicast handling\n", currentClient->id);
    char buffer[MAX_BUFFER_SIZE];
    if (currentClient->fifosize <= 0) {
        printf("[sigusr1-handler-child] Client #%d broadcast/unicast handling stopped: no notification\n", currentClient->id);
    }
    size_t n = readFIFO(currentClient->fifopath, buffer);
    if (n == 0) {
        printf("Error: childBroadcastHandler failed to read fifo [%s]\n", currentClient->fifopath);
    }
    currentClient->fifosize--;
    writeFD(currentClient->fd, buffer);
    writeFD(currentClient->fd, "% ");
    if (DEBUG) printf("[sigusr1-handler-child] Client #%d broadcast/unicast handled\n", currentClient->id);
}

void rootBroadcastHandler(int signo) {
    if (DEBUG) printf("[sigusr1-handler] Server broadcast handling\n");
    char buffer[MAX_BUFFER_SIZE];
    size_t n = readFIFO(BROADCAST_FIFO, buffer);
    if (n == 0) {
        printf("[ERROR] Failed to read broadcast FIFO\n");
    }
    for (int id = 0; id < MAX_CLIENT_NUMBER; ++id) {
        if (!clientList[id].online) {
            continue;
        }
        kill(clientList[id].pid, SIGUSR1);
        if (DEBUG) printf("[sigusr1-handler] Server is further broadcasting to Client #%d\n", clientList[id].id);
        clientList[id].fifosize++;
        writeFIFO(clientList[id].fifopath, buffer);
    }
    if (DEBUG) printf("[sigusr1-handler] Server broadcast handled\n");
}

void waitChildTerminationHandler(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        printf("[sigchild-handler] Child (pid %d) terminated\n", pid);
    }
}

void cleanGlobalObjects(int signo) {
    unlink(BROADCAST_FIFO);
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
        char fifopath[MAX_PATH_LENGTH];
	    snprintf(fifopath, MAX_PATH_LENGTH, "%s%d.fifo", UNICAST_FIFO_PREFIX, i);
        unlink(fifopath);
    }
    destroyClientInfoList(clientList);
    resetAllPipes();
    exit(EXIT_SUCCESS);
}

void registerPerChildSignalHandlers() {
    //signal(SIGCHLD, SIG_IGN);
    signal(SIGCHLD, waitChildTerminationHandler);
    signal(SIGUSR1, childBroadcastHandler);
}

void registerGlobalSignalHandlers() {
    signal(SIGCHLD, waitChildTerminationHandler);
    signal(SIGUSR1, rootBroadcastHandler);
    signal(SIGINT, cleanGlobalObjects);
}

void enterRemoteWorkingGround(struct sockaddr_in &cliAddr, int connfd, pid_t selfpid) {
    int id = getAvailableClientID(clientList);
    if (id >= MAX_CLIENT_NUMBER) {
        writeFD(connfd, "Too many clients\n");
        close(connfd);
        exit(EXIT_SUCCESS);
    }
    currentClient = clientList[id].enable(id+1, cliAddr, connfd, selfpid, clientList);
    currentClient->enableFIFO(clientList);
    currentClient->shell->showLoginMessage();
    currentClient->shell->broadcastUserLoginMessage();
    currentClient->shell->showPrompt();
    for (;;) {
        if (!currentClient->shell->processCommandline()) {
            break;
        }
    }
    printf("[rwg] Client #%d (pid %d) left\n", id+1, selfpid);
    clientList[id].logout();
    if (DEBUG) printf("[rwg] Client #%d logged out\n", id+1);
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

    printf("[main] chdir to %s returns %d\n", RAS_ROOT, chdir(RAS_ROOT));

    int listenfd = getListenSocket(port);
    printf("[main] Server (pid %d) is listening port %d\n", getpid(), port);

    clientList = createClientInfoListWithSharedMemory();
    createFIFO(BROADCAST_FIFO);
    registerGlobalSignalHandlers();
    for (;;) {
        struct sockaddr_in cliAddr;
        int connfd = getAcceptSocket(listenfd, cliAddr);
        if (connfd == -1) {
            continue;
        }
        pid_t pid = fork();
        if (pid == -1) {
            die("Failed to fork process");
        }
        if (pid == 0) { // child
            pid_t childpid = getpid();
            if (DEBUG) printf("[main] Child (pid %d) forked and accepting socket on connfd %d\n", childpid, connfd);
            close(listenfd);
            registerPerChildSignalHandlers();
            enterRemoteWorkingGround(cliAddr, connfd, childpid);
            close(connfd);
            exit(EXIT_SUCCESS);
        }
        close(connfd);
    }
    return EXIT_SUCCESS;
}

