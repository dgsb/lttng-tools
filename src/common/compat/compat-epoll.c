/*
 * Copyright (C)  2011 - David Goulet <david.goulet@polymtl.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <config.h>

#include <common/error.h>
#include <common/defaults.h>

#include "poll.h"

unsigned int poll_max_size;

/*
 * Create epoll set and allocate returned events structure.
 */
int compat_epoll_create(struct lttng_poll_event *events, int size, int flags)
{
	int ret;

	if (events == NULL || size <= 0) {
		goto error;
	}

	/* Don't bust the limit here */
	if (size > poll_max_size && poll_max_size != 0) {
		size = poll_max_size;
	}

	ret = epoll_create1(flags);
	if (ret < 0) {
		/* At this point, every error is fatal */
		PERROR("epoll_create1");
		goto error;
	}

	events->epfd = ret;

	/* This *must* be freed by using lttng_poll_free() */
	events->events = zmalloc(size * sizeof(struct epoll_event));
	if (events->events == NULL) {
		PERROR("zmalloc epoll set");
		goto error_close;
	}

	events->events_size = size;
	events->nb_fd = 0;

	return 0;

error_close:
	ret = close(events->epfd);
	if (ret) {
		PERROR("close");
	}
error:
	return -1;
}

/*
 * Add a fd to the epoll set with requesting events.
 */
int compat_epoll_add(struct lttng_poll_event *events, int fd, uint32_t req_events)
{
	int ret, new_size;
	struct epoll_event ev, *ptr;

	if (events == NULL || events->events == NULL || fd < 0) {
		ERR("Bad compat epoll add arguments");
		goto error;
	}

	ev.events = req_events;
	ev.data.fd = fd;

	ret = epoll_ctl(events->epfd, EPOLL_CTL_ADD, fd, &ev);
	if (ret < 0) {
		switch (errno) {
		case EEXIST:
			/* If exist, it's OK. */
			goto end;
		case ENOSPC:
		case EPERM:
			/* Print PERROR and goto end not failing. Show must go on. */
			PERROR("epoll_ctl ADD");
			goto end;
		default:
			PERROR("epoll_ctl ADD fatal");
			goto error;
		}
	}

	events->nb_fd++;

	if (events->nb_fd >= events->events_size) {
		new_size = 2 * events->events_size;
		ptr = realloc(events->events, new_size * sizeof(struct epoll_event));
		if (ptr == NULL) {
			PERROR("realloc epoll add");
			goto error;
		}
		events->events = ptr;
		events->events_size = new_size;
	}

end:
	return 0;

error:
	return -1;
}

/*
 * Remove a fd from the epoll set.
 */
int compat_epoll_del(struct lttng_poll_event *events, int fd)
{
	int ret;

	if (events == NULL || fd < 0) {
		goto error;
	}

	ret = epoll_ctl(events->epfd, EPOLL_CTL_DEL, fd, NULL);
	if (ret < 0) {
		switch (errno) {
		case ENOENT:
		case EPERM:
			/* Print PERROR and goto end not failing. Show must go on. */
			PERROR("epoll_ctl DEL");
			goto end;
		default:
			PERROR("epoll_ctl DEL fatal");
			goto error;
		}
		PERROR("epoll_ctl del");
		goto error;
	}

	events->nb_fd--;

end:
	return 0;

error:
	return -1;
}

/*
 * Wait on epoll set. This is a blocking call of timeout value.
 */
int compat_epoll_wait(struct lttng_poll_event *events, int timeout)
{
	int ret;

	if (events == NULL || events->events == NULL ||
			events->events_size < events->nb_fd) {
		ERR("Wrong arguments in compat_epoll_wait");
		goto error;
	}

	do {
		ret = epoll_wait(events->epfd, events->events, events->nb_fd, timeout);
	} while (ret == -1 && errno == EINTR);
	if (ret < 0) {
		/* At this point, every error is fatal */
		PERROR("epoll_wait");
		goto error;
	}

	return ret;

error:
	return -1;
}

/*
 * Setup poll set maximum size.
 */
void compat_epoll_set_max_size(void)
{
	int ret, fd;
	char buf[64];

	poll_max_size = DEFAULT_POLL_SIZE;

	fd = open(COMPAT_EPOLL_PROC_PATH, O_RDONLY);
	if (fd < 0) {
		return;
	}

	ret = read(fd, buf, sizeof(buf));
	if (ret < 0) {
		PERROR("read set max size");
		goto error;
	}

	poll_max_size = atoi(buf);
	if (poll_max_size <= 0) {
		/* Extra precaution */
		poll_max_size = DEFAULT_POLL_SIZE;
	}

	DBG("epoll set max size is %d", poll_max_size);

error:
	ret = close(fd);
	if (ret) {
		PERROR("close");
	}
}
