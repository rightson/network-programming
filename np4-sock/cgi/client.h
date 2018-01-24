#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <fstream>

#include "utils.h"
#include "sock4-common.h"

extern void log(const char *fmt, ...);
extern void printServerOutput(int column, const std::string &str, bool isCmd);

class Client {
    public:
        Client();
        void setServreID(const int &id);
        void setHostname(const std::string &name);
        void setPort(const int &port);
        void setCmdfile(const std::string &name);
        void setSockServerIP(const std::string &name);
        void setSockServerPort(const int &port);
        void dump(int serverID);
        bool isSockServerSpecified();
        bool connect();
        bool sendSockRequest();
        bool receiveSockReply();
        int getServerID();
        int getConnfd();
        const std::string& getHostname();
        const int getPort();
        const std::string& getCmdfile();
        const char *getHostIP();
        void startWaitingSock();
        void startReading();
        void startWriting();
        void startClosing();
        bool isConnectionOk();
        bool isConnecting();
        bool isWaitingSock();
        bool isReading();
        bool isWriting();
        bool isClose();
        void writeHTML(const std::string &str, bool isCmd = false);
        std::string read();
        bool write();
        const char *statusName();
    private:
        enum {
            FD_CONNECT,
            FD_SOCK_REQUEST,
            FD_READ,
            FD_WRITE,
            FD_CLOSE,
        };
        int id = -1;
        int connfd = -1;
        int status = FD_CONNECT;
        struct sockaddr_in servAddr;
        std::string hostname;
        std::string ip;
        int port = 0;
        std::string cmdfile;
        std::ifstream ifs;
        std::string sockip;
        int sockport = 0;
        Sock4Request sock4request;
        Sock4Reply sock4reply;
        char *buffer;
        std::string writeBuffer;
};

typedef std::map<int, Client> Clients;

#endif
