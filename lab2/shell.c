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

struct Redirect{ //用于重定向的文件描述符和文件名
	_Bool exist;
	int from;
	char way[64];
	char to[64];
}r[64][64];
struct Variables{ //shell变量
	char name[256];
	char value[256];
}v[256];
int cnt; //shell变量的计数器
char* getvar(char* name){ //获取环境变量或shell变量 
	char* t=getenv(name);
	if(t) return t;
	for(int i=0;i<cnt;i++){
		if(strcmp(v[i].name,name)==0) return v[i].value;
	}
	return NULL;
}
void setvar(char* name,char* value){ //设置环境变量或shell变量
	char* t=getenv(name);
	if(t){
		setenv(name,value,1);
		return;
	}
	for(int i=0;i<cnt;i++){
		if(strcmp(v[i].name,name)==0){
			strcpy(v[i].value,value);
			return;
		}
	}
	strcpy(v[cnt].name,name);
	strcpy(v[cnt].value,value);
	cnt++;
	return;
}
void unsetvar(char* name){ //删除环境变量或shell变量
	unsetenv(name);
	for(int i=0;i<cnt;i++){
		if(strcmp(v[i].name,name)==0){
			memset(v[i].name,'\0',sizeof(v[i].name));
			memset(v[i].value,'\0',sizeof(v[i].value));
			return;
		}
	}
	return;
}
void handler(int signum){ //处理ctrl+c中断信号
	if(waitpid(-1,NULL,WNOHANG)) printf("\n# ");
	fflush(stdout);
}
void execute(char* args[], struct Redirect r[]){//执行args命令的函数
	/* 没有输入命令 */
	if (!args[0])
	    return;
	/* 内建命令 */
	for(int i=0;args[i];i++){//更换变量
		if(args[i][0]=='~'){
			if(strlen(args[i])==1) args[i]=getenv("HOME");
			else if(!strcmp(args[i],"~root")) args[i]="/root";
			continue;
		}
		char temp[256],env[256];
		memset(temp,'\0',sizeof(temp));
		memset(env,'\0',sizeof(env));
		char *p=args[i],*last=args[i];
		_Bool isdollar=0;
		_Bool isbrace=0;
		while(1){
			if(*p=='$'||*p=='\0'){
				if(isdollar){
					strncpy(env,last,p-last);
					char* t=getvar(env);
					if(t) strcat(temp,t);	
				}
				else strncat(temp,last,p-last);
				last=p+1;
				isdollar=1;
				if(*p=='\0') break;
			}
			if(*p=='{'&&!isbrace){
				last=p+1;
				isbrace=1;
			}
			if(*p=='}'&&isbrace){
				isbrace=0;
				strncpy(env,last,p-last);
				char* t=getvar(env);
				if(t) strcat(temp,t);
				isdollar=0;
				last=p+1;
			}
			p++;
		}
		strcpy(args[i],temp);
		if(!strlen(args[i])){
			for(int j=i;args[j];j++){
				args[j]=args[j+1];
			}
		}
		if(!args[0]) return;
	}
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
	if (strcmp(args[0], "export") == 0||strcmp(args[0], "set") == 0||strcmp(args[0], "unset") == 0) {
	    if(strcmp(args[0], "set") == 0&&args[1]==NULL){
	    	for(int i=0;i<cnt;i++){
	    		if(!strlen(v[i].name)) continue;
	    		printf("%s=%s\n",v[i].name,v[i].value);
	    	}
	    }
	    for (int i = 1; args[i] != NULL; i++) {
	        /*处理每个变量*/
	        char *name = args[i];
	        char *value = args[i] + 1;
	        while (*value != '\0' && *value != '=')
	            value++;
	        *value = '\0';
	        value++;
	        if(strcmp(args[0], "unset") == 0) setenv(name, value, 1);
	    	if(strcmp(args[0], "set") == 0) setvar(name,value);
	    	if(strcmp(args[0], "unset") == 0) unsetvar(name);
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
		/*处理重定向*/
		for(int k=0;r[k].exist;k++){
			if(strcmp(r[k].way,">")==0){
				if(r[k].to[0]=='&'){
					dup2(atoi(r[k].to+1),r[k].from);
					continue;
				}
				int fd = open(r[k].to, O_WRONLY | O_CREAT | O_TRUNC,0644);
				dup2(fd,r[k].from);
				close(fd);
			}
			else if(strcmp(r[k].way,">>")==0){
				int fd = open(r[k].to, O_WRONLY | O_CREAT | O_APPEND,0644);
				dup2(fd,r[k].from);
				close(fd);
			}
			else if(strcmp(r[k].way,"<")==0){
				if(r[k].to[0]=='&'){
					dup2(atoi(r[k].to+1),r[k].from);
					continue;
				}
				int fd = open(r[k].to, O_RDONLY,0644);
				if(fd==-1){
					fprintf(stderr,"Bash: %s: %s\n",r[k].to,strerror(errno));
					return;
				}
				dup2(fd,r[k].from);
				close(fd);
			}
			else if(strcmp(r[k].way,"<<")==0){
				char temp[256];
				char *p=temp;
				r[k].to[strlen(r[k].to)+1]='\0';
				r[k].to[strlen(r[k].to)]='\n';
				while(1){
					printf("> ");
					fgets(p,256,stdin);
					if(strcmp(p,r[k].to)==0) break;
					p+=strlen(p)+1;
				}
				int fd = open(".in", O_WRONLY | O_CREAT | O_APPEND,0644);
				write(fd,temp,p-temp);
				close(fd);
				fd = open(".in", O_RDONLY ,0644);
				dup2(fd,0);
				close(fd);
			}
			else if(strcmp(r[k].way,"<<<")==0){
				r[k].to[strlen(r[k].to)+1]='\0';
				r[k].to[strlen(r[k].to)]='\n';
				int fd = open(".in", O_WRONLY | O_CREAT | O_APPEND,0644);
				write(fd,r[k].to,sizeof(r[k].to));
				close(fd);
				fd = open(".in", O_RDONLY ,0644);
				dup2(fd,0);
				close(fd);
			}
			else{
				printf("bash: syntax error\n");
				return;
			}	
		}
	    execvp(args[0], args);
	    /* execvp失败 */
	    fprintf(stderr,"%s: command not found\n",args[0]);
	    exit(0);
	}
	/* 父进程 */
	int status;
	waitpid(pid,&status,0);
	if(!WIFEXITED(status)) printf("\n");	//处理嵌套shell时ctrl+c出现多余空行
	for(int k=0;r[k].exist;k++)
		if(strcmp(r[k].way,"<<")==0||strcmp(r[k].way,"<<<")==0)
			if(fork()==0) 
				execlp("rm","rm",".in" ,NULL);  //删掉here document生成的文件
	return;
}

int main() {
    /* 输入的命令行 */
    char cmd[256];
    char null[256];
    char c;
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128][128];
    _Bool afterspace;

    while (1) {
    	memset(cmd,'\0',sizeof(cmd));
    	args[0][0]=cmd;
    	int i=0,j=0,k=0;
    	for(int x=0;x<64;x++){
    		for(int y=0;y<64;y++){
    			r[x][y].exist=0;
    		}
    	}

        signal(SIGINT, handler);
        
        printf("# ");
        fflush(stdin);
        fflush(stdout);

        /*分割命令行*/
        int cnt=0;
        while(1){        	
        	scanf("%[ ]",null);
      		scanf("%[^ |<>\n]",args[i][j]);
        	afterspace=scanf("%[ ]",null);
        	c=getchar();
        	if(c=='<'||c=='>'){
        		ungetc(c,stdin);
        		scanf("%[<>]",r[i][k].way);
        		if(afterspace||strspn(args[i][j],"0123456789")!=strlen(args[i][j])){
     	  			if(r[i][k].way[0]=='<') r[i][k].from=0;
					else r[i][k].from=1;
     	  			if(strlen(args[i][j])){
        				args[i][j+1]=args[i][j]+strlen(args[i][j])+1;
        				j++;
        			}
        		}
        		else if(strlen(args[i][j])){
        			r[i][k].from=atoi(args[i][j]);
        			args[i][j]=args[i][j]+strlen(args[i][j])+1;
        		}
        		else{
        			if(r[i][k].way[0]=='<') r[i][k].from=0;
					else r[i][k].from=1;
        		}
        		scanf("%[ ]",null);	
        		scanf("%[^ |<>\n]",r[i][k].to);
        		r[i][k].exist=1;
        		k++;
        	}
        	else if(c=='|'){
        		if(strlen(args[i][j])){
        			args[i][j+1]=NULL;
        			args[i+1][0]=args[i][j]+strlen(args[i][j])+1;
        		}
        		else{
        			args[i+1][0]=args[i][j];
        			args[i][j]=NULL;
        		}
        		i++,j=0,k=0;
        	}
        	else if(c=='\n'){
        		if(strlen(args[i][j])) args[i][j+1]=NULL;
        		else args[i][j]=NULL;
        		break;
        	}
        	else{
        		ungetc(c,stdin);
        		if(strlen(args[i][j])){
        			args[i][j+1]=args[i][j]+strlen(args[i][j])+1;
        			j++;
        		}
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