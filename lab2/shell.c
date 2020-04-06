#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
//和Bash的不同：ls>a>b 只会创建文件b
_Bool doing;
struct Redirect{ //用于重定向的文件描述符和文件名
	_Bool exist;
	char fd[128];
	char fn[128];
}r[128];
void handler(int signum){ //处理ctrl+c中断信号
	if(waitpid(-1,NULL,WNOHANG)) printf("\n# ");
	fflush(stdout);
}
void keep(int signum){
}
void execute(char* args[], struct Redirect r){//执行args命令的函数
	/* 没有输入命令 */
	if (!args[0])
	    return;
	/*处理文件重定向*/
	if(r.exist){
		if(strcmp(r.fd,">")==0){
			int fd = open(r.fn, O_WRONLY | O_CREAT,0644);
			dup2(fd,1);
			close(fd);
		}
		if(strcmp(r.fd,">>")==0){
			int fd = open(r.fn, O_WRONLY | O_CREAT | O_APPEND,0644);
			dup2(fd,1);
			close(fd);
		}
		if(strcmp(r.fd,"<")==0){
			int fd = open(r.fn, O_RDONLY,0644);
			dup2(fd,0);
			close(fd);
		}
	}
	/* 内建命令 */
	if (strcmp(args[0], "cd") == 0) {
	    if (args[1])
	        if(chdir(args[1])==-1) 
	        	fprintf(stderr,"bash: cd: %s: %s\n",args[1],strerror(errno));
	    return;
	}
	if (strcmp(args[0], "pwd") == 0) {
	    char wd[4096];
	    puts(getcwd(wd, 4096));
	    return;
	}
	if (strcmp(args[0], "export") == 0) {
	    for (int i = 1; args[i] != NULL; i++) {
	        /*处理每个变量*/
	        char *name = args[i];
	        char *value = args[i] + 1;
	        while (*value != '\0' && *value != '=')
	            value++;
	        *value = '\0';
	        value++;
	        setenv(name, value, 1);
	    }
	    return;
	}


	if (strcmp(args[0], "exit") == 0)
	    exit(0);

	/* 外部命令 */
	pid_t pid = fork();
	if(pid==-1) 
	    printf("%s: %s\n",args[0],strerror(errno));
	if (pid == 0) {
	    /* 子进程 */
	    execvp(args[0], args);
	    /* execvp失败 */
	    fprintf(stderr,"%s: command not found\n",args[0]);
	    exit(0);
	}
	/* 父进程 */
	int status;
	waitpid(pid,&status,0);
	if(!WIFEXITED(status)) printf("\n");
	return;
}

int main() {
    /* 输入的命令行 */
    char cmd[256];
    char null[256];
    char c;
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128][128];
    _Bool isfn;
    while (1) {
    	memset(cmd,'\0',sizeof(cmd));
    	args[0][0]=cmd;
    	int i=0,j=0;
    	r[0].exist=0;
        isfn=0;

        signal(SIGINT, handler);
        
        printf("# ");
        fflush(stdin);
        fflush(stdout);

        /*分割命令行*/
        while(1){
        	scanf("%[ ]",null);
      		if(!isfn) scanf("%[^ |<>\n]%*[ ]",args[i][j]);
        	else{
        		scanf("%[^ |<>\n]%*[ ]",r[i].fn);
        		isfn=0;
        	}

        	c=getchar();
        	if(c=='<'||c=='>'){
        		ungetc(c,stdin);
        		scanf("%[<>]",r[i].fd);
        		r[i].exist=1;
        		isfn=1;
        	}
        	else if(c=='|'){
        		args[i][j+1]=NULL;
        		args[i+1][0]=args[i][j]+strlen(args[i][j])+1;
        		i++,j=0;
        	}
        	else if(c=='\n'){
        		if(strlen(args[i][j])) args[i][j+1]=NULL;
        		else args[i][j]=NULL;
        		break;
        	}
        	else{
        		ungetc(c,stdin);
    			args[i][j+1]=args[i][j]+strlen(args[i][j])+1;
        		j++;
        	}
        }
		/*实现管道命令*/
		int t,fd[2];
		int fdin=dup(0),fdout=dup(1);
		for(t=0;t<i;t++){
			pipe(fd);
			if(fork()==0){
				dup2(fd[1],1);
				close(fd[0]);
				execute(args[t],r[t]);
				exit(0);
			}
			else{
				dup2(fd[0],0);
				close(fd[1]);
			}
		}
		execute(args[t],r[t]);
		dup2(fdin,0);
		dup2(fdout,1);
		fflush(stdout);
    }
}
