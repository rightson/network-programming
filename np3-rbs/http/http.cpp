#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>  // inet_ntoa

#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <streambuf>

#include "utils.hpp"

class HTTPHandler {
public:
    HTTPHandler(const std::string &request,
                const std::string &cwd,
                const int listenfd,
                const int connfd,
                const struct sockaddr_in *addr): cwd(cwd), listenfd(listenfd), connfd(connfd), addr(addr) {
        mustHaveEnv();
        parseRequestHeader(request);
        dump();
    }
    void showStatus200() {
        utils::writeFD(connfd, "HTTP/1.1 200 OK\r\n");
    }
    void showStatus401() {
        utils::writeFD(connfd, "HTTP/1.1 401 Permission Denid\r\n");
    }
    void showStatus404() {
        utils::writeFD(connfd, "HTTP/1.1 404 Not Found\r\n");
    }
    void showStatus405() {
        utils::writeFD(connfd, "HTTP/1.1 405 Method Not Allowed\r\n");
    }
    void showStatus500() {
        utils::writeFD(connfd, "HTTP/1.1 500 Internal Server Error\r\n");
    }
    void showTextHeader(size_t content_length) {
        utils::writeFD(connfd, "Content-Length: %d\r\n", content_length);
        utils::writeFD(connfd, "Content-Type: %s\r\n\r\n", "text/html");
    }
    int sendResponse() {
        if (request_method != "GET") {
            showStatus405();
            return -1;
        }
        if (access(script_filename.c_str(), F_OK) == -1) {
            showStatus404();
            return -1;
        }
        if (script_filename.size() > 4) {
            if (script_filename.substr(script_filename.size() - 4) == ".cgi") {
                showStatus200();
                return executeCGI();
            } else {
                return serveStaticFile();
            }
        }
        showStatus500();
        return EXIT_FAILURE;
    }
    int executeCGI() {
        printf("[server] execute CGI: %s\n", script_filename.c_str());
        if (fork() == 0) {
            printf("[server] CGI exec pid = %d\n", getpid());
            dup2(connfd, STDIN_FILENO);
            dup2(connfd, STDOUT_FILENO);
            dup2(connfd, STDERR_FILENO);
            close(connfd);
            close(listenfd);
            setEnv(false);
            execl(script_filename.c_str(), script_filename.c_str(), NULL);
            exit(-1);
        }
        return 0;
    }
    int serveStaticFile(bool useCStyleFileIO = false) {
        printf("[server] serve static file\n");
        if (useCStyleFileIO) {
            FILE *fp = fopen(script_filename.c_str(), "rb");
            fseek(fp, 0, SEEK_END);
            long fileSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *content = (char *)malloc(fileSize + 1);
            fread(content, 1, fileSize, fp);
            fclose(fp);
            showStatus200();
            showTextHeader(fileSize);
            if (DEBUG) printf("[server] content=%s\n", content);
            size_t n = utils::writeFD(connfd, content);
            free(content);
            return n;
        } else {
            std::ifstream ifs(script_filename);
            if (!ifs.good()) {
                utils::writeFD(connfd, "HTTP/1.1 404 Not Found\r\n");
                return -1;
            }
            ifs.seekg(0, std::ios::end);
            std::streampos fileSize = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            std::vector<char> buffer(fileSize);
            ifs.read(&buffer[0], fileSize);
            std::string content(buffer.begin(), buffer.end());
            showStatus200();
            showTextHeader(fileSize);
            if (DEBUG) printf("[server] content=%s\n", content.c_str());
            return utils::writeFD(connfd, (char *)content.c_str());
        }
        return 0;
    }
    void setEnv(bool verbose = false) {
        clearenv();
        for (auto &kv: env) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
            if (verbose) printf("[server][env] %s = %s\n", kv.first.c_str(), getenv(kv.first.c_str()));
        }
    }
    void mustHaveEnv() {
        env["QUERY_STRING"] = "";
        env["CONTENT_LENGTH"] = "";
        env["REQUEST_METHOD"] = "";
        env["SCRIPT_NAME"] = "";
        env["REMOTE_HOST"] = "";
        env["REMOTE_ADDR"] = "";
        env["AUTH_TYPE"] = "";
        env["REMOTE_USER"] = "";
        env["REMOTE_IDENT"] = "";
    }
    void parseRequestHeader(const std::string &request) {
        std::istringstream iss(request);
        iss >> request_method >> request_uri >> server_protocol;
        script_name = request_uri;
        size_t pos = request_uri.find_first_of('?');
        if (pos != std::string::npos) {
            if ((pos+1) < request_uri.size()) {
                query_string = request_uri.substr(pos+1);
            }
            script_name = request_uri.substr(0, pos);
        }
        script_filename = document_root + script_name;
        if (script_name.size() > 2) {
            // ugly way to handle /~username/* case
            if (script_name[0] == '/' && script_name[1] == '~') {
                size_t pos = script_name.substr(2).find_first_of('/');
                if (pos != std::string::npos) {
                    pos += 2;
                    //FIXME: biased assumption about the path of userdir (e.g. ~username -> /home/username/public_html)
                    script_filename = utils::expandPath(script_name.substr(1, pos)) + "public_html" + script_name.substr(pos);
                }
            }
        }
    }
    void dump() {
        for (auto &kv: env) {
            printf("[server] %s = %s\n", kv.first.c_str(), kv.second.c_str());
        }
    }
private:
    const std::string cwd;
    const int listenfd;
    const int connfd;
    const struct sockaddr_in *addr;
    std::map<std::string, std::string> env;
    //Thanks C++11! I love syntax sugar!
    static const int MAX_ENV_LENGTH = 256;
    std::string &request_method = env["REQUEST_METHOD"];
    std::string &request_uri = env["REQUEST_URI"];
    std::string &server_protocol = env["SERVER_PROTOCOL"];
    std::string &script_name = env["SCRIPT_NAME"];
    std::string &query_string = env["QUERY_STRING"];
    std::string &script_filename = env["SCRIPT_FILENAME"];
    std::string &document_root = env["DOCUMENT_ROOT"] = cwd;
};

class Server {
public:
    Server(int port, const char *cwd, const std::string &mode) :
        mode(mode),
        pid(getpid()),
        port(port),
        cwd(cwd),
        listenfd(utils::getListenSocket(port)) {
        printf("[server] mode = %s\n", mode.c_str());
        if (chdir(cwd) == -1) {
            utils::die("[server] Failed to chdir to %s\n", cwd);
        }
        printf("[server] pid = %d\n", pid);
        printf("[server] port = %d\n", port);
        printf("[server] cwd = %s\n", cwd);
        memset(buffer, 0, MAX_BUFFER_SIZE);
        httpHandler.clear();
    }
    int start() {
        if (mode == "FORK") {
            return forkServer();
        } else if (mode == "SELECT") {
            return selectServer();
        } else {
            utils::die("Unknown server mode %s. Candidates mode: FORK | SELECT\n", mode.c_str());
            return EXIT_FAILURE;
        }
    }
    int forkServer() {
        for (;;) {
            struct sockaddr_in cliAddr;
            int connfd = utils::getAcceptSocket(listenfd, cliAddr);
            if (fork() == 0) {
                if (utils::readFD(connfd, buffer) < 0) {
                    close(connfd);
                    exit(EXIT_FAILURE);
                }
                HTTPHandler http(buffer, cwd, listenfd, connfd, &cliAddr);;
                http.sendResponse();
            }
            close(connfd);
        }
        return EXIT_SUCCESS;
    }
    int selectServer() {
        int maxfd = listenfd;
        fd_set rfds;
        fd_set wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        for (;;) {
            FD_SET(listenfd, &rfds);
            if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) < 0) {
                utils::die("[server] Error: failed to select fds\n");
            }
            if (FD_ISSET(listenfd, &rfds)) {
                struct sockaddr_in cliAddr;
                int connfd = utils::getAcceptSocket(listenfd, cliAddr);
#ifdef POLICY
                std::string ip = inet_ntoa(cliAddr.sin_addr);
                printf("Deny 140.113.167.* - [%s]\n", ip.c_str());
                if (ip.rfind("140.113.167.") != std::string::npos) {
                    utils::writeFD(connfd, "\r\nHTTP/1.1 401 Permission Denid\r\n");
                    utils::writeFD(connfd, "\r\nConnection Denid\r\n");
                    close(connfd);
                    continue;
                }
#endif
                FD_SET(connfd, &rfds);
                FD_SET(connfd, &wfds);
                if (connfd > maxfd) {
                    maxfd = connfd;
                }
                if (utils::readFD(connfd, buffer) < 0) {
                    close(connfd);
                    FD_CLR(connfd, &rfds);
                    continue;
                }
                if (DEBUG) printf("[server] HTTP Request [%d]:\n%s#\n", connfd, buffer);
                httpHandler.insert(std::make_pair(connfd, std::move(HTTPHandler(buffer, cwd, listenfd, connfd, &cliAddr))));;
            }
            for (int fd=0; fd < FD_SETSIZE; ++fd) {
                if (FD_ISSET(fd, &wfds)) {
                    auto it = httpHandler.find(fd);
                    if (it != httpHandler.end()) {
                        it->second.sendResponse();
                        FD_CLR(fd, &rfds);
                        FD_CLR(fd, &wfds);
                        close(fd);
                        it = httpHandler.erase(it);
                    }
                }
            }
        }
        return EXIT_SUCCESS;
    }
private:
    const std::string mode = "SELECT";
    const pid_t pid = -1;
    const int port = -1;
    const char *cwd;
    const int listenfd = -1;
    char buffer[MAX_BUFFER_SIZE];
    std::map<int, HTTPHandler> httpHandler;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        utils::die("Usage: %s PORT DOCUMENT_ROOT [SELECT|FORK]\n", argv[0]);
    }
    Server server(atoi(argv[1]), argv[2], argc==4? argv[3]: "SELECT");
    return server.start();
}

