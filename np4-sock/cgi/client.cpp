#include <string.h>
#include <errno.h>

#include "client.h"

Client::Client() {
    memset(&sock4request, 0, sizeof(sock4request));
    memset(&sock4reply, 0, sizeof(sock4reply));
}

void Client::setServreID(const int &id) {
    this->id = id;
}

void Client::setHostname(const std::string &name) {
    this->hostname = name;
}

void Client::setPort(const int &port) {
    this->port = port;
}

void Client::setCmdfile(const std::string &name) {
    this->cmdfile = name;
}

void Client::setSockServerIP(const std::string &name) {
    this->sockip = name;
}

void Client::setSockServerPort(const int &port) {
    this->sockport = port;
}

void Client::dump(int serverID) {
    log("clients[%d]: { "
        "id: %d, hostname: %s, port: %d, cmdfile: %s, "
        "sockip: %s, sockport: %d, "
        "status: %s }\n",
            serverID, id, hostname.c_str(), port, cmdfile.c_str(),
            sockip.c_str(), sockport, statusName());
}

bool Client::isSockServerSpecified() {
    return !sockip.empty() && sockport != 0;
}

bool Client::connect() {
    if (isSockServerSpecified()) {
        log("Connecting to sock server %s:%d\n", sockip.c_str(), sockport);
        connfd = utils::getClientSocket(servAddr, sockip.c_str(), sockport, true);
    }
    else {
        log("Connecting to remote server\n");
        connfd = utils::getClientSocket(servAddr, hostname.c_str(), port, true);
    }
    int status = ::connect(connfd, (sockaddr *) &servAddr, sizeof(servAddr));
    if (status == -1 && errno != EINPROGRESS) {
        printf("Internal Error: failed to connect to ras/rwg server %d\n", id);
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return true;
}

bool Client::sendSockRequest() {
    sock4request.VN = 4; // sock4
    sock4request.CD = 1; // connect
    sock4request.DST_PORT = htons(port);
    inet_aton(hostname.c_str(), &sock4request.DST_IP);
    char *buffer = new char[SOCK4_MIN_PKT_SIZE+1];
    memcpy(buffer, &sock4request, SOCK4_MIN_PKT_SIZE);
    if (utils::writeFD(connfd, buffer) == -1) {
        log("[cgi] failed to send Sock4 Request\n");
        delete buffer;
        return false;
    }
    delete buffer;
    log("[cgi] Request = { VN: %d, CD: %d, DST_PORT: %d, DST_IP: %s }\n",
            sock4request.VN,
            sock4request.CD,
            ntohs(sock4request.DST_PORT),
            utils::getIPAddress(sock4request.DST_IP));
    return true;
}

bool Client::receiveSockReply() {
    char *buffer = new char[utils::MAX_BUFFER_SIZE+1];
    memset(buffer, 0, utils::MAX_BUFFER_SIZE);
    utils::readFD(connfd, buffer);
    memcpy(&sock4reply, buffer, SOCK4_MIN_PKT_SIZE);
    log("[cgi] Reply = { VN: %d, CD: %d, DST_PORT: %d, DST_IP: %s }\n",
            sock4reply.VN, sock4reply.CD, ntohs(sock4reply.DST_PORT), utils::getIPAddress(sock4reply.DST_IP));
    delete buffer;
    return sock4reply.VN == 0 && sock4reply.CD == 90;
}


int Client::getServerID() {
    return id;
}

int Client::getConnfd() {
    return connfd;
}

const std::string& Client::getHostname() {
    return hostname;
}

const int Client::getPort() {
    return port;
}

const std::string& Client::getCmdfile() {
    return cmdfile;
}

const char* Client::getHostIP() {
    if (connfd != -1) {
        ip = utils::getIPAddress(servAddr);
        return ip.c_str();
    }
    return "";
}

void Client::startWaitingSock() {
    status = FD_SOCK_REQUEST;
}

void Client::startReading() {
    status = FD_READ;
}

void Client::startWriting() {
    status = FD_WRITE;
}

void Client::startClosing() {
    status = FD_CLOSE;
}

bool Client::isConnectionOk() {
    int error = 0;
    socklen_t len = sizeof (error);
    int status = getsockopt(connfd, SOL_SOCKET, SO_ERROR, &error, &len);
    log("clients[%d] isConnectionOk status:%s error:%d\n", id, statusName(), error);
    return status == 0 && error == 0;
}

bool Client::isConnecting() {
    return status == FD_CONNECT;
}

bool Client::isWaitingSock() {
    return status == FD_SOCK_REQUEST;
}

bool Client::isReading() {
    return status == FD_READ;
}

bool Client::isWriting() {
    return status == FD_WRITE;
}

bool Client::isClose() {
    return status == FD_CLOSE;
}

void Client::writeHTML(const std::string &str, bool isCmd) {
    printServerOutput(id, str, isCmd);
}

std::string Client::read() {
    char *buffer = new char[utils::MAX_BUFFER_SIZE+1];
    memset(buffer, 0, utils::MAX_BUFFER_SIZE);
    log("clients[%d] read fd:%d\n", id, connfd);
    int n = utils::readFD(connfd, buffer);
    if (n == -1 && (errno != EWOULDBLOCK || errno != EAGAIN)) {
        status = FD_CLOSE;
        writeHTML("socket read error!");
        log("clients[%d] socket %d read error\n", id, connfd);
        delete buffer;
        return "";
    }
    else if (n == 0) {
        log("clients[%d] socket %d read EOF\n", id, connfd);
        return "";
    } else {
        log("clients[%d] socket %d read [%s]\n", id, connfd, buffer);
        std::string ret(buffer);
        delete buffer;
        return ret;
    }
}

bool Client::write() {
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

const char* Client::statusName() {
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

