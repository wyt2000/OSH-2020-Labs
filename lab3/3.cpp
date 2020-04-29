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
#define MAXLEN 1049000 
#define MAXUSER 35
using std::string;

string send_queue[MAXUSER];
int fd[MAXUSER];
char c;
char buffer[MAXLEN];
char msg[10];

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
    int send_size=1;
    socklen_t optlen;
    optlen=sizeof(send_size);
    setsockopt(socketfd, SOL_SOCKET, SO_SNDBUF,(char *)&send_size, optlen); 
    optlen=sizeof(send_size);
    getsockopt(socketfd, SOL_SOCKET, SO_SNDBUF,&send_size, &optlen); 
    //end

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
    int maxfd=socketfd;
    int select_val;
    int num=0;
    
    while (1) {
        //restore fd sets
        FD_ZERO(&recvfds);
        FD_ZERO(&sendfds);
        for(int i=0;i<MAXUSER;i++){
            if(fd[i]){
                FD_SET(fd[i],&recvfds);
                FD_SET(fd[i],&sendfds);
            }
        }

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
            }
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
                if(c=='\n'){
                    for(int j=0;j<MAXUSER;j++){
                        if(!fd[j]||j==i) continue;
                        send_queue[j]+=c;
                        send_queue[j]+='\0';
                        sprintf(msg,"user%d: ",i);
                        send_queue[i]=string(msg)+send_queue[i];
                    }
                    break;
                }
                else{
                    for(int j=0;j<MAXUSER;j++){
                        if(!fd[j]||j==i) continue;
                        send_queue[i]+=c;
                    }
                }
            }
        }

        //handle send
        for(int i=0;i<MAXUSER;i++){
            if(!fd[i]||!FD_ISSET(fd[i], &sendfds)) continue;
            send_queue[i].copy(buffer,send_queue[i].size()+1);
            buffer[send_queue[i].size()]='\0';
            long long len=send(fd[i], buffer, sizeof(buffer), 0);
            if(len>0&&errno!=EAGAIN) send_queue[i]=send_queue[i].substr(len);
            else send_queue[i].clear();
        }
    }
    return 0;
}