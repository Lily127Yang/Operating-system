#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAXLINE 100 //最大命令数量
#define MAXARGS 100//最大输入命令字符数
#define M 100 //每条命令的最大长度

char his[M][M];//用二维数组保存shell中输入的命令
int his_cnt = 0; //历史命令计数
char *path = NULL;//初始化指向NULL

void doCommand(char *cmdline);
int parseline(const char *cmdline, char **argv);
int builtin_cmd(char **argv);
void pipe_line(char *process1[], char *process2[]);
void mytop();

int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];

    while (1)
    {
        path = getcwd(NULL, 0);//获取当前工作路径
        printf("10215501435_shell>%s# ", path);
        fflush(stdout);//打印shell提示符
        if (fgets(cmdline, MAXLINE, stdin) == NULL)
        {//输入命令到cmdline数组中
            continue;
        }
        for (int i = 0; i < M; i++)
        {//将命令储存到历史命令二维数组his中
            his[his_cnt][i] = cmdline[i];
        }
        his_cnt = his_cnt + 1;
        doCommand(cmdline);//解析命令
        fflush(stdout);
    }
    exit(0);
}

void doCommand(char *cmdline)
//内置命令、program命令、后台运行
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;
    char *file;
    int fd;
    int status;
    int case_command = 0;
    strcpy(buf, cmdline);
//parseline函数可以对命令进行分割，获取命令和参数来判断是不是后台命令
    if ((bg = parseline(buf, argv)) == 1)
    { //后台
        case_command = 4;
    }
    if (argv[0] == NULL)
    {
        return;
    }

    if (builtin_cmd(argv))
        return; //如果是内置命令则直接返回
//根据参数列表argv判断所属情况，给一个case_command去到switch-case结构中
    int i = 0;
    for (i = 0; argv[i] != NULL; i++)
    {
        if (strcmp(argv[i], ">") == 0)
        {
            if (strcmp(argv[i + 1], ">") == 0)
            {
                case_command = 5;
                break;
            }
            case_command = 1;
            file = argv[i + 1];
            argv[i] = NULL;
            break;
        }
    }

    for (i = 0; argv[i] != NULL; i++)
    {
        if (strcmp(argv[i], "<") == 0)
        {
            case_command = 2;
            file = argv[i + 1];//利用参数列表得到符号后的文件名
            printf("filename=%s\n", file);
            argv[i] = NULL;//执行null之前的所有命令
            break;
        }
    }
    char *leftargv[MAXARGS];
    for (i = 0; argv[i] != NULL; i++)
    {
        if (strcmp(argv[i], "|") == 0)
        {
            case_command = 3;
            argv[i] = NULL;
            int j;
            for (j = i + 1; argv[j] != NULL; j++)
            {
                leftargv[j - i - 1] = argv[j];
            }//得到管道前面和后面的命令参数
            leftargv[j - i - 1] = NULL;
            break;
        }
    }
//program命令有六种case
    switch (case_command)
    {
    case 0://未出现管道，重定向，后台命令
        if ((pid = fork()) == 0)
        {//fork一个子进程然后execvp运行
            execvp(argv[0], argv);
        //对shell中的命令进行处理
            exit(0);
        }//处理完以后新进程结束，waitpid等待回收进程
        if (waitpid(pid, &status, 0) == -1)
        {
            printf("error\n");
        }
        break;
    case 1:
        /*包含重定向输出 覆盖写>*/
        if ((pid = fork()) == 0)
        {//fork一个子进程
        //调用open函数得到file文件描述符fd
        //文件描述符0、1、2与进程的标准输入，标准输出，标准错误输出相对应
        //open函数参数：需要读取的文件所在路径，参数2以读、写、读写的方式打开文件/当文件不存在时，创建文件，在文件末尾追加
        //Linux中每一个文件被创建出来都自带权限，分别表示用户权限、组权限、其他权限
        //O_TRUNC属性去打开文件时，如果这个文件的本来是有内容的，则原来的内容会被丢弃。
            fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 7777);
            //可读可写，若文件存在，则长度被截为0，属性不变（将打开文件长度截短为零，相当于清空，不能与O_RDONLY搭配使用）
            //7777最大的权限

            if (fd == -1)
            {
                printf("open %s error!\n", file);
            }
            dup2(fd, 1);//将结果输出到file，将file文件描述符映射到标准输出
            close(fd);//close函数参数指需要关闭的文件的文件描述符
            execvp(argv[0], argv);//执行重定向前的指令
            exit(0);
        }
        if (waitpid(pid, &status, 0) == -1)
        {//父进程中waitpid等待子进程结束并且回收
            printf("error\n");
        }
        break;
    case 2:
        /*包含重定向输入  文件输入<*/
        if ((pid = fork()) == 0)
        {//调用open得到file的文件描述符
            fd = open(file, O_RDONLY);
            dup2(fd, 0);//将file映射到标准输入
            close(fd);//关闭闲置的文件描述防止混乱
            execvp(argv[0], argv);//执行指令
            exit(0);
        }
        if (waitpid(pid, &status, 0) == -1)
        {//父进程waitpid等待子进程结束并回收
            printf("error\n");
        }
        break;
    case 3:
        /*命令包含管道  |*/
        if ((pid = fork()) == 0)
        {//前面得到管道前面和后面的命名参数然后在子进程中pipeline函数实现管道
            pipe_line(argv, leftargv);
        }
        else
        {
            if (waitpid(pid, &status, 0) == -1)
            {//老样子回收回收
                printf("error\n");
            }
        }
        break;
    case 4: //后台运行 &
        if ((pid = fork()) == 0)
        {
            int fd1 = open("/dev/null", O_RDONLY);
            dup2(fd1, 0);//映射到标准输入
            dup2(fd1, 1);//映射到标准输出
            dup2(fd1, 2);
            execvp(argv[0], argv);//执行命令
            signal(SIGCHLD, SIG_IGN);//minix接管此进程，子进程结束了发一个信号给父进程，忽略子进程不用等待它终止
            exit(0);
            }
        else {
            printf("[process id %d]\n", pid);        //若为后台程序，则输出进程号
        } 
        //不等待结束
        break;
    case 5: //追加写>>，与覆盖写显著的区别就是文件的方式是保留不是清空
        if ((pid = fork()) == 0)
        {//open得到文件描述符fd
            fd = open(file, O_RDWR | O_CREAT | O_APPEND, 7777);
            //O_APPEND属性去打开文件时，如果这个文件中本来就是有内容的，则新写入的内容会被接续到原来内容的后面
            if (fd == -1)
            {
                printf("open %s error!\n", file);
            }
            dup2(fd, 1);//映射到标准输出
            close(fd);
            execvp(argv[0], argv);
            exit(0);
        }
        if (waitpid(pid, &status, 0) == -1)
        {
            printf("error\n");
        }
        break;

    default:
        break;
    }
    return;
}

int parseline(const char *cmdline, char **argv)
{
    //解析命令
    static char array[MAXLINE];
    char *buf = array;
    int argc = 0;
    int bg;

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';//将行末尾的回车改为空格
    while (*buf && (*buf == ' '))
        buf++;
//char *strtok(char *s,char *delim) 实现原理：将分隔符出现的地方改为'\0'
//根据空格对命令进行分割
    char *s = strtok(buf, " ");
    if (s == NULL)
    {
        exit(0);
    }
    argv[argc] = s;//得到argv参数序列
    argc++;
    while ((s = strtok(NULL, " ")) != NULL)
    {//参数设置为NULL，从上一次读取的地方继续
        argv[argc] = s;
        argc++;
    }
    argv[argc] = NULL;

    if (argc == 0)
        return 1;
//根据最后一个字符是不是&判断是不是后台命令
    if ((bg = (*argv[(argc)-1] == '&')) != 0)
    {
        argv[--(argc)] = NULL;
    }
    return bg;
}

void mytop() {
    FILE *fp = NULL;            
    char buff[255];

    fp = fopen("/proc/meminfo", "r");   // 以只读方式打开meminfo文件
    fgets(buff, 255, (FILE*)fp);        // 读取meminfo文件内容进buff
    fclose(fp);

    // 获取 pagesize
    int i = 0, pagesize = 0;
    while (buff[i] != ' ') {
        pagesize = 10 * pagesize + buff[i] - 48;
        i++;
    }

    // 获取 页总数 total
    i++;
    int total = 0;
    while (buff[i] != ' ') {
        total = 10 * total + buff[i] - 48;
        i++;
    }

    // 获取空闲页数 free
    i++;
    int free = 0;
    while (buff[i] != ' ') {
        free = 10 * free + buff[i] - 48;
        i++;
    }

    // 获取最大页数量largest
    i++;
    int largest = 0;
    while (buff[i] != ' ') {
        largest = 10 * largest + buff[i] - 48;
        i++;
    }

    // 获取缓存页数量 cached
    i++;
    int cached = 0;
    while (buff[i] >= '0' && buff[i] <= '9') {
        cached = 10 * cached + buff[i] - 48;
        i++;
    }

    // 总体内存大小 = (pagesize * total) / 1024 单位 KB
    int totalMemory  = pagesize / 1024 * total;
    // 空闲内存大小 = pagesize * free) / 1024 单位 KB
    int freeMemory   = pagesize / 1024 * free;
    // 缓存大小    = (pagesize * cached) / 1024 单位 KB
    int cachedMemory = pagesize / 1024 * cached;

    printf("totalMemory  is %d KB\n", totalMemory);
    printf("freeMemory   is %d KB\n", freeMemory);
    printf("cachedMemory is %d KB\n", cachedMemory);

    /* 2. 获取内容2
        进程和任务数量
     */
    fp = fopen("/proc/kinfo", "r");     // 以只读方式打开kinfo文件
    memset(buff, 0x00, 255);            // 格式化buff字符串
    fgets(buff, 255, (FILE*)fp);        // 读取kinfo文件内容进buff
    fclose(fp);

    // 获取进程数量
    int processNumber = 0;
    i = 0;
    while (buff[i] != ' ') {
        processNumber = 10 * processNumber + buff[i] - 48;
        i++;
    }
    printf("processNumber = %d\n", processNumber);

    // 获取任务数量
    i++;
    int tasksNumber = 0;
    while (buff[i] >= '0' && buff[i] <= '9') {
        tasksNumber = 10 * tasksNumber + buff[i] - 48;
        i++;
    }
    printf("tasksNumber = %d\n", tasksNumber);
    return;
}


void pipe_line(char *process1[], char *process2[])
{
    int fd[2];
    pipe(&fd[0]);
//调用pipe函数创建一个管道fd[2]
    int status;
    pid_t pid = fork();
    if (pid == 0)
    {
        close(fd[0]);
        close(1);
        //关闭管道读端fd[0]和文件描述符1
        dup(fd[1]);//将管道的写端映射到标准输出
        close(fd[1]);//关闭管道写端以免堵塞
        execvp(process1[0], process1);//执行管道前部分指令，将进程的输出写入管道
    }
    else
    { //父进程中
        close(fd[1]);
        close(0);//关闭管道写端fd[1]和文件描述符0
        dup(fd[0]);//复制文件描述符，恢复标准输入，读端映射到标准输入，从管道中读入数据
        close(fd[0]);//关闭读端以免堵塞
        waitpid(pid, &status, 0);
        execvp(process2[0], process2);//执行管道后部分指令
    }
}

int builtin_cmd(char **argv)
{
    //内置命令
    //关于exit退出功能
    if (!strcmp(argv[0], "exit"))
    {
        exit(0);//直接退出main函数的while循环
    }
    //关于mytop功能
    if (!strcmp(argv[0], "mytop"))
    {
        mytop();
        return 1;
    }
    //关于cd功能
    if (!strcmp(argv[0], "cd"))
    {
        if (!argv[1])
        { // cd后面没有任何输入
            argv[1] = ".";
        }
        int ret = chdir(argv[1]); //根据参数改变工作目录,将当前工作目录切换到argv[1](从命令行传进来的路径)
        //函数原型 int chdir(const char *path)
        if (ret < 0)
        {
            printf("No such directory!\n");
        }
        else
        {
            path = getcwd(NULL, 0); //path指向当前的工作目录，调用getcwd函数获取当前的工作路径
            //char *getcwd(char *buf, size_t size);
        }
        return 1;
    }
//对于history
    if (!strcmp(argv[0], "history"))
    {
        if (!argv[1])
        { //只输入history
            for (int j = 1; j <= his_cnt; j++)
            {
                printf("%d ", j);
                puts(his[j - 1]);
            }
        }
        else
        {
            int t = atoi(argv[1]);
            if (his_cnt - t < 0)
            {//输入history之后的数字
                printf("please confirm the number below %d\n", his_cnt);
            }
            else
            {
                for (int j = his_cnt - t; j < his_cnt; j++)
                {
                    printf("%d ", j + 1);
                    puts(his[j]);//输出二维数组中的命令历史
                }
            }
        }
        return 1;
    }

    return 0;
}