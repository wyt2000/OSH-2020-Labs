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
#include <stddef.h>
#include <sys/sysmacros.h>
#include "syscall_names.h"
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

//wrap syscall pivot_root
static int pivot_root(const char *new_root, const char *put_old) 
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

void create_file(const char *filename){
    char message[100]="create file: ";
    if (access(filename, F_OK) != -1)
        rmdir(filename);
    if (mkdir(filename, 0777) != 0)
        error_exit(1, strcat(message, filename));
    return;
}

void write_file(const char *filename,const int content){
    char message[100]="write file: ";
    char buf[100]={0};
    sprintf(buf,"%d",content);
    int fdt = open(filename, O_WRONLY);
    if(write(fdt, buf, sizeof(buf)) == -1)
        error_exit(1, strcat(message, filename));
    close(fdt);
    return;
}

void remove_file(const char *filename){
    char message[100]="remove_file: ";
    char tmp[100]={0};
    char ctx[100]={0};
    int fd[2]={0};
    strcpy(tmp,filename);
    strcat(tmp,"restrict/cgroup.procs");
    fd[0]=open(tmp, O_RDONLY);
    if(fd[0]==-1) error_exit(1,"open");
    if(read(fd[0],ctx,sizeof(ctx))==-1)
        error_exit(1,"read");
    strcpy(tmp, filename);
    strcat(tmp, "cgroup.procs");
    fd[1]=open(tmp, O_WRONLY);
    if(fd[1]==-1) error_exit(1,"open");
    if(write(fd[1],ctx,sizeof(ctx))==-1)
        error_exit(1,"write");
    strcpy(tmp, filename);
    strcat(tmp, "restrict");
    if(rmdir(tmp)!=0)
        error_exit(1,"rmdir");
    return;
} 

int child(void *arg)
{
    //prepare pipe for sending message to parent
    struct Message *m = (struct Message *)arg;
    char **target = m->target;
    int *fd = m->fd;
    close(fd[0]);

    //make private
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
        error_exit(1, "mkdir oldroot");
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
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /dev");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /proc");
    if (mount("sysfs", "/sys", "sysfs",MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /sys");
    if (mount("tmpfs", "/run", "tmpfs",MS_NOSUID | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount /run");

    //mount cgroups controller
    //Because sys is readonly, mount a tmpfs first so that folder can be made
    if (mount("tmpfs", "/sys/fs/cgroup", "tmpfs", MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_NODIRATIME, "mode=755") != 0)
        error_exit(1, "mount /sys/fs/cgroup");
    if (chdir("/sys/fs/cgroup") != 0) 
        error_exit(1,"chdir");
    if (mkdir("memory",0) !=0)
        error_exit(1,"mkdir memory");
    if (mkdir("cpu,cpuacct", 0) != 0)
        error_exit(1, "mkdir cpu");
    if (mkdir("pids", 0) != 0)
        error_exit(1, "mkdir pids");
    if (mount("cgroup", "/sys/fs/cgroup/memory", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC , "memory") != 0)
        error_exit(1,"mount memory");
    if (mount("cgroup", "/sys/fs/cgroup/cpu,cpuacct", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC , "cpu,cpuacct") != 0)
        error_exit(1, "mount cpu");
    if (mount("cgroup", "/sys/fs/cgroup/pids", "cgroup", MS_NOSUID | MS_NODEV | MS_NOEXEC, "pids") != 0)
        error_exit(1, "mount pids");
    //change tmpfs to readonly
    if (mount("tmpfs", "/sys/fs/cgroup", "tmpfs", MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_NODIRATIME, "mode=755") != 0)
        error_exit(1, "mount /sys/fs/cgroup");

    if (chdir("/") == -1)
        error_exit(1, "chdir");

    //make some device files
    if (mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)) == -1)
        error_exit(1, "mknod null");
    if (mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5)) == -1)
        error_exit(1, "mknod zero");
    if (mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)) == -1)
        error_exit(1, "mknod urandom");
    if (mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0)) == -1)
        error_exit(1, "mknod tty");

    //Keep several capabilities
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_BOUNDING_SET,
                  CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN,
                  CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID,
                  CAP_KILL, CAP_SETGID, CAP_SETUID, CAP_NET_BIND_SERVICE,
                  CAP_SYS_CHROOT, CAP_SETFCAP,
                  -1);
    capng_apply(CAPNG_SELECT_BOTH);
    
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
    //restrict memory resource
    create_file("/sys/fs/cgroup/memory/restrict");
    write_file("/sys/fs/cgroup/memory/restrict/memory.limit_in_bytes", 67108864);
    write_file("/sys/fs/cgroup/memory/restrict/memory.kmem.limit_in_bytes", 67108864);
    write_file("/sys/fs/cgroup/memory/restrict/memory.swappiness", 0);
    write_file("/sys/fs/cgroup/memory/restrict/cgroup.procs", getpid());

    //restrict cpu shares
    create_file("/sys/fs/cgroup/cpu,cpuacct/restrict");
    write_file("/sys/fs/cgroup/cpu,cpuacct/restrict/cpu.shares", 256);
    write_file("/sys/fs/cgroup/cpu,cpuacct/restrict/cgroup.procs", getpid());

    //restrict max number of pids
    create_file("/sys/fs/cgroup/pids/restrict");
    write_file("/sys/fs/cgroup/pids/restrict/pids.max", 64);
    write_file("/sys/fs/cgroup/pids/restrict/cgroup.procs", getpid());

    if (chdir(argv[1]) == -1)
        error_exit(1, argv[1]);

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

    //filter some syscalls
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == NULL)
        error_exit(1, "seccomp_init");
    char name[50];
    for (int i = 0; names[i]!=-1 ; i++)
    {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, names[i], 0) < 0)
            error_exit(1, "seccomp_rule_add");
    }
    if (seccomp_load(ctx) < 0)
        error_exit(1, "seccomp_load");

    // Child goes for target program
    pid_t child_pid = clone(child, child_stack_start,
          CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD,
          (void*) &msg);
    
    if(child_pid == -1)
        error_exit(1, "clone");

    //when finish mounting, remove tmpdir.
    char tmpdir[30];
    close(fd[1]);
    while (1)
    {
        if (read(fd[0], tmpdir, sizeof(tmpdir)) != 0) break;
        //if child process return before finish mounting, exit immediately
        if (waitpid(child_pid, NULL, WNOHANG) != 0){
            seccomp_release(ctx);
            error_exit(1,"child");
        }
    }
    if(rmdir(tmpdir)!=0)
        error_exit(1,"rmdir tmpdir");

    // Parent waits for child
    int status, ecode = 0;
    wait(&status);

    //release filters
    seccomp_release(ctx);

    //remove restrict
    remove_file("/sys/fs/cgroup/memory/");
    remove_file("/sys/fs/cgroup/cpu,cpuacct/");
    remove_file("/sys/fs/cgroup/pids/");

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
