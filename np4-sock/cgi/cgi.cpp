#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <error.h>
#include <sys/socket.h>
#include <arpa/inet.h>  // inet_ntoa
#include <unistd.h>

#include <string>
#include <iostream>
#include <fstream>

#include "client.h"

void log(const char *fmt, ...);
void setupClients(Clients &clients);
void printHTTPHeader();
void printDocumentHeader();
void printDocumentBodys(Clients &clients);
void printDocumentFooter();
void printServerOutput(int column, const std::string &str, bool isCmd);
void interactWithServers(Clients &clients);

int main(int argc, char *argv[])
{
    Clients clients;
    setupClients(clients);
    printHTTPHeader();
    printDocumentHeader();
    printDocumentBodys(clients);
    printDocumentFooter();
    return 0;
}

void log(const char *fmt, ...) {
    static FILE *fp = NULL;
    if (fp == NULL) {
        fp = fopen("debug.log", "w");
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fflush(fp);
}

void createNewClient(Clients &clients, int serverID) {
    if (clients.find(serverID) == clients.end()) {
        clients[serverID] = Client();
        clients[serverID].setServreID(serverID);
    }
}

void setupClients(Clients &clients) {
    utils::StringMap stringMap;
    const char *queryString = getenv("QUERY_STRING");
    if (queryString == NULL) {
        utils::die("Error: QUERY_STRING is NULL\n");
    }
    utils::parseQueryString(queryString, stringMap);
    for (auto &kv: stringMap) {
        auto &key = kv.first;
        auto &value = kv.second;
        if (key.size() < 2) continue;
        if (key.size() >= 3) { // sh1=, sp1=
            int serverID = stoi(key.substr(2));
            if (key.substr(0,2) == "sh") {
                createNewClient(clients, serverID);
                clients[serverID].setSockServerIP(value);
            }
            else if (key.substr(0,2) == "sp") {
                createNewClient(clients, serverID);
                clients[serverID].setSockServerPort(stoi(value));
            }
        }
        else { // h1=, p1=, f1=
            int serverID = stoi(key.substr(1));
            if (key[0] == 'h') {
                createNewClient(clients, serverID);
                clients[serverID].setHostname(value);
            }
            else if (key[0] == 'p') {
                createNewClient(clients, serverID);
                if (value.size() > 0) {
                    clients[serverID].setPort(stoi(value));
                }
            }
            else if (key[0] == 'f') {
                createNewClient(clients, serverID);
                clients[serverID].setCmdfile(value);
            }
        }
    }
    for (auto &kv: clients) kv.second.dump(kv.first);
}

void printHTTPHeader() {
    printf("Content-Type: text/html\n\n");
}

void printDocumentHeader() {
    printf(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>NP Project #4</title>\n"
        "    <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-beta.2/css/bootstrap.min.css\" integrity=\"sha384-PsH8R72JQ3SOdhVi3uxftmaW6Vc51MKb0q5P2rRUpPvrszuE4W1povHYgTpBfshb\" crossorigin=\"anonymous\">\n"
        "<style>\n"
        "h1, h6, strong {\n"
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
        "</head>\n");
}

void printDocumentBodys(Clients &clients) {
    printf(
        "<body>\n"
        "    <nav class=\"navbar navbar-dark bg-faded\">\n"
        "        <a class=\"navbar-brand\" href=\"#\">NP Project #4</a>\n"
        "    </nav>");
    printf(
        "    <table class=\"table table-bordered table-dark\" width=\"100%%\">\n"
        "    <thead>\n"
        "        <tr>\n");
    for (auto &kv: clients) {
        auto &c = kv.second;
        printf(
                "            <th>\n"
                "               <h6>%s</h6>\n"
                "               <ul>\n"
                "                   <li>port: %d</li>\n"
                "                   <li>file: %s</li>\n"
                "               </ul>\n"
                "            </th>\n",
                c.getHostname().c_str(), c.getPort(), c.getCmdfile().c_str());
    }
    printf(
            "        </tr>\n"
            "    </thead>\n"
            "    <tbody>\n"
            "        <tr>\n");
    for (auto &kv: clients) {
        auto &c = kv.second;
        printf(
                "            <td class=\"\" valigh=\"top\" id=\"server%d\"></td>\n", c.getServerID());
    }
    printf(
        "        </tr>\n"
        "    </tbody>\n"
        "    </table>\n");
    interactWithServers(clients);
    printf("</body>\n");
}

void printDocumentFooter() {
    printf("</html>\n");
}

void printServerOutput(int column, const std::string &str, bool isCmd = false) {
    std::string msg(utils::replaceString(str, "\n", "<br />"));
    msg = utils::replaceString(msg, "\r", "");
    msg = utils::replaceString(msg, "\"", "\\\"");
    if (isCmd) {
        printf("    <script>document.getElementById('server%d').innerHTML += (\"<strong>%s</strong>\");</script>\n", column, msg.c_str());
    } else {
        printf("    <script>document.getElementById('server%d').innerHTML += (\"%s\");</script>\n", column, msg.c_str());
    }
    fflush(stdout);
}

void interactWithServers(Clients &clients) {
    int connections = 0;
    for (auto &kv: clients) {
        int serverID = kv.first;
        if (clients[serverID].connect()) {
            connections++;
        }
    }
    int nfds = FD_SETSIZE;
    fd_set rfds;
    fd_set wfds;
    fd_set rs;
    fd_set ws;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    for (auto &kv: clients) {
        int serverID = kv.first;
        FD_SET(clients[serverID].getConnfd(), &rs);
        FD_SET(clients[serverID].getConnfd(), &ws);
    }
    while (connections > 0) {
        memcpy(&rfds, &rs, sizeof(rfds));
        memcpy(&wfds, &ws, sizeof(wfds));
        int n = select(nfds, &rfds, &wfds, NULL, 0);
        log("select's n = %d\n", n);
        for (auto &kv: clients) {
            int i = kv.first;
            auto &c = clients[i];
            if (c.isConnecting() && c.isConnectionOk()) {
                log("clients[%d] is connected\n", i);
                if (FD_ISSET(c.getConnfd(), &rfds) || FD_ISSET(c.getConnfd(), &wfds)) {
                    if (c.isSockServerSpecified()) {
                        if (!c.sendSockRequest()) {
                            c.isClose();
                            FD_CLR(c.getConnfd(), &rs);
                            FD_CLR(c.getConnfd(), &ws);
                            close(c.getConnfd());
                            log("clients[%d] connection is closed since sock server is rejected\n", i);
                            connections--;
                            continue;
                        }
                        c.startWaitingSock();
                    } else {
                        c.startReading();
                        FD_CLR(c.getConnfd(), &ws);
                    }
                }
            }
            else if (c.isWaitingSock() && FD_ISSET(c.getConnfd(), &rfds)) {
                if (c.receiveSockReply()) {
                    log("clients[%d] sock server is taking over now\n", i);
                    c.startReading();
                    FD_CLR(c.getConnfd(), &ws);
                } else {
                    c.isClose();
                    FD_CLR(c.getConnfd(), &rs);
                    FD_CLR(c.getConnfd(), &ws);
                    close(c.getConnfd());
                    log("clients[%d] connection is closed since sock server is rejected\n", i);
                    connections--;
                }
            }
            else if (c.isReading() && FD_ISSET(c.getConnfd(), &rfds)) {
                log("clients[%d] is reading\n", i);
                std::string msg(c.read());
                if (msg == "") {
                    c.isClose();
                    FD_CLR(c.getConnfd(), &rs);
                    FD_CLR(c.getConnfd(), &ws);
                    close(c.getConnfd());
                    connections--;
                    log("clients[%d] connection is closed\n", i);
                    continue;
                }
                c.writeHTML(msg);
                if (msg.rfind("% ") != std::string::npos) {
                    c.startWriting();
                    FD_CLR(c.getConnfd(), &rs);
                    FD_SET(c.getConnfd(), &ws);
                    log("clients[%d] is switching from reading to writing\n", i);
                }
            }
            else if (c.isWriting() && FD_ISSET(c.getConnfd(), &wfds)) {
                log("clients[%d] is writing\n", i);
                if (c.write()) {
                    c.startReading();
                    FD_SET(c.getConnfd(), &rs);
                    FD_CLR(c.getConnfd(), &ws);
                    log("clients[%d] is switching from writing to reading\n", i);
                }
            }
        }
    }
}

