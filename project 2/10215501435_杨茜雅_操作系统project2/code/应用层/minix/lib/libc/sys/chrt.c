#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include<sys/time.h>

int chrt(long deadline){
//struct timespec time={0,0};
struct timeval tv;
struct timezone tz;
message m;
memset(&m,0,sizeof(m));
//设置alarm
alarm((unsigned int)deadline);
//将当前时间记录下来算deadline
if(deadline>0){
gettimeofday(&tv,&tz);
deadline = tv.tv_sec + deadline;//进程结束时的时间：现在系统上的时间+设置的deadline=message中应该传输的时间
}//tv.tv_sec获取系统时间
//存deadline
m.m2_l1=deadline;
return(_syscall(PM_PROC_NR,PM_CHRT,&m));
}