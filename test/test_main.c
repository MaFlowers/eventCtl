#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "event_ctl.h"

struct event_master *master = NULL;
#define EXPECTED_SIZE	100
#define EVENTS_MAX		20

void test(struct event_node *node)
{
	char *buf = EVENT_ARG(node);
	char outbuf[1000] = {0};
	static int index = 1;
	
	snprintf(outbuf, 1000, "%s%d", buf, index);
	printf("%s\n", outbuf);
	event_timer_add_msec(master, test, buf, 100);

	index++;
}

void test1(struct event_node *node)
{
	char *buf = EVENT_ARG(node);
	char outbuf[1000] = {0};
	static int index = 1000;	

	snprintf(outbuf, 1000, "%s%d", buf, index);
	printf("%s\n", outbuf);
	event_timer_add_msec(master, test1, buf, 1000);

	index++;
}

#define NUM 100
int array[NUM];

int main()
{
	/*create master of event*/
	char buf[32]="hello";
	struct event_node fetch;
	
	master = event_master_create(EXPECTED_SIZE, EVENTS_MAX);

	event_timer_add(master, test, buf, 3);
	event_timer_add(master, test1, buf, 3);
	
	while(event_waiting_process(master, &fetch))
		event_call(&fetch);
	
	return 0;
}

