// TCP 서버 구현을 위한 필요한 헤더 파일들을 포함
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
// TCP 포트 번호와 최대 클라이언트 수 정의
#define TCP_PORT 5101
#define MAX_CLIENTS 10

// 전역 변수 선언
int ssock;  // 서버 소켓
int client_count = 0;  // 현재 연결된 클라이언트 수
pid_t client_pids[MAX_CLIENTS];  // 클라이언트 프로세스 ID 배열
int parent_to_child[MAX_CLIENTS][2];  // 부모에서 자식으로의 파이프
int child_to_parent[MAX_CLIENTS][2];  // 자식에서 부모로의 파이프

struct message {
    pid_t pid;
    char mesg[BUFSIZ];
};


/* 파이프 : 논블로킹 모드로 설정 */
// 파일 디스크립터를 논블로킹 모드로 설정하는 함수
void set_nonblocking(int fd) {
    // 현재 파일 디스크립터의 플래그를 가져옴
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl - F_GETFL"); // 플래그를 가져오는 데 실패하면 오류 메시지 출력
        return;
    }
    
    // 기존 플래그에 O_NONBLOCK 플래그를 추가
    flags |= O_NONBLOCK;
    
    // 새로운 플래그를 파일 디스크립터에 설정
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl - F_GETFL"); // 플래그를 가져오는 데 실패하면 오류 메시지 출력
    }
}

// 시그널 핸들러 함수 추가
void handle_sigusr1(int sig) {
    printf("자식 프로세스가 클라이언트와 연결에 성공했습니다.\n");
}

void handle_sigusr2(int sig) {
    printf("자식 프로세스가 클라이언트와 연결에 실패했습니다.\n");
}

// 클라이언트 처리 함수 [자식 프로세스 부분]
void handle_client(int csock, int client_index) {
    // 이 함수는 서버가 클라이언트와 성공적으로 연결을 수립한 후 호출됩니다.
    // csock: 클라이언트와 연결된 소켓 디스크립터
    // client_index: 클라이언트의 고유 인덱스

    struct message msg;
    int n;
    pid_t pid = getpid();  // 현재 프로세스의 PID 얻기

    // 클라이언트와의 연결 성공 시 부모 프로세스에게 SIGUSR1 시그널 전송
    kill(getppid(), SIGUSR1);

    while (1) {
        memset(msg.mesg, 0, BUFSIZ);  // 메시지 버퍼 초기화
        n = read(csock, msg.mesg, BUFSIZ);  // 클라이언트로부터 메시지 읽기
        // n의 값은 다음과 같은 의미를 가집니다:
        // 현재 코드에서는 클라이언트로부터 읽은 데이터를 부모 프로세스로 전달하지 않습니다.
        // 만약 부모 프로세스로 데이터를 전달하려면 파이프를 사용하여 추가적인 코드가 필요합니다.
        // 양수: 실제로 읽은 바이트 수 (성공)
        // 0: 파일의 끝에 도달 (EOF) 또는 연결이 정상적으로 닫힘
        // -1: 오류 발생 (errno 변수를 통해 구체적인 오류 확인 가능)
        if (n <= 0) {  // 연결 종료 확인
            // 클라이언트 연결 종료는 read() 함수의 반환값으로 확인됩니다.
            // SIGCHLD 시그널은 자식 프로세스가 종료될 때 발생하며, 
            // 클라이언트 연결 종료와는 직접적인 관련이 없습니다.
            printf("클라이언트 연결 종료 (PID: %d)\n", pid);
            break;
        }
        printf("받은 데이터 (PID: %d): %s", pid, msg.mesg);  // 받은 메시지 출력

        // 부모 프로세스에게 메시지 전달
        // 이 부분은 중복되는 것 같습니다. 클라이언트로부터 직접 읽은 메시지를
        // 부모 프로세스에게 다시 전달하는 것은 불필요해 보입니다.
        // 만약 부모 프로세스가 이 정보�� 필요로 한다면, 다른 방식으로 구현해야 할 것 같습니다.

        // 부모 프로세스에게 메시지 전달
        ssize_t bytes_written = write(child_to_parent[client_index][1], &msg, sizeof(msg));
        if (bytes_written == -1) {
            perror("부모 프로세스로 데이터 전송 실패");
        } else {
            printf("부모 프로세스로 %zd 바이트 전송됨\n", bytes_written);
        }


        // 클라이언트가 'quit' 메시지를 보냈는지 확인
        if (strncmp(msg.mesg, "quit", 4) == 0) {
            printf("클라이언트가 종료를 요청했습니다. (PID: %d)\n", pid);
            break;
        }
    }
    close(csock);  // 클라이언트 소켓 닫기
    exit(0);  // 클라이언트가 종료하면 해당 자식 프로세스도 종료 -> SIGCHLD 시그널 발생
}

// SIGINT 시그널 핸들러 (Ctrl+C)
void handle_sigint(int sig) {
    printf("\n서버(부모 프로세스)가 SIGINT 시그널을 받아 종료합니다...\n");
    close(ssock);  // 서버 소켓 닫기
    exit(0);  // 프로그램 종료
}

// SIGCHLD 시그널 핸들러 (자식 프로세스 종료)
void handle_sigchld(int sig) {
    pid_t pid;
    int status;
    // waitpid 함수를 사용하여 종료된 자식 프로세스의 PID를 얻습니다.
    // -1: 아무 자식 프로세스나 기다립니다.
    // &status: 자식 프로세스의 종료 상태를 저장합니다.
    // WNOHANG: 종료된 자식이 없으면 즉시 반환합니다.
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 종료된 자식 프로세스 찾기
        for (int i = 0; i < client_count; i++) {
            // client_pids 배열에 저장된 PID와 종료된 프로세스의 PID를 비교합니다.
            if (client_pids[i] == pid) {
                // 일치하는 PID를 찾으면, 해당 클라이언트의 연결을 종료합니다.
                close(parent_to_child[i][0]);
                close(parent_to_child[i][1]);
                close(child_to_parent[i][0]);
                close(child_to_parent[i][1]);
                printf("클라이언트 연결 종료 (PID: %d)\n", pid);

                /* 클라이언트 연결 종료 후 배열 재정렬 */
                for (int j = i; j < client_count - 1; j++) {
                    client_pids[j] = client_pids[j + 1];
                    parent_to_child[j][0] = parent_to_child[j + 1][0];
                    parent_to_child[j][1] = parent_to_child[j + 1][1];
                    child_to_parent[j][0] = child_to_parent[j + 1][0];
                    child_to_parent[j][1] = child_to_parent[j + 1][1];
                }
                client_count--;
                break;
            }
        }
    }
}   

// 메인 함수
int main(int argc, char **argv) {
    int csock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clen;

    // 파이프 논블로킹 설정 
    int client_index = 0;
    set_nonblocking(parent_to_child[client_index][1]);
    set_nonblocking(child_to_parent[client_index][0]);

    // 서버 소켓 생성
    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }
    // 서버 소켓 논블로킹 설정
    int flags = fcntl(ssock, F_GETFL, 0);
    fcntl(ssock, F_SETFL, flags | O_NONBLOCK);

    // 서버 주소 구조체 초기화
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    // 소켓에 주소 바인딩
    if (bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        return -1;
    }

    // 연결 대기열 생성
    if (listen(ssock, 8) < 0) {
        perror("listen()");
        return -1;
    }

    signal(SIGINT, handle_sigint);  // SIGINT 핸들러 등록
    signal(SIGCHLD, handle_sigchld);  // SIGCHLD 핸들러 등록    
    signal(SIGUSR1, handle_sigusr1); // SIGUSR1 핸들러 등록
    signal(SIGUSR2, handle_sigusr2); // SIGUSR2 핸들러 등록

    printf("서버가 시작되었습니다. 포트 %d에서 대기 중...\n", TCP_PORT);
    printf("서버를 종료하려면 Ctrl+C를 누르세요.\n");

    // 클라이언트 연결 수락 루프
    while (1) {
        // 자식 프로세스로부터 데이터 읽기
    for (int i = 0; i < client_count; i++) {
        struct message msg;
        ssize_t bytes_read;
        
        bytes_read = read(child_to_parent[i][0], &msg, sizeof(struct message));
        if (bytes_read > 0) {
            printf("자식 프로세스 %d (PID: %d)로부터 받은 데이터: %s", i, msg.pid, msg.mesg);
        } else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 데이터가 없음, 계속 진행
        } else if (bytes_read == -1) {
            perror("자식으로부터 읽기 오류");
        }
    }
        clen = sizeof(cliaddr);
        csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);  //양수(3이상): 연결성공
        /* csock : 서버측의 소켓, 클라이언트와의 통신을 위한 소켓 */
        /* cliaddr : 클라이언트의 주소 정보를 저장하는 구조체 (ip주소, 포트번호) */
        /* clen : 클라이언트 주소 구조체의 크기 */  
        if (csock < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                continue;
            } else {
                // 클라이언트의 연결 요청을 받지 못한 경우 */
                perror("accept()"); // accept() 함수가 실패하면 에러 메시지를 출력
                continue; // 다시 반복문의 처음으로 돌아가 새로운 클라이언트의 연결을 기다림    
            }
        }

        // 최대 클라이언트 수 체크
        if (client_count >= MAX_CLIENTS) {
            printf("최대 클라이언트 수 초과. 연결 거부.\n");
            close(csock);
            continue;
        }

        // 파이프 생성
        if (pipe(parent_to_child[client_count]) < 0 || pipe(child_to_parent[client_count]) < 0) {
            perror("pipe()");
            continue;
        }
        // 파이프 논블로킹 설정
        set_nonblocking(parent_to_child[client_count][0]);
        set_nonblocking(parent_to_child[client_count][1]);
        set_nonblocking(child_to_parent[client_count][0]);
        set_nonblocking(child_to_parent[client_count][1]);
        // 네, 맞습니다. 부모와 자식 프로세스 간의 파이프에서 읽기와 쓰기 모두를 논블로킹으로 설정해야
        // 비동기식 데이터 처리가 가능합니다. 이렇게 하면 부모와 자식 프로세스가 서로 기다리지 않고
        // 독립적으로 작업을 수행할 수 있습니다.

        /* [자식 프로세스 부분] */
        // 프로세스 포크
        pid_t pid = fork();
        if (pid == 0) {  // 자식 프로세스
            close(ssock);  // 자식 프로세스는 서버 소켓을 사용하지 않으므로 닫습니다.
            close(parent_to_child[client_count][1]);  // 자식은 부모->자식 파이프의 쓰기 끝을 닫습니다. 읽기만 할 것이기 때문입니다.
            // 네, 맞습니다. [1]은 쓰기 끝이고 [0]은 읽기 끝입니다.
            close(child_to_parent[client_count][0]);  // 자식은 자식->부모 파이프의 읽기 끝을 닫습니다. 쓰기만 할 것이기 때문입니다.
            printf("새로운 클라이언트 연결: %s (PID: %d)\n", inet_ntoa(cliaddr.sin_addr), getpid());
            // cliaddr은 accept() 함수에서 설정된 클라이언트의 주소 정보입니다.
            // accept() 함수는 연결된 클라이언트의 정보를 cliaddr에 저장합니다.
            // 따라서 이 시점에서 cliaddr은 현재 연결된 클라이언트의 정보를 담고 있습니다.
            //----------------------------------------------------------------------
            // inet_ntoa(cliaddr.sin_addr)는 클라이언트의 IP 주소를 문자열로 변환합니다.
            // cliaddr은 클라이언트의 주소 정보를 담고 있는 구조체입니다.
            // sin_addr는 이 구조체 내의 IP 주소 필드입니다.
            // inet_ntoa 함수는 이 IP 주소를 점으로 구분된 십진수 표기법(예: "192.168.0.1")으로 변환합니다.
            handle_client(csock, client_count); // [자식 프로세스 부분]
        } else if (pid > 0) {  // 부모 프로세스
            close(csock);  // 부모 프로세스는 클라이언트 소켓을 사용하지 않으므로 닫습니다.
            close(parent_to_child[client_count][0]);  // 부모는 부모->자식 파이프의 읽기 끝을 닫습니다. 쓰기만 할 것이기 때문입니다.
            close(child_to_parent[client_count][1]);  // 부모는 자식->부모 파이프의 쓰기 끝을 닫습니다. 읽기만 할 것이기 때문입니다.
            client_pids[client_count] = pid;
            client_count++;
        } else {
            perror("fork()");  // fork() 실패 시 에러 메시지를 출력합니다.
        }
    }

    close(ssock);  // 서버 소켓 닫기
    return 0;
}