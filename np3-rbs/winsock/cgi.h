#ifndef __CGI_H__
#define __CGI_H__

#include <winsock.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <map>
#include <regex>
#include <iterator>
#include <iostream>
#include <fstream>

#include "utils.h"

extern HWND hwndEdit;
extern int EditPrintf(HWND, TCHAR *, ...);

#define WM_SOCKET_NOTIFICATION_CGI_SERVICE (WM_USER + 2)


class QueryString {
public:
    QueryString() {}
    QueryString(const std::string &qs) {
        parse(qs);
    }
    QueryString* parse(const std::string &qs) {
        queryStringMap.clear();
        std::regex pattern("([\\w+]+)=([^&]*)");
        auto begin = std::sregex_iterator(qs.begin(), qs.end(), pattern);
        auto end = std::sregex_iterator();
        for (auto i = begin; i != end; ++i) {
            std::smatch sm(*i);
            if (sm.size() == 3) {
                queryStringMap[sm[1].str()] = sm[2].str();
            }
        }
        return this;
    }
    std::map<std::string, std::string>& getQs() {
        return queryStringMap;
    }
    void dump() {
        for (auto &kv : getQs()) {
            EditPrintf(hwndEdit, "queryStringMap[%s] = [%s]\n", kv.first.c_str(), kv.second.c_str());
        }
    }
private:
    std::map<std::string, std::string> queryStringMap;
};

class HTMLRenderer;
class CGIWorker;

struct RequestProfile {
    int id = 0;
    int port = 0;
    SOCKET ssock = -1;
    SOCKET connfd = -1;
    bool canWrite = false;
    bool active = false;
    struct sockaddr_in sockAddr;
    std::string hostname;
    std::string filename;
    std::string status;
    std::ifstream ifs;
    HTMLRenderer *html;
    CGIWorker *cgiWorker;
};

class HTMLRenderer {
    //TODO: it would be better to create a HTML template engine rather than an ad-hoc renderer
public:
    std::string getDocumentHeader(std::map<int, RequestProfile>& profiles) {
        std::string output;
        char buffer[utils::MAX_BUFFER_SIZE];
        memset(buffer, 0, utils::MAX_BUFFER_SIZE);
        sprintf(buffer,
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>Network Programming Homework 3</title>\n"
            "    <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-beta.2/css/bootstrap.min.css\" integrity=\"sha384-PsH8R72JQ3SOdhVi3uxftmaW6Vc51MKb0q5P2rRUpPvrszuE4W1povHYgTpBfshb\" crossorigin=\"anonymous\">\n"
            "<style>\n"
            "h6, strong {\n"
            "    color: yellow;\n"
            "    font-weight: bold;\n"
            "}\n"
            "@font-face {\n"
            "    font-family: \"Courier New\";\n"
            "}\n"
            "body {\n"
            "   font-family: \"Courier New\";\n"
            "   font-size: x-small;\n"
            "   background: #212529\n"
            "}\n"
            "</style>\n"
            "</head>\n"
            "<body>\n"
            "    <table class=\"table table-bordered table-dark\" width=\"100%%\">\n"
            "    <thead>\n"
            "        <tr>\n");
        EditPrintf(hwndEdit, "%s", buffer);
        output += buffer;
        for (auto &kv : profiles) {
            auto &request = kv.second;
            memset(buffer, 0, utils::MAX_BUFFER_SIZE);
            sprintf(buffer,
                "            <th>\n"
                "               <h6>%s</h6>\n"
                "               <ul>\n"
                "                   <li>port: %d</li>\n"
                "                   <li>file: %s</li>\n"
                "                   <li>status: %s</li>\n"
                "               </ul>\n"
                "            </th>\n",
                inet_ntoa(request.sockAddr.sin_addr), request.port, request.filename.c_str(), request.status.c_str());
            output += buffer;
            EditPrintf(hwndEdit, "%s", buffer);
        }
        memset(buffer, 0, utils::MAX_BUFFER_SIZE);
        sprintf(buffer,
            "        </tr>\n"
            "    </thead>\n"
            "    <tbody>\n"
            "        <tr>\n");
        EditPrintf(hwndEdit, "%s", buffer);
        output += buffer;
        for (auto &kv : profiles) {
            auto &request = kv.second;
            memset(buffer, 0, utils::MAX_BUFFER_SIZE);
            sprintf(buffer,
                "            <td class=\"\" valigh=\"top\" id=\"server%d\"></td>\n", request.id);
            output += buffer;
            EditPrintf(hwndEdit, "%s", buffer);
        }
        memset(buffer, 0, utils::MAX_BUFFER_SIZE);
        sprintf(buffer,
            "        </tr>\n"
            "    </tbody>\n"
            "    </table>\n"
            "</body>\n"
            "</html>\n");
        output += buffer;
        EditPrintf(hwndEdit, "%s", buffer);
        return output;
    }
    std::string getDocumentContent(int column, const std::string &str, bool isCmd = false) {
        std::string msg(utils::replaceString(str, "\n", "<br />"));
        msg = utils::replaceString(msg, "\"", "\\\"");
        msg = utils::replaceString(msg, "% ", "&percnt;&nbsp;");
        char buffer[utils::MAX_BUFFER_SIZE];
        memset(buffer, 0, utils::MAX_BUFFER_SIZE);
        if (isCmd) {
            snprintf(buffer, utils::MAX_BUFFER_SIZE, "<script>document.getElementById('server%d').innerHTML += (\"<strong>%s</strong>\");</script>\n", column, msg.c_str());
        }
        else {
            snprintf(buffer, utils::MAX_BUFFER_SIZE, "<script>document.getElementById('server%d').innerHTML += (\"%s\");</script>\n", column, msg.c_str());
        }
        EditPrintf(hwndEdit, "[html] html buffer: [%s]\r\n", buffer);
        return buffer;
    }
    void printDocumentFooter() {
        fflush(stdout);
    }
};

class CGIWorker {
public:
    CGIWorker() {}
    void setQueryString(const std::string &queryString) {
        qs = QueryString(queryString);
        //qs.dump();
    }
    bool setupRequestProfiles(SOCKET ssock) {
        requestProfiles.clear();
        for (auto &kv : qs.getQs()) {
            if (kv.first.size() < 2) continue;
            int serverID = stoi(kv.first.substr(1));
            if (requestProfiles.find(serverID) == requestProfiles.end()) {
                requestProfiles[serverID] = RequestProfile();
                requestProfiles[serverID].id = serverID;
                requestProfiles[serverID].ssock = ssock;
                requestProfiles[serverID].html = &html;
                requestProfiles[serverID].cgiWorker = this;
            }
            switch (kv.first[0]) {
            case 'h':
                requestProfiles[serverID].hostname = kv.second;
                break;
            case 'p':
                if (kv.second.size() > 0) {
                    requestProfiles[serverID].port = stoi(kv.second);
                }
                break;
            case 'f':
                requestProfiles[serverID].filename = kv.second;
                break;
            }
        }
        return true;
    }
    bool setupConnectSockets(HWND hwnd, std::map<SOCKET, RequestProfile&>& cgiHandleMap, int cgiNotifyCode) {
        std::vector<std::map<int, RequestProfile>::iterator> disabled;
        for (auto it = requestProfiles.begin(); it != requestProfiles.end(); ++it) {
            auto &kv = *it;
            auto &request = kv.second;
            if (request.hostname.empty()) {
                disabled.push_back(it);
                continue;
            }
            request.ifs.open(request.filename, std::ifstream::in);
            if (!request.ifs.good()) {
                EditPrintf(hwndEdit, "[cgi-worker] error: failed to open file %s\n", request.filename.c_str());
                return false;
            }
        }
        for (auto &it : disabled) {
            requestProfiles.erase(it);
        }
        for (auto &kv : requestProfiles) {
            auto &request = kv.second;
            auto &connfd = request.connfd;

            connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (connfd == INVALID_SOCKET) {
                EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\n"));
                WSACleanup();
                return false;
            }

            EditPrintf(hwndEdit, "[cgi-worker] setup connection SOCKET: %d, port: %d cgiNotifyCode: %d\n", connfd, request.port, cgiNotifyCode);
            int err = WSAAsyncSelect(connfd, hwnd, cgiNotifyCode, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);
            if (err == SOCKET_ERROR) {
                EditPrintf(hwndEdit, TEXT("=== Error: select error %d ===\n"), WSAGetLastError());
                closesocket(connfd);
                WSACleanup();
                return false;
            }

            cgiHandleMap.insert(std::pair<SOCKET, RequestProfile&>(connfd, request));

            struct hostent *hostentry;
            hostentry = gethostbyname(request.hostname.c_str());
            struct sockaddr_in &addr = request.sockAddr;
            memset((char *)&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(request.port);
            addr.sin_addr = *((struct in_addr *)hostentry->h_addr_list[0]);
            if ((addr.sin_addr.s_addr = inet_addr(request.hostname.c_str())) == INADDR_NONE) {
                EditPrintf(hwndEdit, TEXT("=== Error: gethostname error ===\n"));
            }
        }
        return true;
    }
    void connectToClients() {
        connNum = 0;
        for (auto &kv : requestProfiles) {
            auto &request = kv.second;
            auto &connfd = request.connfd;
            struct sockaddr_in &addr = request.sockAddr;
            int status = connect(connfd, (sockaddr *)&addr, sizeof(addr));
            if (status != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
                EditPrintf(hwndEdit, "[cgi-worker] Internal Error: failed to connect to ras/rwg server %d (status=%d, error=%d)\n", request.id, status, WSAGetLastError());
                request.status = "failed to connect";
                continue;
            }
            else {
                request.status = "ok";
            }
            ++connNum;
            request.active = true;
        }
    }
    std::string getDocument() {
        return html.getDocumentHeader(requestProfiles);
    }
    int readRequest(SOCKET connfd, RequestProfile &request) {
        if (connfd != request.connfd) {
            EditPrintf(hwndEdit, "[cgi-worker] invalid SOCKET of cgi handle (%d vs. %d)\n", connfd, request.connfd);
            return -1;
        }
        char buffer[utils::MAX_BUFFER_SIZE];
        memset(buffer, 0, utils::MAX_BUFFER_SIZE);
        int n = utils::readFD(connfd, buffer);
        if (n == -1) {
            EditPrintf(hwndEdit, "[cgi-worker] Internal Error: failed to connect to ras/rwg server %d\n", request.id);
            return n;
        } else if (n == 0) {
            request.active = false;
            connNum--;
            return n;
        } else {
            std::string msg(buffer);
            EditPrintf(hwndEdit, "[cgi-worker] server [%d] read [%d] msg [%s]\n", request.id, n, buffer);
            if (msg.size()) {
                std::string output(html.getDocumentContent(request.id, msg));
                utils::writeFD(request.ssock, (char *)output.c_str());
            }
            if (msg.size() >= 2 && msg.substr(msg.size()-2,2) == "% ") {
                request.canWrite = true;
            }
            return n;
        }
    }
    int writeCommand(SOCKET connfd, RequestProfile &request) {
        if (connfd != request.connfd) {
            EditPrintf(hwndEdit, "[cgi-worker] invalid SOCKET of cgi handle (%d vs. %d)\n", connfd, request.connfd);
            return -1;
        }
        if (request.canWrite == false) return 0;
        std::string cmd;
        getline(request.ifs, cmd);
        cmd += "\n";
        std::string output(html.getDocumentContent(request.id, cmd, true));
        utils::writeFD(connfd, (char *)cmd.c_str());
        utils::writeFD(request.ssock, (char *)output.c_str());
        request.canWrite = false;
    }
private:
    QueryString qs;
    std::map<int, RequestProfile> requestProfiles;
    int connNum = 0;
    HTMLRenderer html;
};

#endif
