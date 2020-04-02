#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128];
    int i;
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
		args[0] = p;
        for (i=0; *args[i]; i++)
            for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
                if(*args[i+1]==' '){
                /* 除去参数中间的空格 */  
                	while(*args[i+1] == ' ') { 
                    	*args[i+1] = '\0';
                    	args[i+1]++;
                	}
                break;
            }
        args[i] = NULL;
        
        /* 没有输入命令 */
        if (!args[0])
            continue;

        /* 内建命令 */
        if (strcmp(args[0], "cd") == 0) {
            if (args[1])
                if(chdir(args[1])==-1) 
                	printf("bash: cd: %s: %s\n",args[1],strerror(errno));
            continue;
        }
        if (strcmp(args[0], "pwd") == 0) {
            char wd[4096];
            puts(getcwd(wd, 4096));
            continue;
        }
        if (strcmp(args[0], "export") == 0) {
            for (i = 1; args[i] != NULL; i++) {
                /*处理每个变量*/
                char *name = args[i];
                char *value = args[i] + 1;
                while (*value != '\0' && *value != '=')
                    value++;
                *value = '\0';
                value++;
                setenv(name, value, 1);
            }
            continue;
        }
        if (strcmp(args[0], "exit") == 0)
            return 0;

        /* 外部命令 */
        pid_t pid = fork();
        if(pid==-1) 
            printf("%s: %s\n",args[0],strerror(errno));
        if (pid == 0) {
            /* 子进程 */
            execvp(args[0], args);
            /* execvp失败 */
            printf("%s: command not found\n",args[0]);
            return 255;
        }
        /* 父进程 */
        wait(NULL);
    }
}
