
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>   // backtrace

// 注意：绝对不要出现出现编译警告 [-Wimplicit-function-declaration]
/*
gcc -g -o lite lite.c;./lite
=================>>>catch signal 11<<<=====================
Dump stack start...
[00] ./lite(+0x129b) [0x55cf53b8129b]
[01] ./lite(+0x1392) [0x55cf53b81392]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7fb4a1616210]
[03] ./lite(+0x13e3) [0x55cf53b813e3]
[04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7fb4a15f70b3]
[05] ./lite(+0x11ae) [0x55cf53b811ae]
Dump stack end...
Segmentation fault

addr2line -e lite 0x13e3
/mnt/e/ubuntu/codes/git/BacktraceDemo/lite.c:73
*/

// 定义保存栈帧的最大深度 根据项目复杂度定
#define STACK_FRAME_BUFFER_SIZE (int)32

void dump(void)
{
    // 定义栈帧缓冲区
    void *stack_frame_buffer[STACK_FRAME_BUFFER_SIZE];

    // 获取当前线程的栈帧
    int stack_frames_size = backtrace(stack_frame_buffer, STACK_FRAME_BUFFER_SIZE);

    // 将栈帧信息转化为字符串
    char **stack_frame_string_buffer = backtrace_symbols(stack_frame_buffer, stack_frames_size);
    if (stack_frame_string_buffer == NULL)
    {
        perror("backtrace_symbols");
        exit(-1);
    }

    // 遍历打印栈帧信息
    for (int i = 0; i < stack_frames_size; i++)
    {
        printf("[%02d] %s\n", i, stack_frame_string_buffer[i]);
    }

    // 释放栈帧信息字符串缓冲区
    free(stack_frame_string_buffer);
}

void signal_handler(int signo)
{
    printf("\n=================>>>catch signal %d<<<=====================\n", signo);
    printf("Dump stack start...\n");
    dump();
    printf("Dump stack end...\n");

    // 恢复信号默认处理(SIG_DFL)并重新发送信号(raise) 
    signal(signo, SIG_DFL);
    raise(signo);
}

int main()
{
   // 捕获段错误信号SIGSEGV
    signal(SIGSEGV, signal_handler);
    
    // 模拟段错误信号
    int *pTmp = NULL;
    *pTmp = 1;	//对未分配内存空间的指针进行赋值，模拟访问非法内存段错误
    
    return 0;
}