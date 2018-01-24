#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "utils.hpp"

extern void log(const char *fmt, ...);
extern void printServerOutput(int column, const std::string &str, bool isCmd);

class Client {
    public:
        void setServreID(const int &id) {
            this->id = id;
        }
        void setHostname(const std::string &name) {
            this->hostname = name;
        }
        void setPort(const int &port) {
            this->port = port;
        }
        void setCmdfile(const std::string &name) {
            this->cmdfile = name;
        }
        void dump(int serverID) {
            log("clients[%d]: { id: %d, hostname: %s, port: %d, cmdfile: %s, status: %s }\n",
                    serverID, id, hostname.c_str(), port, cmdfile.c_str(), statusName());
        }
        bool connect() {
            connfd = utils::getClientSocket(sockAddr, hostname.c_str(), port, true);
            int status = ::connect(connfd, (sockaddr *) &sockAddr, sizeof(sockAddr));
            if (status == -1 && errno != EINPROGRESS) {
                printf("Internal Error: failed to connect to ras/rwg server %d\n", id);
                printf("%s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            return true;
        }
        int getServerID() {
            return id;
        }
        int getConnfd() {
            return connfd;
        }
        const std::string& getHostname() {
            return hostname;
        }
        const int getPort() {
            return port;
        }
        const std::string& getCmdfile() {
            return cmdfile;
        }
        const char *getHostIP() {
            if (connfd != -1) {
                ip = inet_ntoa(sockAddr.sin_addr);
                return ip.c_str();
            }
            return "";
        }
        void startReading() {
            status = FD_READ;
        }
        void startWriting() {
            status = FD_WRITE;
        }
        void startClosing() {
            status = FD_CLOSE;
        }
        bool isConnectionOk() {
            int error = 0;
            socklen_t len = sizeof (error);
            int status = getsockopt(connfd, SOL_SOCKET, SO_ERROR, &error, &len);
            log("clients[%d] isConnectionOk status:%s error:%d\n", id, statusName(), error);
            return status == 0 && error == 0;
        }
        bool isConnecting() {
            //log("clients[%d] isConnecting status:%s\n", id, statusName());
            return status == FD_CONNECT;
        }
        bool isReading() {
            //log("clients[%d] isReading status:%s\n", id, statusName());
            return status == FD_READ;
        }
        bool isWriting() {
            //log("clients[%d] isWriting status:%s\n", id, statusName());
            return status == FD_WRITE;
        }
        bool isClose() {
            //log("clients[%d] isClose status:%s\n", id, statusName());
            return status == FD_CLOSE;
        }
        void writeHTML(const std::string &str, bool isCmd = false) {
            printServerOutput(id, str, isCmd);
        }
        std::string read() {
            char buffer[utils::MAX_BUFFER_SIZE];
            memset(buffer, 0, utils::MAX_BUFFER_SIZE);
            log("clients[%d] read fd:%d\n", id, connfd);
            int n = utils::readFD(connfd, buffer);
            if (n == -1 && (errno != EWOULDBLOCK || errno != EAGAIN)) {
                status = FD_CLOSE;
                writeHTML("socket read error!");
                log("clients[%d] socket %d read error\n", id, connfd);
                return "";
            }
            else if (n == 0) {
                log("clients[%d] socket %d read EOF\n", id, connfd);
                return "";
            } else {
                log("clients[%d] socket %d read [%s]\n", id, connfd, buffer);
                return buffer;
            }
        }
        bool write() {
            if (!ifs.is_open()) {
                ifs.open(cmdfile, std::ifstream::in);
            }
            if (!ifs.good()) {
                writeHTML("failed to open file %s\n", cmdfile.c_str());
                utils::die("This program should be closed\n");
            }
            if (writeBuffer.empty()) {
                getline(ifs, writeBuffer);
                writeBuffer += "\n";
            }
            writeHTML(writeBuffer, true);
            ssize_t n = utils::writeFD(connfd, (char *)writeBuffer.c_str());
            if (n == -1 && (errno != EWOULDBLOCK || errno != EAGAIN)) {
                writeHTML("Error: failed to write to server");
                utils::die("This program should be closed\n");
            }
            else if (n != (ssize_t)writeBuffer.size() && n != -1) {
                log("clients[%d] write imcompleted\n", id);
                writeBuffer = writeBuffer.substr(n);
                return false;
            } else if (n == (ssize_t)writeBuffer.size()) {
                log("clients[%d] write completed\n", id);
                writeBuffer.clear();
            }
            return true;
        }
        const char *statusName() {
            switch (status) {
                case FD_CONNECT:
                    return "FD_CONNECT";
                case FD_READ:
                    return "FD_READ";
                case FD_WRITE:
                    return "FD_WRITE";
                case FD_CLOSE:
                    return "FD_CLOSE";
                default:
                    return "Unknown";
            }
        }
    private:
        enum {
            FD_CONNECT,
            FD_READ,
            FD_WRITE,
            FD_CLOSE,
        };
        int id = -1;
        int connfd = -1;
        int status = FD_CONNECT;
        struct sockaddr_in sockAddr;
        std::string hostname;
        std::string ip;
        int port = 0;
        std::string cmdfile;
        std::ifstream ifs;
        std::string writeBuffer;
};

typedef std::map<int, Client> Clients;

#endif
