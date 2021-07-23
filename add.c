// gcc -g -rdynamic add.c -fPIC -shared -o libadd.so
// gcc -g -rdynamic add.c -fPIC -shared -o libadd.so -Wl,-Map,add.map;
int add(int a, int b)
{
    // 模拟段错误信号
    int *pTmp = 0;
    *pTmp = 1;	//对未分配内存空间的指针进行赋值，模拟访问非法内存段错误
    
    return a + b;
}