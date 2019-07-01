#include <sys/timerfd.h>
#include "memory.h"
#include "memtypes.h"
#include "event_ctl.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/*create timer file description*/
extern int timerfd_create(int clockid, int flags);

/*set timer by timerfd*/
extern int timerfd_settime(int fd, int flags,
								const struct itimerspec *new_value,
								struct itimerspec *old_value);

/*get expired time by timerfd*/
extern int timerfd_gettime(int fd, struct itimerspec *curr_value);

/* Struct timespec's tv_nsec one second value.  */
#define TIMER_SECOND_MICRO 1000000000L

static long timespec_cmp(struct timespec a, struct timespec b)
{
  return (a.tv_sec == b.tv_sec
	  ? a.tv_nsec - b.tv_nsec : a.tv_sec - b.tv_sec);
}

/* Adjust so that tv_usec is in the range [0,TIMER_SECOND_MICRO).
   And change negative values to 0. */
static struct timespec timespec_adjust(struct timespec a)
{
	while(a.tv_nsec >= TIMER_SECOND_MICRO)
	{
		a.tv_nsec -= TIMER_SECOND_MICRO;
		a.tv_sec++;
	}

	while(a.tv_nsec < 0)
	{
		a.tv_nsec += TIMER_SECOND_MICRO;
		a.tv_sec--;
	}

	if(a.tv_sec < 0)
		/* Change negative timeouts to 0. */
		a.tv_sec = a.tv_nsec = 0;

	return a;
}


/*create timer, return timer file description.*/
static int _timerfd_create(int clockid, int flags, const char *funcname, int line)
{
	int timerfd;
	timerfd = timerfd_create(clockid, flags);
	if(timerfd < 0)
	{
		fprintf(stderr, "<%s, %d> timerfd create error!\n", funcname, line);
		abort();
	}

	return timerfd;
}

#define event_timer_create(clockid,flags) _timerfd_create(clockid,flags,__FUNCTION__,__LINE__)

static struct event_node *event_timer_get(struct event_master *m)
{
	struct event_node *first_timer = m->timer.head;
	struct event_node *first_rear = m->rear.head;
	
	if(first_rear)
		return first_timer ? 
			((timespec_cmp(first_rear->time.it_value, first_timer->time.it_value) > 0) ? first_timer : first_rear) : 
			first_rear;
	else
		return first_timer ? first_timer : NULL;/*if no time node, return NULL.*/
}

/*set timer*/
static int _timerfd_settime(struct event_master *m, struct event_node *node, const char *funcname, int line)
{
	int ret = -1;

	ret = timerfd_settime(m->event_timer.timerfd, TFD_TIMER_ABSTIME, &node->time, NULL);

	if(ret < 0)
	{
		fprintf(stderr, "<%s, %d> timer set failed!\n", funcname, line);
		return -1;
	}

	m->event_timer.event = node;

	return 0;
}

#define event_timer_set(master,node) _timerfd_settime(master,node,__FUNCTION__,__LINE__)
/*reset timer*/
#define event_timer_reset(master)\
	do{\
		struct event_node *node = NULL;\
		node = event_timer_get(master);\
		if(node)\
			event_timer_set(master, node);\
		else\
			event_timer_cancel(master);\
	}while(0)

/*cancel timer*/
static int _timerfd_cancel(struct event_master *m, const char *funcname, int line)
{
	int ret = -1;
	struct itimerspec zero;
	if(NULL == m->event_timer.event)
		return -1;
	
	memset(&zero, 0, sizeof(struct itimerspec));
	ret = timerfd_settime(m->event_timer.timerfd, TFD_TIMER_ABSTIME, &zero, NULL);

	if(ret < 0)
	{
		fprintf(stderr, "<%s, %d> timer cancel failed!\n", funcname, line);
		return -1;
	}

	m->event_timer.event = NULL;

	return 0;
}

#define event_timer_cancel(master) _timerfd_cancel(master,__FUNCTION__,__LINE__)

extern void hash_clean(struct hash *hash, void (*free_func)(void *));

/*添加到链表尾*/
static inline void event_list_add(event_list *list, struct event_node *event)
{
	event->next = NULL;
	event->prev = list->tail;
	if(list->tail)
		list->tail->next = event;
	else
		list->head = event;
	list->tail = event;
	list->length++;
}

/*添加到指定位置前面*/
static inline void event_list_add_before(event_list *list, struct event_node *point, struct event_node *event)
{
	event->next = point;
	event->prev = point->prev;
	if(point->prev)
		point->prev->next = event;
	else
		list->head = event;
	point->prev = event;
	list->length++;
}

/*从链表中删除一个结点*/
static inline struct event_node *event_list_delete(event_list *list, struct event_node *event)
{
	if(event->next)
		event->next->prev = event->prev;
	else
		list->tail = event->prev;
	if(event->prev)
		event->prev->next = event->next;
	else
		list->head = event->next;
	event->next = event->prev = NULL;
	list->length--;
	
	return event;
}

static inline void event_list_add_unuse(struct event_master *m, struct event_node *event)
{
	assert(m != NULL && event != NULL);
	assert(event->prev == NULL);
	assert(event->next == NULL);
	assert(event->type == EVENT_TYPE_UNUSED);
	event_list_add(&m->unuse, event);
}

static inline void event_list_free(struct event_master *m, event_list *list)
{
	struct event_node *t, *next;
	for(t = list->head; t; t = next)
	{
		next = t->next;
		XFREE(MTYPE_EVENT_NODE, t);
		list->length--;
		m->events_alloc--;
	}
}

static inline int event_list_empty(event_list *list)
{
	int ret;
	ret =  list->head? 0 : 1;
	return ret;
}

static inline int event_list_check_first(event_list *list, struct event_node *node)
{
	int ret;
	ret = list->head==node? 1 : 0;
	return ret;
}

static inline struct event_node *event_list_trim_head(event_list *list)
{
	if(!event_list_empty(list))
		return event_list_delete(list, list->head);
	return NULL;
}

static inline struct event_node *event_list_lookup(event_list *list, struct event_node *node)
{
	struct event_node *tmp = NULL;
	if(event_list_empty(list))
		return NULL;

	for(tmp  = list->head; tmp  != NULL; tmp  = tmp ->next)
	{
		if(tmp->fd == node->fd && tmp->type == node->type)
			return tmp;
	}
	return NULL;
}

static struct event_node *event_node_get(struct event_master *m, event_type type, void (*func)(struct event_node *), void *arg)
{
	struct event_node *event;
	if(!event_list_empty(&m->unuse))
	{
		event = event_list_trim_head(&m->unuse);
	}
	else
	{
		event= (struct event_node *)XCALLOC(MTYPE_EVENT_NODE, sizeof(struct event_node));
		m->events_alloc++;
	}

	event->type = type;
	event->add_type = type;
	event->master = m;
	event->func = func;
	event->arg = arg;

	return event;
}

static void event_node_free(void *ev)
{
	struct event_node *node = (struct event_node *)ev;
	node->master->events_alloc--;
	XFREE(MTYPE_EVENT_NODE, node);;
}

static unsigned int event_hash_key(struct event_node *ev)
{
	return ev->fd;
}

static int event_hash_cmp(struct event_node *ev1, struct event_node *ev2)
{
	return ev1->fd == ev2->fd && ev1->type == ev2->type;
}

/*create master of event*/
#if 0
struct event_master *event_master_create(int expected_size, int maxevents, int max_thread_num)
{
	struct event_master *m = XCALLOC(MTYPE_EVENT_MASTER, sizeof(struct event_master));
	/*create epoll file description*/
	m->expected_size = expected_size;
	m->epollfd = epoll_create(expected_size);
	if(-1 == m->epollfd)
	{
		fprintf(stderr, "<%s, %d>epoll_create error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		abort();
	}

	/*Initialize returing events which size is maxevents*/
	m->maxevents = maxevents;
	m->events = XCALLOC(MTYPE_EPOLL_EVENT, sizeof(struct epoll_event)*maxevents);
	/*create timefd*/
#ifdef EVENT_REALTIME	
	m->event_timer.timerfd = event_timer_create(CLOCK_REALTIME, TFD_CLOEXEC);;
	CThread_mutex_init(&m->event_timer.tlock, NULL);
	m->event_timer.event = NULL;
#else
	m->event_timer.timerfd = event_timer_create(CLOCK_MONOTONIC, TFD_CLOEXEC);;
	CThread_mutex_init(&m->event_timer.tlock, NULL);
	m->event_timer.event = NULL;
#endif
	/*Init the link list*/
	CThread_mutex_init(&m->timer.qlock, NULL);
	CThread_mutex_init(&m->rear.qlock, NULL);
	CThread_mutex_init(&m->unuse.qlock, NULL);
	/*Init the hash list*/
	CThread_mutex_init(&m->event.hlock, NULL);
	m->event.hash = hash_create_size(EVENT_HASH_TABLE_SIZE, 
								(unsigned int (*) (void *))event_hash_key, 
								(int (*)(const void *, const void *))event_hash_cmp);
	m->events_alloc = 0;
	/*listen timerfd*/
	event_epoll_add(m, NULL, NULL, EPOLLIN|EPOLLET, m->event_timer.timerfd);
	/*Initialize the thread pool*/
	CThread_pool_init(max_thread_num);
	return m;
}
#else
struct event_master *event_master_create(int expected_size, int maxevents)
{
	struct event_master *m = XCALLOC(MTYPE_EVENT_MASTER, sizeof(struct event_master));
	/*create epoll file description*/
	m->expected_size = expected_size;
	m->epollfd = epoll_create(expected_size);
	if(-1 == m->epollfd)
	{
		fprintf(stderr, "<%s, %d>epoll_create error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		abort();
	}

	/*Initialize returing events which size is maxevents*/
	m->maxevents = maxevents;
	m->events = XCALLOC(MTYPE_EPOLL_EVENT, sizeof(struct epoll_event)*maxevents);
	/*create timefd*/
#ifdef EVENT_REALTIME	
	m->event_timer.timerfd = event_timer_create(CLOCK_REALTIME, TFD_CLOEXEC);;
	m->event_timer.event = NULL;
#else
	m->event_timer.timerfd = event_timer_create(CLOCK_MONOTONIC, TFD_CLOEXEC);;
	m->event_timer.event = NULL;
#endif
	m->event.hash = hash_create_size(EVENT_HASH_TABLE_SIZE, 
								(unsigned int (*) (void *))event_hash_key, 
								(int (*)(const void *, const void *))event_hash_cmp);
	m->events_alloc = 0;
	/*listen timerfd*/
	event_epoll_add(m, NULL, NULL, EPOLLIN|EPOLLET, m->event_timer.timerfd);

	return m;
}

#endif
void event_master_destroy(struct event_master *m)
{
	assert(m);
	/*destroy  hash list*/
	hash_clean((&m->event)->hash,(event_node_free));
	hash_free(m->event.hash);
	/*destroy rear list*/
	event_list_free(m, &m->rear);
	/*destroy unuse list*/
	event_list_free(m, &m->unuse);
	/*destroy timer list*/
	event_list_free(m, &m->timer);
	/*Close timer file description*/
	close(m->event_timer.timerfd);
	/*free events*/
	if(m->events)
		XFREE(MTYPE_EPOLL_EVENT, m->events);
	/*Close epoll file description*/
	close(m->epollfd);
	/*free master*/
	XFREE(MTYPE_EVENT_MASTER, m);
}

/*get current clock time*/
static struct event_node *event_timer_add_itimerspec(struct event_master *m, 
											void (*func)(struct event_node *), 
											event_type type,
											void *arg, 
											struct itimerspec *ts)
{
	struct event_node *ev, *tt;
	event_list *list;

	assert(m != NULL);
	assert(type == EVENT_TYPE_TIMER || type == EVENT_TYPE_REAR);
	assert(ts);

	list = ((type == EVENT_TYPE_TIMER) ? &m->timer : &m->rear);
	ev = event_node_get(m, type, func, arg);
	
	ev->time.it_value.tv_sec = ts->it_value.tv_sec;
	ev->time.it_value.tv_nsec = ts->it_value.tv_nsec;
	ev->time.it_interval.tv_sec = 0;
	ev->time.it_interval.tv_nsec = 0;
	
	for(tt = list->head; tt; tt = tt->next)
		if(timespec_cmp(ev->time.it_value, tt->time.it_value) < 0)
			break;

	if(tt)
		event_list_add_before(list, tt, ev);
	else
		event_list_add(list, ev);

	if(!m->event_timer.event)/*if event pointer to null, set timer*/
		event_timer_set(m, ev);
	else if(timespec_cmp(m->event_timer.event->time.it_value, ev->time.it_value) > 0)
		event_timer_set(m, ev);
	
	return ev;
}

/*add timer event: seconds*/
struct event_node *event_timer_add(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time)
{
	struct itimerspec ts;
	struct timespec now;

#ifdef EVENT_REALTIME
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#else
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#endif
	ts.it_value.tv_sec = now.tv_sec + time;
	ts.it_value.tv_nsec = now.tv_nsec;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	ts.it_value = timespec_adjust(ts.it_value);

	return event_timer_add_itimerspec(m, func, EVENT_TYPE_TIMER, arg, &ts);
}

/*add timer event: msec*/
struct event_node *event_timer_add_msec(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time)
{
	struct itimerspec ts;
	struct timespec now;

#ifdef EVENT_REALTIME
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#else
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#endif
	ts.it_value.tv_sec = now.tv_sec + time / 1000;
	ts.it_value.tv_nsec = now.tv_nsec + (time % 1000) * 1000000;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	ts.it_value = timespec_adjust(ts.it_value);

	return event_timer_add_itimerspec(m, func, EVENT_TYPE_TIMER, arg, &ts);
}

/*add timer event: seconds*/
struct event_node *event_rear_add(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time)
{
	struct itimerspec ts;
	struct timespec now;

#ifdef EVENT_REALTIME
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#else
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#endif
	ts.it_value.tv_sec = now.tv_sec + time;
	ts.it_value.tv_nsec = now.tv_nsec;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	ts.it_value = timespec_adjust(ts.it_value);

	return event_timer_add_itimerspec(m, func, EVENT_TYPE_REAR, arg, &ts);
}

/*add timer event: msec*/
struct event_node *event_rear_add_msec(struct event_master *m,
							void (*func)(struct event_node *),
							void *arg,
							long time)
{
	struct itimerspec ts;
	struct timespec now;

#ifdef EVENT_REALTIME
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#else
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		fprintf(stderr, "clock_gettime error.\n");
		return NULL;
	}
#endif
	ts.it_value.tv_sec = now.tv_sec + time / 1000;
	ts.it_value.tv_nsec = now.tv_nsec + (time % 1000) * 1000000;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	ts.it_value = timespec_adjust(ts.it_value);

	return event_timer_add_itimerspec(m, func, EVENT_TYPE_REAR, arg, &ts);
}

/*add immediate event: cannot cancel*/
void event_immediate_execute(void (*func)(void *), void *arg)
{
	fprintf(stderr, "<%s,%d> invalid execute",__FUNCTION__, __LINE__);
	return;
}

/*hash list ref*/
static void* event_ref(void *data)
{
	return data;
}

/*add file description's event*/
struct event_node *event_epoll_add(struct event_master *m,
									void (*func)(struct event_node *),
									void *arg,
									uint32_t ep_events,/*Epoll events*/
									int fd)
{
	struct epoll_event ev;
	struct event_node *node;

	assert(m != NULL);
	
	node = event_node_get(m, EVENT_TYPE_EVENT, func, arg);
	
	ev.events = ep_events;
	ev.data.ptr = node;		/*pointer to event node*/
	node->listen_event = ev;
	node->fd = fd;

	/*This will wake up the function epoll_wait*/
	if(epoll_ctl(m->epollfd, EPOLL_CTL_ADD, fd,  &ev) < 0)
	{
		fprintf(stderr, "<%s, %d> epoll_ctl error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		return NULL;
	}

	hash_get(m->event.hash, node, event_ref);/*add into hash list*/
	
	return node;
}

/*add file description's event*/
struct event_node *event_epoll_reset(struct event_master *m,
									struct event_node *node)
{
	assert(m != NULL);
	node->type = EVENT_TYPE_EVENT;
	
	/*This will wake up the function epoll_wait*/
	if(epoll_ctl(m->epollfd, EPOLL_CTL_ADD, node->fd,  &node->listen_event) < 0)
	{
		fprintf(stderr, "<%s, %d> epoll_ctl error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		return NULL;
	}

	hash_get(m->event.hash, node, event_ref);/*add into hash list*/
	
	return node;
}

#if 1
/*modify file description's event*/
struct event_node *event_epoll_mod(struct event_master *m,
									void (*func)(struct event_node *),
									void *arg,
									uint32_t ep_events,/*Epoll events*/
									int fd)
{
	struct epoll_event ev;
	struct event_node *node;
	assert(m != NULL);

	node = event_epoll_lookup_by_fd(m, fd);
	if(NULL == node)
	{
		fprintf(stderr, "<%s, %d> event_epoll_mod error: cannot find event node, fd:%d.\n", 
			__FUNCTION__, __LINE__, fd);
		return NULL;
	}

	ev.events = ep_events;
	ev.data.ptr = node; 	/*pointer to event node*/

	node->listen_event = ev;
	node->func = func;
	node->arg = arg;

	/*This will wake up the function epoll_wait*/
	if(epoll_ctl(m->epollfd, EPOLL_CTL_MOD, fd,  &ev) < 0)
	{
		fprintf(stderr, "<%s, %d> epoll_ctl error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		return NULL;
	}
	
	return node;
}
#endif
struct event_node *event_epoll_lookup_by_fd(struct event_master *m, int fd)
{
	struct event_node tmp, *node = NULL;
	assert(m != NULL);

	tmp.fd = fd;
	tmp.type = EVENT_TYPE_EVENT;

	node = hash_lookup(m->event.hash, &tmp);
	if(node)
		return node;

	tmp.type = EVENT_TYPE_READY;
	node = event_list_lookup(&m->ready, &tmp);	
	if(node)
		return node;

	fprintf(stderr, "<%s, %d> event_epoll_mod error: cannot find event node.", __FUNCTION__, __LINE__);
	return NULL;
}

/*del epoll event*/
static struct event_node *_event_epoll_del_node(struct event_master *m, struct event_node *node)
{
	struct epoll_event ev;
	assert(node != NULL);
	/*This will wake up the function epoll_wait*/
	if(epoll_ctl(m->epollfd, EPOLL_CTL_DEL, node->fd, &ev) < 0)
	{
		fprintf(stderr, "<%s, %d> event_epoll_del error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
		return NULL;
	}
	
	return node;
}
									
/*cancel event*/
void event_cancel(struct event_node *event)
{
	event_list *list;
	event_hash *hlist;
	struct event_master *master = event->master;

	switch(event->type)
	{
		case EVENT_TYPE_TIMER:
			list = &master->timer;
			event_list_delete(list, event);
			if(event == master->event_timer.event)
				event_timer_reset(master);
			break;
		
		case EVENT_TYPE_REAR:
			list = &master->rear;
			event_list_delete(list, event);
			if(event == master->event_timer.event)
				event_timer_reset(master);
			break;

		case EVENT_TYPE_EVENT:
			hlist = &master->event;
			hash_release(hlist->hash, event);
			_event_epoll_del_node(master, event);
			break;

		case EVENT_TYPE_READY:
			list = &master->ready;
			event_list_delete(list, event);
			if(event == master->event_timer.event)
				event_timer_reset(master);
			break;
		
		case EVENT_TYPE_IMMEDIATE:
			/*none*/
			fprintf(stderr, "<%s,%d> event_cancel error: invalid event type(%d)\n", __FUNCTION__, __LINE__, event->type);
			return;
			break;
			
		default:
			//fprintf(stderr, "<%s,%d> event_cancel error: unknown event type(%d)\n", __FUNCTION__, __LINE__, event->type);
			return;
			break;
	}
	
	event->type = EVENT_TYPE_UNUSED;
	event_list_add_unuse(event->master, event);
}

/*deal expired events*/
static unsigned int event_timer_process(event_list *list, struct timespec *timenow)
{
	struct event_node *timer;
	unsigned int ready = 0;

	for (timer = list->head; timer; timer = timer->next)
	{
		if (timespec_cmp(*timenow, timer->time.it_value) < 0)
			return ready;
		event_list_delete(list, timer);
		timer->type = EVENT_TYPE_READY;
		event_list_add(&timer->master->ready, timer);
		ready++;
	}
	
	return ready;
}

static struct event_node *
event_run (struct event_master *m, struct event_node *ev,
	    struct event_node *fetch)
{
	*fetch = *ev;
	if(ev->add_type == EVENT_TYPE_EVENT)
	{
		ev->type = EVENT_TYPE_EVENT;
		hash_get(m->event.hash, ev, event_ref);/*add into hash list*/
	}
	else
	{
		ev->type = EVENT_TYPE_UNUSED;
		event_list_add_unuse (m, ev);
	}
	return fetch;
}


/*waiting events happen, and process them*/
struct event_node * event_waiting_process(struct event_master *m, struct event_node *fetch)
{
	int num, i;
	struct timespec now;
	struct event_node *ev = NULL;
	
	while(1)
	{
		/*run ready event*/
		if ((ev = event_list_trim_head (&m->ready)) != NULL)
			return event_run (m, ev, fetch);
	
		num = epoll_wait(m->epollfd, m->events, m->maxevents, -1);
		if(num < 0)
		{
			if(errno == EINTR)/* signal received - process it */
				continue;
			
			fprintf(stderr, "<%s, %d>epoll_wait error: %s\n", __FUNCTION__, __LINE__, safe_strerror(errno));
			continue;
		}

		/*process events*/
		if(num > 0)
		{
			/*get time now*/
#ifdef EVENT_REALTIME
			if (clock_gettime(CLOCK_REALTIME, &now) == -1)
			{
				fprintf(stderr, "clock_gettime error.\n");
				return NULL;
			}
#else
			if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
			{
				fprintf(stderr, "clock_gettime error.\n");
				return NULL;
			}
#endif
			/*According to the priority of events, processing events*/
			/*firstly, deal timer event*/
			event_timer_process(&m->timer, &now);
	
			/*then deal epoll event*/
			for(i = 0; i < num; i++)
			{
				ev = (struct event_node *)m->events[i].data.ptr;
				if(ev->type == EVENT_TYPE_TIMER || ev->type == EVENT_TYPE_REAR || ev->fd == m->event_timer.timerfd)
					continue;

				ev->events = m->events[i].events;
				hash_release(m->event.hash, ev);

				ev->type = EVENT_TYPE_READY;
				event_list_add(&ev->master->ready, ev);
			}
	
			/*finally deal rear event*/
			event_timer_process(&m->rear, &now);
		}

		/*reset timer*/
		event_timer_reset(m);

		/*run ready event*/
		if ((ev = event_list_trim_head (&m->ready)) != NULL)
			return event_run (m, ev, fetch);		
	}

	return NULL;
}


void
event_call(struct event_node *ev)
{
	(*ev->func) (ev);
}

