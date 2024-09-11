#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5100

int main(int argc, char **argv) {
    int sock;
    struct sockaddr_in servaddr;
    char mesg[BUFSIZ];

    if (argc < 2) {
        printf("사용법: %s [IP 주소]\n", argv[0]);
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr.s_addr);
    servaddr.sin_port = htons(TCP_PORT);

    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect()");
        return -1;
    }

    printf("서버에 연결되었습니다. 메시지를 입력하세요 ('quit'으로 종료):\n");

while (1) {
    printf("메시지를 입력하세요 ('quit'으로 종료): ");
    fgets(mesg, BUFSIZ, stdin);

    if (send(sock, mesg, strlen(mesg), 0) <= 0) {
        perror("send()");
        break;
    }

    if (strncmp(mesg, "quit", 4) == 0) {
        printf("서버와의 연결을 종료합니다.\n");
        break;
    }
}

    close(sock);
    return 0;
}