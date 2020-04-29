#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#define MAXLEN 1049000 


int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }

    //In order to test non-block send, I manually make the buffer smaller.
    int send_size=1;
    socklen_t optlen;
    optlen=sizeof(send_size);
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF,(char *)&send_size, optlen); 
    optlen=sizeof(send_size);
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF,&send_size, &optlen); 
    printf("send size is %d\n",send_size);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
        perror("accept");
        return 1;
    }
    fd_set clients;
    char buf[MAXLEN] = "Message:";
    char *p=buf+8;
    fcntl(fd1, F_SETFL, fcntl(fd1, F_GETFL, 0) | O_NONBLOCK);
    fcntl(fd2, F_SETFL, fcntl(fd2, F_GETFL, 0) | O_NONBLOCK);
    while (1) {
        FD_ZERO(&clients);
        FD_SET(fd1, &clients);
        FD_SET(fd2, &clients);
        if (select(fd2 + 1, &clients, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(fd1, &clients)) {
                while (1) {
                    if(recv(fd1, p, 1, 0)<=0){
                        printf("error!\n");
                        return 0;
                    }
                    if(*p=='\n'){
                        *(p+1)='\0';
                        break;
                    }
                    else p++;
                }
                send(fd2, buf, strlen(buf), 0);
                strcpy(buf,"Message:");
                p=buf+8;
            }
            if (FD_ISSET(fd2, &clients)) {
                while (1) {
                    if(recv(fd2, p, 1, 0)<=0){
                        printf("error!\n");
                        return 0;
                    }
                    if(*p=='\n'){
                        *(p+1)='\0';
                        break;
                    }
                    else p++;
                }
                send(fd1, buf, strlen(buf), 0);
                strcpy(buf,"Message:");
                p=buf+8;
            }
        } 
        else break;
    }
    return 0;
}