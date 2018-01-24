#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <netinet/in.h> // struct sockaddr_in

#include <vector>
#include <string>

const int MAX_BUFFER_SIZE = 10001;
const int MAX_PATH_LENGTH = 512;

extern void die(const char *format, ...);
char* formatToMessage(const char *format, ...);

size_t writeFD(int fd, const char *format, ...);
size_t readFD(int fd, char *message);

void createFIFO(const char *fifopath);
size_t readFIFO(const char *fifopath, char *message);
size_t writeFIFO(const char *fifopath, const char *message);

char *getIpPortBinding(struct sockaddr_in &cliAddr);

int getListenSocket(const int port, const int queueSize = 30);
int getAcceptSocket(int listenfd, struct sockaddr_in &cliAddr);

void explodeString(const std::string &buffer, std::vector<std::string>& tokens);
char** stringVector2CharArray(const std::vector<std::string> &stringVector, std::vector<char*>& argv);
std::string stringVectorToString(std::vector<std::string> &stringVector);
bool execProcess(int stdinfd, int stdoutfd, int stderrfd, const std::vector<std::string> &stringVector);

#endif
