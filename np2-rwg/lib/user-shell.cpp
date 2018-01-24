#include <string.h>
#include <fcntl.h>      // O_WRONLY

#include <regex>        // C++11 feature

#include "user-shell.hpp"
#include "utility.hpp"

extern int DEBUG;
extern void broadcast(const char *format, ...);
extern void unicast(int userID, const char *format, ...);

UserShell::UserShell(pid_t mypid, int connfd, int clientID, ClientInfo *clientList) :
    mypid(mypid),
    connfd(connfd),
    clientID(clientID),
    clientList(clientList)
{
    printf("[user-shell] construct Client %d\n", clientID);
    currentClient = &clientList[clientID-1];
    memset(buffer, 0, MAX_BUFFER_SIZE);
    localenv["PATH"] = "bin:.";
}

UserShell::~UserShell() {
    resetMyPipes(clientID);
}

void UserShell::showLoginMessage() {
    writeFD(connfd,
            "****************************************\n"
            "** Welcome to the information server. **\n"
            "****************************************\n");
}

void UserShell::showPrompt() {
    writeFD(connfd, "%% ");
}

bool UserShell::processCommandline() {
    int n = readFD(connfd, buffer);
    if (n == 0) {
        printf("[user-shell] Client pid=%d read EOF\n", mypid);
        return false;
    }
    buffer[strcspn(buffer, "\r\n")] = 0;

    CommandlineInfo cmdline;
    cmdline.cmdline = buffer;
    explodeString(buffer, cmdline.tokens);
    if (cmdline.tokens.size() == 0) {
        showPrompt();
        return true;
    }
    int status = processShellCommands(cmdline);
    if (status == -1) {
        return false;
    }
    else if (status != 0) {
        processBinCommands(cmdline);
    }
    showPrompt();
    return true;
}

int UserShell::processShellCommands(CommandlineInfo &cmdline) {
    std::vector<std::string> &tokens = cmdline.tokens;
    std::string &cmd = tokens[0];
    if (cmd == "exit") {
        return -1;
    }
    if (cmd == "pwd") {
        if (getcwd(buffer, MAX_BUFFER_SIZE)) {
            writeFD(connfd, "%s\n", buffer);
        } else {
            writeFD(connfd, "Failed to getcwd()\n", buffer);
        }
        return 0;
    }
    if (cmd == "printenv") {
        if (tokens.size() > 1) {
            writeFD(connfd, "%s=%s\n", tokens[1].c_str(), localenv[ tokens[1] ].c_str());
        } else {
            writeFD(connfd, "Usage: printenv ENV_NAME\n");
        }
        return 0;
    }
    if (cmd == "setenv") {
        if (tokens.size() < 2) {
            writeFD(connfd, "Usage: setenv ENV_NAME [value]\n");
        } else if (tokens.size() == 2) {
            localenv[tokens[1]] = "";
        } else {
            localenv[tokens[1]] = tokens[2];
        }
        return 0;
    }
    if (cmd == "who") {
        writeFD(connfd, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
        for (int i = 0; i < MAX_CLIENT_NUMBER; ++i) {
            if (!clientList[i].online) {
                continue;
            }
            writeFD(connfd, "%d\t%s\t%s%s\n",
                    clientList[i].id,
                    clientList[i].nickname,
                    clientList[i].address,
                    (clientList[i].id == currentClient->id)? "\t<-me": "");
        }
        return 0;
    }
    if (cmd == "tell") {
        if (tokens.size() < 3) {
            writeFD(connfd, "Usage: tell ID MESSAGE\n");
            return 0;
        }
        std::vector<std::string> contents(tokens.begin()+2, tokens.end());
        int id = atoi(tokens[1].c_str());
        std::smatch sm;
        std::regex e("tell\\s+\\d+\\s+(.*)");
        if (!std::regex_search(cmdline.cmdline, sm, e)) {
            writeFD(id, "Error: regex search error: Usage: tell FD MESSAGE\n");
            return 0;
        }
        if (sm.size() != 2) {
            writeFD(id, "Error: regex match error: Usage: tell FD MESSAGE\n");
            return 0;
        }
        if (id == currentClient->id) {
            writeFD(id, "*** %s told you ***: %s\n", currentClient->nickname, sm[1].str().c_str());
        } else {
            unicast(id, "*** %s told you ***: %s\n", currentClient->nickname, sm[1].str().c_str());
        }
        return 0;
    }
    if (cmd == "yell") {
        std::vector<std::string> contents(tokens.begin()+1, tokens.end());
        std::string msg(stringVectorToString(contents));
        broadcast("*** %s yelled ***: %s\n", currentClient->nickname, msg.c_str());
        return 0;
    }
    if (cmd == "name") {
        if (tokens.size() != 2) {
            writeFD(connfd, "Usage: name NICKNAME\n");
        } else {
            for (int i=0; i<MAX_CLIENT_NUMBER; ++i) {
                if (tokens[1] == std::string(clientList[i].nickname)) {
                    writeFD(currentClient->fd, "*** User '%s' already exists. ***\n", clientList[i].nickname);
                    return 0;
                }
            }
            strcpy(currentClient->nickname, tokens[1].c_str());
            broadcast("*** User from %s is named '%s'. ***\n", currentClient->address, currentClient->nickname);
        }
        return 0;
    }
    return 1;
}

bool UserShell::parseCommands(CommandlineInfo &cmdline) {
    std::vector<std::string> &tokens = cmdline.tokens;
    std::vector<CommandInfo> &cmds = cmdline.cmds;
    cmds.clear();
    CommandInfo cmd;
    resetCommandInfo(cmd);
    cmd.cmdline = cmdline.cmdline.c_str();
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i][0] == '|') {
            std::size_t sz;
            cmd.pipeToNextNCmd = tokens[i].size() < 2? 1: std::stoi(tokens[i].substr(1), &sz);
            cmds.push_back(cmd);
            resetCommandInfo(cmd);
            continue;
        }
        if (tokens[i][0] == '>') {
            if (tokens[i].size() > 1) {
                cmd.pipeToClientID = atoi(tokens[i].substr(1).c_str());
                getPeerNamedPipe(buffer, clientID, cmd.pipeToClientID);
                if (DEBUG) writeFD(connfd, " %d --> %d \n", clientID, cmd.pipeToClientID);
                cmd.outfile = buffer;
            } else {
                if ((i+1) == tokens.size()) {
                    writeFD(connfd, "Error: '>' should have a following file name\n");
                    return false;
                }
                cmd.outfile = tokens[++i];
                cmds.push_back(cmd);
                resetCommandInfo(cmd);
            }
            continue;
        }
        if (tokens[i][0] == '<') {
            if (tokens[i].size() <= 1) {
                writeFD(connfd, "Error: invalid '%s' format\n", tokens[i].c_str());
                return false;
            }
            cmd.pipeFromClientID = atoi(tokens[i].substr(1).c_str());
            getPeerNamedPipe(buffer, cmd.pipeFromClientID, clientID);
            if (DEBUG) writeFD(connfd, " %d --> %d \n", cmd.pipeFromClientID, clientID);
            cmd.infile = buffer;
            continue;
        }
        cmd.argv.push_back(tokens[i]);
    }
    if (cmd.argv.size()) {
        cmds.push_back(cmd);
    }
    return true;
}


int UserShell::processBinCommands(CommandlineInfo &cmdline) {
    if (!parseCommands(cmdline)) {
        printf("Error: invalid cmdline format: [%s]\n", cmdline.cmdline.c_str());
        return 0;
    }
    std::vector<CommandInfo> &cmds = cmdline.cmds;
    if (DEBUG) dumpCommands(cmds);

    int stdinfd = STDIN_FILENO;
    int stdoutfd = currentClient->fd;
    int stderrfd = currentClient->fd;
    printf("[bin-cmd] Processing cmdline: [%s] cmds.size(): %lu\n", cmdline.cmdline.c_str(), cmds.size());
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        ++currentClient->cmdID;
        CommandInfo &cmd = cmds[i];
        printf("[bin-cmd] Processing cmds[%d]: %s\n", currentClient->cmdID, cmdline.cmds[0].argv[0].c_str());

        bool outPipe = false;
        bool inPipe = false;

        if (cmd.pipeFromClientID != -1) {
            if (!clientList[cmd.pipeFromClientID-1].online) {
                writeFD(connfd, "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
                        cmd.pipeFromClientID, clientID);
                return 0;
            }
            inPipe = cmd.infile.size() > 0;
            if (inPipe) {
                stdinfd = open(cmd.infile.c_str(), O_RDONLY|O_CLOEXEC);
                if (stdinfd == -1) {
                    writeFD(connfd, "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
                            cmd.pipeFromClientID, clientID);
                    return 0;
                }
            }
            printf("inPipe: %d -> %d\n", cmd.pipeFromClientID-1, clientID);
            ClientInfo *src = &clientList[cmd.pipeFromClientID-1];
            ClientInfo *dst = &clientList[clientID-1];
            broadcast("*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
                dst->nickname, clientID,
                src->nickname, cmd.pipeFromClientID,
                cmd.cmdline);
        }
        if (cmd.pipeToClientID != -1) {
            if (!clientList[cmd.pipeToClientID-1].online) {
                writeFD(connfd, "*** Error: user #%d does not exist yet. ***\n", cmd.pipeToClientID);
                        
                return 0;
            }
            outPipe = cmd.outfile.size() > 0;
            if (outPipe) {
                if(access(cmd.outfile.c_str(), F_OK) != -1 && !inPipe) {
                    writeFD(connfd, "*** Error: the pipe #%d->#%d already exists. ***\n",
                        clientID, cmd.pipeToClientID);
                    return 0;
                }
            }
        }

        bool fileRedirected = false;
        if (cmd.outfile.size() > 0) {
            stdoutfd = open(cmd.outfile.c_str(), O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0755);
            if (stdoutfd == -1) {
                writeFD(connfd, "Error: failed to open file %s\n", cmd.outfile.c_str());
                return 0;
            }
            fileRedirected = true;
        }

        bool isStdinPiped = false;
        if (pipeMgr.isValidPipe(currentClient->cmdID)) {
            stdinfd = pipeMgr.getReadFD(currentClient->cmdID);
            pipeMgr.closeWriteFD(currentClient->cmdID);
            isStdinPiped = true;
        }

        if (cmd.pipeToNextNCmd) {
            int targetCmdID = currentClient->cmdID + cmd.pipeToNextNCmd;
            if (pipeMgr.isValidPipe(targetCmdID)) {
                stdoutfd = pipeMgr.getWriteFD(targetCmdID);
            } else {
                pipeMgr.createPipe(currentClient->cmdID, cmd.pipeToNextNCmd);
                stdoutfd = pipeMgr.getWriteFD(targetCmdID);
            }
        } else {
            if (!fileRedirected) {
                stdoutfd = currentClient->fd;
            }
        }

        dumpCommands(cmds);
        pipeMgr.dumpPipes();

        std::string realPathEnv(getenv("PATH"));
        setenv("PATH", localenv["PATH"].c_str(), 1);
        if (!execProcess(stdinfd, stdoutfd, stderrfd, cmd.argv)) {
            printf("Error: Failed to execProcess\n");
            if (!isStdinPiped) {
                return 0;
            }
            if (i != 0) {
                --currentClient->cmdID;
            } else {
                pipeMgr.closeAndErasePipe(currentClient->cmdID);
            }
            return 0;
        }
        setenv("PATH", realPathEnv.c_str(), 1);

        if (cmd.outfile.size() > 0) {
            close(stdoutfd);
        }

        if (inPipe) {
            unlink(cmd.infile.c_str());
        }

        if (outPipe) {
            printf("outPipe: %d -> %d\n", clientID, cmd.pipeToClientID-1);
            ClientInfo *src = &clientList[clientID-1];
            ClientInfo *dst = &clientList[cmd.pipeToClientID-1];
            broadcast("*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
                src->nickname, clientID,
                cmd.cmdline,
                dst->nickname, cmd.pipeToClientID);
        }
    }
    return 0;
}

void UserShell::broadcastUserLoginMessage() {
    snprintf(buffer, MAX_BUFFER_SIZE, "*** User '%s' entered from %s. ***\n",
            currentClient->nickname, currentClient->address);
    printf("[user-shell] broadcastUserLoginMessage [%s]\n", buffer);
    broadcast(buffer);
    printf("[user-shell] broadcastUserLoginMessage [%s] done\n", buffer);
}

void UserShell::broadcastUserLogoutMessage() {
    snprintf(buffer, MAX_BUFFER_SIZE, "*** User '%s' left. ***\n", currentClient->nickname);
    broadcast(buffer);
}

