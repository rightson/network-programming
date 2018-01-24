#pragma once

#include <windows.h>

#include <map>

#include "http-handler.h"

extern HWND hwndEdit;
extern int EditPrintf(HWND, TCHAR *, ...);

class HttpServer {
public:
    HttpServer(HWND _hwnd, int cgiNotifyCode) : hwnd(_hwnd), cgiNotifyCode(cgiNotifyCode) {
        EditPrintf(hwndEdit, TEXT("[server] http server started\r\n"));
    }
    size_t clientNumbers() {
        return httpHandler.size();
    }
    void acceptConnection(SOCKET &msock, SOCKET &ssock) {
        EditPrintf(hwndEdit, TEXT("[server] accepted SOCKET %d\r\n"), ssock);
        httpHandler.insert(std::make_pair(ssock, std::move(HTTPHandler(hwnd, msock, ssock, cgiHandleMap, cgiNotifyCode))));
        readInited[ssock] = false;
        writeInited[ssock] = false;
    }
    int readRequest(SOCKET &ssock) {
        auto it = httpHandler.find(ssock);
        if (it == httpHandler.end()) {
            EditPrintf(hwndEdit, TEXT("Error: no http handler for server to read request from SOCKET: %d\r\n"), ssock);
            return -1;
        }
        readInited[ssock] = true;
        int n = it->second.receiveRequest();
        if (n == -1) {
            EditPrintf(hwndEdit, TEXT("Error: server cannot read request from SOCKET: %d\r\n"), ssock);
            return n;
        }
        if (writeInited[ssock]) {
            EditPrintf(hwndEdit, TEXT("[server] reply immediate after read request since write is ready\r\n"));
            return it->second.replyResponse();
        }
        return 1;
    }
    int writeResponse(SOCKET &ssock) {
        EditPrintf(hwndEdit, TEXT("[server]: write SOCKET %d\r\n"), ssock);
        auto it = httpHandler.find(ssock);
        if (it == httpHandler.end()) {
            EditPrintf(hwndEdit, TEXT("Error: no http handler for server to write response from SOCKET: %d\r\n"), ssock);
            return -1;
        }
        writeInited[ssock] = true;
        if (!readInited[ssock]) {
            EditPrintf(hwndEdit, TEXT("[server] neet to wait until read request is done\r\n"));
            return 1;
        }
        EditPrintf(hwndEdit, TEXT("[server] write response after read is ready\r\n"));
        return it->second.replyResponse();
    }
    bool cleanupConnection(SOCKET &ssock) {
        EditPrintf(hwndEdit, TEXT("[server]: close SOCKET %d\r\n"), ssock);
        auto it = httpHandler.find(ssock);
        if (it != httpHandler.end()) {
            closesocket(ssock);
            httpHandler.erase(it);
        }
        return true;
    }
    bool cgiServerHandler(SOCKET &connfd, int mode) {
        switch (mode) {
            case FD_CONNECT:
                WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_READ| FD_WRITE | FD_CLOSE);
                break;
            case FD_READ: {
                auto it = cgiHandleMap.find(connfd);
                if (it != cgiHandleMap.end()) {
                    EditPrintf(hwndEdit, TEXT("[server][cgi] read SOCKET: %d\r\n"), connfd);
                    RequestProfile& request = it->second;
                    WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_CLOSE);
                    request.cgiWorker->readRequest(connfd, request);
                    WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_READ | FD_WRITE | FD_CLOSE);
                }
                else {
                    EditPrintf(hwndEdit, TEXT("[server][cgi] No handler for SOCKET: %d\r\n"), connfd);
                }
                break;
            }
            case FD_WRITE: {
                auto it = cgiHandleMap.find(connfd);
                if (it != cgiHandleMap.end()) {
                    EditPrintf(hwndEdit, TEXT("[server][cgi] write SOCKET: %d\r\n"), connfd);
                    RequestProfile& request = it->second;
                    WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_WRITE | FD_CLOSE);
                    request.cgiWorker->writeCommand(connfd, request);
                    WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_READ | FD_WRITE | FD_CLOSE);
                }
                break;
            }
            case FD_CLOSE: {
                auto it = cgiHandleMap.find(connfd);
                if (it != cgiHandleMap.end()) {
                    EditPrintf(hwndEdit, TEXT("[server][cgi] close SOCKET: %d\r\n"), connfd);
                    SOCKET ssock = it->second.ssock;
                    cgiHandleMap.erase(it);
                    closesocket(connfd);
                    if (cgiHandleMap.size() == 0) {
                        closesocket(ssock);
                    }
                }
                break;
            }
        };
        return true;
    }
private:
    HWND hwnd;
    int cgiNotifyCode;
    std::map<SOCKET, bool> readInited;
    std::map<SOCKET, bool> writeInited;
    std::map<SOCKET, HTTPHandler> httpHandler;
    std::map<SOCKET, RequestProfile&> cgiHandleMap;
};
