# Lab4 Report

## PB18111684 吴钰同

### 思考题

**1.用于限制进程能够进行的系统调用的 seccomp 模块实际使用的系统调用是哪个？用于控制进程能力的 capabilities 实际使用的系统调用是哪个？尝试说明为什么本文最上面认为「该系统调用非常复杂」。**

seccomp 模块实际使用的是 seccomp(2)，它的函数头如下：

``` c
int seccomp(unsigned int operation, unsigned int flags, void *args);
```

其中，当 operation 为 SECCOMP_SET_MODE_FILTER 时，根据 args 指针指向的 sock_fprog 结构体设置可以通过过滤的系统调用。该结构体在 filter.h 中定义如下：

```c
struct sock_filter         	/* Filter block */
{
        __u16       code;	/* Actual filter code */
        __u8        jt;     /* Jump true */
        __u8        jf;     /* Jump false */
        __u32       k;      /* Generic multiuse field */
};

struct sock_fprog        	/* Required for SO_ATTACH_FILTER. */
{
        unsigned short     len;        /* Number of filter blocks */
        struct sock_filter *filter;
};
```

 filter code 就是用于设置过滤器的汇编代码，显然手写这个代码非常复杂，而且跟处理器的架构有关；而 libseccomp 使用 gen_bpf.c 中写好的 _gen_bpf_build_bpf 函数根据输入的系统调用自动生成该汇编代码。

控制进程能力的 capabilities 实际使用的系统调用是 capset ，它的函数头如下：

```C
int capset(cap_user_header_t hdrp, const cap_user_data_t datap);
```

其中第二个参数对应的结构体就是要设置的能力：

```C
typedef struct __user_cap_data_struct {
	__u32 effective;
	__u32 permitted;
    __u32 inheritable;
} *cap_user_data_t;
```

能力由上述三个 32 位整数中的二进制位决定，显然对照文档一位位地数非常复杂，而 libcap-ng 已经把设置操作封装好了。

**2.当你用 cgroup 限制了容器中的 CPU 与内存等资源后，容器中的所有进程都不能够超额使用资源，但是诸如 htop 等「任务管理器」类的工具仍然会显示主机上的全部 CPU 和内存（尽管无法使用）。查找资料，说明原因，尝试提出一种解决方案，使任务管理器一类的程序能够正确显示被限制后的可用 CPU 和内存（不要求实现）。**

