#include<windows.h>
#include<iostream>
#include<string>
#include<time.h>

#define BufAmount 6
#define BufLen 10
#define P(S) WaitForSingleObject(S,INFINITE);
#define V(S) ReleaseSemaphore(S,1,NULL);

static LPCTSTR filemapping_name = "FileMapping";
HANDLE ProcessHandle[5];
using namespace std;

struct BUF {
	char array[BufAmount][BufLen];
	int head;
	int tail;
	int IsEmpty;
};

//返回随机休眠时间
int GetRandomSleep() {
	return (rand() + GetCurrentProcessId())%100 + 1000;
}

//返回随机字符
char GetRandomChar() {
	return ((rand() + GetCurrentProcessId()) % 26 + 'A');
}

int GetRandomNum() {
	return ((rand() + GetCurrentProcessId()) % 9 + 1);
}

///创建子进程 
void StartClone(const int id) {
	TCHAR szFilename[MAX_PATH];
	GetModuleFileName(NULL, szFilename, MAX_PATH);
	
	TCHAR szcmdLine[MAX_PATH];
	sprintf(szcmdLine, "\"%s\" %d", szFilename, id);
	
	STARTUPINFO si;
	ZeroMemory(reinterpret_cast<void*>(&si), sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	
	BOOL bCreateOK = CreateProcess(
		szFilename,
		szcmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_DEFAULT_ERROR_MODE,
		NULL,
		NULL,
		&si,
		&pi);
	//通过返回的hProcess来关闭进程
	if (bCreateOK) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		ProcessHandle[id] = pi.hProcess;
	}
	else {
		printf("child process error!\n");
		exit(0);
	}
}

///父进程程序 
void ParentProc() {
	//创建信号量 
	HANDLE MUTEX = CreateSemaphore(NULL, 1, 1, "mymutex");
	HANDLE EMPTY = CreateSemaphore(NULL, 3, 3, "myempty");
	HANDLE FULL = CreateSemaphore(NULL, 0, 3, "myfull");
	
	//创建内存映射
	HANDLE hMapping = CreateFileMapping(
		NULL, 
		NULL, 
		PAGE_READWRITE, 
		0, 
		sizeof(struct BUF), 
		filemapping_name);
	if (hMapping != INVALID_HANDLE_VALUE) {
		LPVOID pfile = MapViewOfFile(
			hMapping, 
			FILE_MAP_ALL_ACCESS, 
			0, 
			0, 
			0);
		if (pfile != NULL) {
			ZeroMemory(pfile, sizeof(struct BUF));
		}
		//初始化
		struct BUF* pbuf = reinterpret_cast<struct BUF*>(pfile);
		pbuf->head = 0;
		pbuf->tail = 0;
		pbuf->IsEmpty = 0;
		memset(pbuf->array,0,sizeof(pbuf->array));
		//解除映射
		UnmapViewOfFile(pfile);
		pfile = NULL;
		CloseHandle(pfile);
	}
	//生产者
	for (int i = 0; i < 2; i++) {
		StartClone(i);
	}
	//消费者
	for (int i = 2; i < 5; i++) {
		StartClone(i);
	}
	WaitForMultipleObjects(5, ProcessHandle, TRUE, INFINITE);
	//回收信号量 
	CloseHandle(EMPTY);
	CloseHandle(FULL);
	CloseHandle(MUTEX);
}

///生产者 
void Producer() {
	//打开信号量
	//HANDLE MUTEX = CreateMutex(NULL,FALSE,"mymutex");
	HANDLE MUTEX = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "mymutex");
	HANDLE EMPTY = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "myempty");
	HANDLE FULL = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "myfull");
	
	//打开共享内存区，并加载到当前进程地址空间
	HANDLE hmap = OpenFileMapping(
		FILE_MAP_ALL_ACCESS, 
		FALSE, 
		filemapping_name);
	LPVOID pfile = MapViewOfFile(
		hmap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);
	struct BUF* pbuf = reinterpret_cast<struct BUF*>(pfile);
	
	//12是生产次数
	for (int i = 0; i < 12; i++) {
		P(EMPTY);
//		//sleep
		Sleep(GetRandomSleep());
		P(MUTEX);
		
		char charray[BufLen];
		int num = GetRandomNum();
		memset(charray,0,sizeof(charray));
		for(int i = 0; i < num; i++){
			charray[i] = GetRandomChar();
		}
		
		strcpy(pbuf->array[pbuf->tail],charray);//存数据 
		pbuf->tail = (pbuf->tail + 1) % BufAmount;//修改指针 
		pbuf->IsEmpty = 1;//修改状态 
		time_t t=time(NULL);
		struct tm* ptm = localtime(&t);
		//GetSystemTime(&tm);
		printf("\nProducerID:%6d, puts data:%-10s\tbuffer 1:%-10s\t 2:%-10s\t 3:%-10s\t 4:%-10s\t 5:%-10s\t 6:%-10s\t %d时%d分%d秒",
			(int)GetCurrentProcessId(),
			charray,
			pbuf->array[0], 
			pbuf->array[1], 
			pbuf->array[2],
			pbuf->array[3],
			pbuf->array[4], 
			pbuf->array[5],
			ptm->tm_hour,
			ptm->tm_min,
			ptm->tm_sec);
		fflush(stdout);
		V(MUTEX);//释放缓冲区使用权 
		V(FULL);
	}
	//解除映射
	UnmapViewOfFile(pfile);
	pfile = NULL;
	CloseHandle(pfile);
	//关闭信号量
	CloseHandle(MUTEX);
	CloseHandle(EMPTY);
	CloseHandle(FULL);
}

///消费者 
void Customer() {
	//打开信号量
	HANDLE MUTEX = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "mymutex");
	//HANDLE MUTEX = CreateMutex(NULL,FALSE,"mymutex");
	HANDLE EMPTY = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "myempty");
	HANDLE FULL = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "myfull");
	
	//打开共享内存区，并加载到当前进程地址空间
	HANDLE hmap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filemapping_name);
	LPVOID pfile = MapViewOfFile(
		hmap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);
	struct BUF* pbuf = reinterpret_cast<struct BUF*>(pfile);
	
	//读数据(取产品)
	for (int i = 0; i < 8; i++) {
		P(FULL);
		//sleep
		Sleep(GetRandomSleep());
		P(MUTEX);
		char charray[10];
		memset(charray, '\0', sizeof(charray));
		strcpy(charray,pbuf->array[pbuf->head]);
//		pbuf->array[pbuf->head] = ' ';//将缓冲置空 
		memset(pbuf->array[pbuf->head],'\0',sizeof(pbuf->array[pbuf->head]));
		pbuf->head = (pbuf->head + 1) % BufAmount;//修改指针 
		pbuf->IsEmpty = (pbuf->head == pbuf->tail);//修改状态 
		time_t t=time(NULL);
		struct tm* ptm = localtime(&t);
		printf("\nCustomerID:%6d, gets data:%-10s\tbuffer 1:%-10s\t 2:%-10s\t 3:%-10s\t 4:%-10s\t 5:%-10s\t 6:%-10s\t %d时%d分%d秒",
			(int)GetCurrentProcessId(),
			charray,
			pbuf->array[0], 
			pbuf->array[1], 
			pbuf->array[2],
			pbuf->array[3], 
			pbuf->array[4], 
			pbuf->array[5],
			ptm->tm_hour,
			ptm->tm_min,
			ptm->tm_sec);
		fflush(stdout);
		V(EMPTY);
		//ReleaseMutex(MUTEX);
		V(MUTEX);//释放缓冲区使用权 
	}
	//解除映射
	UnmapViewOfFile(pfile);
	pfile = NULL;
	CloseHandle(pfile);
	//关闭信号量
	CloseHandle(MUTEX);
	CloseHandle(EMPTY);
	CloseHandle(FULL);
}

int main(int argc, char* argv[]) {
	if (argc > 1) {
		int id = atoi(argv[1]);
		if (id < 0) {
			printf("maybe child process error!\n");
			exit(-1);
		}
		else if (id < 2) {
			Producer();
		}
		else if (id < 5) {
			Customer();
		}
	}
	else {
		ParentProc();
	}
	return 0;
}
