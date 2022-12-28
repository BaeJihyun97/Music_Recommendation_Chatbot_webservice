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
#include <dirent.h>



#define JHLOG(format...)	JhLog(__LINE__, format, ##format)
#define DeleteTime(a)		(time(NULL) - (a))



//----------------------------------------------------------
// 현재시간을 구하는 함수이다.
// in 		--> day
// out 		--> none
// return 	--> 현재시간
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
// log messgae를 출력하는 모듈이다.
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
    printf("APP_DEL(%04d, %s):: %s\n", line, GetCurTime(day), msg);
    return;
}


//----------------------------------------------------------
// 지정된 path에서 일시간이 경과된 파일을 삭제한다.
// in 		--> path, delTime
// out 		--> none
// return 	--> result
//----------------------------------------------------------
int JhDelDir(char* path, time_t delTime)
{
    DIR* dir;
    struct stat st;
    char fname[512];
    struct dirent* dirent;

    if ((dir = opendir(path)) == NULL) {
        JHLOG("open directory error. (%s)", path);
        return(-1);
    }
    while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) continue;
        sprintf(fname, "%s/%s", path, dirent->d_name);
        if (stat(fname, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        if (st.st_mtime <= delTime) {
            JHLOG("JhDelDir, delete file=%s", fname);
            errno = 0;
            if (unlink(fname) < 0) {
                JHLOG("JhDelDir, file=%s, 삭제중 오류가 발생 했습니다. errno=%d", fname, errno);
                continue;
            }
        }
    }
    closedir(dir);
    return(0);
}



//----------------------------------------------------------
// 지정 process실행 유무를 검사한다.
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
// MAIN
// in 		--> argc, argv
// out 		--> none
// return 	--> none
//----------------------------------------------------------
int main(int argc, char* argv[])
{
    int parent;
    time_t delTime, next_work_time = 0;
    char chatLogPath[128] = "/var/www/html/chat/log";

    if (argc < 2) {
        JHLOG("Usage :: app_del parent_id");
        return(1);
    }
    parent = atoi(argv[1]);

    while (1) {
        if (chkproc(parent) < 0) {
            JHLOG("parent process not found. exit.");
            return(1);
        }
        if (next_work_time < time(NULL)) {
            next_work_time = time(NULL) + 60;
            delTime = DeleteTime(24 * 3600);
            JhDelDir(chatLogPath, delTime);
        }
        usleep(250 * 1000);
    }

    return(0);
}

