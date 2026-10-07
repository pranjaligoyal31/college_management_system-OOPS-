#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstring>

using namespace std;

class RedisClient {
private:
    string host;
    int port;
    bool connected;
    int hitCount;
    int missCount;

    SOCKET createSocket() {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            return INVALID_SOCKET;
        }

        // Set timeout of 2 seconds
#ifdef _WIN32
        DWORD timeout = 2000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif

        struct hostent* he = gethostbyname(host.c_str());
        if (!he) {
            closesocket(sock);
            return INVALID_SOCKET;
        }

        sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }

        return sock;
    }

    string sendCommand(const string& cmd) {
        SOCKET sock = createSocket();
        if (sock == INVALID_SOCKET) {
            connected = false;
            return "";
        }
        connected = true;

        send(sock, cmd.c_str(), (int)cmd.length(), 0);

        char buffer[16384];
        int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
        closesocket(sock);

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            return string(buffer);
        }
        return "";
    }

public:
    RedisClient(string h = "127.0.0.1", int p = 6379) : host(h), port(p), connected(false), hitCount(0), missCount(0) {
        // Read environment variables if available
        const char* envHost = getenv("REDIS_HOST");
        const char* envPort = getenv("REDIS_PORT");
        if (envHost) host = envHost;
        if (envPort) port = atoi(envPort);
    }

    bool ping() {
        string resp = sendCommand("*1\r\n$4\r\nPING\r\n");
        return resp.find("+PONG") != string::npos;
    }

    // Redis SET key value with optional TTL (seconds)
    bool set(const string& key, const string& value, int ttlSeconds = 0) {
        stringstream cmd;
        if (ttlSeconds > 0) {
            string ttlStr = to_string(ttlSeconds);
            cmd << "*5\r\n$3\r\nSET\r\n$" << key.length() << "\r\n" << key
                << "\r\n$" << value.length() << "\r\n" << value
                << "\r\n$2\r\nEX\r\n$" << ttlStr.length() << "\r\n" << ttlStr << "\r\n";
        } else {
            cmd << "*3\r\n$3\r\nSET\r\n$" << key.length() << "\r\n" << key
                << "\r\n$" << value.length() << "\r\n" << value << "\r\n";
        }
        string resp = sendCommand(cmd.str());
        return resp.find("+OK") != string::npos;
    }

    // Redis GET key
    string get(const string& key) {
        stringstream cmd;
        cmd << "*2\r\n$3\r\nGET\r\n$" << key.length() << "\r\n" << key << "\r\n";
        string resp = sendCommand(cmd.str());

        if (resp.empty() || resp.substr(0, 3) == "$-1") {
            missCount++;
            return "";
        }

        if (resp[0] == '$') {
            size_t firstCRLF = resp.find("\r\n");
            if (firstCRLF != string::npos) {
                int len = atoi(resp.substr(1, firstCRLF - 1).c_str());
                if (len > 0 && firstCRLF + 2 + len <= resp.length()) {
                    hitCount++;
                    return resp.substr(firstCRLF + 2, len);
                }
            }
        }
        missCount++;
        return "";
    }

    // Redis DEL key
    bool del(const string& key) {
        stringstream cmd;
        cmd << "*2\r\n$3\r\nDEL\r\n$" << key.length() << "\r\n" << key << "\r\n";
        string resp = sendCommand(cmd.str());
        return !resp.empty();
    }

    // Redis PUBLISH channel message (Pub/Sub pattern)
    int publish(const string& channel, const string& message) {
        stringstream cmd;
        cmd << "*3\r\n$7\r\nPUBLISH\r\n$" << channel.length() << "\r\n" << channel
            << "\r\n$" << message.length() << "\r\n" << message << "\r\n";
        string resp = sendCommand(cmd.str());
        if (!resp.empty() && resp[0] == ':') {
            return atoi(resp.substr(1).c_str());
        }
        return 0;
    }

    // Cache metrics
    int getHits() const { return hitCount; }
    int getMisses() const { return missCount; }
    string getHost() const { return host; }
    int getPort() const { return port; }
    bool isConnected() { return ping(); }
};

#endif // REDISCLIENT_H
