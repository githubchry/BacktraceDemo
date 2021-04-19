

[在Linux中如何利用backtrace信息解决问题](https://blog.csdn.net/jxgz_leo/article/details/53458366)

# 相关函数原型

[参考地址](https://www.cnblogs.com/cmt/p/14553189.html)

```c
#include <execinfo.h>  

int backtrace(void **buffer, int size);
该函数获取当前线程的调用堆栈，获取的信息将会被存放在buffer中，它是一个指针数组，参数size用来指定buffer中可以保存多少个void*元素。函数的返回值是实际返回的void*元素个数。buffer中的void*元素实际是从堆栈中获取的返回地址。
    
char **backtrace_symbols(void *const *buffer, int size);
该函数将backtrace函数获取的信息转化为一个字符串数组，参数buffer是backtrace获取的堆栈指针，size是backtrace返回值。函数返回值是一个指向字符串数组的指针，它包含char*元素个数为size。每个字符串包含了一个相对于buffer中对应元素的可打印信息，包括函数名、函数偏移地址和实际返回地址。backtrace_symbols生成的字符串占用的内存是malloc出来的，但是是一次性malloc出来的，释放是只需要一次性释放返回的二级指针即可。

void backtrace_symbols_fd(void *const *buffer, int size, int fd);
该函数与backtrace_symbols函数功能相同，只是它不会malloc内存，而是将结果写入文件描述符为fd的文件中，每个函数对应一行。该函数可重入。
```

函数使用注意事项

- backtrace的实现依赖于栈指针（fp寄存器），在gcc编译过程中任何非零的优化等级（-On参数）或加入了栈指针优化参数-fomit-frame-pointer后多将不能正确得到程序栈信息；
- backtrace_symbols的实现需要符号名称的支持，在gcc编译过程中需要加入-rdynamic参数；
- 内联函数没有栈帧，它在编译过程中被展开在调用的位置；
- 尾调用优化（Tail-call Optimization）将复用当前函数栈，而不再生成新的函数栈，这将导致栈信息不能正确被获取。

```c

```

# 定位



## 方法一：**addr2line命令**

**必须加`-g`参数**

```
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ gcc -g -o lite lite.c
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ ./lite
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
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ addr2line -e lite 0x13e3
/mnt/e/ubuntu/codes/git/BacktraceDemo/lite.c:92
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ 
```





## 方法二： map文件

[gcc编译链接之Map文件分析](https://www.jianshu.com/p/a1573d401ea3)

没空研究



# 策略



分别用编译两个程序， 一个加-g参数， 一个不加-g参数

不加-g参数的程序发布给现场使用， 出问题时拿到打印， 然后使用addr2line指定加-g参数的程序获取调试信息

```
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ gcc -o lite lite.c
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ gcc -g -o lite_debug lite.c
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ 
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ 
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ ./lite

=================>>>catch signal 11<<<=====================
Dump stack start...
[00] ./lite(+0x129b) [0x55d53e34a29b]
[01] ./lite(+0x1392) [0x55d53e34a392]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7fda0d1c0210]
[03] ./lite(+0x13e3) [0x55d53e34a3e3]
[04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7fda0d1a10b3]
[05] ./lite(+0x11ae) [0x55d53e34a1ae]
Dump stack end...
Segmentation fault
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ addr2line -e lite 0x13e3
??:?
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ addr2line -e lite_debug 0x13e3
/mnt/e/ubuntu/codes/git/BacktraceDemo/lite.c:91
chry@DESKTOP-UKSV006:/mnt/e/ubuntu/codes/git/BacktraceDemo$ 

```



# 编译参数

[rdynamic参数的作用](https://stackoverflow.com/questions/36692315)

```
选项 -rdynamic 用来通知链接器将所有符号添加到动态符号表中，目的是能够通过使用 dlopen 来实现向后跟踪


gcc -o main main.c -ldl -lpthread

=================>>>catch signal 11<<<=====================
Dump stack start...
 [00] ./main(+0x1a3a) [0x559dd547da3a]
 [01] ./main(+0x1b3d) [0x559dd547db3d]
 [02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f3430ae7210]
 [03] ./main(+0x1b9f) [0x559dd547db9f]
 [04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f3430ac80b3]
 [05] ./main(+0x12ee) [0x559dd547d2ee]



gcc -o main main.c -ldl -lpthread -rdynamic

=================>>>catch signal 11<<<=====================
Dump stack start...
 [00] ./main(dump+0x32) [0x562e285b2a3a]
 [01] ./main(signal_handler+0x36) [0x562e285b2b3d]
 [02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f950f125210]
 [03] ./main(main+0x35) [0x562e285b2b9f]
 [04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f950f1060b3]
 [05] ./main(_start+0x2e) [0x562e285b22ee]
 
 
```

