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

#define MAXCOLAB			32			// colab client ���� �ִ� �Ǽ�
#define MAXWAIT				60
#define ThreadStl			__thread	// thread ���� �޸𸮸� �����Ѵ�.
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
	int			stat;			// ������� --> 0:empty 1:wait 2:run 3:close
	int 		loop;			// thread loop
	SOCKET 		sock;			// current work socket
	char		chead[32];		// colab header
	pthread_t	tid;			// current thread id
	JhTick		link_check;		// link check �ð��� �����Ѵ�.
	JhTick		timeout;		// work timeout (PHP : MAXWAIT * 1000)
	char		request[8192];	// �۾���û
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
// ����ð��� ���ϴ� �Լ��̴�.
// in 		--> day
// out 		--> none
// return 	--> ����ð�
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
	printf("APP_GW (%04d,%s):: %s\n", line, GetCurTime(day), msg);
	return;
}

/*-----------------------------------------------------------------------------
// 1/1000���� ���� time�� ���ϴ� �Ա��̴�.
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



//-----------------------------------------------------
// PHP �۾���û�� �޴� �Լ��̴�.
// in 		--> parm
// out 		--> none
// ���ǻ��� --> timeout�� ó���Ѵ�.
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
	JHLOG("php ���� :: %s", data);
	if (strlen(data) <= 8) {
		JHLOG("PhpWork, �ڷ� ���� �����Դϴ�. close");
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
		colab->stat = 3; // close ��û
		JHLOG("COLAB���� ���� ������ ���� �� �����ϴ�. timeout");
		usleep(1500 * 1000); // timeout�� 1.5 second delay
		goto WorkEnd;

	}
	// STEP 4 : send php ...
	wtime = curmem->timeout - JhGetTickCount();
	JHLOG("php response :: %s", curmem->request);
	SendChunkData(curmem->sock, curmem->request, strlen(curmem->request), wtime);

WorkEnd:;
	usleep(10000);  // half-close�� �������� delay�Ѵ�.
	close(curmem->sock);
	free(curmem);
	pthread_exit(NULL);
	return(NULL);
}


//-----------------------------------------------------
// COLAB �۾���û�� �޴� �Լ��̴�.
// in 		--> parm
// out 		--> none
// ���ǻ��� --> 
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
			JHLOG("%s, Socket close ��û : %d", curmem->chead, curmem->sock);
			if (curmem->sock > 0) close(curmem->sock);
			curmem->sock = 0;
			goto FailWork;
		}
		// STEP 2: �۾���û
		if (curmem->work != NULL && php == NULL) {
			php = (CURRENTTHREAD)curmem->work;
			curmem->timeout = php->timeout;
			len = (int)strlen(curmem->request);
			JHLOG("%s send : %s", curmem->chead, curmem->request);
			if (SendChunkData(curmem->sock, curmem->request, len, MAXWAIT * 1000) != len) {
				JHLOG("%s, ���ۿ��� !! : %s", curmem->chead, curmem->request);
				JhCreatePacket(php->request, "2001", "Colab ������ ������ �߻� �߽��ϴ�.");
				goto FailWork;
			}
		}
		// STEP 3: timeout check
		if (php != NULL && curmem->timeout < JhGetTickCount()) {
			JHLOG("%s, �۾� ��� �ð��� �ʰ� �Ǿ����ϴ�.", curmem->chead);
			JhCreatePacket(php->request, "2001", "colab �۾� ��� �ð��� �ʰ� �Ǿ����ϴ�.");
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
				JHLOG("%s, LINK CHECK ���� ���� !! : %s", curmem->chead, data);
				if (curmem->sock > 0) close(curmem->sock);
				curmem->sock = 0;
				goto FailWork;
			}
		}
		// STEP 5: LINK check ���� no response
		if (curmem->timeout > 0 && curmem->timeout < JhGetTickCount()) {
			JHLOG("%s, LINK CHECK ���� ���� !! :: colab���� ������ �����ϴ�.", curmem->chead);
			if (curmem->sock > 0) close(curmem->sock);
			curmem->sock = 0;
			goto FailWork;
		}
		// STEP 6: ��û���
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
			// �۾���û packet�� �����Ѵ�.
			if (GetPacketData(curmem->chead, data, sizeof(data), php) < 0) {
				close(curmem->sock);
				curmem->sock = 0;
				goto FailWork;
			}
			// PHP Transfer
			JHLOG("%s --> php response : %s", curmem->chead, data);
			strcpy(php->request, data);
			php->work = NULL; // �۾��Ϸ� �˸�.
			curmem->stat = 1;
			memset(curmem->request, 0x00, sizeof(curmem->request));
			curmem->work = NULL;
			curmem->timeout = 0;
			php = NULL;
			break;
		case -1: // colab disconnect
			close(curmem->sock);
			curmem->sock = 0;
			if (php != NULL) JhCreatePacket(php->request, "2001", "������ ���� �Ǿ����ϴ�");
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

	// �ش�ä�� CLOSE
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
		JHLOG("Socket�� �ʱ�ȭ ���� ���߽��ϴ�. exit");
		return(1);
	}
	memset(colabptr, 0x00, sizeof(colabptr));

	//-----------------------------------------------------------------------------------
	// php   :: N-session ���� �����Ѵ�.
	// colab :: M-session ���� �����Ѵ�.
	// data exchange protocol :: length(4) + command(4) + data(length)�� �����Ѵ�. 
	// data�� ��� ������ ASCII �������� �ۼ��Ͽ� ��ȯ�Ѵ�.
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
			// php ���� ��û
			/////////////////
			if ((pfd[0].revents & POLLERR) == POLLERR || (pfd[0].revents & POLLHUP) == POLLHUP) {
				JHLOG("main, php socket error(exit). errno=%d", errno);
				return(1);
			}
			else if ((pfd[0].revents & POLLIN) == POLLIN) {
				// php ���� ��û(����)
				addrLen = sizeof(addr);
				if ((sd = accept(php_sd, (struct sockaddr*)&addr, &addrLen)) < 0) {
					JHLOG("accept error. exit.");
					return(1);
				}
				JHLOG("php remote :: %s connect OK", JsGetIPv46InfoString(&addr, ip_addr));
				if ((cmem = (CURRENTTHREAD)malloc(sizeof(CurrentThread))) == NULL) {
					JHLOG("�޸� �Ҵ� �����Դϴ�. exit.");
					return(1);
				}
				memset(cmem, 0x00, sizeof(CurrentThread));
				cmem->stat = 1;
				cmem->loop = 1;
				cmem->sock = sd;
				pthread_create(&cmem->tid, NULL, PhpWork, cmem);
			}
			///////////////////
			// colab ���� ��û
			///////////////////
			if ((pfd[1].revents & POLLERR) == POLLERR || (pfd[1].revents & POLLHUP) == POLLHUP) {
				JHLOG("main, colab socket error. exit. errno=%d", errno);
				return(1);
			}
			else if ((pfd[1].revents & POLLIN) == POLLIN) {
				// colab ���� ��û(����)
				addrLen = sizeof(addr);
				if ((sd = accept(colab_sd, (struct sockaddr*)&addr, &addrLen)) < 0) {
					JHLOG("accept error. exit.");
					return(1);
				}
				JHLOG("colab remote :: %s connect OK", JsGetIPv46InfoString(&addr, ip_addr));
				if ((cmem = (CURRENTTHREAD)malloc(sizeof(CurrentThread))) == NULL) {
					JHLOG("�޸� �Ҵ� �����Դϴ�. exit.");
					return(1);
				}
				memset(cmem, 0x00, sizeof(CurrentThread));
				cmem->stat = 1;
				cmem->loop = 1;
				cmem->sock = sd;
				if (SetColabChannel(cmem) < 0) {
					JHLOG("colab channel �Ҵ� �����Դϴ�. continue.");
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
