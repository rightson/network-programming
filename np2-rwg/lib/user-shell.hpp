#ifndef _USER_SHELL_H_
#define _USER_SHELL_H_

#include "client-info.hpp"
#include "command-info.hpp"
#include "pipe-manager.hpp"

class UserShell {
    public:
        UserShell(pid_t mypid, int connfd, int clientID, ClientInfo *clientList);
        ~UserShell();
        bool processCommandline();

        void showLoginMessage();
        void showPrompt();
        void broadcastUserLoginMessage();
        void broadcastUserLogoutMessage();

    private:
        int processShellCommands(CommandlineInfo &cmdline);
        int processBinCommands(CommandlineInfo &cmdline);
        bool parseCommands(CommandlineInfo &cmdline);

        const pid_t mypid;
        const int connfd;
        const int clientID;
        ClientInfo *clientList;
        ClientInfo *currentClient;

        PipeManager pipeMgr;
        std::map<std::string, std::string> localenv;

        char buffer[MAX_BUFFER_SIZE];
};

#endif
