#ifndef __EVENT_CTL_H__
#define __EVENT_CTL_H__
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>

#ifndef _TIME_H
#define _TIME_H
struct timespec
{
	__time_t tv_sec;            /* Seconds.  */
	long int tv_nsec;           /* Nanoseconds.  */
};

struct itimerspec
{
	struct timespec it_interval;
	struct timespec it_value;
};
#endif

#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H
typedef union epoll_data
{
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
}epoll_data_t;

struct epoll_event
{
	uint32_t events;      /* Epoll events */
	epoll_data_t data;    /* User data variable */
};
#endif

#define EVENT_TYPE_UNUSED		1
#define EVENT_TYPE_TIMER		2
#define EVENT_TYPE_REAR			3
#define EVENT_TYPE_EVENT		4
#define EVENT_TYPE_IMMEDIATE 	5	/*由于该事件发生时，直接添加到线程池，所有不使用*/
#define EVENT_TYPE_READY		6	

typedef unsigned int event_type;

typedef struct
{
	struct hash *hash;
}event_hash;


/*Link list of event node*/
typedef struct
{
	struct event_node *head;	/**/
	struct event_node *tail;
	int length;
}event_list;


/*Master of the event*/
#define EVENT_HASH_TABLE_SIZE		2048

struct event_timer
{
	int timerfd;					/*file description for set timer*/
	struct event_node *event;		/*pointer to the event node. when event is null, no timer is set.*/
};

struct event_master
{
	int epollfd;					/*epoll file description*/
	int expected_size;				/*the count of monitoring expectly fd description*/
	struct epoll_event *events;		/*events for returing*/
	int maxevents;					/*the max size of returing events*/
	unsigned long events_alloc;		/*the count of the event allocation*/
	/*
		之前因线程池的原因，epoll自带定时器被设置后，无法被取消，所以采用timerfd的方式设置定时器
	*/
	struct event_timer event_timer;	/*the structure of event timer*/
	event_list timer;				/*the link list of the timer event*/
	event_list rear;				/*the link list of the rear event*/
	event_list unuse;				/*the link list of the unuse event*/
	event_hash event;				/*the hash list of the epoll event*/
	event_list ready;				/*the link list of the ready event*/
};
/*event itself*/
struct event_node
{
	struct event_node *next;	/*next pointer of struct event_node*/
	struct event_node *prev;	/*previous pointer*/
	event_type type;			/*event type*/
	event_type add_type;		/*last event type*/
	void (*func)(struct event_node *);		/*function*/
	uint32_t events;      /* Epoll events that event happened */
	void *arg;					/*event argument*/
	int fd;					/*file description*/
	struct epoll_event listen_event;/*file description's listen event*/
	struct itimerspec time;		/*timeout*/
	struct event_master *master;/*pointer to event master*/
};

#define EVENT_ARG(X) ((X)->arg)
#define EVENT_FD(X) ((X)->fd)

#define EVENT_UNSET_WRITE(X) (((X)->listen_event.events) &= (~EVENT_WRITE))
#define GET_LISTEN_EVENTS(X) ((X)->listen_event.events)

#define EVENT_READ		EPOLLIN			/*文件描述符可读*/
#define EVENT_WRITE		EPOLLOUT		/*文件描述符可写*/
#define EVENT_RDHUP		EPOLLRDHUP		/*流式套接字的对端关闭了连接，或者执行了半关闭*/
#define EVENT_PRI		EPOLLPRI		/*文件描述符收到了紧急数据*/
#define EVENT_ERR		EPOLLERR		/*该事件总是被监视*/
#define EVENT_HUP		EPOLLHUP		/*文件描述符挂起，该事件总是被监视*/
#define EVENT_EDGE_MODE	EPOLLET			/*边沿模式*/

#define EVENT_FD_IF_READ(EN) ((EN) & EVENT_READ)
#define EVENT_FD_IF_WRITE(EN) ((EN) & EVENT_WRITE)
#define EVENT_FD_IF_ERROR(EN) (((EN)&EVENT_ERR) || ((EN)&EVENT_HUP))
/*
	在文件描述符发生了事件并进行了相关操作后，将不再被监视，
	此时可以通过EPOLL_CTL_MOD对该文件描述符上的新事件重新进行监视
*/
#define EVENT_ONESHOT	EPOLLONESHOT	

#define EPOLL_TIMER_ADD(master,timer,func,arg,time)\
	do{\
		if(!timer)\
			timer=event_timer_add(master,func,arg,time);\
	}while(0)

#define EPOLL_TIMER_ADD_MSEC(master,timer,func,arg,time)\
	do{\
		if(!timer)\
			timer=event_timer_add_msec(master,func,arg,time);\
	}while(0)

#define EPOLL_REAR_ADD(master,timer,func,arg,time)\
	do{\
		if(!timer)\
			timer=event_rear_add(master,func,arg,time);\
	}while(0)

#define	EPOLL_REAR_ADD_MSEC(master,timer,func,arg,time)\
	do{\
		if(!timer)\
			timer=event_rear_add_msec(master,func,arg,time);\
	}while(0)

#define EVENT_EXECUTE(func,arg)\
	do{\
		if(func)\
			event_immediate_execute(func,arg);\
	}while(0)

#define EPOLL_EVENT_ADD(master,event,func,arg,ep_events/*Epoll events*/,fd)\
	do{\
		if(!event)\
			event=event_epoll_add(master,func,arg,ep_events,fd);\
	}while(0)
#if 1
#define EPOLL_EVENT_MOD(master,event,func,arg,ep_events/*Epoll events*/,fd)\
	do{\
		struct event_node *epoll_event_node_tmp_2 = event_epoll_mod(master,func,arg,ep_events,fd);\
		if(event != epoll_event_node_tmp_2)\
			abort();\
		else\
			event = epoll_event_node_tmp_2;\
	}while(0)
#endif
#define EPOLL_CANCEL(event)\
	do{\
		if(event)\
		{\
			event_epoll_del(event);\
			event=NULL;\
		}\
	}while(0)

#define EPOLL_TIMER_OFF(X) EPOLL_CANCEL(X)
#define EPOLL_EVENT_OFF(X)	EPOLL_CANCEL(X)

/*create master of event*/
struct event_master *event_master_create(int expected_size, int maxevents);

/*destroy master*/
void event_master_destroy(struct event_master *m);

/*add timer event: seconds*/
struct event_node *event_timer_add(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time);

/*add timer event: msec*/
struct event_node *event_timer_add_msec(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time);

/*add timer event: seconds*/
struct event_node *event_rear_add(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time);

/*add timer event: msec*/
struct event_node *event_rear_add_msec(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time);

/*add immediate event: cannot cancel*/
void event_immediate_execute(void (*func)(void *), void *arg);

/*add file description's event*/
struct event_node *event_epoll_add(struct event_master *m,
									void (*func)(struct event_node *),
									void *arg,
									uint32_t ep_events,/*Epoll events*/
									int fd);
#if 1
/*modify file description's event*/
struct event_node *event_epoll_mod(struct event_master *m,
									void (*func)(struct event_node *),
									void *arg,
									uint32_t ep_events,/*Epoll events*/
									int fd);
#endif

#define event_epoll_del(ev) event_cancel(ev)

/*look up event by file description*/
struct event_node *event_epoll_lookup_by_fd(struct event_master *m, int fd);

/*cancel event*/
void event_cancel(struct event_node *event);

/*waiting events happen, and process them*/
struct event_node *  event_waiting_process(struct event_master *m, struct event_node *fetch);

void
event_call(struct event_node *ev);

#endif
