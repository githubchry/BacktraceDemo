#define _GNU_SOURCE
#include <dlfcn.h>  //dlopen
#include <gnu/lib-names.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>   // backtrace
#include <unistd.h>     // getpid
#include <string.h>     // getpid

// 注意：绝对不要出现出现编译警告 [-Wimplicit-function-declaration]
// clear;rm main;gcc -o  main test.c -ldl -lpthread;./main


// https://stackoverflow.com/questions/55450932/how-ro-resolve-cpp-symbols-from-backtrace-symbols-in-the-offset-during-runtime

// 定义保存栈帧的最大深度 根据项目复杂度定
#define STACK_FRAME_BUFFER_SIZE (int)128

static void * STACK_FRAMES_BUFFER[STACK_FRAME_BUFFER_SIZE];
static void * OFFSET_FRAMES_BUFFER[128];
static char   EXECUTION_FILENAME[32] = "main";


/*-----------------------------------------------------------------------------------*/
/*
 * Parse a string which was returned from backtrace_symbols() to get the symbol name
 * and the offset. 
 */

void parseStrings(char * stackFrameString, char * symbolString, char * offsetString)
{
   char *        symbolStart = NULL;
   char *        offsetStart = NULL;
   char *        offsetEnd = NULL;
   unsigned char stringIterator = 0;

   //iterate over the string and search for special characters
   for(char * iteratorPointer = stackFrameString; *iteratorPointer; iteratorPointer++)
   {
      //The '(' char indicates the beginning of the symbol
      if(*iteratorPointer == '(')
      {
         symbolStart = iteratorPointer;
      }
      //The '+' char indicates the beginning of the offset
      else if(*iteratorPointer == '+')
      {
         offsetStart = iteratorPointer;
      }
      //The ')' char indicates the end of the offset
      else if(*iteratorPointer == ')')
      {
         offsetEnd = iteratorPointer;
      }

   }
   //Copy the symbol string into an array pointed by symbolString
   for(char * symbolPointer = symbolStart+1; symbolPointer != offsetStart; symbolPointer++)
   {
      symbolString[stringIterator] = *symbolPointer;
      ++stringIterator;
   }
   //Reset string iterator for the new array which will be filled
   stringIterator = 0;
   //Copy the offset string into an array pointed by offsetString
   for(char * offsetPointer = offsetStart+1; offsetPointer != offsetEnd; offsetPointer++)
   {
      offsetString[stringIterator] = *offsetPointer;
      ++stringIterator;
   }
}

/*-----------------------------------------------------------------------------------*/
/*
 * Use add2line on the obtained addresses to get a readable sting
 */
static void Addr2LinePrint(void const * const addr)
{
  char addr2lineCmd[512] = {0};

  //have addr2line map the address to the relent line in the code
  (void)sprintf(addr2lineCmd,"addr2line -C -i -f -p -s -a -e ./%s %p ", EXECUTION_FILENAME, addr);

  //This will print a nicely formatted string specifying the function and source line of the address
  (void)system(addr2lineCmd);
}
/*-----------------------------------------------------------------------------------*/
/*
 * Pass a string which was returned by a call to backtrace_symbols() to get the total offset
 * which might be decoded as (symbol + offset). This function will return the calculated offset
 * as void pointer, this pointer can be passed to addr2line in a following call.
 */
char *CalculateOffset(char * stackFrameString)
{
    
    // printf(" %s:%d %s\n", __FILE__, __LINE__, stackFrameString);


    void *     objectFile;
    void *     address;
    void *     offset = NULL;
    char       symbolString[75] = {'\0'};
    char       offsetString[25] = {'\0'};
    char *      dlErrorSting;
    int        checkDladdr = 0;
    Dl_info    symbolInformation;

    // parse the string obtained by backtrace_symbols() to get the symbol and offset
    // 从栈帧信息中解析出symbol和offset信息 ./main(dump+0x32) [0x565145fa9897] => dump 0x32
    parseStrings(stackFrameString, symbolString, offsetString);

    // printf("%s:%d %s, %s\n", __FILE__, __LINE__, symbolString, offsetString);

    //convert the offset from a string to a pointer
    // 将offset字符串转化为数字 "0x32" => 0x32
    if (sscanf(offsetString, "%p",&offset) != 1)
    {
        return NULL;
    }
    
    // printf("%s:%d 0x%x\n", __FILE__, __LINE__, offset);
    

    //check if a symbol string was created,yes, convert symbol string to offset
    // 如果symbol不存在， 说明没有打开-rdynamic选项
    if(symbolString[0] != '\0')
    {
        //open the object (if NULL the executable itself)
        objectFile = dlopen(NULL, RTLD_LAZY);
        //check for error
        if(!objectFile)
        {
            dlErrorSting = dlerror();
            (void)write(STDERR_FILENO, dlErrorSting, strlen(dlErrorSting));
        }
        //convert sting to a address
        address = dlsym(objectFile, symbolString);
        //check for error
        if(address == NULL)
        {
            dlErrorSting = dlerror();
            (void)write(STDERR_FILENO, dlErrorSting, strlen(dlErrorSting));
        }
        //extract the symbolic information pointed by address
        checkDladdr = dladdr(address, &symbolInformation);

        if(checkDladdr != 0)
        {
            //calculate total offset of the symbol
            offset = (symbolInformation.dli_saddr - symbolInformation.dli_fbase) + offset;
            //close the object
            dlclose(objectFile);
        }
        else
        {
            dlErrorSting = dlerror();
            (void)write(STDERR_FILENO, dlErrorSting, strlen(dlErrorSting));
        }
    }

    printf(" %s:%d %p\n", __FILE__, __LINE__, offset);

    return offset;
}



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
       
#if __x86_64__
        //calculate the offset on x86_64 and print the file and line number with addr2line
        // x86_64下直接获取栈帧信息打印出来的地址是无法直接被addr2line获取的，需要额外计算出偏移量

        printf("[%02d] %s\n", i, stack_frame_string_buffer[i]);
        CalculateOffset(stack_frame_string_buffer[i]);
#elif

        printf("[%02d] %s\n", i, stack_frame_string_buffer[i]);
#endif
    }

    // 释放栈帧信息字符串缓冲区
    free(stack_frame_string_buffer);
}

void signal_handler(int signo)
{
#if 0
    char buf[64] = {0};
    sprintf(buf, "cat /proc/%d/maps", getpid());
    system((const char*)buf);
#endif
    printf("\n=================>>>catch signal %d<<<=====================\n", signo);
    printf("Dump stack start...\n");
    dump();
    printf("Dump stack end...\n");

    // 取消捕获信号并由系统处理已经捕获到的信号(raise) 
    signal(signo, SIG_DFL);   // SIG_DFL、SIG_IGN 分别表示无返回值的函数指针，指针值分别是0和1
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