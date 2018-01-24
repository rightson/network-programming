#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // inet_ntoa
#include <fcntl.h>		// O_WRONLY
#include <sys/stat.h>   // mkfifo
#include <sys/wait.h>   // waitpid

#include <sstream>

#include "utility.hpp"

void die(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

char *formatToMessage(const char *format, ...) {
    static char message[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    message[strlen(message)] = '\0';
    return message;
}

size_t writeFD(int fd, const char *format, ...) {
    char buffer[MAX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_BUFFER_SIZE, format, args);
    va_end(args);
    buffer[strlen(buffer)] = '\0';
    return write(fd, buffer, strlen(buffer));
}

size_t readFD(int fd, char *message) {
    int n = read(fd, message, MAX_BUFFER_SIZE);
    if (n >= 0) {
        message[n] = '\0';
    }
    return n;
}

void createFIFO(const char *fifopath) {
    if (access(fifopath, F_OK) != -1) {
        printf("[utility] unlink(%s)\n", fifopath);
        unlink(fifopath);
    }
    printf("[utility] mkfifo(%s, 0755)\n", fifopath);
    if (mkfifo(fifopath, 0755) == -1) {
        die("Failed to mkfifo at %s\n");
    }
}

size_t readFIFO(const char *fifopath, char *message) {
    int fifofd = open(fifopath, O_RDONLY);
    size_t n = readFD(fifofd, message);
    close(fifofd);
    return n;
}

size_t writeFIFO(const char *fifopath, const char *message) {
    int fifofd = open(fifopath, O_WRONLY);
    size_t n = writeFD(fifofd, message);
    close(fifofd);
    return n;
}

char *getIpPortBinding(struct sockaddr_in &cliAddr) {
    const int MAX_IP_ADDR = 30;
    char ipAddr[MAX_IP_ADDR];
    inet_ntop(AF_INET, &cliAddr.sin_addr, ipAddr, sizeof(ipAddr));
    int port = ntohs(cliAddr.sin_port);
    const int MAX_NAME_SIZE = 36;
    static char buffer[MAX_NAME_SIZE]; // dirty
    sprintf(buffer, "%s/%d", ipAddr, port);
    return buffer;
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

void explodeString(const std::string &buffer, std::vector<std::string>& tokens) {
    std::string token;
    std::istringstream iss(buffer);
    while (iss >> token) {
        tokens.push_back(token);
    }
}

char** stringVector2CharArray(const std::vector<std::string> &stringVector, std::vector<char*>& argv) {
    argv.clear();
    for (size_t i = 0; i < stringVector.size(); ++i) {
        argv.push_back((char *)stringVector[i].c_str());
    }
    argv.push_back(NULL);
    return &argv[0];
}

std::string stringVectorToString(std::vector<std::string> &stringVector) {
    std::ostringstream oss;
    for (std::size_t j = 0; j < stringVector.size(); ++j) {
        oss << stringVector[j];
        if ((j+1) != stringVector.size()) {
            oss << " " << std::flush;
        }
    }
    return oss.str();
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

