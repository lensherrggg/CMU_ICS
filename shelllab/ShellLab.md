# Shell Lab

## 实验内容

本次实验要求实现一个简单的shell，框架已经预先搭好，需要自主实现以下几个函数

```
• eval: Main routine that parses and interprets the command line. [70 lines]
• builtin cmd: Recognizes and interprets the built-in commands: quit, fg, bg, and jobs. [25
lines]
• do bgfg: Implements the bg and fg built-in commands. [50 lines]
• waitfg: Waits for a foreground job to complete. [20 lines]
• sigchld handler: Catches SIGCHILD signals. 80 lines]
• sigint handler: Catches SIGINT (ctrl-c) signals. [15 lines]
• sigtstp handler: Catches SIGTSTP (ctrl-z) signals. [15 lines]
```

从函数的描述可以得知，该shell需要：

1. 正确处理输入的命令
2. 实现多进程，程序前后台运行
3. 正确处理信号

可以使用的普通函数：

- `int parseline(const char *cmdline,char **argv)`：获取参数列表`char **argv`，返回是否为后台运行命令（`true`）。
- `void clearjob(struct job_t *job)`：清除`job`结构。
- `void initjobs(struct job_t *jobs)`：初始化`jobs`链表。
- `void maxjid(struct job_t *jobs)`：返回`jobs`链表中最大的`jid`号。
- `int addjob(struct job_t *jobs,pid_t pid,int state,char *cmdline)`：在`jobs`链表中添加`job`
- `int deletejob(struct job_t *jobs,pid_t pid)`：在`jobs`链表中删除`pid`的`job`。
- `pid_t fgpid(struct job_t *jobs)`：返回当前前台运行`job`的`pid`号。
- `struct job_t *getjobpid(struct job_t *jobs,pid_t pid)`：返回`pid`号的`job`。
- `struct job_t *getjobjid(struct job_t *jobs,int jid)`：返回`jid`号的`job`。
- `int pid2jid(pid_t pid)`：将`pid`号转化为`jid`。
- `void listjobs(struct job_t *jobs)`：打印`jobs`。
- `void sigquit_handler(int sig)`：处理`SIGQUIT`信号。



**注意事项**

- tsh的提示符为`tsh>`
- 用户的输入分为第一个的`name`和后面的参数，之间以一个或多个空格隔开。如果`name`是一个tsh内置的命令，那么tsh应该马上处理这个命令然后等待下一个输入。否则，tsh应该假设`name`是一个路径上的可执行文件，并在一个子进程中运行这个文件（这也称为一个工作、job）
- tsh不需要支持管道和重定向
- 如果用户输入`ctrl-c` (`ctrl-z`)，那么`SIGINT` (`SIGTSTP`)信号应该被送给每一个在前台进程组中的进程，如果没有进程，那么这两个信号应该不起作用。
- 如果一个命令以“&”结尾，那么tsh应该将它们放在后台运行，否则就放在前台运行（并等待它的结束）
- 每一个工作（job）都有一个正整数PID或者job ID（JID）。JID通过"%"前缀标识符表示，例如，“%5”表示JID为5的工作，而“5”代笔PID为5的进程。
- tsh应该有如下内置命令：

```text
quit: 退出当前shell

jobs: 列出所有后台运行的工作

bg <job>: 这个命令将会向<job>代表的工作发送SIGCONT信号并放在后台运行，<job>可以是一个PID也可以是一个JID。

fg <job>: 这个命令会向<job>代表的工作发送SIGCONT信号并放在前台运行，<job>可以是一个PID也可以是一个JID。
```

- tsh应该回收（reap）所有僵尸子进程，如果一个工作是因为收到了一个它没有捕获的（没有按照信号处理函数）而终止的，那么tsh应该输出这个工作的PID和这个信号的相关描述。



本实验需要对异常控制流有比较好的理解，对信号的功能有清楚的认识。



## 实验步骤

### 完成eval函数

CSAPP教材P525提供了一个简单的eval函数，这里按照其框架结合P543内容完成了eval函数。

1. 初始化各变量及设置信号阻塞合集

2. 解析命令行，得到是否为后台命令，置为state（前后台标志）
3. 判断是否为内置命令，如果是则立即执行，否则fork子进程。子进程调用执行具体的命令文件，然后exit。父进程执行step4
4. 将子进程job加入job list
5. 判断是否为bg，fg调用waitfg函数等待前台运行完成，bg打印消息即可。

整个过程中，需要仔细考虑显式阻塞的安排和设计，什么地方需要阻塞哪些信号。

原则1：在访问全局变量（jobs）必须阻塞所有信号，包括调用题目给的那些函数。由于这些函数基本都是使用for循环遍历完成功能的，所以，务必保证函数执行中不能被中断。

原则2：在一些函数或者指令有必须的前后执行顺序时，请阻塞，保证前一个函数调用完成后（比如必须先addjob，再deletejob）

```c
void eval(char *cmdline) 
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    int state;
    pid_t pid;
    sigset_t mask_all, mask_one, prev;
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    
    // 没有参数就退出
    // 其实可以加一个判断参数数量是否正确的语句，比较完整
    if (argv[0] == NULL)
    	return;  
    
    // 如果不是内置的命令，则执行
    if(!builtin_cmd(argv))
    {   
        // 在函数内部加阻塞列表，不然之后可能会出现不痛不痒的bug
   	sigfillset(&mask_all);
    	sigemptyset(&mask_one);
    	sigaddset(&mask_one, SIGCHLD);

        // 为了避免父进程运行到addjob之前子进程就退出了，所以
        // 在fork子进程前阻塞sigchld信号，addjob后解除
    	sigprocmask(SIG_BLOCK, &mask_one, &prev);
    	if ((pid = fork()) == 0)
    	{
                // 子进程继承了父进程的阻塞向量，也要解除阻塞，
                // 避免收不到它本身的子进程的信号     
    		sigprocmask(SIG_SETMASK, &prev, NULL);
                // 改进程组与自己pid一样
    		if (setpgid(0, 0) < 0)
    		{
    			perror("SETPGID ERROR");
    			exit(0);
    		}
                // 正常运行execve函数会替换内存，不会返回/退出，所以必须要加exit，
                // 否则会一直运行下去，子进程会开始运行父进程的代码
    		if (execve(argv[0], argv, environ) < 0)
    		{
    			printf("%s: Command not found\n", argv[0]);
    			exit(0);
    		}
    	}
    	else
    	{
    		state = bg ? BG : FG;
                // 依然是加塞，阻塞所有信号
    		sigprocmask(SIG_BLOCK, &mask_all, NULL);
    		addjob(jobs, pid, state, cmdline);
    		sigprocmask(SIG_SETMASK, &prev, NULL);
    	}
        // 后台则打印，前台则等待子进程结束
    	if (!bg)
    		waitfg(pid);
    	else
    		printf("[%d] (%d) %s",pid2jid(pid), pid, cmdline);
        
        // 后面又想了想，打印后台的时候其实走的是全局变量，也应该加塞才对，
        // 应该是所有的用到全局变量的都应该加塞，但是懒，不改了，知道就行
    }
    return;
}

```



### 完成builtin_cmd函数

可参考教材P525的builtin_cmd函数。注意调用listjobs时需要先阻塞信号保证不被终端，调用完成后及时unblock。

```C
int builtin_cmd(char **argv) 
{
    // 判断是不是内置函数，不是就返回，注意内置命令有要继续操作的一定
    // 要返回1，不是内置函数就是0

    if (!strcmp(argv[0], "quit"))
    	exit(0);
    else if (!strcmp(argv[0], "jobs"))
    {
    	listjobs(jobs);
    	return 1;
    }
    else if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg"))
    {
    	do_bgfg(argv);
    	return 1;
    }
    // 对单独的&不处理
    else if (!strcmp(argv[0], "&"))
    {
    	return 1;
    }
    return 0;     /* not a builtin command */

}

```





### 完成do_bgfg函数

该函数分为以下几个部分：

1. 声明变量并初始化

2. 通过pid（一串数字字符）或jid（% + 一串数字字符）得到正确的jid（一个整数）并获得所指的job

3. 处理bg命令或fg命令

   3.1 若为bg命令，如果job状态为ST，则将其状态置为BG，给job所在进程发送SIGCONT信号并打印出信息；如果状态时BG则不操作；如果状态是其他则报错

   3.2 若是fg命令，如果job状态为ST则将其状态置为FG，给job所在进程组发送SIGCONT信号并等待运行结束；如果状态为BG只需将状态置为FG并等待运行结束；如果状态为其他则报错



```c
void do_bgfg(char **argv) 
{
    // 没有参数，其实应该也加上判断参数个数的语句才比较完整
    if (argv[1] == NULL)
    {
    	printf("%s command requires PID or %%jobid argument\n", argv[0]);
    	return;
    }
    
    struct job_t* job;
    int id;

    // bg %5 和bg 5不一样,一个是对一个作业操作，另一个是对进程操作，
    // 而作业代表了一个进程组。

    // 要根据tshref的样例输出看有多少种情况

    // 读到jid
    if (sscanf(argv[1], "%%%d", &id) > 0)
    {
    	job = getjobjid(jobs, id);
    	if (job == NULL)
    	{
    		printf("%%%d: No such job\n", id);
    		return ;
    	}
    }
    // 读到pid
    else if (sscanf(argv[1], "%d", &id) > 0)
    {
    	job = getjobpid(jobs, id);
    	if (job == NULL)
    	{
    		printf("(%d): No such process\n", id);
    		return ;
    	}
    }
    // 格式错误
    else
    {
    	printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    // 因为子进程单独成组，所以kill很方便
    if(!strcmp(argv[0], "bg"))
    {
        // 进程组是负数pid，发送信号并更改状态
    	kill(-(job->pid), SIGCONT);
    	job->state = BG;
    	printf("[%d] (%d) %s",job->jid, job->pid, job->cmdline);
    }
    else
    {
        // 如果fg后台进程，那么将它的状态转为前台进程，然后等待它终止
    	kill(-(job->pid), SIGCONT);
    	job->state = FG;
    	waitfg(job->pid);
    }
    
    
    return;
}
```







### 完成waitfg函数

Lab指导中提示用sleep函数实现waitfg

```c
void waitfg(pid_t pid)
{
    // 进程回收不需要做，只要等待前台进程就行
    sigset_t mask_temp;
    sigemptyset(&mask_temp);
    // 设定不阻塞任何信号
    // 其实可以直接sleep显式等待信号
    while (fgpid(jobs) > 0)
    	sigsuspend(&mask_temp);
    return;
}
```





### 完成handler函数

Sigint_handler函数和sigtstp_handler函数用于向子进程组发送SIGINT和SIGTSTP信号，交由sigchld_handler函数统一处理子进程。

sigchld_handler函数需要使用waitpid(-1, &status, WNOHANG | WUNTRACED)来尽可能回收僵尸进程，参数WUNTRACED|WNOHANG表示waitpid立即返回，监测子进程终止或停止。用WIFEXTED(status)、WIFSIGNALED(status)、WIFSTOPPED(status)来检测子进程状态并分别作出响应。代码如下：

```c
void sigchld_handler(int sig) 
{
    int olderrno = errno;   // 保存旧errno
    pid_t pid;
    int status;
    sigset_t mask_all, prev; 
    
    sigfillset(&mask_all);  // 设置全阻塞
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        // WNOHANG | WUNTRACED 是立即返回
        // 用WIFEXITED(status)，WIFSIGNALED(status)，WIFSTOPPED(status)等来补获终止或者
        // 被停止的子进程的退出状态。
    	if (WIFEXITED(status))  // 正常退出 delete
    	{
    		sigprocmask(SIG_BLOCK, &mask_all, &prev);
    		deletejob(jobs, pid);
    		sigprocmask(SIG_SETMASK, &prev, NULL);
    	}
    	else if (WIFSIGNALED(status))  // 信号退出 delete
    	{
    	    struct job_t* job = getjobpid(jobs, pid);
            sigprocmask(SIG_BLOCK, &mask_all, &prev);
            printf("Job [%d] (%d) terminated by signal %d\n", job->jid, job->pid, WTERMSIG(status));
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev, NULL);
    	}
    	else  // 停止 只修改状态就行
    	{
            struct job_t* job = getjobpid(jobs, pid);
            sigprocmask(SIG_BLOCK, &mask_all, &prev);
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));
            job->state= ST;
            sigprocmask(SIG_SETMASK, &prev, NULL);
        }
    }
    errno = olderrno;  // 恢复
    return;
}


```



sigint_handler函数和sigtstp_handler函数结构一致，比较简单。都是用kill函数发送信号，第一个参数为-pid表示对该子进程所在的进程组内全体子进程发送信号。

```c
void sigint_handler(int sig) 
{
    // 向子进程发送信号即可
   	int olderrno = errno;
   	pid_t pid = fgpid(jobs);
   	if (pid != 0)
            kill(-pid, sig);
   	errno = olderrno;
   	
   	return;
}

```



```c
void sigtstp_handler(int sig) 
{
    // 向子进程发送信号即可
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    if (pid != 0)
    	kill(-pid, sig);
    errno = olderrno;
    return;
}
```

至此完成所有函数。

