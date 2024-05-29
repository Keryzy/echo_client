#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif


#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
    printf("tcp server %s\n",
#include "../version.txt"
   );
    printf("\n");
    printf("syntax: ts <port> [-e] [-si <src ip>]\n");
    printf("  -e : echo\n");
    printf("sample: ts 1234\n");
}

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};
    uint32_t srcIp{0};

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc;) {

            // echo
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                i++;
                continue;
            }

            // broadcast
            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                i++;
                continue;
            }

            if (strcmp(argv[i], "-si") == 0) {
                int res = inet_pton(AF_INET, argv[i + 1], &srcIp);
                switch (res) {
                    case 1: break;
                    case 0: fprintf(stderr, "not a valid network address\n"); return false;
                    case -1: myerror("inet_pton"); return false;
                }
                i += 2;
                continue;
            }

            if (i < argc) port = atoi(argv[i++]);
        }
        return port != 0;
    }
} param;

// client socket descripter를 저장하는 vector와 mutext 정의
std::vector<int> clients;
std::mutex clientsMutex;

// mutex를 통하여 동시 접근 제어
void broadcastMessage(const char* message, ssize_t length) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (int clientSd : clients) {
        ::send(clientSd, message, length, 0);
    }
}


void recvThread(int sd) {
    printf("connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %zd", res);
            myerror(" ");
            break;
        }
        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);
        if (param.echo) {
            res = ::send(sd, buf, res, 0);
            if (res == 0 || res == -1) {
                fprintf(stderr, "send return %zd", res);
                myerror(" ");
                break;
            }
        }
        if (param.broadcast) {
            /*
                buf = 클라이언트로부터 수신된 메세지
                res = 수신된 메세지 길이
            */
            broadcastMessage(buf, res);
        }
    }
    /*
        수신이 끝나면 해당 client socket descripter 제거

        lock_guard = mutex 자동으로 잠그고, 블록이 끝나면 자동으로 잠금 해제
        lock(clientMutex) = 클라이언트 목록에 대한 동시 접근 방지
    */

    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.erase(std::remove(clients.begin(), clients.end(), sd), clients.end());

    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

#ifdef WIN32
    WSAData wsaData;
    WSAStartup(0x0202, &wsaData);
#endif // WIN32

    //
    // socket
    //
    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }

#ifdef __linux__
    //
    // setsockopt
    //
    {
        int optval = 1;
        int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }
#endif // __linux

    //
    // bind
    //
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = param.srcIp;
        addr.sin_port = htons(param.port);

        ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1) {
            myerror("bind");
            return -1;
        }
    }

    //
    // listen
    //
    {
        int res = listen(sd, 5);
        if (res == -1) {
            myerror("listen");
            return -1;
        }
    }

    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
        if (newsd == -1) {
            myerror("accept");
            break;
        }

        std::lock_guard<std::mutex> lock(clientsMutex);
        // 클라이언트 목록에 소켓 추가
        clients.push_back(newsd);

        std::thread* t = new std::thread(recvThread, newsd);
        t->detach();
    }
    ::close(sd);
}
