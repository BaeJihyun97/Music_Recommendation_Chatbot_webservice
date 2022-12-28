#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>


#define JHLOG(format...)	JhLog(__LINE__, format, ##format)




//----------------------------------------------------------
// ����ð��� ���ϴ� �Լ��̴�.
// in 		--> day
// out 		--> none
// return 	--> ����ð�
//----------------------------------------------------------
char* GetCurTime(char* day)
{
    struct tm tm;
    struct timeval ctm;

    gettimeofday(&ctm, NULL);
    localtime_r(&ctm.tv_sec, &tm);
    sprintf(day, "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(ctm.tv_usec / 1000));
    return(day);
}


//----------------------------------------------------------
// log messgae�� ����ϴ� ����̴�.
// in 		--> line, fmt
// out 		--> none
// return 	--> none
//----------------------------------------------------------
void JhLog(int line, const char* fmt, ...)
{
    int x, p;
    va_list args;
    char msg[2048], day[32];

    memset(msg, 0x00, sizeof(msg));
    va_start(args, fmt);
    vsnprintf(msg, 2047, fmt, args);
    va_end(args);
    printf("APP_MAN(%04d, %s):: %s\n", line, GetCurTime(day), msg);
    return;
}


//----------------------------------------------------------
// ���� process�� ���� ��Ű�� ����� �Ѵ�.
// in 		--> runname
// out 		--> none
// return 	--> pid
//----------------------------------------------------------
int vcRunProcess(char* runname)
{
    int  pid;
    char* arg[8];
    char args[8][128];

    memset(arg, 0x00, sizeof(arg));
    memset(args, 0x00, sizeof(args));
    sprintf(args[0], "./%s", runname);
    sprintf(args[1], "%s", runname);
    sprintf(args[2], "%d", getpid());
    arg[0] = args[1];
    arg[1] = args[2];

    pid = fork();
    if (pid < 0) {
        JHLOG("Process [%s] Start Fail.", runname);
        return(-1);
    }
    else if (pid == 0) {
        execv(args[0], arg);
        JHLOG("Process [%s] Start Fail errno[%d]", runname, errno);
        exit(0);
    }
    else {
        JHLOG("Process [%s] Start. OK.", runname);
    }
    return(pid);
}


//----------------------------------------------------------
// ���� process���� ������ �˻��Ѵ�.
// in 		--> runname
// out 		--> none
// return 	--> pid
//----------------------------------------------------------
int chkproc(int id)
{
    struct stat st;
    char cmd[100];

    sprintf(cmd, "/proc/%d", id);
    if (stat(cmd, &st) != 0) {
        if (errno == EACCES)
            JHLOG("process not access owner error [%d]", id);
        else
            return(-1);
    }
    return(0);
}


//----------------------------------------------------------
// child�� ���Ḧ ó���ϴ� �Լ��̴�.
// in 		--> none
// out 		--> none
// return 	--> none
//----------------------------------------------------------
void CleanProcess()
{
    int pid, stat;

    if ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        if (WTERMSIG(stat) == 9 || WTERMSIG(stat) == 15)
            JHLOG("�������� ���� ���� ����Ǿ����ϴ�.");
        else if (WTERMSIG(stat) == 10)
            JHLOG("�������� �ñ׳��� �޾ҽ��ϴ�. ���α׷��� Ȯ���Ͻʽÿ�.");
        else if (WTERMSIG(stat) == 11)
            JHLOG("(Segmentation Violation). ���α׷��� Ȯ���Ͻʽÿ�.");
        else if (WEXITSTATUS(stat) != 0 && pid > 0)
            JHLOG("�����ڵ尡 (%d)�Դϴ�.", WEXITSTATUS(stat));
        JHLOG("Child Down Signal Get [%d][%x][%d]", pid, stat, errno);
    }
    return;
}


//----------------------------------------------------------
// Daemon���� ������ �ϸ� app_gw�� ���� ������ �����Ѵ�.
// in 		--> argc, argv
// out 		--> none
// return 	--> result
//----------------------------------------------------------
int main(int argc, char* argv[])
{
    int pid, child = 0, child_del = 0;

    switch ((pid = fork())) {
    case 0:
        break;
    case -1:
        JHLOG("Daemon Fork Fail");
        return(1);
    default:
        JHLOG("Daemon Fork Success.");
        return(1);
    }

    // daemon start...
    signal(SIGCHLD, SIG_IGN);
    //sigset(SIGCHLD, SIG_IGN);
    setpgrp(); // ����ܸ����� ���� ������ ������Ų��. (background daemon���� ����ȴ�)
    while (1) {
        CleanProcess();
        if (child <= 0 || chkproc(child) < 0) {
            if ((child = vcRunProcess("app_gw")) <= 0) {
                JHLOG("app_gw task start fail.");
                usleep(1500 * 1000);
            }
        }
        if (child_del <= 0 || chkproc(child_del) < 0) {
            if ((child_del = vcRunProcess("app_del")) <= 0) {
                JHLOG("app_del task start fail.");
                usleep(1500 * 1000);
            }
        }
        usleep(250 * 1000);
    }
}

