#ifndef __UTILS_H__
#define __UTILS_H__

#include <netinet/in.h> // struct sockaddr_in
#include <netdb.h>      // gethostbyname
#include <arpa/inet.h>  // inet_ntoa

#include <string>
#include <vector>
#include <map>
#include <tuple>

namespace utils {

const int MAX_BUFFER_SIZE = 10001;
typedef std::map<std::string, std::string> StringMap;

void die(const char *fmt, ...);
void parseQueryString(const std::string &qs, StringMap &queryStringMap);
void showStringMap(const StringMap &queryStringMap);
int getClientSocket(struct sockaddr_in &cliAddr, const char *hostname, int port, bool nonblocking = true);
int getListenSocket(const int port, const int queueSize = 30);
int getListenSocketWithAddr(const int port, struct sockaddr_in &servAddr, const int queueSize = 30);
int getAcceptSocket(int listenfd, struct sockaddr_in &cliAddr);
char *getIPAddress(struct sockaddr_in &sockaddr);
char *getIPAddress(struct in_addr &sin_addr);
char *getIPAddress(struct in_addr &sin_addr);
int getPort(int sockfd, struct sockaddr_in &servAddr);
std::tuple<struct in_addr*, uint16_t> getIPAndPort(int sockfd);

ssize_t writeFD(int fd, const char *format, ...);
ssize_t readFD(int fd, char *message);

char** stringVector2CharArray(const std::vector<std::string> &stringVector, std::vector<char*>& argv);
bool execProcess(int stdinfd, int stdoutfd, int stderrfd, const std::vector<std::string> &stringVector);
std::string replaceString(std::string subject, const std::string& search, const std::string& replace);
void explodeString(const std::string &buffer, std::vector<std::string>& tokens);
std::string expandPath(const std::string &str);
bool is_directory(const std::string &path);
bool is_regular_file(const std::string &path);

}; // end of namespace utils

#endif
