#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define TCP_PORT 5100

int ssock;  // 전역 변수로 선언

void handle_client(int csock) {
    char mesg[BUFSIZ];
    int n;
    pid_t pid = getpid();  // 현재 프로세스의 PID 얻기

    while (1) {
        memset(mesg, 0, BUFSIZ);
        n = read(csock, mesg, BUFSIZ);
        if (n <= 0) {
            printf("클라이언트 연결 종료 (PID: %d)\n", pid);
            break;
        }
        printf("받은 데이터 (PID: %d): %s", pid, mesg);

        if (strncmp(mesg, "quit", 4) == 0) {
            printf("클라이언트가 종료를 요청했습니다. (PID: %d)\n", pid);
            break;
        }
    }
    close(csock);
    exit(0);
}

void handle_sigint(int sig) {
    printf("\n서버를 종료합니다...\n");
    close(ssock);
    exit(0);
}

int main(int argc, char **argv) {
    int csock;
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

    signal(SIGINT, handle_sigint);  // SIGINT 핸들러 등록
    signal(SIGCHLD, SIG_IGN);  // 좀비 프로세스 방지

    printf("서버가 시작되었습니다. 포트 %d에서 대기 중...\n", TCP_PORT);
    printf("서버를 종료하려면 Ctrl+C를 누르세요.\n");

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
            printf("새로운 클라이언트 연결: %s (PID: %d)\n", inet_ntoa(cliaddr.sin_addr), getpid());
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