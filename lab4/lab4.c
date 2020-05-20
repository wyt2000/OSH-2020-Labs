#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <sched.h>  // For clone(2)
#include <signal.h> // For SIGCHLD constant
#include <unistd.h>
#include <sys/mman.h>  // For mmap(2)
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sys/mount.h> // For mount(2)
#include <sys/syscall.h> // For syscall(2)
#define STACK_SIZE (1024 * 1024)

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
    char **target = (char **)arg;
    if (chroot(".") == -1)
        error_exit(1, "chroot");
    if (mount("udev", "/dev", "devtmpfs",MS_NOSUID | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount 2");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount 3");
    if (mount("sysfs", "/sys", "sysfs",MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount 4");
    if (mount("tmpfs", "/run", "tmpfs",MS_NOSUID | MS_NOEXEC | MS_RELATIME, NULL) != 0)
        error_exit(1, "mount 5");
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

    if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL)!=0)
        error_exit(1, "mount 1");

    // Child goes for target program
    clone(child, child_stack_start,
          CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD,
          argv);

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
