#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string>
#include <iostream>
#define MAXLEN 1049000*35
#define MAXUSER 35
using std::string;

string send_queue[MAXUSER];
int fd[MAXUSER];
char c;
char msg[10];
char buffer[MAXLEN];
string tmp;

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int socketfd;

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    //set non-blocking socket
    fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);

    //In order to test non-blocking send, I manually make the buffer smaller.
    /*
    int send_size=1;
    socklen_t optlen;
    optlen=sizeof(send_size);
    setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF,(char *)&send_size, optlen); 
    //end
    */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(socketfd, 2)) {
        perror("listen");
        return 1;
    }

    fd_set recvfds,sendfds;
    fd_set recvfds_bk,sendfds_bk;
    FD_ZERO(&recvfds_bk);
    FD_ZERO(&sendfds_bk);
    FD_SET(socketfd,&recvfds_bk);
    int maxfd=socketfd;
    int select_val;
    int num=0;

    while (1) {
        //restore fd sets
        recvfds=recvfds_bk;
        sendfds=sendfds_bk;

        //select the recvable and sendable fds
        select_val=select(maxfd + 1, &recvfds, &sendfds, NULL, NULL);
        if(select_val==-1){
            perror("select");
            return 1;
        }

        //handle accept
        num=-1;
        for(int i=0;i<MAXUSER;i++){
            if(!fd[i]){
                num=i;
                break;
            }
        }
        if(num!=-1){
            fd[num]=accept(socketfd, NULL, NULL);
            if(fd[num]!=-1){
                printf("user%d connected!\n",num);
                fcntl(fd[num], F_SETFL, fcntl(fd[num], F_GETFL, 0) | O_NONBLOCK);
                if(fd[num]>maxfd) maxfd=fd[num];
                FD_SET(fd[num],&recvfds_bk);
                FD_SET(fd[num],&sendfds_bk);
            }
            else fd[num]=0;
        }

        //handle recv
        for(int i=0;i<MAXUSER;i++){
            if(!fd[i]||!FD_ISSET(fd[i], &recvfds)) continue;
            while (1) {
                if(recv(fd[i], &c, 1, 0)<=0){
                    fd[i]=0;
                    printf("user%d disconnect!\n",i);
                    send_queue[i].clear();
                    break;
                }
                tmp+=c;
                if(c=='\n'){
                    for(int j=0;j<MAXUSER;j++){
                        if(!fd[j]||j==i) continue;
                        if(send_queue[j].empty()){
                            sprintf(msg,"user%d: ",i);
                            send_queue[j]=string(msg);
                        }
                        send_queue[j]+=tmp;
                    }
                    tmp.clear();
                    break;
                }
            }
        }

        //handle send
        for(int i=0;i<MAXUSER;i++){
            if(!fd[i]||!FD_ISSET(fd[i], &sendfds)||send_queue[i].empty()) continue;
            send_queue[i].copy(buffer,send_queue[i].size()+1);
            buffer[send_queue[i].size()]='\0';
            ssize_t len=send(fd[i], buffer, strlen(buffer), 0);
            if(len>0&&len<send_queue[i].size()) send_queue[i]=send_queue[i].substr(len);
            else send_queue[i].clear();
        }
    }
    return 0;
}