

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



# 基地址定位

假设崩溃源在libadd.so中，backtrace信息如下：

```
gcc -Wall -o dynamic_link dynamic_link.c -L. -ladd -ldl -g;./dynamic_link
=================>>>catch signal 11<<<=====================
[00] ./dynamic_load(+0x135b) [0x55dcb09d935b]
[01] ./dynamic_load(+0x1556) [0x55dcb09d9556]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f348212d210]
[03] ./libadd.so(add+0x1a) [0x7f34822e7113]
[04] ./dynamic_load(+0x1609) [0x55dcb09d9609]
[05] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f348210e0b3]
[06] ./dynamic_load(+0x126e) [0x55dcb09d926e]
```

此时关注`[03] ./libadd.so(add+0x1a) [0x7f34822e7113]`：

- `add+0x1a`表示一个相对libadd.so文件里面add符号入口地址往后偏移0x1a
- `0x7f34822e7113`不是一个实际的物理地址（用户空间的程序无法直接访问物理地址），而是经过MMU（内存管理单元）映射过的内存地址。

两者是不能直接交给`addr2line`的。需要想办法获得相对于libadd.so文件里面的绝对地址。



## 方法一：cat /proc/pid/maps

通过查看进程的maps文件来了解进程的内存使用情况和动态链接库的加载情况，所以我们在打印栈信息前再把进程的maps文件也打印出来。

以下是崩溃时代码执行`system("cat /proc/xxx/maps | grep r-xp")`的打印，xxx表示当前进程的pid号。

```

55dcb09d9000-55dcb09da000 r-xp 00001000 00:2c 21673573206951167          /mnt/d/codes/git/backtracedemo/dynamic_load
7f34820c2000-7f34820d4000 r-xp 00003000 08:10 12081                      /usr/lib/x86_64-linux-gnu/libgcc_s.so.1
7f348210c000-7f3482284000 r-xp 00025000 08:10 11971                      /usr/lib/x86_64-linux-gnu/libc-2.31.so
7f34822da000-7f34822dc000 r-xp 00001000 08:10 12010                      /usr/lib/x86_64-linux-gnu/libdl-2.31.so
7f34822e7000-7f34822e8000 r-xp 00001000 00:2c 19703248369976082          /mnt/d/codes/git/backtracedemo/libadd.so
7f34822ec000-7f348230f000 r-xp 00001000 08:10 11854                      /usr/lib/x86_64-linux-gnu/ld-2.31.so
7ffebf5c2000-7ffebf5c3000 r-xp 00000000 00:00 0                          [vdso]
Segmentation fault
```

Maps信息第一项表示的为地址范围，第二项r-xp分别表示只读、可执行、私有的。由此可知这里存放的为对应文件的.text段即代码段



可以看到/mnt/d/codes/git/backtracedemo/libadd.so在内存在起始地址为7f34822e7000，根据backtrack描述出错的绝对地址为0x7f34822e7113，偏移地址为00001000，三者都是16进制。

绝对地址 - 起始地址 + 偏移地址 = 对应libadd.so所在地址

0x7f34822e7113 - 0x7f34822e7000 + 0x00001000 = 0x1113

```
echo "obase=16;$((0x7f34822e7113-0x7f34822e7000+0x1000))"|bc
1113
addr2line -fe libadd.so 1113
add
/mnt/d/codes/git/backtracedemo/add.c:6

echo "obase=16;$((0x7f895c6afc6c-0x7f895c683000+0x35000))"|bc
```





## 方法二： nm

既然`add+0x1a`表示一个相对libadd.so文件里面add符号所在地址往后偏移0x1a，直接通过`nm`命令即可拿到libadd.so中add符号地址：

```shell
nm libadd.so | grep add
00000000000010f9 T add
```

0x10f9 + 0x1a = 0x1113

与方法一殊道同归。

```
addr2line -fe libadd.so 1113
add
/mnt/d/codes/git/backtracedemo/add.c:6
```



# 策略

运行在现场的程序肯定是没有-g参数的，甚至可能加了-O2优化，这就会导致打印backtrace时候即使得到地址，但是没有符号信息。因为符号信息依赖-g参数。



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

##  -rdynamic

[rdynamic参数的作用](https://stackoverflow.com/questions/36692315)

选项`-rdynamic`用来通知链接器将所有符号添加到动态符号表中，而不仅仅是使用过的符号，目的有二：
1. 能够通过使用`dlopen`动态库，然后在里面查找函数符号并调用；
2. 允许从一个程序中获得回溯，即打印`backtrace`会显示符号名。

```shell
gcc -o main main.c -ldl -lpthread
./main
=================>>>catch signal 11<<<=====================
Dump stack start...
 [00] ./main(+0x1a3a) [0x559dd547da3a]
 [01] ./main(+0x1b3d) [0x559dd547db3d]
 [02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f3430ae7210]
 [03] ./main(+0x1b9f) [0x559dd547db9f]
 [04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f3430ac80b3]
 [05] ./main(+0x12ee) [0x559dd547d2ee]

 
```

此时关注`[03] ./main(+0x1b9f) [0x559dd547db9f]`中的`0x1b9f`，可以直接`addr2line -e main 0x1b9f`拿到详细信息。

如果加了`-rdynamic`参数，`backtrace`的信息会发生变化：

```shell
gcc -o main main.c -ldl -lpthread -rdynamic
./main
=================>>>catch signal 11<<<=====================
Dump stack start...
 [00] ./main(dump+0x32) [0x562e285b2a3a]
 [01] ./main(signal_handler+0x36) [0x562e285b2b3d]
 [02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f950f125210]
 [03] ./main(main+0x35) [0x562e285b2b9f]
 [04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f950f1060b3]
 [05] ./main(_start+0x2e) [0x562e285b22ee]
 
```

此时对应信息`[03] ./main(main+0x35) [0x562e285b2b9f]`中的`main+0x35`，是不能直接交给`addr2line`的。



## -On

网上有说法`-On`(n>0)参数会把栈帧信息优化掉，导致backtrace失效。本着实事求是的态度验证一下:

```shell
gcc -Wall -o lite lite.c;./lite
=================>>>catch signal 11<<<=====================
[00] ./lite(+0x12fb) [0x55c1cb3332fb]
[01] ./lite(+0x14f6) [0x55c1cb3334f6]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7f6a60cf6210]
[03] ./lite(+0x1545) [0x55c1cb333545]
[04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7f6a60cd70b3]
[05] ./lite(+0x120e) [0x55c1cb33320e]
55c1cb333000-55c1cb334000 r-xp 00001000 00:2c 37154696926037154          /mnt/d/codes/git/backtracedemo/lite
7f6a60c98000-7f6a60caa000 r-xp 00003000 08:10 12081                      /usr/lib/x86_64-linux-gnu/libgcc_s.so.1
7f6a60cd5000-7f6a60e4d000 r-xp 00025000 08:10 11971                      /usr/lib/x86_64-linux-gnu/libc-2.31.so
7f6a60eaf000-7f6a60ed2000 r-xp 00001000 08:10 11854                      /usr/lib/x86_64-linux-gnu/ld-2.31.so
7ffea9bb7000-7ffea9bb8000 r-xp 00000000 00:00 0                          [vdso]
Segmentation fault



gcc -Wall -o lite lite.c -O2;./lite
=================>>>catch signal 11<<<=====================
[00] ./lite(+0x1336) [0x559ae31fc336]
[01] ./lite(+0x14f3) [0x559ae31fc4f3]
[02] /lib/x86_64-linux-gnu/libc.so.6(+0x46210) [0x7fb6936e8210]
[03] ./lite(+0x11fb) [0x559ae31fc1fb]
[04] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf3) [0x7fb6936c90b3]
[05] ./lite(+0x123e) [0x559ae31fc23e]
559ae31fc000-559ae31fd000 r-xp 00001000 00:2c 37717646879458466          /mnt/d/codes/git/backtracedemo/lite
7fb69368a000-7fb69369c000 r-xp 00003000 08:10 12081                      /usr/lib/x86_64-linux-gnu/libgcc_s.so.1
7fb6936c7000-7fb69383f000 r-xp 00025000 08:10 11971                      /usr/lib/x86_64-linux-gnu/libc-2.31.so
7fb6938a1000-7fb6938c4000 r-xp 00001000 08:10 11854                      /usr/lib/x86_64-linux-gnu/ld-2.31.so
7ffdea7c5000-7ffdea7c6000 r-xp 00000000 00:00 0                          [vdso]
Segmentation fault

```

可以看出地址值的确发生了一些变化，再试一下加上-g参数看能否还原

```shell
gcc -Wall -o lite_debug lite.c -O2 -g 
addr2line -fe lite_debug  +0x11fb
main
/mnt/d/codes/git/backtracedemo/lite.c:103
```

不确定复杂的项目程序会被优化成什么样，就以上结果来说，能用。



# c++filt还原函数签名

c++函数在linux系统下编译之后会变成类似下面的样子：

```
_ZN6apsara5pangu15ScopedChunkInfoINS0_12RafChunkInfoEED1Ev
```

使用c++filt得到函数的原始名称

```
c++filt _ZN6apsara5pangu15ScopedChunkInfoINS0_12RafChunkInfoEED1Ev
Json::Value::operator[](char const*) const
```



