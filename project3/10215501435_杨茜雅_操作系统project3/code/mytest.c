#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<time.h>
#include<string.h>

#define TIMES 500//读写次数
//每组读写要反复持续一段时间 过短的时间会造成误差较大
#define maxline (1024*1024)
#define filesize (300*1024*1024)//文件总大小300MB
#define buffsize (1024*1024*1024)
char rbuff[buffsize];

char *filepathDisk[7]={"/usr/file1.txt","/usr/file2.txt","/usr/file3.txt","/usr/file4.txt","/usr/file5.txt","/usr/file6.txt","/usr/file7.txt"};
char *filepathRam[7]={"/root/myram/file1.txt","/root/myram/file2.txt","/root/myram/file3.txt","/root/myram/file4.txt","/root/myram/file5.txt",
"/root/myram/file6.txt","/root/myram/file7.txt"};

char buff[maxline]="10215501435";
void write_file(int blocksize, bool isrand, char *filepath){
    int fd=open(filepath,O_RDWR|O_CREAT|O_SYNC,0755);
    if(fd<0){
        printf("Open file error!");
        //return;
    }
    int temp;//记录实际写入
    //多次重复写入计算时间
    for(int i=0;i<TIMES;i++){
        if((temp=write(fd,buff,blocksize))!=blocksize){
            printf("%d\n",temp);
            printf("Write file error!\n");
        }
        if(isrand)
                lseek(fd,rand() % filesize,SEEK_SET);//利用随机函数写到文件的任意一个位置
        //如果是随机
       }
       //如果读到末尾则从文件开头开始读。
    lseek(fd, 0, SEEK_SET);//重设文件指针
        //顺序读写时默认文件指针自由移动
}


void read_file(int blocksize, bool isrand, char *filepath){
    int fd=open(filepath,O_RDWR|O_CREAT|O_SYNC,0755);
    if(fd<0){
        printf("Open file error!");
        //return;
    }
    int temp;//记录实际写入
    //多次重复写入计算时间
    for(int i=0;i<TIMES;i++){
        if((temp=read(fd,rbuff,blocksize))!=blocksize){
            printf("%d\n",temp);
            printf("Read file error!\n");
        }
        if(isrand)
                lseek(fd,rand() % filesize,SEEK_SET);//利用随机函数写到文件的任意一个位置
        //如果是随机
       }
       //如果读到末尾则从文件开头开始读。
    lseek(fd, 0, SEEK_SET);//重设文件指针
        //顺序读写时默认文件指针自由移动
}

long get_time_left(struct timeval starttime,struct timeval endtime){
    long spendtime;
    spendtime=(long)(endtime.tv_sec-starttime.tv_sec)*1000+(endtime.tv_usec-starttime.tv_usec)/1000;
        //换算成秒
    //spendtime=spendtime/1000;
    return spendtime;
}

int main(){
    srand((unsigned)time(NULL));
    struct timeval starttime, endtime;
    double spendtime;
    for(int i=0;i<maxline;i+=16){
        strcat(buff,"abcdefghijklmnop");
    }
    int blocksize=64*4*4*4;
    for(int blocksize=64;blocksize<=1024*64;blocksize=blocksize*4){
        //for(int Concurrency=1;Concurrency<=15;Concurrency=Concurrency+1){
            int Concurrency=7;//7-15个达到饱和
            gettimeofday(&starttime, NULL);
            for(int i=0;i<Concurrency;i++){
                if(fork()==0){
                //随机写
                write_file(blocksize,true,filepathDisk[i]);
                //write_file(blocksize,true,filepathRam[i]);
                
                //顺序写
                //write_file(blocksize,false,filepathDisk[i]);
                //write_file(blocksize,false,filepathRam[i]);
                
                //随机读
                //read_file(blocksize,true,filepathDisk[i]);
                //read_file(blocksize,true,filepathRam[i]);
                
                //顺序读
                //read_file(blocksize,false,filepathDisk[i]);
                //read_file(blocksize,false,filepathRam[i]);
                exit(0);
                }
            }
            //等待所有子进程结束
            while(wait(NULL)!=-1);
            gettimeofday(&endtime, NULL);
            spendtime=get_time_left(starttime,endtime)/1000.0;
            double eachtime=spendtime/TIMES;
            double block=blocksize*Concurrency/1024.0/1024.0;
            printf("blocksize_KB=%.4fKB=%dB,speed=%fMB/s\n",(double)blocksize/1024.0,blocksize,block/eachtime);
            
            //printf("Concurrency=%d,speed=%fMB/s\n",Concurrency,block/eachtime);
        //}
    }
    return 0;
}