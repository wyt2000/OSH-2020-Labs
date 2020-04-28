#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <queue>
#define MAXLEN 1049000 
#define MAXUSER 35
using std::queue;

queue<char> send_queue[MAXUSER];

pthread_mutex_t send_mutex[MAXUSER];
pthread_cond_t cv[MAXUSER];
pthread_t recv_thread[MAXUSER];
pthread_t send_thread[MAXUSER];
pthread_t log_thread[MAXUSER];
bool ready[MAXUSER];

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
    char buf[MAXLEN];
    sprintf(buf,"user%d: ",pipe->uid);
    int pos=strlen(buf);
    while (1) {
        
        //wait for the ready signal to send
        pthread_mutex_lock(&send_mutex[pipe->uid]);
        while(!ready[pipe->uid]){
            pthread_cond_wait(&cv[pipe->uid],&send_mutex[pipe->uid]);
        }
        //send all the characters in the send queue to the other clients
        while(!send_queue[pipe->uid].empty()){
            buf[pos]=send_queue[pipe->uid].front();
            send_queue[pipe->uid].pop();
            pos++;
        }
        buf[pos]='\0';
        for(int i=0;i<MAXUSER;i++){
            if(pipe->fd[i]&&i!=pipe->uid){
                send(pipe->fd[i], buf, strlen(buf), 0);
            }
        }
        sprintf(buf,"user%d: ",pipe->uid);
        pos=strlen(buf);

        //unlock all the mutex for other threads to send 
        for(int i=0;i<MAXUSER;i++) if(pipe->fd[i]) pthread_mutex_unlock(&send_mutex[i]);
        ready[pipe->uid]=0;
    }
    return NULL;
}

void *handle_recv(void *data){
    struct Pipe *pipe = (struct Pipe *)data;
    char buf;
    char *p=&buf;
    pthread_create(&send_thread[pipe->uid], NULL, handle_send, (void *)pipe); 
    //get characters into the recv queue
    while(1){
        if(recv(pipe->fd[pipe->uid], p, 1, 0)<=0) break;
        send_queue[pipe->uid].push(*p);
         
        //when '\n' appears, lock all the mutexes except the mutex of this uid.
        //then send the signal to wake up the send thread.
        //if other recv finishes after that, it will block here.
        if(*p=='\n'){
            for(int i=0;i<MAXUSER;i++) if(pipe->fd[i]) pthread_mutex_lock(&send_mutex[i]);
            ready[pipe->uid]=1;
            pthread_cond_signal(&cv[pipe->uid]);
            pthread_mutex_unlock(&send_mutex[pipe->uid]);
        }

    }
    pthread_cancel(send_thread[pipe->uid]);
    return NULL;
}

//wait until recv thread returned
void *handle_logout(void *data){ 
    struct ID *id = (struct ID *)data;
    pthread_join(id->tid,NULL);
    id->fd[id->uid]=0;
    *id->puid=id->uid;
    printf("user%d disconnected!\n",id->uid);
    pthread_mutex_destroy(&send_mutex[id->uid]);
    pthread_cond_destroy(&cv[id->uid]);
    ready[id->uid]=0;
    return NULL;
}

int main(int argc, char **argv) {
    int socketfd,fd[MAXUSER]={0};
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
    if (listen(socketfd, MAXUSER)) {
        perror("listen");
        return 1;
    }

    struct Pipe pipe[MAXUSER];
    struct ID id[MAXUSER];
    for(int i=0;i<MAXUSER;i++){
        pipe[i].fd=fd;
        id[i].fd=fd;
    }
    while(1){
        int uid=-1;
        for(int i=0;i<MAXUSER;i++){
            if(!fd[i]){
                uid=i;
                break;
            } 
        }
        if(uid==-1){
            continue;
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
        send_mutex[uid]=PTHREAD_MUTEX_INITIALIZER;
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
