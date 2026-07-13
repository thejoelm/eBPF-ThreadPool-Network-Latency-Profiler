#ifndef __COMMON_H
#define __COMMON_H

struct event_t {
	__u64			pid;
    __u64			socket_ptr;
    __u64			delta_ns;
};

#endif