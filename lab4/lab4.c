#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <sched.h>  // For clone(2)
#include <signal.h> // For SIGCHLD constant
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>  // For mmap(2)
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sys/mount.h> // For mount(2)
#include <sys/syscall.h> // For syscall(2)
#include <cap-ng.h>
#include <seccomp.h>
#include "syscall_names.h"
#define STACK_SIZE (1024 * 1024)
#define NAME_NUM 326

struct Message{
    char **target;
    int *fd;
}msg;

scmp_filter_ctx ctx;

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

void write_file(const char *filename,const int content){
    char buf[100]={0};
    sprintf(buf,"%d",content);
    int fdt = open(filename, O_WRONLY);
    if(write(fdt, buf, sizeof(buf)) == -1)
        error_exit(1, filename);
    close(fdt);
    return;
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
    if (mkdir(oldrootdir, 0) != 0)
        error_exit(1, "mkdir");
    if (pivot_root(tmpdir, oldrootdir) != 0)
        error_exit(1, "pivot_root");
    
    //umount oldroot
    if (umount2("/oldroot", MNT_DETACH) !=0)
        error_exit(1, "umount2");
    if (rmdir("/oldroot") != 0)
        error_exit(1, "rmdir oldroot");

    //imform father process to remove file
    write(fd[1], tmpdir, sizeof(tmpdir));

    //mount file systems
    if (mount("udev", "/dev", "devtmpfs", MS_NOSUID | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /dev");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /proc");
    if (mount("sysfs", "/sys", "sysfs",MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /sys");
    if (mount("tmpfs", "/run", "tmpfs",MS_NOSUID | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /run");

    //mount cgroups controller
    if (mount("tmpfs", "/sys/fs/cgroup", "tmpfs", MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_NODIRATIME, "mode=755") != 0)
        error_exit(1, "mount /sys/fs/cgroup");
    if (chdir("/sys/fs/cgroup") != 0) 
        error_exit(1,"chdir");
    if (mkdir("memory",0) !=0)
        error_exit(1,"mkdir memory");
    if (mkdir("cpu", 0) != 0)
        error_exit(1, "mkdir cpu");
    if (mkdir("pids", 0) != 0)
        error_exit(1, "mkdir pids");
    if (mount("cgroup", "/sys/fs/cgroup/memory", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC , "memory") != 0)
        error_exit(1,"mount memory");
    if (mount("cgroup", "/sys/fs/cgroup/cpu", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC , "cpu,cpuacct") != 0)
        error_exit(1, "mount cpu");
    if (mount("cgroup", "/sys/fs/cgroup/pids", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC, "pids") != 0)
        error_exit(1, "mount pids");



    //Keep several capabilities
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_BOUNDING_SET,
                  CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN,
                  CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID,
                  CAP_KILL, CAP_SETGID, CAP_SETUID, CAP_NET_BIND_SERVICE,
                  CAP_SYS_CHROOT, CAP_SETFCAP
                  -1);
    capng_apply(CAPNG_SELECT_BOTH);

    //filter some syscalls
    ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == NULL)
        error_exit(1, "seccomp_init");
    char name[50];
    for (int i = 0; i < NAME_NUM; i++)
    {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, names[i], 0) < 0)
            error_exit(1, "seccomp_rule_add");
    }
    if (seccomp_load(ctx) < 0)
        error_exit(1, "seccomp_load");
    
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
    
    //restrict system resource
    if (chdir("/sys/fs/cgroup/memory") != 0)
        error_exit(1, "chdir");
    if (access("restrict", F_OK) != -1)
        rmdir("restrict");
    if (mkdir("restrict", 0777) != 0)
        error_exit(1, "mkdir restrict");
    if (chdir("/sys/fs/cgroup/memory/restrict") != 0)
        error_exit(1, "chdir");
    write_file("memory.limit_in_bytes", 67108864);
    write_file("memory.swappiness", 0);

    write_file("cgroup.procs", getpid());
    if (chdir("/") != 0)
        error_exit(1, "chdir");

    //set memory stack for child
    void *child_stack = mmap(NULL, STACK_SIZE,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                             -1, 0);
    void *child_stack_start = child_stack + STACK_SIZE;
    
    //set pipe to send and receive message
    int fd[2]={0};
    if(pipe(fd)==-1)
        error_exit(1,"pipe");
    msg.target=argv;
    msg.fd=fd;

    // Child goes for target program
    clone(child, child_stack_start,
          CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD,
          (void*) &msg);
    
    //when finish mounting, remove tmpdir
    char tmpdir[30];
    close(fd[1]);
    while (1)
    {
        if(read(fd[0], tmpdir, sizeof(tmpdir))!=0) 
        break;
    }
    if(rmdir(tmpdir)!=0)
        error_exit(1,"rmdir tmpdir");

    // Parent waits for child
    int status, ecode = 0;
    wait(&status);

    //release filters
    seccomp_release(ctx);

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
