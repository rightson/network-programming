// Name: Remote Access System (ras) for NP

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h> // socket, setsockopt, bind, listen, accept, etc.
#include <netinet/in.h> // struct sockaddr_in
#include <unistd.h>
#include <sys/wait.h>   // waitpid
#include <signal.h>     // signal, SIGCHLD
#include <errno.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <utility>
#include <algorithm>

int DEBUG = 0; // for runtime debugging, rather than compile time flag 
void die(const char *fmt, ...);

class ArgParse {
    int argc;
    char **argv;
    unsigned long int port;
    enum { MAX_PORT_NUMBER = 65535 };
public:
    ArgParse(int argc, char **argv): port(0) {
        this->argc = argc;
        this->argv = argv;
        checkArgs();
        parseArgs();
    }
    void checkArgs() {
        if (this->argc != 2) {
            if (this->argc == 3)
                DEBUG = atoi(this->argv[2]);
            else
                die("Usage: %s PORT\n", this->argv[0]);
        }
    }
    void parseArgs() {
        this->port = strtoul(argv[1], NULL, 10);
        if (this->port == 0 || this->port > MAX_PORT_NUMBER) {
            die("Error: invalid PORT format, should be a number between 0-65535\n");
        }
    }
    const uint16_t getPort() {
        return (uint16_t)this->port;
    }
};

struct CommandInfo {
    CommandInfo(): pipeToNextNCmd(0) {}
    std::vector<std::string> argv;
    int pipeToNextNCmd;
    std::string outfile;
    void clear() {
        argv.clear();
        pipeToNextNCmd = 0;
        outfile.clear() ;
    }
};

struct Pipe {
    int source;
    int offset;
    int readFD;
    int writeFD;
};

class PipeManager {
    std::map<int, Pipe> pipeMap;
public:
    bool isValidPipe(std::size_t cmdID) {
        return (this->pipeMap.find(cmdID) != this->pipeMap.end());
    }
    std::size_t createPipe(std::size_t cmdID, int pipeToNextNCmd = 0) {
        int targetCmdID = cmdID + pipeToNextNCmd;
        int pipefd[2];
        pipe(pipefd);
        fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
        Pipe newPipe;
        newPipe.source = cmdID;
        newPipe.offset = pipeToNextNCmd;
        newPipe.readFD = pipefd[0];
        newPipe.writeFD = pipefd[1];
        this->pipeMap[targetCmdID] = newPipe;
        return targetCmdID;
    }
    std::size_t getReadFD(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            return this->pipeMap[cmdID].readFD;
        } else {
            return STDIN_FILENO;
        }
    }
    std::size_t getWriteFD(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            return this->pipeMap[cmdID].writeFD;
        } else {
            return STDOUT_FILENO;
        }
    }
    void closeReadFD(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            if (this->pipeMap[cmdID].readFD != -1) {
                close(this->pipeMap[cmdID].readFD);
                this->pipeMap[cmdID].readFD = -1;
            }
        }
    }
    void closeWriteFD(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            if (this->pipeMap[cmdID].writeFD != -1) {
                close(this->pipeMap[cmdID].writeFD);
                this->pipeMap[cmdID].writeFD = -1;
            }
        }
    }
    void erasePipe(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            this-> pipeMap.erase(cmdID);
        }
    }
    void closeAndErasePipe(std::size_t cmdID) {
        if (this->isValidPipe(cmdID)) {
            this->closeReadFD(cmdID);
            this->closeWriteFD(cmdID);
            this-> pipeMap.erase(cmdID);
        }
    }
    std::string dumpPipes() {
        std::ostringstream oss;
        for (std::map<int, Pipe>::iterator it=pipeMap.begin(); it!=pipeMap.end(); ++it) {
            oss << "[" << it->second.source << "]<" << it->second.readFD << "," << it->second.writeFD << ">";
            oss << " -> ";
            oss << "[" << it->second.source + it->second.offset << "](" << it->second.offset << ")<"
                       << it->second.readFD << "," << it->second.writeFD << ">";
            oss << ", " << std::flush;
        }
        return oss.str();
    }
};

class RemoteShellService {
    std::string RAS_ROOT;
    int connfd;
    int cmdID;
    enum {MAX_LINE_BUFFER_SIZE = 10000 };
    std::vector<std::string> builtinCmds;
    std::vector<std::string> tokens;
    std::vector<CommandInfo> commands;
    PipeManager pipeMgr;
public:
    RemoteShellService() : cmdID(0) {
        this->RAS_ROOT = std::string(getenv("HOME")) + std::string("/ras/");
        this->builtinCmds.push_back("printenv");
        this->builtinCmds.push_back("setenv");
        this->builtinCmds.push_back("exit");
        this->builtinCmds.push_back("pwd");
    }
    void start(int connfd) {
        this->connfd = connfd;
        chdir(this->RAS_ROOT.c_str());
        setenv("PATH","bin:.",1);
        dup2(connfd, STDIN_FILENO);
        dup2(connfd, STDOUT_FILENO);
        dup2(connfd, STDERR_FILENO);
        std::cout << "****************************************" << std::endl
                  << "** Welcome to the information server. **" << std::endl
                  << "****************************************" << std::endl;
        for (;;) {
            if (DEBUG) {
                std::cout << "[" << (this->cmdID+1) << "]" << "% " << std::flush;
            } else {
                std::cout << "% " << std::flush;
            }
            std::string line;
            std::getline(std::cin, line);
            processCommandLine(line);
        }
    }
    void processCommandLine(std::string& line) {
        this->tokens.clear();
        explodeString(line, this->tokens);
        this->commands.clear();
        extractCommandInfo(this->tokens);
        if (this->tokens.size() == 0) {
            return;
        }
        if (this->isBuiltInCommands()) {
            this->executeBuiltInCommands();
        } else {
            this->executeShellCommands();
        }
    }
    void explodeString(const std::string &buffer, std::vector<std::string>& tokens) {
        std::string token;
        std::istringstream iss(buffer);
        while (iss >> token) {
            tokens.push_back(token);
        }
    }
    void extractCommandInfo(const std::vector<std::string> &tokens) {
        CommandInfo cmdInfo;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i][0] == '|') {
                std::size_t sz;
                cmdInfo.pipeToNextNCmd = tokens[i].size() < 2? 1: std::stoi(tokens[i].substr(1), &sz);
                this->commands.push_back(cmdInfo);
                cmdInfo.clear();
            } else if (tokens[i][0] == '>') {
                if ((i+1) == tokens.size()) {
                    std::cerr << "Error: '>' should have a following name" << std::endl;
                }
                cmdInfo.outfile = tokens[++i];
                this->commands.push_back(cmdInfo);
                cmdInfo.clear();
            } else {
                cmdInfo.argv.push_back(tokens[i]);
            }
        }
        if (cmdInfo.argv.size()) {
            this->commands.push_back(cmdInfo);
        }
    }
    void executeShellCommands() {
        int stdinfd = STDIN_FILENO; // determined by previous command if any
        int stdoutfd = STDOUT_FILENO;
        int stderrfd = STDERR_FILENO;
        for (std::size_t i = 0; i < this->commands.size(); ++i) {
            this->cmdID++;
            CommandInfo &cmd = commands[i];

            bool fileRedirected = false;
            if (cmd.outfile.size()) {
                stdoutfd = open(cmd.outfile.c_str(), O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0755);
                if (stdoutfd == -1) {
                    std::cout << "Error: cannot open file: " << cmd.outfile << std::endl;
                    return;
                }
                fileRedirected = true;
            }

            bool isStdinPiped = false;
            if (this->pipeMgr.isValidPipe(this->cmdID)) {
                isStdinPiped = true;
                stdinfd = this->pipeMgr.getReadFD(this->cmdID);
                this->pipeMgr.closeWriteFD(this->cmdID);
            }
            if (cmd.pipeToNextNCmd) {
                std::size_t targetCmdID = this->cmdID + cmd.pipeToNextNCmd;
                if (this->pipeMgr.isValidPipe(targetCmdID)) {
                    stdoutfd = this->pipeMgr.getWriteFD(targetCmdID);
                } else {
                    this->pipeMgr.createPipe(this->cmdID, cmd.pipeToNextNCmd);
                    stdoutfd = this->pipeMgr.getWriteFD(targetCmdID);
                }
            }

            if (DEBUG) printf("<<DEBUG>> Before exec: ");
            dumpOne(this->cmdID, cmd, commands);
            if (!execProcess(stdinfd, stdoutfd, stderrfd, cmd.argv)) {
                if (DEBUG) printf("<<DEBUG>> After exec ERROR: ");
                // close
                if (!isStdinPiped) {
                    if (DEBUG) printf("stdin is not from piped, bye");
                    return;
                }
                if (i == 0) {
                    if (DEBUG) printf("Failed command counts, no backup, clean pipeline");
                    this->pipeMgr.closeAndErasePipe(cmdID);
                    return;
                }

                dumpOne(this->cmdID, cmd, commands);
                cmdID--;
                return;
            }

            if (fileRedirected) {
                close(stdoutfd);
            }
            this->pipeMgr.closeAndErasePipe(cmdID);
            if (DEBUG) printf("<<DEBUG>> After exec: ");
            dumpOne(this->cmdID, cmd, commands);
        }
    }
    bool isBuiltInCommands() {
        std::vector<std::string> &v = this->builtinCmds;
        return std::find(v.begin(), v.end(), this->tokens[0]) != v.end();
    }
    void executeBuiltInCommands() {
        std::string &cmd = this->tokens[0];
        if (cmd == "exit") {
            close(this->connfd);
            exit(0);
        } else if (cmd == "pwd") {
            char buffer[MAX_LINE_BUFFER_SIZE];
            if (getcwd(buffer, MAX_LINE_BUFFER_SIZE)) {
                std::cout << buffer << std::endl;
            } else {
                std::cerr << "Error: failed to getcwd()" << std::endl;
            }
        } else if (cmd == "printenv") {
            if (tokens.size() > 1) {
#ifdef __APPLE__
                char *result = getenv(tokens[1].c_str());
#else
                char *result = secure_getenv(tokens[1].c_str());
#endif
                std::cout << tokens[1] << "=" << (result != NULL? result: "") << std::endl;
            } else {
                std::cout << "Usage: printenv ENV_NAME" << std::endl;
            }
        } else if (cmd == "setenv") {
            if (tokens.size() < 2) {
                std::cout << "Usage: setenv ENV_NAME [value]" << std::endl;
            } else {
                setenv(tokens[1].c_str(), (tokens.size() > 2? tokens[2].c_str(): ""), 1);
            }
        }
    }
    bool execProcess(int stdinfd, int stdoutfd, int stderrfd, const std::vector<std::string> &tokens) {
        pid_t pid;
        if ((pid = fork()) == 0) {
            dup2(stdinfd, STDIN_FILENO);
            dup2(stdoutfd, STDOUT_FILENO);
            dup2(stderrfd, STDERR_FILENO);

            std::vector<char*> argv;
            char **command = stringVector2CharArray(tokens, argv);
            if (execvp(command[0], (char * const*)command) == -1) {
                std::cerr << "Unknown command: [" << command[0] << "]." << std::endl;
                exit(1);
            }
        }
        int status;
        return (pid == waitpid(pid, &status, 0) && status == 0);
    }
    char **stringVector2CharArray(const std::vector<std::string> &tokens, std::vector<char*>& argv) {
        argv.clear();
        for (size_t i = 0; i < tokens.size(); ++i) {
            argv.push_back((char *)tokens[i].c_str());
        }
        argv.push_back(NULL);
        return &argv[0];
    }
    std::string argvToString(std::vector<std::string> &argv) {
        std::ostringstream oss;
        for (std::size_t j = 0; j < argv.size(); ++j) {
            oss << argv[j];
            if ((j+1) != argv.size()) {
                oss << " " << std::flush;
            }
        }
        return oss.str();
    }
    void dumpOne(std::size_t cmdID, CommandInfo& cmd, std::vector<CommandInfo>& cmds) {
        if (!DEBUG) return;
        printf("[%lu]<%s> {%s}\n", cmdID, argvToString(cmd.argv).c_str(), this->pipeMgr.dumpPipes().c_str());
    }
    void dump(std::size_t cmdID, std::vector<CommandInfo>& cmds) {
        if (!DEBUG) return;

        printf("[%lu]<", cmdID);
        for (std::size_t i = 0; i < cmds.size(); ++i) {
            printf("%s", argvToString(cmds[i].argv).c_str());
            if ((i+1) != cmds.size()) {
                printf(", ");
            }
        }
        printf("> ");
        printf("{%s}\n", this->pipeMgr.dumpPipes().c_str());
    }
};

class SocketServer {
    int port;
    int listenfd;
    int max_backlog;
public:
    SocketServer(int port): port(0), listenfd(-1), max_backlog(5) {
        this->port = port;
    }
    void start() {
        this->listenfd = this->getListenSocket(this->port, this->max_backlog);
        std::cout << "Server is listening port " << this->port << std::endl;
        for (;;) {
            int connfd = this->accept(listenfd);
            if (fork() == 0) { // child
                std::cout << "Child " << getpid() << " accepts connections " << connfd << std::endl;
                close(listenfd);
                RemoteShellService ras;
                ras.start(connfd);
                exit(0);
            } else {
                close(connfd);
            }
        }
    }
    const int accept(const int listenfd) {
        int sockfd = ::accept(listenfd, (struct sockaddr *)NULL, NULL);
        if (sockfd < 0) {
            die("Error: failed to accept socket\n");
        }
        return sockfd;
    }
    int getListenSocket(const int port, const int max_backlog) {
        int sockfd;
        struct sockaddr_in sockaddr;

        sockfd = ::socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            die("Error: failed to create socket\n");
        }

        int enable = 1;
        if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            die("Error: failed to set socket option to SO_REUSEADDR");
        }

        bzero((char *) &sockaddr, sizeof(sockaddr));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
            die("Error: failed to bind socket %d with port %d\n", sockfd, port);
        }

        if (::listen(sockfd, max_backlog) < 0) {
            die("Error: failed to listen socket %d with port %d\n", sockfd, port);
        }

        return sockfd;
    }
};

void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void waitChildrenTerminated(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        //std::cout << "Child " << pid << " terminated" << std::endl;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGCHLD, waitChildrenTerminated);
    ArgParse parser(argc, argv);
    SocketServer server(parser.getPort());
    server.start();
    return EXIT_SUCCESS;
}

