#ifndef __MEMTYPES_H__
#define __MEMTYPES_H__

enum
{
	MTYPE_LIST = 0,
	MTYPE_LIST_NODE,
	MTYPE_HOSTNAME_STRING,
	MTYPE_HASH,
	MTYPE_HASH_INDEX,
	MTYPE_HASH_BACKET,
	MTYPE_EVENT_NODE,
	MTYPE_EVENT_MASTER,
	MTYPE_EPOLL_EVENT,/*event*/
	MTYPE_PEER,
	MTYPE_TCP_CLIENT_INFO_NODE,
	MTYPE_TCP_SERVER_INFO_NODE,
	MTYPE_STREAM_FIFO,
	MTYPE_STREAM,
	MTYPE_STREAM_DATA,
	MTYPE_STRING,
	MTYPE_DNS_QUERY,
	MTYPE_DNS_ANSWER,
	MTYPE_DNS_RESULT,
	MTYPE_HTTP_REQUEST_SLINE,
	MTYPE_HTTP_RESPONSE_SLINE,
	MTYPE_DNS_CACHE,
	MTYPE_HTTP_CACHE_ROOT,
	MTYPE_MAX
};

extern const char *mtype_str[];
#endif