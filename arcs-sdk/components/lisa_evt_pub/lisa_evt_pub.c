#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "cc/slist.h"

#include "lisa_mem.h"
#include "lisa_log.h"
#include "lisa_mutex.h"

#include "lisa_evt_pub.h"

struct lisa_evt_publisher {
	SList *slist;
	lisa_mutex_t *lock;
	bool bit_ops;
};

struct lisa_evt_element {
	uint32_t evt;
	lisa_evt_publisher_cb_t cb;
	void *user_data;
};
typedef struct lisa_evt_element lisa_evt_element_t;

static lisa_evt_publisher_t lisa_evt_publisher_new_private(bool bit_ops)
{
	struct lisa_evt_publisher *pub = lisa_mem_alloc(sizeof(struct lisa_evt_publisher));

	if (pub == NULL) {
		return NULL;
	}

	pub->bit_ops = bit_ops;

	pub->lock = lisa_mutex_create();
	if (pub->lock == NULL) {
		lisa_mem_free(pub);
		return NULL;
	}

	SListConf conf = {
		.mem_alloc = (void *(*)(size_t))lisa_mem_alloc,
		.mem_free = lisa_mem_free,
		.mem_calloc = (void *(*)(size_t, size_t))lisa_mem_calloc,
	};

	enum cc_stat stat = slist_new_conf(&conf, &pub->slist);
	if (stat != CC_OK) {
		lisa_mutex_delete(pub->lock);
		lisa_mem_free(pub);
		return NULL;
	}

	return pub;
}

lisa_evt_publisher_t lisa_evt_publisher_new()
{
	return lisa_evt_publisher_new_private(true);
}

lisa_evt_publisher_t lisa_evt_publisher_new_eq()
{
	return lisa_evt_publisher_new_private(false);
}

int lisa_evt_publisher_has_subscriber(lisa_evt_publisher_t p, uint32_t evt)
{
	if (p == NULL) {
		return 0;
	}

	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;
	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);

	SLIST_FOREACH(el, pub->slist, {
		lisa_evt_element_t *e = el;
		if (pub->bit_ops ? (e->evt & evt) > 0 : e->evt == evt) {
			lisa_mutex_unlock(pub->lock);
			return 1;
		}
	});

	lisa_mutex_unlock(pub->lock);
	return 0;
}

void lisa_evt_publisher_publish(lisa_evt_publisher_t p, uint32_t evt, void *data, uint32_t len)
{
	if (p == NULL) {
		return;
	}

	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;
	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);
	SLIST_FOREACH(el, pub->slist, {
		lisa_evt_element_t *e = el;
		uint8_t can_publish = pub->bit_ops ? (e->evt & evt) > 0 : (e->evt == evt);
		if (e->cb && can_publish) {
			e->cb(pub->bit_ops ? e->evt & evt : evt, data, len, e->user_data);
		}
	});

	lisa_mutex_unlock(pub->lock);
}

int lisa_evt_publisher_evt_add(lisa_evt_publisher_t p, uint32_t evt, lisa_evt_publisher_cb_t cb,
			       void *user_data)
{
	if (p == NULL || cb == NULL) {
		return -1;
	}

	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;

	if (pub->bit_ops && evt == 0) {
		return -1;
	}

	lisa_evt_element_t *el = lisa_mem_alloc(sizeof(lisa_evt_element_t));

	if (el == NULL) {
		return -1;
	}

	el->cb = cb;
	el->evt = evt;
	el->user_data = user_data;

	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);

	enum cc_stat stat = slist_add(pub->slist, el);
	if (stat != CC_OK) {
		lisa_mem_free(el);
		lisa_mutex_unlock(pub->lock);
		return -1;
	}

	lisa_mutex_unlock(pub->lock);

	return 0;
}

static void cc_slist_remove_cb_handle(void *data)
{
	lisa_mem_free(data);
}

void lisa_evt_publisher_cb_remove(lisa_evt_publisher_t p, lisa_evt_publisher_cb_t cb)
{
	if (p == NULL) {
		return;
	}

	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;
	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);

	SLIST_FOREACH(el, pub->slist, {
		if (((lisa_evt_element_t *)el)->cb == cb) {
			slist_remove(pub->slist, el, NULL);
			lisa_mem_free(el);
		}
	});

	lisa_mutex_unlock(pub->lock);
}

int lisa_evt_publisher_clear(lisa_evt_publisher_t p)
{
	int err;

	if (p == NULL) {
		return -1;
	}

	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;
	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);

	err = slist_remove_all_cb(pub->slist, cc_slist_remove_cb_handle);

	lisa_mutex_unlock(pub->lock);

	return err == CC_OK ? 0 : -1;
}

void lisa_evt_publisher_destroy(lisa_evt_publisher_t p)
{
	if (p == NULL) {
		return;
	}
	struct lisa_evt_publisher *pub = (struct lisa_evt_publisher *)p;
	lisa_mutex_lock(pub->lock, LISA_OS_WAIT_FOREVER);

	lisa_evt_publisher_clear(pub);
	slist_destroy(pub->slist);
	pub->slist = NULL;

	lisa_mutex_unlock(pub->lock);

	lisa_mutex_delete(pub->lock);
	lisa_mem_free(pub);
}
