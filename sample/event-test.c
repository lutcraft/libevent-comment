/*
 * XXX This sample code was once meant to show how to use the basic Libevent
 * interfaces, but it never worked on non-Unix platforms, and some of the
 * interfaces have changed since it was first written.  It should probably
 * be removed or replaced with something better.
 *
 * Compile with:
 * cc -I/usr/local/include -o event-test event-test.c -L/usr/local/lib -levent
 */

#include <event2/event-config.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/queue.h>
#include <unistd.h>
#include <sys/time.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <event.h>

static void
fifo_read(evutil_socket_t fd, short event, void *arg)
{
	char buf[255];
	int len;
	struct event *ev = arg;
#ifdef WIN32
	DWORD dwBytesRead;
#endif

	/* Reschedule this event 循环触发*/
	event_add(ev, NULL);

	fprintf(stderr, "fifo_read called with fd: %d, event: %d, arg: %p\n",
	    (int)fd, event, arg);
#ifdef WIN32
	len = ReadFile((HANDLE)fd, buf, sizeof(buf) - 1, &dwBytesRead, NULL);

	/* Check for end of file. */
	if (len && dwBytesRead == 0) {
		fprintf(stderr, "End Of File");
		event_del(ev);
		return;
	}

	buf[dwBytesRead] = '\0';
#else
	len = read(fd, buf, sizeof(buf) - 1);

	if (len == -1) {
		perror("read -1");
		return;
	} else if (len == 0) {
		fprintf(stderr, "Connection closed\n");
		return;
	}

	buf[len] = '\0';
#endif
	fprintf(stdout, "Read: %s\n", buf);
}

int
main(int argc, char **argv)
{
	struct event evfifo;
#ifdef WIN32
	HANDLE socket;
	/* Open a file. */
	socket = CreateFileA("test.txt",	/* open File */
			GENERIC_READ,		/* open for reading */
			0,			/* do not share */
			NULL,			/* no security */
			OPEN_EXISTING,		/* existing file only */
			FILE_ATTRIBUTE_NORMAL,	/* normal file */
			NULL);			/* no attr. template */

	if (socket == INVALID_HANDLE_VALUE)
		return 1;

#else
	struct stat st;
	const char *fifo = "event.fifo";
	int socket;

	/**
	 * 获取文件或者符号链接的元数据
	 * lstat可以获取到文件类型，权限，大小，所有者等元数据信息
	 * st_size	大小
	 * st_mode	权限
	 * 
	 * 与stat不同，lstat不会对符号链接进行解引用，而是直接返回符号链接本身的信息
	 * 
	 * 注意lstat只能获取文件元数据，不能打开、读取或者写入文件内容，这是open read write的事
	 */
	if (lstat(fifo, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFREG) {
			errno = EEXIST;
			perror("lstat");
			exit(1);
		}
	}
	/**
	 * unlink用于删除文件，释放文件空间，无法恢复
	*/
	unlink(fifo);
	/**
	 * mkfifo用于创建命名管道，管道本质是文件，所以也有权限位
	 * 命名管道默认是阻塞的，即：在管道空时读会阻塞，在管道满时写也会阻塞， 所以要特别防止死锁。
	*/
	if (mkfifo(fifo, 0600) == -1) {
		perror("mkfifo");
		exit(1);
	}

	/* Linux pipes are broken, we need O_RDWR 读写模式 instead of O_RDONLY只读模式 */
#ifdef __linux
	socket = open(fifo, O_RDWR | O_NONBLOCK, 0);		//非阻塞的句柄，不能使用read
#else
	socket = open(fifo, O_RDONLY | O_NONBLOCK, 0);
#endif

	//对文件句柄打开socket
	if (socket == -1) {
		perror("open");
		exit(1);
	}

	fprintf(stderr, "Write data to %s\n", fifo);
#endif
	/* Initalize the event library
		初始化reactor实例 */
	event_init();

	/* Initalize one event */
#ifdef WIN32
	event_set(&evfifo, (evutil_socket_t)socket, EV_READ, fifo_read, &evfifo);
#else
	//创建event事件处理器对象，监控socket事件，event内部存有reactor对象的引用，此接口的reactor对象是current-reactor

	//所有新创建的事件都处于已初始化和非未决状态 ,调用 event_add()可以使其成为未决的。
	event_set(&evfifo, socket, EV_READ, fifo_read, &evfifo);
	/// 	event对象句柄
#endif

	/* Add it to the active events, without a timeout 
	立即将本事件处理器添加到自身reactor对象的激活事件队列*/
	//在非未决的事件上调用 event_add()将使其在配置的 event_base 中成为未决的。成功时 函数返回0,失败时返回-1。
	//如果 tv 为 NULL,添加的事件不会超时。否则, tv 以秒和微秒指定超时值。
	event_add(&evfifo, NULL);

	//事件分发，没有指定用哪个reactor，因为这个函数会使用current的reactor进行分发
	event_dispatch();
#ifdef WIN32
	CloseHandle(socket);
#endif
	return (0);
}

