#include <unistd.h>

#include "command-info.hpp"
#include "client-info.hpp"

void resetCommandInfo(struct CommandInfo &cmd) {
    cmd.argv.clear();
    cmd.outfile.clear();
    cmd.infile.clear();
    cmd.pipeToNextNCmd = 0;
    cmd.pipeToClientID = -1;
    cmd.pipeFromClientID = -1;
}

bool parseCommands(int connfd, int clientID, CommandlineInfo &cmdline) {
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
                cmd.pipeFromClientID = clientID;
                cmd.pipeToClientID = atoi(tokens[i].substr(1).c_str());
                char buffer[MAX_BUFFER_SIZE];
                getPeerNamedPipe(buffer, cmd.pipeFromClientID, cmd.pipeToClientID);
                cmd.outfile = buffer;
                cmds.push_back(cmd);
                return true;
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
            cmd.pipeToClientID = clientID;
            char buffer[MAX_BUFFER_SIZE];
            getPeerNamedPipe(buffer, cmd.pipeFromClientID, cmd.pipeToClientID);
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

void dumpCommandInfo(CommandInfo &cmd) {
    printf("{ argv=[%s]", stringVectorToString(cmd.argv).c_str());
    if (cmd.pipeToNextNCmd) printf(", pipeToNextNCmd=%d", cmd.pipeToNextNCmd);
    if (cmd.pipeToClientID != -1) printf(", pipeToClientID=%d", cmd.pipeToClientID);
    if (cmd.pipeFromClientID != -1) printf(", pipeFromClientID=%d", cmd.pipeFromClientID);
    if (cmd.outfile.size()) printf(", outfile=[%s]", cmd.outfile.c_str());
    if (cmd.infile.size()) printf(", infile=[%s]", cmd.infile.c_str());
    printf(" }");
}

void dumpCommands(std::vector<CommandInfo>& cmds) {
    printf("[cmds] dump cmds: [");
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        dumpCommandInfo(cmds[i]);
        if ((i+1) != cmds.size()) {
            printf(", ");
        }
    }
    printf("]\n");
}

