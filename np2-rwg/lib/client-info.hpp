#ifndef _CLIENT_INFO_
#define _CLIENT_INFO_

#include <unistd.h>

#include "utility.hpp"

struct sockaddr_in;
class UserShell;

const int MAX_CLIENT_NUMBER = 30;
const int MAX_ATTR_SIZE = 32;
const int MAX_FIFO_BUFFER_SIZE = 1024;
const char BROADCAST_FIFO[] = IPC_ROOT"/broadcast";
const char UNICAST_FIFO_PREFIX[] = IPC_ROOT"/client";

class ClientInfo {
    public:
        ClientInfo();
        ~ClientInfo();
        bool isOnline();
        ClientInfo* clear();
        ClientInfo* dump();
        ClientInfo *enable(int clientID,
                           struct sockaddr_in &cliAddr,
                           int connfd,
                           pid_t selfpid,
                           ClientInfo *clientList);
        ClientInfo *login(int clientID,
                           struct sockaddr_in &cliAddr,
                           int connfd,
                           pid_t selfpid,
                           ClientInfo *clientList);
        ClientInfo* logout();
        void enableFIFO(ClientInfo *clientInfo);
        size_t write(const char *message);
        size_t read(char *buffer);

        int id;
        int pid;
        int fd;
        int fifosize;
        int cmdID;
        bool online;
        char nickname[MAX_ATTR_SIZE];
        char address[MAX_ATTR_SIZE];
        char fifopath[MAX_PATH_LENGTH];
        UserShell *shell;
};

ClientInfo *createClientInfoListWithSharedMemory();
void destroyClientInfoList(ClientInfo *clientList);
int getAvailableClientID(ClientInfo *clientList);

void resetClientInfo(ClientInfo *clientList);
void resetClientInfoList(ClientInfo *clientList);
ClientInfo *enableClient(ClientInfo *clientInfo,
                        struct sockaddr_in &cliAddr,
                        int connfd,
                        pid_t selfpid);
void disableClient(ClientInfo *clientInfo);
void dumpClientInfo(ClientInfo *clientInfo);

void getPeerNamedPipe(char *name, int from, int to);
void clearAllReceivingPipes(ClientInfo *client);

void resetAllPipes();
void resetMyPipes(int id);
void resetFIFOs();

#endif

