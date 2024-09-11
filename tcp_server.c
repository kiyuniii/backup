#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define TCP_PORT 5100

void handle_client(int csock) {
    char mesg[BUFSIZ];
    int n;

    while (1) {
        memset(mesg, 0, BUFSIZ);
        n = read(csock, mesg, BUFSIZ);
        if (n <= 0) {
            printf("클라이언트 연결 종료\n");
            break;
        }
        printf("받은 데이터: %s", mesg);

        if (write(csock, mesg, n) < 0) {
            perror("write()");
            break;
        }

        if (strncmp(mesg, "quit", 4) == 0) {
            printf("클라이언트가 종료를 요청했습니다.\n");
            break;
        }
    }
    close(csock);
    exit(0);
}

int main(int argc, char **argv) {
    int ssock, csock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clen;

    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    if (bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        return -1;
    }

    if (listen(ssock, 8) < 0) {
        perror("listen()");
        return -1;
    }

    signal(SIGCHLD, SIG_IGN);  // 좀비 프로세스 방지

    while (1) {
        clen = sizeof(cliaddr);
        csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);

        if (csock < 0) {
            perror("accept()");
            continue;
        }

        printf("클라이언트 연결: %s\n", inet_ntoa(cliaddr.sin_addr));

        pid_t pid = fork();
        if (pid == 0) {  // 자식 프로세스
            close(ssock);
            handle_client(csock);
        } else if (pid > 0) {  // 부모 프로세스
            close(csock);
        } else {
            perror("fork()");
        }
    }

    close(ssock);
    return 0;
}