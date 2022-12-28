#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <strings.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <execinfo.h>



#define MAXCOLAB			32			// colab client 접속 최대 건수
#define MAXWAIT				60
#define ThreadStl			__thread	// thread 독립 메모리를 선언한다.
#define CNV2DT(a)			((((a)[0]-'0')*10)+((a)[1]-'0'))
#define CNV4DT(a)			((((a)[0]-'0')*1000)+(((a)[1]-'0')*100)+(((a)[2]-'0')*10)+((a)[3]-'0'))
#define JHLOG(format...)	JhLog(__LINE__, format, ##format)
#define DTCNT(a)			(sizeof(a)/sizeof(a[0]))


typedef int SOCKET;
typedef uint32_t jhUint32, * JHUINT32;
typedef uint64_t jhUint64, * JHUINT64;
typedef jhUint64 JhTick, * JHTICK;
typedef struct timespec JhTimeSpec, * JHTIMESPEC;



typedef struct __proto_head
{
	char		pk_size[4];		// Packet size
	char		command[4];		// command : request data(1001), response data(2001) ...
}ProtoHead, * PROTOHEAD;


typedef struct __current_thread
{
	int			chan;			// channel number
	int			stat;			// 사용정보 --> 0:empty 1:wait 2:run 3:close
	int 		loop;			// thread loop
	SOCKET 		sock;			// current work socket
	char		chead[32];		// colab header
	pthread_t	tid;			// current thread id
	JhTick		link_check;		// link check 시간을 지정한다.
	JhTick		timeout;		// work timeout (PHP : MAXWAIT * 1000)
	char		request[8192];	// 작업요청
	void* work;			// request(PhpWork)
}CurrentThread, * CURRENTTHREAD;


extern int errno;
static ThreadStl CURRENTTHREAD curmem = NULL;
static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;
static CURRENTTHREAD colabptr[MAXCOLAB] = { NULL, };


char* GetCurTime(char* day);
char* GetCurDay(char* day);
void JhLog(int line, const char* fmt, ...);
int JhLock();
void JhUnlock();
JhTick JhGetTickCount();




//----------------------------------------------------------
// 신규 colab channel을 할당 한다.
// in 		--> chan
// out 		--> none
// return 	--> result
//----------------------------------------------------------
int SetColabChannel(CURRENTTHREAD chan)
{
	int k;

	for (k = 0; k < DTCNT(colabptr); k++) {
		if (colabptr[k] == NULL) {
			chan->chan = k + 1;
			colabptr[k] = chan;
			JHLOG("colab channel=%d 할당 했습니다.", chan->chan);
			sprintf(chan->chead, "colab=%d", chan->chan);
			return(1);
		}
	}
	JHLOG("colab channel 할당 오류입니다.");
	return(-1);
}


//----------------------------------------------------------
// idle colab channel을 검색하여 작업을 할당 한다.
// in 		--> php, data
// out 		--> none
// return 	--> colab
//----------------------------------------------------------
CURRENTTHREAD SetColabWork(CURRENTTHREAD php, char* data)
{
	int k;

	while (php->timeout >= JhGetTickCount()) {
		for (k = 0; k < DTCNT(colabptr); k++) {
			if (colabptr[k] == NULL || colabptr[k]->work != NULL) continue;
			if (JhLock() < 0) return(NULL);
			if (colabptr[k]->work != NULL) {
				JhUnlock();
				JHLOG("colab 작업이 이미 할당 되었습니다. continue");
				continue;
			}
			strcpy(colabptr[k]->request, data);
			colabptr[k]->work = php;
			php->work = colabptr[k];
			colabptr[k]->timeout = php->timeout;
			colabptr[k]->stat = 2; // run work.
			JhUnlock();
			return(colabptr[k]);
		}
		usleep(1000);
	}

	JHLOG("Idle colab channel not found.");
	return(NULL);
}


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
// 현재시간을 구하는 함수이다.
// in 		--> day
// out 		--> none
// return 	--> 현재시간
//----------------------------------------------------------
char* GetCurDay(char* day)
{
	struct tm tm;
	struct timeval ctm;

	gettimeofday(&ctm, NULL);
	localtime_r(&ctm.tv_sec, &tm);
	sprintf(day, "%04d/%02d/%02d %02d:%02d:%02d.%03d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(ctm.tv_usec / 1000));
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
	printf("APP_GW (%04d,%s):: %s\n", line, GetCurTime(day), msg);
	return;
}


//----------------------------------------------------------
// signal을 catch하여 처리를 한다.
// in 		--> sig
// out 		--> none
// return 	--> none
//----------------------------------------------------------
void vcSigCore(int sig)
{
	int bt_size, k;
	void* frames[1024];
	char** bt_symbols;
	static int call = 0;

	if (call == 1) exit(1);
	call = 1;
	JHLOG("");
	JHLOG("signal no=[%d]", sig);
	switch (sig) {
	case SIGFPE:
		JHLOG("프로그램이 비정적으로 종료됩니다.(SIGFPE) !!!!!");
		break;
	case SIGILL:
		JHLOG("프로그램이 비정적으로 종료됩니다.(SIGILL) !!!!!");
		break;
	case SIGBUS:
		JHLOG("프로그램이 비정적으로 종료됩니다.(SIGBUS) !!!!!");
		break;
	case SIGSEGV:
		JHLOG("프로그램이 비정적으로 종료됩니다.(SIGSEGV) !!!!!");
		break;
	default:
		JHLOG("프로그램이 비정적으로 종료됩니다.미정의(%d) !!!!!", sig);
		break;
	}
	JHLOG("");

	bt_size = backtrace(frames, 1024);
	bt_symbols = backtrace_symbols(frames, bt_size);
	for (k = 0; k < bt_size; k++) {
		if (bt_symbols)
			JHLOG("%d: 0x%016lx %s", k, frames[k], bt_symbols[k]);
		else
			JHLOG("%d: 0x%016lx", k, frames[k]);
	}
	if (bt_symbols) free(bt_symbols);
	JHLOG("");
	abort(); // create core dump 
}


//----------------------------------------------------------
// signal을 catch하여 처리를 한다.
// in 		--> sig
// out 		--> none
// return 	--> none
//----------------------------------------------------------
void vcDefaultSig(int sig)
{
	signal(sig, SIG_IGN);
	switch (sig) {
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		JHLOG("vcDefaultSig, 사용자 미정의 Signal(%d)을 받았습니다.!!!(작업종료)", sig);
		exit(1);
	case SIGUSR1:
		JHLOG("vcDefaultSig, 사용자 미정의 Signal(%d)을 받았습니다.!!!", sig);
		signal(sig, vcDefaultSig);
		break;
	case SIGUSR2:
		JHLOG("vcDefaultSig, 사용자 미정의 Signal(%d)을 받았습니다.!!!", sig);
		signal(sig, vcDefaultSig);
		break;
	case SIGPIPE:
		JHLOG("vcDefaultSig, 사용자 미정의 Signal(%d)을 받았습니다.!!!", sig);
		signal(sig, vcDefaultSig);
		break;
	}
	return;
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
// mutex를 사용하여 동시 접근을 제어 한다.
// in 		--> none
// out 		--> none
// return 	--> result
//----------------------------------------------------------
int JhLock()
{
	int ret;

	if ((ret = pthread_mutex_lock(&__mutex)) != 0) {
		JHLOG("myget_context_inter, lock error. errno=%d, return=%d", errno, ret);
		return(-1);
	}
	return(0);
}


//----------------------------------------------------------
// mutex를 사용하여 동시 접근을 제어 한다.
// in 		--> none
// out 		--> none
// return 	--> none
//----------------------------------------------------------
void JhUnlock()
{
	pthread_mutex_unlock(&__mutex);
}


/*-----------------------------------------------------------------------------
// 1/1000초의 선형 time을 구하는 함구이다.
// in 		--> none
// out 		--> none
// return 	--> JhTick
-----------------------------------------------------------------------------*/
JhTick JhGetTickCount()
{
	JhTick tick;
	JhTimeSpec rv;

	clock_gettime(CLOCK_MONOTONIC, &rv);
	tick = (rv.tv_sec * 1000L) + (rv.tv_nsec / 1000000L);
	return(tick);
}


/*-----------------------------------------------------------------------------
// 현재시간과 과거의 특정 시점의 시차를 구한다.
// in 		--> old
// out 		--> none
// return 	--> JhTick
-----------------------------------------------------------------------------*/
JhTick MsTimeSub(JhTick old)
{
	return(JhGetTickCount() - old);
}


/*-----------------------------------------------------------------------------
// 현재시간과 과거의 특정 시점의 시차를 구한다.
// in 		--> fd, wait_msec
// out 		--> none
// return 	--> count
-----------------------------------------------------------------------------*/
int JhPoll(SOCKET fd, int wait_msec)
{
	struct pollfd pfd[1];

	memset(pfd, 0x00, sizeof(pfd));
	pfd[0].fd = fd;
	pfd[0].events = POLLIN | POLLERR | POLLHUP;
	if (poll(pfd, 1, wait_msec) > 0) {
		if ((pfd[0].revents & POLLERR) == POLLERR || (pfd[0].revents & POLLHUP) == POLLHUP) {
			JHLOG("HsPollFileDesc, client down. thread exit. errno=%d", errno);
			return(-1);
		}
		else if ((pfd[0].revents & POLLIN) == POLLIN) {
			return(1);
		}
	}
	return(0);
}


//----------------------------------------------------------
// 지정된 size만큼 socket으로 자료를 Read한다.
// in 		--> fd, length, wait_msec
// out 		--> buff
// return 	--> size
//----------------------------------------------------------
int ReadChunkData(SOCKET fd, char* buff, int length, int wait_msec)
{
	JhTick start;
	char* cp = (char*)buff;
	int len, rsize, wtm, size = 0, e_inc = 0;

	if (fd <= 0) return(-1);
	start = JhGetTickCount();
	while (size < length) {
		wtm = wait_msec - MsTimeSub(start);
		switch (JhPoll(fd, wtm <= 0 ? 0 : wtm)) {
		case 1:
			errno = 0;
			len = length - size;
			if ((rsize = read(fd, cp + size, len)) <= 0) {
				if (errno == EINTR) {
					if (e_inc++ < 3) {
						errno = 0;
						continue;
					}
				}
				JHLOG("ReadChunkData(%d), read(%d) error. errno=%d(%d), cp=%p-%p, size=%d/%d",
					fd, rsize, errno, e_inc, cp, buff, size, len);
				return(-1);
			}
			size = size + rsize;
			break;
		case -1:
			JHLOG("ReadChunkData, JhPoll error. errno=%d", errno);
			return(-1);
		}
		if (MsTimeSub(start) >= wait_msec) {
			JHLOG("ReadChunkData, wait timeout.");
			return(-1);
		}
	}
	cp[length] = 0x00;
	if (size > 0) curmem->link_check = JhGetTickCount() + ((MAXWAIT - 5) * 1000);
	return(size);
}


//----------------------------------------------------------
// 지정된 size만큼 socket으로 자료를 write한다.
// in 		--> fd, length, wait_msec
// out 		--> buff
// return 	--> size
//----------------------------------------------------------
int SendChunkData(SOCKET fd, void* buff, int length, int wait_msec)
{
	JhTick start;
	char* cp = (char*)buff;
	int ret, totSize, wsize = length;

	if (fd <= 0) return(-1);
	start = JhGetTickCount();
	for (totSize = 0; totSize < length && wait_msec >= MsTimeSub(start); ) {
		if ((ret = write(fd, cp, wsize)) <= 0) {
			totSize = -1;
			break;
		}
		totSize += ret;
		cp = buff + totSize;
		wsize = length - totSize;
	}
	if (totSize > 0) curmem->link_check = JhGetTickCount() + ((MAXWAIT - 5) * 1000);
	return(totSize);
}



//-----------------------------------------------------
// ipv4 + ipv6 address convert
// in 		--> ipstr, ip_type
// out 		--> ipstr
// return   --> none
//-----------------------------------------------------
void JhModIpvXAddr(char* ipstr, int ip_type)
{
	char* cstr, tmp[128];

	if (ipstr == NULL || strncasecmp(ipstr, "::FFFF:", 7) != 0) return;
	// IPv4 + IPv6의 주소를 동시에 가지고 있음.
	if (strstr(ipstr, ".") != NULL && strstr(ipstr, ":") != NULL) {
		for (cstr = ipstr + (strlen(ipstr) - 1); cstr > ipstr; cstr--)
			if (*cstr == ':') { cstr++; break; }
		if (cstr <= ipstr) return;
		strcpy(tmp, cstr);
		if (ip_type == AF_INET) strcpy(ipstr, cstr);
		else *cstr = 0;
	}
	return;
}


//-----------------------------------------------------
// ipv4 + ipv6 address convert
// in 		--> addr
// out 		--> ipstr
// return   --> ipstr
//-----------------------------------------------------
char* JsGetIPv46InfoString(struct sockaddr_storage* addr, char* ipstr)
{
	if (addr->ss_family == AF_INET) {
		struct sockaddr_in* addr4 = (struct sockaddr_in*)addr;
		inet_ntop(addr->ss_family, &addr4->sin_addr, ipstr, 20);
		sprintf(ipstr + strlen(ipstr), ":%d", ntohs(addr4->sin_port));
	}
	else if (addr->ss_family == AF_INET6) {
		struct sockaddr_in6* addr6 = (struct sockaddr_in6*)addr;
		inet_ntop(addr->ss_family, &addr6->sin6_addr, ipstr, 50);
		JhModIpvXAddr(ipstr, AF_INET);
		sprintf(ipstr + strlen(ipstr), ":%d", ntohs(addr6->sin6_port));
	}
	return(ipstr);
}


//----------------------------------------------------------
// TCP PORTfmf listen 하는 함수이다.
// in 		--> port
// out 		--> none
// return 	--> socket
//----------------------------------------------------------
SOCKET CreateSocket(int port)
{
	SOCKET ld;
	struct sockaddr addr;
	struct sockaddr_in name;
	int rc, addrlen, optval = 1;

	if ((ld = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		JHLOG("Socket Open Fail");
		return(-1);
	}
	bzero(&name, sizeof name);
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	setsockopt(ld, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*)&optval, sizeof(optval));
	if (bind(ld, (struct sockaddr*)&name, sizeof(name)) < 0) {
		JHLOG("INET BIND FAIL. errno=%d, port=%d", errno, port);
		close(ld);
		return(-1);
	}
	addrlen = sizeof(addr);
	if ((rc = getsockname(ld, &addr, (socklen_t*)&addrlen)) < 0) {
		close(ld);
		JHLOG("getsocketname Fail Return Code=[%d], ErrNo=[%d]", rc, errno);
		return(-1);
	}

	if (listen(ld, 5) < 0) {
		close(ld);
		JHLOG("Socket Listen Fail ErrNo = [%d]", errno);
		return(-1);
	}
	return(ld);
}


//-----------------------------------------------------
// Packet을 생성하는 함수이다.
// in 		--> cmd, format
// out 		--> data
// return   --> none
//-----------------------------------------------------
void JhCreatePacket(char* data, char* cmd, char* format, ...)
{
	va_list args;
	char length[32];

	va_start(args, format);
	vsprintf(data + 8, format, args);
	va_end(args);

	sprintf(length, "%04d%-4.4s", (int)strlen(data + 8), cmd);
	memcpy(data, length, 8);
	return;
}


//-----------------------------------------------------
// Packet을 수신하는 함수이다.
// in 		--> tag, dsize, php
// out 		--> data
// return   --> length
//-----------------------------------------------------
int GetPacketData(char* tag, char* data, size_t dsize, CURRENTTHREAD php)
{
	int len, wtime;
	PROTOHEAD pkhd = (PROTOHEAD)data;

	len = sizeof(*pkhd);
	memset(data, 0x00, dsize);
	wtime = (php ? php->timeout : curmem->timeout) - JhGetTickCount();
	if (ReadChunkData(curmem->sock, data, len, wtime) != len) {
		JHLOG("%s, packet header read error.", tag);
		if (php != NULL) JhCreatePacket(php->request, "2001", "%s, packet header read error.", tag);
		return(-1);
	}
	if ((len = CNV4DT(pkhd->pk_size)) > 4096) {
		JHLOG("%s, packet data size error. length=%d", tag, len);
		if (php != NULL) JhCreatePacket(php->request, "2001", "%s, packet data read error.", tag);
		return(-1);
	}
	if (ReadChunkData(curmem->sock, data + sizeof(*pkhd), len, wtime) != len) {
		JHLOG("%s, packet data read error.", tag);
		if (php != NULL) JhCreatePacket(php->request, "2001", "%s, packet data read error.", tag);
		return(-1);
	}
	return(0);
}


//-----------------------------------------------------
// PHP의 작업 요청 자료를 Colab으로 전송한다.
// in 		--> data
// out 		--> none
// return   --> result
//-----------------------------------------------------
CURRENTTHREAD SendColab(char* data)
{
	CURRENTTHREAD colab;

	memset(curmem->request, 0x00, sizeof(curmem->request));
	if ((colab = SetColabWork(curmem, data)) == NULL) {
		JHLOG("SendColab,  channel 할당 오류입니다.");
		JhCreatePacket(curmem->request, "2001", "SendColab, channel 할당 오류입니다.");
		return(NULL);
	}
	JHLOG("php --> colab :: %s", data);
	return(colab);
}


//-----------------------------------------------------
// PHP 작업요청을 받는 함수이다.
// in 		--> parm
// out 		--> none
// 주의사항 --> timeout을 처리한다.
//-----------------------------------------------------
void* PhpWork(void* parm)
{
	int wtime, len;
	char data[8192];
	CURRENTTHREAD colab;

	curmem = (CURRENTTHREAD)parm;
	pthread_detach(pthread_self());
	curmem->timeout = JhGetTickCount() + (MAXWAIT * 1000);
	// STEP 1 : PHP packet read
	wtime = curmem->timeout - JhGetTickCount();
	memset(curmem->request, 0x00, sizeof(curmem->request));
	if (GetPacketData("PhpWork", data, sizeof(data), NULL) < 0) {
		JHLOG("PhpWork, packet data read error.");
		goto WorkEnd;
	}
	JHLOG("php 수신 :: %s", data);
	if (strlen(data) <= 8) {
		JHLOG("PhpWork, 자료 수신 오류입니다. close");
		goto WorkEnd;
	}
	// STEP 2 : send colab ...
	if ((colab = SendColab(data)) == NULL) {
		wtime = curmem->timeout - JhGetTickCount();
		JHLOG("php response :: %s", curmem->request);
		SendChunkData(curmem->sock, curmem->request, strlen(curmem->request), wtime);
		goto WorkEnd;
	}
	// STEP 3 : COLAB response wait.
	while (curmem->timeout >= JhGetTickCount()) {
		if (curmem->work == NULL) break;
		usleep(10 * 1000);
	}
	if (curmem->timeout < JhGetTickCount()) {
		colab->stat = 3; // close 요청
		JHLOG("COLAB으로 부터 응답을 받을 수 없습니다. timeout");
		usleep(1500 * 1000); // timeout시 1.5 second delay
		goto WorkEnd;

	}
	// STEP 4 : send php ...
	wtime = curmem->timeout - JhGetTickCount();
	JHLOG("php response :: %s", curmem->request);
	SendChunkData(curmem->sock, curmem->request, strlen(curmem->request), wtime);

WorkEnd:;
	usleep(10000);  // half-close를 막기위해 delay한다.
	close(curmem->sock);
	free(curmem);
	pthread_exit(NULL);
	return(NULL);
}


//-----------------------------------------------------
// COLAB 작업요청을 받는 함수이다.
// in 		--> parm
// out 		--> none
// 주의사항 --> 
//-----------------------------------------------------
void* ColabWork(void* parm)
{
	int len, wtime;
	char data[8192], cday[32];
	CURRENTTHREAD php = NULL;

	curmem = (CURRENTTHREAD)parm;
	pthread_detach(pthread_self());
	curmem->link_check = JhGetTickCount() + ((MAXWAIT - 5) * 1000);
	while (curmem->loop == 1 && curmem->sock > 0) {
		// STEP 1: socket close
		if (curmem->stat == 3) {
			JHLOG("%s, Socket close 요청 : %d", curmem->chead, curmem->sock);
			if (curmem->sock > 0) close(curmem->sock);
			curmem->sock = 0;
			goto FailWork;
		}
		// STEP 2: 작업요청
		if (curmem->work != NULL && php == NULL) {
			php = (CURRENTTHREAD)curmem->work;
			curmem->timeout = php->timeout;
			len = (int)strlen(curmem->request);
			JHLOG("%s send : %s", curmem->chead, curmem->request);
			if (SendChunkData(curmem->sock, curmem->request, len, MAXWAIT * 1000) != len) {
				JHLOG("%s, 전송오류 !! : %s", curmem->chead, curmem->request);
				JhCreatePacket(php->request, "2001", "Colab 전송중 오류가 발생 했습니다.");
				goto FailWork;
			}
		}
		// STEP 3: timeout check
		if (php != NULL && curmem->timeout < JhGetTickCount()) {
			JHLOG("%s, 작업 대기 시간이 초과 되었습니다.", curmem->chead);
			JhCreatePacket(php->request, "2001", "colab 작업 대기 시간이 초과 되었습니다.");
			if (curmem->sock > 0) close(curmem->sock);
			curmem->sock = 0;
			goto FailWork;
		}
		// STEP 4: link check
		if (php == NULL && curmem->sock > 0 && curmem->link_check < JhGetTickCount()) {
			curmem->timeout = JhGetTickCount() + (15 * 1000);
			JhCreatePacket(data, "0000", "%s", GetCurDay(cday));
			JHLOG("%s, send : %s", curmem->chead, data);
			len = strlen(data);
			if (SendChunkData(curmem->sock, data, len, MAXWAIT * 1000) != len) {
				JHLOG("%s, LINK CHECK 전송 오류 !! : %s", curmem->chead, data);
				if (curmem->sock > 0) close(curmem->sock);
				curmem->sock = 0;
				goto FailWork;
			}
		}
		// STEP 5: LINK check 응답 no response
		if (curmem->timeout > 0 && curmem->timeout < JhGetTickCount()) {
			JHLOG("%s, LINK CHECK 응답 오류 !! :: colab에서 응답이 없습니다.", curmem->chead);
			if (curmem->sock > 0) close(curmem->sock);
			curmem->sock = 0;
			goto FailWork;
		}
		// STEP 6: 요청결과
		switch (JhPoll(curmem->sock, 100)) {
		case 0: break;
		case 1:
			// LINK CHECK
			if (php == NULL) {
				if (GetPacketData(curmem->chead, data, sizeof(data), php) < 0) {
					close(curmem->sock);
					curmem->sock = 0;
					goto FailWork;
				}
				curmem->timeout = 0;
				break;
			}
			// 작업요청 packet을 수신한다.
			if (GetPacketData(curmem->chead, data, sizeof(data), php) < 0) {
				close(curmem->sock);
				curmem->sock = 0;
				goto FailWork;
			}
			// PHP Transfer
			JHLOG("%s --> php response : %s", curmem->chead, data);
			strcpy(php->request, data);
			php->work = NULL; // 작업완료 알림.
			curmem->stat = 1;
			memset(curmem->request, 0x00, sizeof(curmem->request));
			curmem->work = NULL;
			curmem->timeout = 0;
			php = NULL;
			break;
		case -1: // colab disconnect
			close(curmem->sock);
			curmem->sock = 0;
			if (php != NULL) JhCreatePacket(php->request, "2001", "연결이 종료 되었습니다");
			goto FailWork;
			break;
		}
		continue;

	FailWork:;
		if (php != NULL) php->work = NULL;
		php = NULL;
		curmem->stat = 1;
		memset(curmem->request, 0x00, sizeof(curmem->request));
		curmem->work = NULL;
		curmem->timeout = 0;
	}

	// 해당채널 CLOSE
	JHLOG("%s, Channel Close.", curmem->chead);
	if (curmem->chan > 0) colabptr[curmem->chan - 1] = NULL;
	if (curmem->sock > 0) close(curmem->sock);
	free(curmem);
	pthread_exit(NULL);
	return(NULL);
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
	JhTick tick;
	char ip_addr[128];
	struct pollfd pfd[2];
	SOCKET php_sd, colab_sd, sd;
	socklen_t addrLen;
	struct sockaddr_storage addr;
	CURRENTTHREAD cmem;

	if (argc < 2) {
		JHLOG("Usage :: app_gw parent_id");
		return(1);
	}

	// core signal install
	signal(SIGILL, vcSigCore);
	signal(SIGFPE, vcSigCore);
	signal(SIGBUS, vcSigCore);
	signal(SIGSEGV, vcSigCore);
	// normal signal install
	signal(SIGINT, vcDefaultSig);
	signal(SIGTERM, vcDefaultSig);
	signal(SIGQUIT, vcDefaultSig);
	signal(SIGUSR1, vcDefaultSig);
	signal(SIGUSR2, vcDefaultSig);
	signal(SIGPIPE, vcDefaultSig);
	// child process exit signal ignore
	signal(SIGCHLD, SIG_IGN);


	// socket init
	parent = atoi(argv[1]);
	php_sd = CreateSocket(8082);
	colab_sd = CreateSocket(8083);
	if (php_sd < 0 || colab_sd < 0) {
		JHLOG("Socket을 초기화 하지 못했습니다. exit");
		return(1);
	}
	memset(colabptr, 0x00, sizeof(colabptr));

	//-----------------------------------------------------------------------------------
	// php   :: N-session 으로 구성한다.
	// colab :: M-session 으로 구성한다.
	// data exchange protocol :: length(4) + command(4) + data(length)로 구성한다. 
	// data는 모든 정보를 ASCII 기준으로 작성하여 교환한다.
	//-----------------------------------------------------------------------------------

	while (1) {
		if (chkproc(parent) < 0) {
			JHLOG("parent process not found. exit.");
			return(1);
		}
		memset(pfd, 0x00, sizeof(pfd));
		pfd[0].fd = php_sd;
		pfd[0].events = POLLIN | POLLERR | POLLHUP;
		pfd[1].fd = colab_sd;
		pfd[1].events = POLLIN | POLLERR | POLLHUP;
		if (poll(pfd, 2, 1500) > 0) {
			/////////////////
			// php 연결 요청
			/////////////////
			if ((pfd[0].revents & POLLERR) == POLLERR || (pfd[0].revents & POLLHUP) == POLLHUP) {
				JHLOG("main, php socket error(exit). errno=%d", errno);
				return(1);
			}
			else if ((pfd[0].revents & POLLIN) == POLLIN) {
				// php 연결 요청(생성)
				addrLen = sizeof(addr);
				if ((sd = accept(php_sd, (struct sockaddr*)&addr, &addrLen)) < 0) {
					JHLOG("accept error. exit.");
					return(1);
				}
				JHLOG("php remote :: %s connect OK", JsGetIPv46InfoString(&addr, ip_addr));
				if ((cmem = (CURRENTTHREAD)malloc(sizeof(CurrentThread))) == NULL) {
					JHLOG("메모리 할당 오류입니다. exit.");
					return(1);
				}
				memset(cmem, 0x00, sizeof(CurrentThread));
				cmem->stat = 1;
				cmem->loop = 1;
				cmem->sock = sd;
				pthread_create(&cmem->tid, NULL, PhpWork, cmem);
			}
			///////////////////
			// colab 연결 요청
			///////////////////
			if ((pfd[1].revents & POLLERR) == POLLERR || (pfd[1].revents & POLLHUP) == POLLHUP) {
				JHLOG("main, colab socket error. exit. errno=%d", errno);
				return(1);
			}
			else if ((pfd[1].revents & POLLIN) == POLLIN) {
				// colab 연결 요청(생성)
				addrLen = sizeof(addr);
				if ((sd = accept(colab_sd, (struct sockaddr*)&addr, &addrLen)) < 0) {
					JHLOG("accept error. exit.");
					return(1);
				}
				JHLOG("colab remote :: %s connect OK", JsGetIPv46InfoString(&addr, ip_addr));
				if ((cmem = (CURRENTTHREAD)malloc(sizeof(CurrentThread))) == NULL) {
					JHLOG("메모리 할당 오류입니다. exit.");
					return(1);
				}
				memset(cmem, 0x00, sizeof(CurrentThread));
				cmem->stat = 1;
				cmem->loop = 1;
				cmem->sock = sd;
				if (SetColabChannel(cmem) < 0) {
					JHLOG("colab channel 할당 오류입니다. continue.");
					close(sd);
					free(cmem);
				}
				else {
					pthread_create(&cmem->tid, NULL, ColabWork, cmem);
				}
			}
		}
	}

	return(0);
}
