#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <sched.h>  // For clone(2)
#include <signal.h> // For SIGCHLD constant
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>  // For mmap(2)
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sys/mount.h> // For mount(2)
#include <sys/syscall.h> // For syscall(2)
#define STACK_SIZE (1024 * 1024)

struct Message{
    char **target;
    int *fd;
}msg;

const char *usage =
    "Usage: %s <directory> <command> [args...]\n"
    "\n"
    "  Run <directory> as a container and execute <command>.\n";

void error_exit(int code, const char *message)
{
    perror(message);
    _exit(code);
}

static int pivot_root(const char *new_root, const char *put_old) 
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

int child(void *arg)
{
    struct Message *m = (struct Message *)arg;
    char **target = m->target;
    int *fd = m->fd;
    close(fd[0]);
    if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0)
        error_exit(1, "mount /");

    //create tmpdir for bind
    char tmpdir[] = "./tmp/lab4-XXXXXX";
    strcpy(tmpdir, mkdtemp(tmpdir));
    if (tmpdir == NULL)
        error_exit(1, "mkdtemp");

    //bind mount the root to tmpdir
    if (mount(".", tmpdir, NULL, MS_BIND, NULL) != 0)
        error_exit(1, "mount tempdir");
    
    //change root file system
    char oldrootdir[] = "./tmp/lab4-XXXXXX/oldroot";
    sprintf(oldrootdir, "%s/oldroot", tmpdir);
    mkdir(oldrootdir, 0);

    if (pivot_root(tmpdir, oldrootdir) != 0)
        error_exit(1, "pivot_root");
    
    //umount oldroot
    if (umount2("/oldroot", MNT_DETACH) !=0)
        error_exit(1, "umount2");
    if (rmdir("/oldroot") != 0)
        error_exit(1, "rmdir");
    //imform father pross to remove file
    write(fd[1],tmpdir,sizeof(tmpdir));
    
    //mount file systems
    if (mount("udev", "/dev", "devtmpfs", MS_NOSUID | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /dev");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /proc");
    if (mount("sysfs", "/sys", "sysfs",MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /sys");
    if (mount("tmpfs", "/run", "tmpfs",MS_NOSUID | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /run");
    execvp(target[2], target + 2);
    error_exit(255, "exec");
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (chdir(argv[1]) == -1)
        error_exit(1, argv[1]);

    void *child_stack = mmap(NULL, STACK_SIZE,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                             -1, 0);
    // Assume stack grows downwards
    void *child_stack_start = child_stack + STACK_SIZE;
    int fd[2]={0};
    if(pipe(fd)==-1)
        error_exit(1,"pipe");

    msg.target=argv;
    msg.fd=fd;

    // Child goes for target program
    clone(child, child_stack_start,
          CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD,
          (void*) &msg);
    char tmpdir[30];

    close(fd[1]);
    while (1)
    {
        if(read(fd[0], tmpdir, sizeof(tmpdir))!=0) 
        break;
    }
    if(rmdir(tmpdir)!=0)
        error_exit(1,"rmdir");

    // Parent waits for child
    int status, ecode = 0;
    wait(&status);
    if (WIFEXITED(status))
    {
        printf("Exited with status %d\n", WEXITSTATUS(status));
        ecode = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        printf("Killed by signal %d\n", WTERMSIG(status));
        ecode = -WTERMSIG(status);
    }
    return ecode;
}
