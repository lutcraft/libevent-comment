/*
 * Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT_INTERNAL_H_
#define _EVENT_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event2/event-config.h"
#include <time.h>
#include <sys/queue.h>
#include "event2/event_struct.h"
#include "minheap-internal.h"
#include "evsignal-internal.h"
#include "mm-internal.h"
#include "defer-internal.h"

/* map union members back */

/* mutually exclusive */
#define ev_signal_next	_ev.ev_signal.ev_signal_next
#define ev_io_next	_ev.ev_io.ev_io_next
#define ev_io_timeout	_ev.ev_io.ev_timeout

/* used only by signals */
#define ev_ncalls	_ev.ev_signal.ev_ncalls
#define ev_pncalls	_ev.ev_signal.ev_pncalls

/* Possible values for ev_closure in struct event. 
	event结构中ev_closure的可能值。
*/
#define EV_CLOSURE_NONE 0
#define EV_CLOSURE_SIGNAL 1
#define EV_CLOSURE_PERSIST 2

/** Structure to define the backend of a given event_base.
 * 后端往往指与底层操作系统交互的事件循环机制的 实现 。它负责管理
 * I/O 操作、计时器和其他异步事件。
 * 后端处理特定操作系统环境中如何监视和触发事件的详细信息，
 * 使库能够跨不同平台高效工作并处理各种事件驱动的任务。
 * 
 * 
 * eventop结构体代表了事件循环机制在操作系统环境中特定的实现方式。
 * 它包含了一组函数指针，这些函数指针定义了如何监视和处理事件，
 * 以及如何与底层操作系统。通过适当的“eventop”结构体实现交互，
 * “libevent”能够在不同的操作系统上高效地管理事件和异步操作。
 * 
 */
struct eventop {
	/** The name of this backend. 后端名*/
	const char *name;
	/** Function to set up an event_base to use this backend.  It should
	 * create a new structure holding whatever information is needed to
	 * run the backend, and return it.  The returned pointer will get
	 * stored by event_init into the event_base.evbase field.  On failure,
	 * this function should return NULL.
	 * 
	 * 后端初始化方法，它应该创建一个新的结构体实例，包含运行后端所需的任何信息，并返回它。
	 * 返回的指针将通过event_init存储到event_base.evbase字段中。失败时，此函数应返回NULL。
	 */
	void *(*init)(struct event_base *);
	/** Enable reading/writing on a given fd or signal.  'events' will be
	 * the events that we're trying to enable: one or more of EV_READ,
	 * EV_WRITE, EV_SIGNAL, and EV_ET.  'old' will be those events that
	 * were enabled on this fd previously.  'fdinfo' will be a structure
	 * associated with the fd by the evmap; its size is defined by the
	 * fdinfo field below.  It will be set to 0 the first time the fd is
	 * added.  The function should return 0 on success and -1 on error.
	 * 
	 * 启用对给定fd或信号的读取/写入。
	 * 'events“将是我们试图启用的事件：EV_READ、EV_WRITE、EV_SIGNAL和EV_ET中的一个或多个。
	 * “old”将是以前在此fd上启用的那些事件。
	 * “fdinfo”将是evmap与fd关联的结构,其大小由下面的fdinfo字段定义。第一次添加fd时，它将被设置为0。
	 * 函数应在成功时返回0，在出错时返回-1。
	 * 
	 */
	int (*add)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
	/** As "add", except 'events' contains the events we mean to disable. 
	 * 和“添加”类似，除了“events”是我们想要禁用的事件。
	*/
	int (*del)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
	/** Function to implement the core of an event loop.  It must see which
	    added events are ready, and cause event_active to be called for each
	    active event (usually via event_io_active or such).  It should
	    return 0 on success and -1 on error.
		event loop的实现。
		它必须查看哪些添加的事件已准备就绪，并为每个活动事件调用event_active回调（通常通过event_io_active等）。
		成功时应返回0，出错时应返回-1。
	 */
	int (*dispatch)(struct event_base *, struct timeval *);
	/** Function to clean up and free our data from the event_base. */
	void (*dealloc)(struct event_base *);
	/** Flag: set if we need to reinitialize the event base after we fork.
	 * 标识进程fork后是否需要重新实例化后端，epoll是需要重新实例化
	 */
	int need_reinit;
	/** Bit-array of supported event_method_features that this backend can
	 * provide.
	 * 此后端可以提供的受支持的 event_method_features 的位数组。
	 */
	enum event_method_feature features;
	/** Length of the extra information we should record for each fd that
	    has one or more active events.  This information is recorded
	    as part of the evmap entry for each fd, and passed as an argument
	    to the add and del functions above.
		fd记录的额外信息的长度。
		这些信息被记录为每个fd的 evmap 条目的一部分，并作为参数传递给上面的add和del函数。
	 */
	size_t fdinfo_len;
};

#ifdef WIN32
/* If we're on win32, then file descriptors are not nice low densely packed
   integers.  Instead, they are pointer-like windows handles, and we want to
   use a hashtable instead of an array to map fds to events.
*/
#define EVMAP_USE_HT
#endif

/* #define HT_CACHE_HASH_VALS */

#ifdef EVMAP_USE_HT
#include "ht-internal.h"
struct event_map_entry;
HT_HEAD(event_io_map, event_map_entry);
#else
#define event_io_map event_signal_map
#endif

/* Used to map signal numbers to a list of events.  If EVMAP_USE_HT is not
   defined, this structure is also used as event_io_map, which maps fds to a
   list of events.
*/
struct event_signal_map {
	/* An array of evmap_io * or of evmap_signal *; empty entries are
	 * set to NULL.
	 evmap_io * 或 evmap_signal*的数组；空条目设置为NULL。
	 */
	void **entries;		//每一个都是一个 evmap_signal + fdinfo 的指针 ，evmap_signal是一个链表		(sizeof(struct evmap_io) + evsel->fdinfo_len)
	/* The number of entries available in entries */
	int nentries;
};

/* A list of events waiting on a given 'common' timeout value.  Ordinarily,
 * events waiting for a timeout wait on a minheap.  Sometimes, however, a
 * queue can be faster.
 **/
struct common_timeout_list {
	/* List of events currently waiting in the queue. */
	struct event_list events;
	/* 'magic' timeval used to indicate the duration of events in this
	 * queue. */
	struct timeval duration;
	/* Event that triggers whenever one of the events in the queue is
	 * ready to activate */
	struct event timeout_event;
	/* The event_base that this timeout list is part of */
	struct event_base *base;
};

/** Mask used to get the real tv_usec value from a common timeout. */
#define COMMON_TIMEOUT_MICROSECONDS_MASK       0x000fffff

struct event_change;

/* List of 'changes' since the last call to eventop.dispatch.  Only maintained
 * if the backend is using changesets.
 自上次调用eventop.dispatch以来的“更改”列表。仅当后端使用changesets时才生效。
  */
struct event_changelist {
	struct event_change *changes;
	int n_changes;
	int changes_size;
};

#ifndef _EVENT_DISABLE_DEBUG_MODE
/* Global internal flag: set to one if debug mode is on. */
extern int _event_debug_mode_on;
#define EVENT_DEBUG_MODE_IS_ON() (_event_debug_mode_on)
#else
#define EVENT_DEBUG_MODE_IS_ON() (0)
#endif

struct event_base {
	/** Function pointers and other data to describe this event_base's
	 * backend.
	 * 指向一种后端的实现
	 * 函数指针和其他用于描述此后端的数据。
	 * 
	 */
	const struct eventop *evsel;

	/** Pointer to backend-specific data.
	 * 指向每一个初始化好的后端实例的数据信息，是一个epollop
	 */
	void *evbase;	//epolllop存储了epoll_wait的所需信息

	/** List of changes to tell backend about at next dispatch.  Only used
	 * by the O(1) backends.
	 * 
	 * 在下一次事件分发时，告知后端的change列表。只能用O（1）复杂的后端否则影响性能。
	 */
	struct event_changelist changelist;

	/** Function pointers used to describe the backend that this event_base
	 * uses for signals */
	const struct eventop *evsigsel;
	/** Data to implement the common signal handelr code. 实现通用信号handel 处理的代码和数据 */
	struct evsig_info sig;

	/** Number of virtual events */
	int virtual_event_count;
	/** Number of total events added to this event_base
	 * 添加到此event_base的事件总数
	 */
	int event_count;
	/** Number of total events active in this event_base
	 * 此event_base中活动的事件总数
	 */
	int event_count_active;

	/** Set if we should terminate the loop once we're done processing
	 * events. */
	int event_gotterm;
	/** Set if we should terminate the loop immediately */
	int event_break;
	/** Set if we should start a new instance of the loop immediately.
	 * 设置是否应立即启动一个event-loop的新实例
	 * 
	 */
	int event_continue;

	/** The currently running priority of events
	 * 当前正在运行的事件优先级
	 */
	int event_running_priority;

	/** Set if we're running the event_base_loop function, to prevent
	 * reentrant invocation.
	 * 
	 * 如果我们正在运行event_base_loop函数，请进行设置，以防止重新进入调用。
	 *  */
	int running_loop;

	/* Active event management. 活动事件管理 */
	/** An array of nactivequeues queues for active events (ones that
	 * have triggered, and whose callbacks need to be called).  Low
	 * priority numbers are more important, and stall higher ones.
	 * 激活事件的队列（已触发且需要调用其回调的事件）。根据优先级不同，同一个后端对象拥有多个活动事件队列
	 * 优先级越低，事件越重要，可能会拖慢更高优先级的事件
	 */
	struct event_list *activequeues;
	/** The length of the activequeues array 活动事件的队列长度*/
	int nactivequeues;

	/* common timeout logic */

	/** An array of common_timeout_list* for all of the common timeout
	 * values we know. */
	struct common_timeout_list **common_timeout_queues;
	/** The number of entries used in common_timeout_queues
	 * 通用定时器 common_timeout_queues 队列中的条目数
	 */
	int n_common_timeouts;
	/** The total size of common_timeout_queues. */
	int n_common_timeouts_allocated;

	/** List of defered_cb that are active.  We run these after the active
	 * events. 
	 * 活动延迟回调 list，在运行活动时间回调后之后运行，类似后处理
	 * */
	struct deferred_cb_queue defer_queue;

	/** Mapping from file descriptors to enabled (added) events 
	 * 从 文件描述符 到 激活（added）事件的映射
	*/
	struct event_io_map io;

	/** Mapping from signal numbers to enabled (added) events.
	 * 从 信号编号 到 激活（added）事件的映射。
	 */
	struct event_signal_map sigmap;

	/** All events that have been enabled (added) in this event_base
	 * 注册事件队列，所有被add的事件，将会被加到这里
	 */
	struct event_list eventqueue;

	/** 存储时间值；用于检测时间何时倒退。 */
	struct timeval event_tv;

	/** 事件优先队列.事件支持超时  */
	struct min_heap timeheap;

	/** 缓存 timeval: 避免过于频繁地调用 gettimeofday/clock_gettime。*/
	struct timeval tv_cache;

#if defined(_EVENT_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	/** Difference between internal time (maybe from clock_gettime) and
	 * gettimeofday. */
	struct timeval tv_clock_diff;
	/** Second in which we last updated tv_clock_diff, in monotonic time. */
	time_t last_updated_clock_diff;
#endif

#ifndef _EVENT_DISABLE_THREAD_SUPPORT
	/* threading support */
	/** The thread currently running the event_loop for this base
	 * 当前运行event_loop的线程id
	 */
	unsigned long th_owner_id;
	/** A lock to prevent conflicting accesses to this event_base
	 * 用于防止对此event_base进行冲突访问的锁
	 */
	void *th_base_lock;
	/** The event whose callback is executing right now 
	 * 后端当前正在处理的ev
	*/
	struct event *current_event;
	/** A condition that gets signalled when we're done processing an
	 * event with waiters on it.
	 * 当我们处理完一个有 current_event_waiters 的事件时，符合cond会发出信号通知。
	 *  */
	void *current_event_cond;
	/** Number of threads blocking on current_event_cond. 
	 * block在 current_event_cond 上的线程数
	*/
	int current_event_waiters;
#endif

#ifdef WIN32
	/** IOCP support structure, if IOCP is enabled. */
	struct event_iocp_port *iocp;
#endif

	/** Flags that this base was configured with
	 * 后端配置flag
	 */
	enum event_base_config_flag flags;

	/* Notify main thread to wake up break, etc. */
	/** True if the base already has a pending notify, and we don't need
	 * to add any more.
	 * 如果base已经有一个挂起的notify，并且我们不需要再添加任何通知，则为True。
	 */
	int is_notify_pending;
	/** A socketpair used by some th_notify functions to wake up the main
	 * thread. 在 th_notify_fn 中可能会使用的套接字对，用来唤醒主线程。 */
	evutil_socket_t th_notify_fd[2];
	/** An event used by some th_notify functions to wake up the main
	 * thread. */
	struct event th_notify;
	/** A function used to wake up the main thread from another thread.、
	 * 用于从另一个线程唤醒主线程的函数
	 */
	int (*th_notify_fn)(struct event_base *base);
};

struct event_config_entry {
	TAILQ_ENTRY(event_config_entry) next;

	const char *avoid_method;
};

/** Internal structure: describes the configuration we want for an event_base
 * that we're about to allocate. */
struct event_config {
	TAILQ_HEAD(event_configq, event_config_entry) entries;

	int n_cpus_hint;
	enum event_method_feature require_features;
	enum event_base_config_flag flags;
};

/* Internal use only: Functions that might be missing from <sys/queue.h> */
#if defined(_EVENT_HAVE_SYS_QUEUE_H) && !defined(_EVENT_HAVE_TAILQFOREACH)
#ifndef TAILQ_FIRST
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#endif
#ifndef TAILQ_END
#define	TAILQ_END(head)			NULL
#endif
#ifndef TAILQ_NEXT
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#endif

#ifndef TAILQ_FOREACH
#define TAILQ_FOREACH(var, head, field)					\
	for ((var) = TAILQ_FIRST(head);					\
	     (var) != TAILQ_END(head);					\
	     (var) = TAILQ_NEXT(var, field))
#endif

#ifndef TAILQ_INSERT_BEFORE
#define	TAILQ_INSERT_BEFORE(listelm, elm, field) do {			\
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
	(elm)->field.tqe_next = (listelm);				\
	*(listelm)->field.tqe_prev = (elm);				\
	(listelm)->field.tqe_prev = &(elm)->field.tqe_next;		\
} while (0)
#endif
#endif /* TAILQ_FOREACH */

#define N_ACTIVE_CALLBACKS(base)					\
	((base)->event_count_active + (base)->defer_queue.active_count)

int _evsig_set_handler(struct event_base *base, int evsignal,
			  void (*fn)(int));
int _evsig_restore_handler(struct event_base *base, int evsignal);


void event_active_nolock(struct event *ev, int res, short count);

/* FIXME document. */
void event_base_add_virtual(struct event_base *base);
void event_base_del_virtual(struct event_base *base);

/** For debugging: unless assertions are disabled, verify the referential
    integrity of the internal data structures of 'base'.  This operation can
    be expensive.

    Returns on success; aborts on failure.
*/
void event_base_assert_ok(struct event_base *base);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_INTERNAL_H_ */

