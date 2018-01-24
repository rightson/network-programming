#pragma once
#include <io.h>         // _access
#include <direct.h>     // _getcwd
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <utility>

#include "utils.h"
#include "cgi.h"

extern HWND hwndEdit;
extern int EditPrintf(HWND, TCHAR *, ...);

const int DEBUG = 1;

class HTTPHandler {
public:
    HTTPHandler(
        HWND hwnd,
        SOCKET msock,
        SOCKET ssock,
        std::map<SOCKET, RequestProfile&>& cgiHandleMap,
        int cgiNotifyCode) : hwnd(hwnd), msock(msock), ssock(ssock), cgiHandleMap(cgiHandleMap), cgiNotifyCode(cgiNotifyCode) {
        mustHaveEnv();
        char *cwd = _getcwd(NULL, 0);
        EditPrintf(hwndEdit, TEXT("[http] create new HTTP Handler for ssock: %d cwd: %s\r\n"), ssock, cwd);
        document_root = std::string(cwd);
        statusCode[200] = "HTTP/1.1 200 OK\r\n";
        statusCode[400] = "HTTP/1.1 400 Bad Request\r\n";
        statusCode[401] = "HTTP/1.1 401 Forbidden\r\n";
        statusCode[404] = "HTTP/1.1 404 Not Found\r\n";
        statusCode[405] = "HTTP/1.1 405 Method Not Allowed\r\n";
        statusCode[500] = "HTTP/1.1 500 Internal Error\r\n";
    }
    int receiveRequest() {
        char buffer[utils::MAX_BUFFER_SIZE] = { 0 };
        size_t n = utils::readFD(ssock, buffer);
        if (n == -1) {
            EditPrintf(hwndEdit, TEXT("Error: Failed to read HTTP request\r\n"));
            return showErrorStatus(500);
        }

        EditPrintf(hwndEdit, TEXT("[http] receive HTTP request SOCKET: %d\r\n"), ssock);
        parseRequestHeader(buffer);
        if (request_method != std::string("GET")) {
            return showErrorStatus(405);
        }
        //dump();
        return n;
    }
    int replyResponse() {
        EditPrintf(hwndEdit, TEXT("[http] reply HTTP response SOCKET: %d\r\n"), ssock);
        std::string filepath(document_root + "\\" +script_name.substr(1));
        EditPrintf(hwndEdit, TEXT("[http] request method = [%s]\r\n"), request_method.c_str());
        if (script_filename.size() > 4) {
            if (script_filename.substr(script_filename.size() - 4) == ".cgi") {
                return executeCGI(filepath);
            } else {
                return serveStaticFile(filepath);
            }
        }
        return showErrorStatus(400);
    }
    int showStatus200() {
        utils::writeFD(ssock, statusCode[200]);
        return 0;
    }
    int showErrorStatus(int code, bool showBodyAsWell = true) {
        utils::writeFD(ssock, statusCode[code]);
        if (showBodyAsWell) {
            utils::writeFD(ssock, "\r\n%s", statusCode[code]);
        }
        return code >= 500? -1: 0;
    }
    void showTextHeader(size_t content_length) {
        utils::writeFD(ssock, "Content-Length: %d\r\n", content_length);
        utils::writeFD(ssock, "Content-Type: %s\r\n\r\n", "text/html");
    }
    int executeCGI(const std::string &filepath) {
        EditPrintf(hwndEdit, "[http] execute CGI: %s (%s)\r\n", script_name.c_str(), script_filename.c_str());
        if (script_name.rfind("hw3.cgi") != std::string::npos) {
            EditPrintf(hwndEdit, "[http] CGI qs = [%s]\r\n", query_string.c_str());
            cgiWorker.setQueryString(query_string);
            cgiWorker.setupRequestProfiles(ssock);
            cgiWorker.setupConnectSockets(hwnd, cgiHandleMap, cgiNotifyCode);
            cgiWorker.connectToClients();
            showStatus200();
            utils::writeFD(ssock, "\r\n%s", cgiWorker.getDocument().c_str());
            return 1;
        }
        return serveStaticFile(filepath);
    }
    int serveStaticFile(const std::string &filepath, bool useCStyleFileIO = true) {
        EditPrintf(hwndEdit, TEXT("[http] serve static file [%s]\r\n"), filepath.c_str());
        if (_access(filepath.c_str(), 0) == -1) {
            EditPrintf(hwndEdit, TEXT("Error cannot access filepath [%s] (%s)\r\n"), filepath.c_str(), strerror(errno));
            return showErrorStatus(404);
        }
        if (useCStyleFileIO) {
            FILE *fp = fopen(filepath.c_str(), "rb");
            if (fp == NULL) {
                EditPrintf(hwndEdit, TEXT("Error: failed to open file [%s]\r\n"), filepath.c_str());
                return showErrorStatus(404);
            }
            fseek(fp, 0, SEEK_END);
            long fileSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *content = (char *)malloc(fileSize + 1);
            fread(content, 1, fileSize, fp);
            fclose(fp);
            showStatus200();
            showTextHeader(fileSize);
            if (DEBUG) EditPrintf(hwndEdit, TEXT("[http] content=%s\r\n"), content);
            utils::writeFD(ssock, content);
            free(content);
            return 0;
        } else {
            std::ifstream ifs(filepath.c_str());
            if (!ifs.good()) {
                EditPrintf(hwndEdit, TEXT("Error: failed to open file [%s]\r\n"), filepath.c_str());
                return showErrorStatus(400);
            }
            ifs.seekg(0, std::ios::end);
            std::streampos fileSize = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            std::vector<char> buffer(fileSize);
            ifs.read(&buffer[0], fileSize);
            std::string content(buffer.begin(), buffer.end());
            showStatus200();
            showTextHeader(fileSize);
            if (DEBUG) EditPrintf(hwndEdit, TEXT("[http] content=%s\r\n"), content.c_str());
            utils::writeFD(ssock, (char *)content.c_str());
            return 0;
        }
        return 0;
    }
    void setEnv(bool verbose = false) {
        for (auto &kv: env) {
            SetEnvironmentVariable(kv.first.c_str(), kv.second.c_str());
            LPTSTR value;
            GetEnvironmentVariable(kv.first.c_str(), value, MAX_ENV_LENGTH);
            if (verbose) EditPrintf(hwndEdit, TEXT("[http][env] %s = %s\r\n"), kv.first.c_str(), value);
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
        script_filename = document_root + "\\" + script_name.substr(1);
    }
    void dump() {
        for (auto &kv: env) {
            EditPrintf(hwndEdit, TEXT("[http] %s = %s\r\n"), kv.first.c_str(), kv.second.c_str());
        }
    }
    CGIWorker& getCGIWorker() {
        return cgiWorker;
    }
private:
    HWND hwnd;
    SOCKET msock;
    SOCKET ssock;
    std::map<SOCKET, RequestProfile&>& cgiHandleMap;
    int cgiNotifyCode;
    struct sockaddr_in *addr;
    std::map<std::string, std::string> env;
    std::map<int, char *> statusCode;
    static const int MAX_ENV_LENGTH = 256;
    std::string &request_method = env["REQUEST_METHOD"];
    std::string &request_uri = env["REQUEST_URI"];
    std::string &server_protocol = env["SERVER_PROTOCOL"];
    std::string &script_name = env["SCRIPT_NAME"];
    std::string &query_string = env["QUERY_STRING"];
    std::string &script_filename = env["SCRIPT_FILENAME"];
    std::string &document_root = env["DOCUMENT_ROOT"];
    CGIWorker cgiWorker;
};
