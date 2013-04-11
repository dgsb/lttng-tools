/*
 * Copyright (C) 2013 - Julien Desfossez <jdesfossez@efficios.com>
 *                      David Goulet <dgoulet@efficios.com>
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
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#include <common/common.h>
#include <common/utils.h>
#include <common/sessiond-comm/sessiond-comm.h>
#include <common/ust-consumer/ust-consumer.h>
#include <common/consumer.h>

#include "consumer-metadata-cache.h"

/*
 * Extend the allocated size of the metadata cache. Called only from
 * lttng_ustconsumer_write_metadata_cache.
 *
 * Return 0 on success, a negative value on error.
 */
static int extend_metadata_cache(struct lttng_consumer_channel *channel,
		unsigned int size)
{
	int ret = 0;
	char *tmp_data_ptr;
	unsigned int new_size;

	assert(channel);
	assert(channel->metadata_cache);

	new_size = max_t(unsigned int,
			channel->metadata_cache->cache_alloc_size + size,
			channel->metadata_cache->cache_alloc_size << 1);
	DBG("Extending metadata cache to %u", new_size);
	tmp_data_ptr = realloc(channel->metadata_cache->data, new_size);
	if (!tmp_data_ptr) {
		ERR("Reallocating metadata cache");
		free(channel->metadata_cache->data);
		ret = -1;
		goto end;
	}
	channel->metadata_cache->data = tmp_data_ptr;
	channel->metadata_cache->cache_alloc_size = new_size;

end:
	return ret;
}

/*
 * Write metadata to the cache, extend the cache if necessary. We support
 * non-contiguous updates but not overlapping ones. If there is contiguous
 * metadata in the cache, we send it to the ring buffer. The metadata cache
 * lock MUST be acquired to write in the cache.
 *
 * Return 0 on success, a negative value on error.
 */
int consumer_metadata_cache_write(struct lttng_consumer_channel *channel,
		unsigned int offset, unsigned int len, char *data)
{
	int ret = 0;
	struct consumer_metadata_cache *cache;

	assert(channel);
	assert(channel->metadata_cache);

	cache = channel->metadata_cache;
	DBG("Writing %u bytes from offset %u in metadata cache", len, offset);

	if (offset + len > cache->cache_alloc_size) {
		ret = extend_metadata_cache(channel,
				len - cache->cache_alloc_size + offset);
		if (ret < 0) {
			ERR("Extending metadata cache");
			goto end;
		}
	}

	memcpy(cache->data + offset, data, len);
	cache->total_bytes_written += len;
	if (offset + len > cache->max_offset) {
		cache->max_offset = offset + len;
	}

	if (cache->max_offset == cache->total_bytes_written) {
		offset = cache->rb_pushed;
		len = cache->total_bytes_written - cache->rb_pushed;
		ret = lttng_ustconsumer_push_metadata(channel, cache->data, offset,
				len);
		if (ret < 0) {
			ERR("Pushing metadata");
			goto end;
		}
		cache->rb_pushed += len;
	}

end:
	return ret;
}

/*
 * Create the metadata cache, original allocated size: max_sb_size
 *
 * Return 0 on success, a negative value on error.
 */
int consumer_metadata_cache_allocate(struct lttng_consumer_channel *channel)
{
	int ret;

	assert(channel);

	channel->metadata_cache = zmalloc(
			sizeof(struct consumer_metadata_cache));
	if (!channel->metadata_cache) {
		PERROR("zmalloc metadata cache struct");
		ret = -1;
		goto end;
	}
	ret = pthread_mutex_init(&channel->metadata_cache->lock, NULL);
	if (ret != 0) {
		PERROR("mutex init");
		goto end_free_cache;
	}

	channel->metadata_cache->cache_alloc_size = DEFAULT_METADATA_CACHE_SIZE;
	channel->metadata_cache->data = zmalloc(
			channel->metadata_cache->cache_alloc_size * sizeof(char));
	if (!channel->metadata_cache->data) {
		PERROR("zmalloc metadata cache data");
		ret = -1;
		goto end_free_mutex;
	}
	DBG("Allocated metadata cache of %" PRIu64 " bytes",
			channel->metadata_cache->cache_alloc_size);

	ret = 0;
	goto end;

end_free_mutex:
	pthread_mutex_destroy(&channel->metadata_cache->lock);
end_free_cache:
	free(channel->metadata_cache);
end:
	return ret;
}

/*
 * Destroy and free the metadata cache
 */
void consumer_metadata_cache_destroy(struct lttng_consumer_channel *channel)
{
	if (!channel || !channel->metadata_cache) {
		return;
	}

	DBG("Destroying metadata cache");

	if (channel->metadata_cache->max_offset >
			channel->metadata_cache->rb_pushed) {
		ERR("Destroying a cache not entirely commited");
	}

	pthread_mutex_destroy(&channel->metadata_cache->lock);
	free(channel->metadata_cache->data);
	free(channel->metadata_cache);
}

/*
 * Check if the cache is flushed up to the offset passed in parameter.
 *
 * Return 0 if everything has been flushed, 1 if there is data not flushed.
 */
int consumer_metadata_cache_flushed(struct lttng_consumer_channel *channel,
		uint64_t offset)
{
	int ret;
	struct consumer_metadata_cache *cache;

	assert(channel);
	assert(channel->metadata_cache);

	cache = channel->metadata_cache;

	pthread_mutex_lock(&channel->metadata_cache->lock);
	if (cache->rb_pushed >= offset) {
		ret = 0;
	} else {
		ret = 1;
	}
	pthread_mutex_unlock(&channel->metadata_cache->lock);

	return ret;
}