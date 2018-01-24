#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h> // socket, setsockopt, bind, listen, accept, etc.
#include <sys/wait.h>   // waitpid
#include <fcntl.h>
#include <unistd.h>
#include <wordexp.h>    // wordexp_t, wordexp, wordfree

#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <iterator>

#include "utils.hpp"

namespace utils {

void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void parseQueryString(const std::string &qs, StringMap &queryStringMap) {
    queryStringMap.clear();
    std::regex pattern("([\\w+]+)=([^&]*)");
    auto begin = std::sregex_iterator(qs.begin(), qs.end(), pattern);
    auto end = std::sregex_iterator();
    for (auto i=begin; i!=end; ++i) {
        std::smatch sm(*i);
        if (sm.size() == 3) {
            queryStringMap[sm[1].str()] = sm[2].str();
        }
    }
}

void showStringMap(const StringMap &queryStringMap) {
    for (auto &kv: queryStringMap) {
        printf("StringMap[%s] = [%s]\n", kv.first.c_str(), kv.second.c_str());
    }
}

void makeSocketNonblocking(int sockfd) {
    int flag = fcntl(sockfd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flag);
}

int getClientSocket(struct sockaddr_in &sockaddr, const char *hostname, int port, bool nonblocking) {
    int sockfd;
    sockfd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        die("Error: failed to create socket\n");
    }
    int enable = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        die("Error: failed to set socket option to SO_REUSEADDR");
    }
    struct hostent *hostentry;
    hostentry = gethostbyname(hostname);
    bzero((char *) &sockaddr, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr = *((struct in_addr *)hostentry->h_addr_list[0]);
    if (nonblocking) {
        makeSocketNonblocking(sockfd);
        if (sockfd == -1) {
            die("Failed to set non-blocking socket\n");
        }
    }
    return sockfd;
}

int getListenSocket(const int port, const int listenQ) {
    int sockfd;
    struct sockaddr_in servAddr;

    sockfd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        die("Error: failed to create socket\n");
    }

    int enable = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        die("Error: failed to set socket option to SO_REUSEADDR");
    }

    memset((char *) &servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        die("Error: failed to bind socket %d with port %d\n", sockfd, port);
    }

    if (::listen(sockfd, listenQ) < 0) {
        die("Error: failed to listen socket %d with port %d\n", sockfd, port);
    }
    return sockfd;
}

int getAcceptSocket(int listenfd, struct sockaddr_in &cliAddr) {
    memset((char *) &cliAddr, 0, sizeof(cliAddr));
    socklen_t clientLen = sizeof(cliAddr);
    int sockfd = ::accept(listenfd, (struct sockaddr *)&cliAddr, &clientLen);
    if (sockfd < 0) {
        printf("Error: failed to accept socket (connfd = %d)\n", sockfd);
    }
    return sockfd;
}

char** stringVector2CharArray(const std::vector<std::string> &stringVector, std::vector<char*>& argv) {
    argv.clear();
    for (size_t i = 0; i < stringVector.size(); ++i) {
        argv.push_back((char *)stringVector[i].c_str());
    }
    argv.push_back(NULL);
    return &argv[0];
}

bool execProcess(int stdinfd, int stdoutfd, int stderrfd, const std::vector<std::string> &stringVector) {
    pid_t pid;
    if ((pid = fork()) == 0) {
        dup2(stdinfd, STDIN_FILENO);
        dup2(stdoutfd, STDOUT_FILENO);
        dup2(stderrfd, STDERR_FILENO);

        std::vector<char*> argv;
        char **command = stringVector2CharArray(stringVector, argv);
        if (execvp(basename(command[0]), (char * const*)command) == -1) {
            writeFD(stderrfd, "Unknown command: [%s].\n", command[0]);
            exit(EXIT_FAILURE);
        }
    }
    int status;
    return (pid == waitpid(pid, &status, 0) && status == 0);
}

ssize_t writeFD(int fd, const char *format, ...) {
    char buffer[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    buffer[strlen(buffer)] = '\0';
	return write(fd, buffer, strlen(buffer));
}

ssize_t readFD(int fd, char *message) {
    int n = read(fd, message, MAX_BUFFER_SIZE);
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

void explodeString(const std::string &buffer, std::vector<std::string>& tokens) {
    std::string token;
    std::istringstream iss(buffer);
    while (iss >> token) {
        tokens.push_back(token);
    }
}

std::string expandPath(const std::string &str) {
  wordexp_t p;
  char** w;
  wordexp(str.c_str(), &p, 0);
  w = p.we_wordv;
  std::string ret;
  if (p.we_wordc > 0) {
    ret = w[0];
  }
  wordfree(&p);
  return ret;
}

} // end of namespace utils
