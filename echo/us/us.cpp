#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif // WIN32
#include <thread>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
	printf("udp server %s\n",
#include "../version.txt"
	);
	printf("\n");
	printf("syntax: us <port> [-e] [-si <src ip>]\n");
	printf("  -e : echo\n");
	printf("sample: us 1234\n");
}

struct Param {
	bool echo{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
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

void recvThread(int sd) {
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		memset(&addr, 0, sizeof(addr));
		ssize_t res = ::recvfrom(sd, buf, BUFSIZE - 1, 0, (struct sockaddr*)&addr, &len);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recvfrom return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			res = ::sendto(sd, buf, res, 0, (struct sockaddr*)&addr, sizeof(addr));
			if (res == 0 || res == -1) {
				fprintf(stderr, "sendto return %zd", res);
				myerror(" ");
				break;
			}
		}
	}
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
	int sd = ::socket(AF_INET, SOCK_DGRAM, 0);
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
#endif // __linux__

	//
	// bind
	//
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(param.port);
		addr.sin_addr.s_addr = param.srcIp;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

		ssize_t res = ::bind(sd, (struct sockaddr*)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	std::thread t(recvThread, sd);
	t.join();
	::close(sd);
}
