
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>   // backtrace
#include <unistd.h>     // getpid
#include <dlfcn.h>      // dlopen, dlerror, dlsym, dlclose


// 注意：绝对不要出现出现编译警告 [-Wimplicit-function-declaration]

/*
gcc -Wall -o dynamic_load dynamic_load.c -L. -ladd -ldl -g;./dynamic_load
=================>>>catch signal 11<<<=====================
[00] ./dynamic_load(+0x135b) [0x55dcb09d935b]
[01] ./dynamic_load(+0x1556) [0x55dcb09d9556]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f348212d210]
[03] ./libadd.so(add+0x1a) [0x7f34822e7113]
[04] ./dynamic_load(+0x1609) [0x55dcb09d9609]
[05] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f348210e0b3]
[06] ./dynamic_load(+0x126e) [0x55dcb09d926e]
55dcb09d9000-55dcb09da000 r-xp 00001000 00:2c 21673573206951167          /mnt/d/codes/git/backtracedemo/dynamic_load
7f34820c2000-7f34820d4000 r-xp 00003000 08:10 12081                      /usr/lib/x86_64-linux-gnu/libgcc_s.so.1
7f348210c000-7f3482284000 r-xp 00025000 08:10 11971                      /usr/lib/x86_64-linux-gnu/libc-2.31.so
7f34822da000-7f34822dc000 r-xp 00001000 08:10 12010                      /usr/lib/x86_64-linux-gnu/libdl-2.31.so
7f34822e7000-7f34822e8000 r-xp 00001000 00:2c 19703248369976082          /mnt/d/codes/git/backtracedemo/libadd.so
7f34822ec000-7f348230f000 r-xp 00001000 08:10 11854                      /usr/lib/x86_64-linux-gnu/ld-2.31.so
7ffebf5c2000-7ffebf5c3000 r-xp 00000000 00:00 0                          [vdso]
Segmentation fault


0x7f34822e7113 - 0x7f34822e7000 + 0x1000 => 0x1110
addr2line -fe libadd.so 0x1113
add
/mnt/d/codes/git/backtracedemo/add.c:6
*/

// 定义保存栈帧的最大深度 根据项目复杂度定
#define STACK_FRAME_BUFFER_SIZE (int)128

void dump_backtrace() {
    // 定义栈帧缓冲区
    void *stack_frame_buffer[STACK_FRAME_BUFFER_SIZE];

    // 获取当前线程的栈帧
    int stack_frames_size = backtrace(stack_frame_buffer, STACK_FRAME_BUFFER_SIZE);

    // 将栈帧信息转化为字符串
    char **stack_frame_string_buffer = backtrace_symbols(stack_frame_buffer, stack_frames_size);
    if (NULL == stack_frame_string_buffer) {
        printf("failed to backtrace_symbols\n");
        return;
    }

    // 遍历打印栈帧信息
    for (int i = 0; i < stack_frames_size; i++) {
        printf("[%02d] %s\n", i, stack_frame_string_buffer[i]);
    }

    // 释放栈帧信息字符串缓冲区
    free(stack_frame_string_buffer);
}

void dump_maps() {
    char cmd[128];
    //snprintf(cmd, sizeof(cmd), "cat /proc/%d/maps", getpid());
    snprintf(cmd, sizeof(cmd), "cat /proc/%d/maps | grep r-xp", getpid());

    FILE *stream = popen(cmd, "r");
    if (NULL == stream) {
        printf("popen [%s] failed\n", cmd);
        return;
    }

    ssize_t read;
    char *line = NULL;
    size_t len = 0;

    while ((read = getline(&line, &len, stream)) != -1) {
        printf("%s", line);
    }

    pclose(stream);
}

void signal_handler(int signo) {
    printf("\n=================>>>catch signal %d<<<=====================\n", signo);
    dump_backtrace();
    dump_maps();

#ifdef ENABLE_LOG
    zlog_fini();
    system("sync");
#endif

    // 恢复信号默认处理(SIG_DFL)并重新发送信号(raise)
    signal(signo, SIG_DFL);
    raise(signo);
}

int main()
{
    // 捕获段错误信号SIGSEGV
    signal(SIGSEGV, signal_handler);


    // 加载动态库
    void *handle_ = dlopen("./libadd.so", RTLD_LAZY);
    if (!handle_)
    {
        printf("can't dlopen libadd.so\n");
        return -1;
    }

    typedef int (*add_func)(int a, int b);

    // 获取动态库函数符号
    add_func add = (add_func)dlsym(handle_, "add");
    if (!add)
    {
        printf("can't dlsym add\n");
        return -2;
    }

    // 运行add();
    int c = add(1,2);

    printf("add(1,2) = [%d]\n", c);
    // 释放
    dlclose(handle_);

    return 0;
}