#ifndef _PIPE_MANAGER_
#define _PIPE_MANAGER_

#include <map>

struct Pipe {
    int source;
    int offset;
    int readFD;
    int writeFD;
};

class PipeManager {
    public:
        bool isValidPipe(std::size_t cmdID);
        std::size_t createPipe(std::size_t cmdID, int pipeToNextNCmd = 0);
        std::size_t getReadFD(std::size_t cmdID);
        std::size_t getWriteFD(std::size_t cmdID);
        void closeReadFD(std::size_t cmdID);
        void closeWriteFD(std::size_t cmdID);
        void erasePipe(std::size_t cmdID);
        void closeAndErasePipe(std::size_t cmdID);
        std::string dumpPipes();

    private:
        std::map<int, Pipe> pipeMap;
};

#endif
