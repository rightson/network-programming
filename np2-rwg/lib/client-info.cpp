#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>   // mmap
#include <sys/stat.h>   // mkfifo
#include <fcntl.h>		// O_WRONLY

#include "utility.hpp"
#include "client-info.hpp"
#include "user-shell.hpp"

ClientInfo *createClientInfoListWithSharedMemory() {
    ClientInfo *clientList = (ClientInfo *)mmap(NULL,
                                    sizeof(ClientInfo)*(MAX_CLIENT_NUMBER+1),
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_SHARED,
                                    -1,
                                    0);
    if (clientList == MAP_FAILED) {
        printf("Failed to allocate shared memory\n");
        exit(EXIT_FAILURE);
    } else {
        memset(clientList, 0, sizeof(ClientInfo) *(MAX_CLIENT_NUMBER+1));
        printf("[client-info] Created shared memory successfully\n");
    }
    return clientList;
}

void destroyClientInfoList(ClientInfo *clientList) {
    if (clientList != NULL) {
        munmap(clientList, sizeof(ClientInfo)*(MAX_CLIENT_NUMBER+1));
        printf("[client-info] Deleted shared memory successfully\n");
        clientList = NULL;
    }
}

ClientInfo::ClientInfo() {
    clear();
}

ClientInfo::~ClientInfo() {
    disableClient(this);
}

bool ClientInfo::isOnline() {
    return online;
}

ClientInfo* ClientInfo::clear() {
    resetClientInfo(this);
    return this;
}

ClientInfo* ClientInfo::dump() {
    dumpClientInfo(this);
    return this;
}

ClientInfo* ClientInfo::enable(
                        int clientID,
                        struct sockaddr_in &cliAddr,
                        int connfd,
                        pid_t selfpid,
                        ClientInfo *clientList) {

    printf("[client-info] enable %d\n", clientID);
    id = clientID;
    pid = selfpid;
    fd = connfd;
    fifosize = 0;
    cmdID = 0;
    online = true;
    strcpy(nickname, "(no name)\0");
#if DEMO
	strcpy(address, "CGILAB/511");
#else
	strcpy(address, getIpPortBinding(cliAddr));
#endif
    shell = new UserShell(selfpid, connfd, clientID, clientList);
    return this;
}

ClientInfo* ClientInfo::login(
                        int clientID,
                        struct sockaddr_in &cliAddr,
                        int connfd,
                        pid_t selfpid,
                        ClientInfo *clientList) {

    printf("[client-info] login %d\n", clientID);
    enable(clientID, cliAddr, connfd, selfpid, clientList);
    shell->showLoginMessage();
    shell->broadcastUserLoginMessage();
    shell->showPrompt();
    printf("[client-login] Client #%d is login (pid=%d)\n", id, pid);
    return this;
}

ClientInfo* ClientInfo::logout() {
    resetMyPipes(id);
    shell->broadcastUserLogoutMessage();
    if (strcmp(fifopath, "") != 0) {
        unlink(fifopath);
    }
    close(fd);
    clear();
    delete shell;
    return this;
}

void ClientInfo::enableFIFO(ClientInfo *clientList) {
    char fifopath[MAX_PATH_LENGTH];
	snprintf(fifopath, MAX_PATH_LENGTH, "%s%d.fifo", UNICAST_FIFO_PREFIX, id);
    strcpy(clientList[id-1].fifopath, fifopath);
    createFIFO(fifopath);
}

size_t ClientInfo::write(const char *message) {
    return writeFD(fd, message);
}

size_t ClientInfo::read(char *buffer) {
    return readFD(fd, buffer);
}

void resetClientInfo(ClientInfo *client) {
    client->id = -1;
    client->pid = -1;
    client->fd = -1;
    client->cmdID = -1;
    client->online = false;
    memset(client->nickname, 0, MAX_ATTR_SIZE);
    memset(client->address, 0, MAX_ATTR_SIZE);
    memset(client->fifopath, 0, MAX_PATH_LENGTH);
}

void dumpClientInfo(ClientInfo *clientInfo) {
    printf("[clientInfo]\n");
    printf("  id = %d\n", clientInfo->id);
    printf("  pid = %d\n", clientInfo->pid);
    printf("  fd = %d\n", clientInfo->fd);
    printf("  cmdID = %d\n", clientInfo->cmdID);
    printf("  online = %d\n", clientInfo->online);
    printf("  nickname = %s\n", clientInfo->nickname);
    printf("  address = %s\n", clientInfo->address);
    printf("  fifopath = %s\n", clientInfo->fifopath);
}

void resetClientList(ClientInfo *clientList) {
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
        resetClientInfo(&clientList[i]);
    }
}

void resetAllPipes() {
    char name[MAX_FIFO_BUFFER_SIZE];
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
        for (int j=0; j<MAX_CLIENT_NUMBER; ++j) {
           getPeerNamedPipe(name, i+1, j+1);
           unlink(name);
        }
    }
}

void resetMyPipes(int clientID) {
    char name[MAX_FIFO_BUFFER_SIZE];
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
        getPeerNamedPipe(name, i+1, clientID);
        //printf("[client-info] unlink %s\n", name);
        unlink(name);
    }
}

void resetFIFOs() {
    char name[MAX_FIFO_BUFFER_SIZE];
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
        sprintf(name, "%s/client%d.fifo", IPC_ROOT, i);
        unlink(name);
    }
}

int getAvailableClientID(ClientInfo *clientList) {
    for (int id = 0; id < MAX_CLIENT_NUMBER; ++id) {
        if (!clientList[id].online) {
            return id;
        }
    }
    return MAX_CLIENT_NUMBER;
}

ClientInfo *enableClient(ClientInfo *clientList, struct sockaddr_in &cliAddr, int connfd, pid_t selfpid) {
    int id;
    for (id = 0; id < MAX_CLIENT_NUMBER; ++id) {
        if (!clientList[id].online) {
            break;
        }
    }
    if (id >= MAX_CLIENT_NUMBER) {
        char msg[] = "Too many clients (should <= 30)";
        write(connfd, msg, sizeof(msg));
        close(connfd);
        exit(EXIT_SUCCESS);
    }
    clientList[id].id = id + 1;
    clientList[id].pid = selfpid;
    clientList[id].fd = connfd;
    clientList[id].fifosize = 0;
    clientList[id].cmdID = 0;
    clientList[id].online = true;
    strcpy(clientList[id].nickname, "(no name)\0");
#if DEMO
	strcpy(clientList[id].address, "CGILAB/511");
#else
	strcpy(clientList[id].address, getIpPortBinding(cliAddr));
#endif
    char fifopath[MAX_PATH_LENGTH];
	snprintf(fifopath, MAX_PATH_LENGTH, "%s%d.fifo", UNICAST_FIFO_PREFIX, id);
    strcpy(clientList[id].fifopath, fifopath);
    createFIFO(fifopath);
    return &clientList[id];
}

void getPeerNamedPipe(char *name, int from, int to) {
    sprintf(name, "%s/from-%d-to-%d.pipe", IPC_ROOT, from, to);
}

void clearAllReceivingPipes(ClientInfo *client) {
    char name[MAX_FIFO_BUFFER_SIZE];
    for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
       getPeerNamedPipe(name, i+1, client->id);
       unlink(name);
    }
}

void disableClient(ClientInfo *client) {
    client->online = false;
    unlink(client->fifopath);
    clearAllReceivingPipes(client);
}

