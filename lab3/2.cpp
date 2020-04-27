#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <queue>
using std::queue;

queue<char> send_queue[32];

pthread_mutex_t mutex[32];
pthread_cond_t cv[32];
pthread_t recv_thread[32];
pthread_t send_thread[32];
pthread_t log_thread[32];
char buf;

struct Pipe {
    int uid;
    int *fd;
};
struct ID{
    pthread_t tid;
    int uid;
    int *puid;
    int *fd;
};

void *handle_send(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char msg[10];
    sprintf(msg,"user%d: ",pipe->uid);
    char c;
    bool ismsg=1;
    printf("user%d: before send!\n",pipe->uid);
    while (1) {
        pthread_mutex_lock(&mutex[pipe->uid]);
        pthread_cond_wait(&cv[pipe->uid],&mutex[pipe->uid]);
        printf("user%d: start send!\n",pipe->uid);
        while(!send_queue[pipe->uid].empty()){
            c=send_queue[pipe->uid].front();
            send_queue[pipe->uid].pop();
            printf("user%d: send %c\n",pipe->uid,c);
            for(int i=0;i<32;i++){
                if(pipe->fd[i]&&i!=pipe->uid){
                    if(ismsg) send(pipe->fd[i], msg, strlen(msg), 0);
                    send(pipe->fd[i], &c, 1, 0);
                }
            }
            if(ismsg) ismsg=0; 
            if(c=='\n') ismsg=1;
        }
        pthread_mutex_unlock(&mutex[pipe->uid]);
    }
    return NULL;
}

void *handle_recv(void *data){
    struct Pipe *pipe = (struct Pipe *)data;
    char *p=&buf;
    pthread_create(&send_thread[pipe->uid], NULL, handle_send, (void *)pipe); 
    sleep(1);
    while(1){
        if(recv(pipe->fd[pipe->uid], p, 1, 0)<=0) break;
        //lock all queues and add message
        for(int i=0;i<32;i++) if(pipe->fd[i]) pthread_mutex_lock(&mutex[i]);
        send_queue[pipe->uid].push(*p);
        //when a client finish input, send cond
        if(*p=='\n') pthread_cond_signal(&cv[pipe->uid]);
        for(int i=0;i<32;i++) if(pipe->fd[i]) pthread_mutex_unlock(&mutex[i]);
    }
    pthread_cancel(send_thread[pipe->uid]);
    return NULL;
}

void *handle_logout(void *data){ 
    //wait until recv thread returned
    struct ID *id = (struct ID *)data;
    pthread_join(id->tid,NULL);
    id->fd[id->uid]=0;
    *id->puid=id->uid;
    printf("user%d disconnected!\n",id->uid);
    pthread_mutex_destroy(&mutex[id->uid]);
    pthread_cond_destroy(&cv[id->uid]);
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

    struct Pipe pipe[32];
    struct ID id[32];
    for(int i=0;i<32;i++){
        pipe[i].fd=fd;
        id[i].fd=fd;
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
        
        //clear send queue
        while(!send_queue[uid].empty()) send_queue[uid].pop();

        //init mutex and cond
        mutex[uid]=PTHREAD_MUTEX_INITIALIZER;
        cv[uid]=PTHREAD_COND_INITIALIZER;

        //create recv thread
        pipe[uid].uid=uid;
        fd[uid]=fdt;
        pthread_create(&recv_thread[uid], NULL, handle_recv, (void *)&pipe[uid]);
        
        //create log thread
        id[uid].tid=recv_thread[uid];
        id[uid].uid=uid;
        id[uid].puid=&uid;
        pthread_create(&log_thread[uid], NULL, handle_logout, (void *)&id[uid]);
    }
    return 0;
}
