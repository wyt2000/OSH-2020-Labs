# Lab2 Report

## PB18111684 吴钰同

### strace输出中的系统调用

fstat：用于获取文件的属性信息，例如文件类型，访问权限和访问信息等。

```c
int fstat(int filedes, struct stat *restrict buf);
```

其中，第一个参数为文件描述符，第二个参数为储存文件属性信息的结构体的地址，成功返回0，出错返回-1。

mmap：将一个文件或者其它对象映射进内存。文件被映射到多个页上，如果文件的大小不是所有页的大小之和，最后一个页不被使用的空间将会清零。

```C
void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
```

其中，start映射区的开始地址，length为映射区的长度，prot为期望的内存保护标志，flags为指定映射对象的类型，fd为文件描述符，off_toffset为被映射对象内容的起点。

clone：用于创建一个新的进程，和fork类似，但它允许子进程与父进程共享内存区域，文件描述符表，信号处理表等。多用于实现共享内存的多线程控制。

```c
 int clone(int(fn)(void), void *child_stack, int flags, void *arg); 
```

其中，fn为指向子进程的函数指针，child_stack为给子进程分配的堆栈空间地址，flags是与父进程共享的资源标记，args是传给子进程的参数。

### 完成的选做：支持基于文件描述符的文件重定向、文件重定向组合

- 基于单个文件描述符的文件重定向，如：`cmd 10> out.txt `
- 基于多个文件描述符的组合重定向，如：`cmd 10>&20 30< in.txt` 

- Here Document：`cmd << EOF`
- `cmd <<< EOF`

### 完成的选做：shell变量与环境变量

- $运算符，如`cmd $abc`，其中`abc`既可以是环境变量也可以是自己设置的shell变量 。
- 设置shell变量，如`set abc=123`。
- 取消shell变量或环境变量的设置，如`unset abc` 。
- `echo ~`和`echo ~root`。

### 和Bash的不同

- **shell变量的设置前面必须加set，如`set abc=123`，如果环境变量中已经存在`abc`，则修改环境变量。**
- 没有把>&看作一个运算符，而是把&数字中的数字视作文件描述符，所以>和&分开也能合并文件描述符。
- 合并文件描述符的时候没有区分是输入文件还是输出文件。
- <<和<<<重定向创建的输入文件就在本目录而不是在/proc/self/fd。
- 部分报错可能不同，将一些非法输入统一处理成了command not found。

## 参考资料

https://linux.die.net/man/2/clone

https://blog.csdn.net/zhoulaowu/article/details/14094577?depth_1-utm_source=distribute.pc_relevant.none-task-blog-OPENSEARCH-2&utm_source=distribute.pc_relevant.none-task-blog-OPENSEARCH-2

http://man7.org/linux/man-pages/man2/mmap.2.html