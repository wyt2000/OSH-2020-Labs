#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

struct Pipe {
    int uid;
    int *fd;
};
struct Con{
    pthread_t pid;
    int uid;
    int *puid;
    int *fd;
};

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char msg[10];
    sprintf(msg,"user%d: ",pipe->uid);
    char buf;
    char *p=&buf;
    _Bool ismsg=1;
    while (1) {
        if(recv(pipe->fd[pipe->uid], p, 1, 0)<=0) break;
        for(int i=0;i<31;i++){
            if(pipe->fd[i]&&i!=pipe->uid){
                if(ismsg) send(pipe->fd[i], msg, strlen(msg), 0);
                send(pipe->fd[i], p, 1, 0);
            }
        }
        if(ismsg) ismsg=0; 
        if(*p=='\n') ismsg=1;
    }
    return NULL;
}
void *logout(void *data){
    struct Con *con = (struct Con *)data;
    pthread_join(con->pid,NULL);
    con->fd[con->uid]=0;
    *con->puid=con->uid;
    printf("user%d disconnected!\n",con->uid);
    return NULL;
}

int main(int argc, char **argv) {
    int socketfd,fd[32]={0};
    int port = atoi(argv[1]);
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(socketfd, 32)) {
        perror("listen");
        return 1;
    }
    pthread_t thread[32];
    pthread_t signal[32];
    struct Pipe pipe[32];
    struct Con con[32];
    for(int i=0;i<32;i++){
        pipe[i].fd=fd;
        con[i].fd=fd;
    }
    while(1){
        int uid=-1;
        for(int i=0;i<32;i++){
            if(!fd[i]){
                uid=i;
                break;
            } 
        }
        if(uid==-1){
            printf("There are already 32 users!\n.");
            break;
        }
        int fdt = accept(socketfd, NULL, NULL);
        if (fdt == -1) {
            perror("accept");
            return 1;
        }
        printf("user%d connected!\n",uid);
        pipe[uid].uid=uid;
        fd[uid]=fdt;
        pthread_create(&thread[uid], NULL, handle_chat, (void *)&pipe[uid]);
        con[uid].pid=thread[uid];
        con[uid].uid=uid;
        con[uid].puid=&uid;
        pthread_create(&signal[uid], NULL, logout, (void *)&con[uid]);
    }
    return 0;
}
