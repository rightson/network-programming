#ifndef _COMMAND_INFO_
#define _COMMAND_INFO_

#include <vector>
#include <string>

struct CommandInfo {
    std::vector<std::string> argv;
    std::string outfile;
    std::string infile;
    int pipeToNextNCmd;
    int pipeToClientID;
    int pipeFromClientID;
    const char *cmdline;
};

struct CommandlineInfo {
    std::string cmdline;
    std::vector<std::string> tokens;
    std::vector<CommandInfo> cmds;
};

void resetCommandInfo(struct CommandInfo &cmd);
bool parseCommands(int connfd, int clientID, CommandlineInfo &cmdline);

void dumpCommandInfo(CommandInfo &cmd);
void dumpCommands(std::vector<CommandInfo>& cmds);


#endif
