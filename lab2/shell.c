#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

void execute(char* args[]){//执行args命令的函数
	/* 没有输入命令 */
	if (!args[0])
	    return;

	/* 内建命令 */
	if (strcmp(args[0], "cd") == 0) {
	    if (args[1])
	        if(chdir(args[1])==-1) 
	        	printf("bash: cd: %s: %s\n",args[1],strerror(errno));
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
	    printf("%s: command not found\n",args[0]);
	    exit(0);
	}
	/* 父进程 */
	waitpid(pid,NULL,0);
	return;
}

int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128][128];
    int i,j;
    int cnt=0;
    while (1) {
        /* 提示符 */
        printf("# ");
        fflush(stdin);
        fgets(cmd, 256, stdin);
        /* 清理结尾的换行符 */
        for (i = 0; cmd[i] != '\n'; i++);
        cmd[i] = '\0';
    	/* 除去开头的空格 */ 
    	char* p;
        for (p = cmd; *p == ' '; p++);  
		args[0][0]=p;

		/*按照'|'拆解命令行*/
		for(i=0,j=0;*p!='\0';p++){
			if(*p==' '){
				while(*p==' '){
					*p='\0';
					p++;
				}
				if(*p=='|'){
					*p='\0';
					p++;
					while(*p==' '){
						*p='\0';
						p++;
					}
					args[i][j+1]=NULL;
					args[++i][0]=p;
					j=0;
				}
				else args[i][++j]=p;
			}
			else if(*p=='|'){
				*p='\0';
				p++;
				while(*p==' '){
					*p='\0';
					p++;
				}
				args[i][j+1]=NULL;
				args[++i][0]=p;  
				j=0;
			}
		}
		if(strlen(args[i][j])) args[i][j+1]=NULL;
		else args[i][j] = NULL;

		int t,fd[2];
		int fdt=dup(0);
		for(t=0;t<i;t++){
			pipe(fd);
			if(fork()==0){ //ls
				dup2(fd[1],1);
				close(fd[0]);
				execute(args[t]);
				exit(0);
			}
			else{
				dup2(fd[0],0);
				close(fd[1]);
			}
		}
		execute(args[t]);  //wc
		dup2(fdt,0);
		fflush(stdout);
    }
}
