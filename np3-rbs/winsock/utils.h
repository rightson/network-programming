#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern HWND hwndEdit;
extern int EditPrintf(HWND, TCHAR *, ...);

namespace utils {

static const int DEBUG = 0;
const int MAX_BUFFER_SIZE = 10001;

size_t writeFD(SOCKET fd, const char *format, ...) {
    char buffer[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    buffer[strlen(buffer)] = '\0';
    return send(fd, buffer, strlen(buffer), 0);
    long needToSend = strlen(buffer);
    long actualSent = 0;
    long totalSent = 0;
    for (;;) {
        if (DEBUG) EditPrintf(hwndEdit, "[utils] writeFD %d sending [%s]\r\n", fd, buffer);
        actualSent = send(fd, buffer + totalSent, needToSend - totalSent, 0);
        if (actualSent == -1) {
            return -1;
        }
        totalSent += actualSent;
        if (totalSent == needToSend) {
            break;
        } else {
            if (DEBUG) EditPrintf(hwndEdit, "[utils] writeFD %d NOT completed (actualSent=%d, totalSent=%d)\n", fd, actualSent, totalSent);
        }
    }
    if (DEBUG) EditPrintf(hwndEdit, "[utils] writeFD %d sent (actualSent=%d, totalSent=%d)\n", fd, actualSent, totalSent);
    return totalSent;
}

size_t readFD(SOCKET fd, char *message) {
    int n = recv(fd, message, MAX_BUFFER_SIZE, 0);
    if (n >= 0) {
        message[n] = '\0';
    }
    return n;
}

std::string replaceString(std::string subject, const std::string &search, const std::string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

SOCKET getClientSocket(struct sockaddr_in &sockaddr, const char *hostname, int port) {
    SOCKET connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct hostent *hostentry;
    hostentry = gethostbyname(hostname);
    memset((char *)&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr = *((struct in_addr *)hostentry->h_addr_list[0]);
    return connfd;
}

}; // end of namespace utils

#endif
