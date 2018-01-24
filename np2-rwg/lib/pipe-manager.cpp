#include <unistd.h>
#include <fcntl.h>

#include <sstream>
#include <string>
#include <vector>
#include <map>

#include "pipe-manager.hpp"

bool PipeManager::isValidPipe(std::size_t cmdID) {
    return (this->pipeMap.find(cmdID) != this->pipeMap.end());
}

std::size_t PipeManager::createPipe(std::size_t cmdID, int pipeToNextNCmd) {
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

std::size_t PipeManager::getReadFD(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        return this->pipeMap[cmdID].readFD;
    } else {
        return STDIN_FILENO;
    }
}

std::size_t PipeManager::getWriteFD(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        return this->pipeMap[cmdID].writeFD;
    } else {
        return STDOUT_FILENO;
    }
}

void PipeManager::closeReadFD(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        if (this->pipeMap[cmdID].readFD != -1) {
            close(this->pipeMap[cmdID].readFD);
            this->pipeMap[cmdID].readFD = -1;
        }
    }
}

void PipeManager::closeWriteFD(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        if (this->pipeMap[cmdID].writeFD != -1) {
            close(this->pipeMap[cmdID].writeFD);
            this->pipeMap[cmdID].writeFD = -1;
        }
    }
}

void PipeManager::erasePipe(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        this-> pipeMap.erase(cmdID);
    }
}

void PipeManager::closeAndErasePipe(std::size_t cmdID) {
    if (this->isValidPipe(cmdID)) {
        this->closeReadFD(cmdID);
        this->closeWriteFD(cmdID);
        this-> pipeMap.erase(cmdID);
    }
}

std::string PipeManager::dumpPipes() {
    std::ostringstream oss;
    for (std::map<int, Pipe>::iterator it=pipeMap.begin(); it!=pipeMap.end(); ++it) {
        printf("%d => [source: %d, offset: %d, rfd: %d, wfd: %d]\n",
                it->first,
                it->second.source,
                it->second.offset,
                it->second.readFD,
                it->second.writeFD);
        oss << "[" << it->second.source << "]<" << it->second.readFD << "," << it->second.writeFD << ">";
        oss << " -> ";
        oss << "[" << it->second.source + it->second.offset << "](" << it->second.offset << ")<"
                   << it->second.readFD << "," << it->second.writeFD << ">";
        oss << ", " << std::flush;
    }
    return oss.str();
}

