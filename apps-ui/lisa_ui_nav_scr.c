#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_log.h"

#include "lisa_ui_nav_scr.h"

#define TAG "LISA_UI"

struct lisa_ui_scr_node {
    const struct lisa_ui_nav_scr *scr;
    struct lisa_ui_scr_node *next;
};

static struct lisa_ui_scr_node *scr_list_head = NULL;

struct lisa_ui_scr_stack_node {
    const struct lisa_ui_nav_scr *scr;
    void *data;
    struct lisa_ui_scr_stack_node *next;
};

static struct lisa_ui_scr_stack_node *scr_stack_top = NULL;

static const struct lisa_ui_nav_scr *lisa_ui_scr_find_by_id(int unique_id)
{
    struct lisa_ui_scr_node *node = scr_list_head;
    while (node) {
        if (node->scr->unique_id == unique_id) {
            return node->scr;
        }
        node = node->next;
    }
    return NULL;
}

static int lisa_ui_scr_stack_push(const struct lisa_ui_nav_scr *scr, void *data)
{
    if (!scr) {
        LISA_UI_LOGE("stack push: scr is NULL");
        return -1;
    }

    struct lisa_ui_scr_stack_node *node = malloc(sizeof(struct lisa_ui_scr_stack_node));
    if (!node) {
        LISA_UI_LOGE("stack push: malloc failed");
        return -1;
    }

    node->scr = scr;
    node->next = scr_stack_top;
    node->data = data;
    scr_stack_top = node;

    return 0;
}

static const struct lisa_ui_nav_scr *lisa_ui_scr_stack_pop(void **data)
{
    if (!scr_stack_top) {
        LISA_UI_LOGE("stack pop: stack is empty");
        return NULL;
    }

    struct lisa_ui_scr_stack_node *node = scr_stack_top;
    const struct lisa_ui_nav_scr *scr = node->scr;
    *data = node->data;

    scr_stack_top = node->next;
    free(node);

    return scr;
}

static const struct lisa_ui_nav_scr *lisa_ui_scr_stack_top_get(void **data)
{
    if (!scr_stack_top) {
        LISA_UI_LOGE("stack top_get: stack is empty");
        return NULL;
    }

    if (data) {
        *data = scr_stack_top->data;
    }

    return scr_stack_top->scr;
}

static int lisa_ui_scr_stack_has_id(int unique_id)
{
    struct lisa_ui_scr_stack_node *node = scr_stack_top;
    while (node) {
        if (node->scr && node->scr->unique_id == unique_id) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int lisa_ui_nav_scr_nav_to(int unique_id)
{
    void *from_data = NULL;
    void *to_data = NULL;

    const struct lisa_ui_nav_scr *from = lisa_ui_scr_stack_top_get(&from_data);
    const struct lisa_ui_nav_scr *to = lisa_ui_scr_find_by_id(unique_id);

    if (from == NULL) {
        LISA_UI_LOGE("nav_to: from scr not found");
        return -1;
    }

    if (to == NULL) {
        LISA_UI_LOGE("nav_to: to scr not found, id=%d", unique_id);
        return -1;
    }

    if (from->unique_id == unique_id) {
        return 0;
    }

    if (lisa_ui_scr_stack_has_id(unique_id)) {
        while (scr_stack_top && scr_stack_top->scr && scr_stack_top->scr->unique_id != unique_id) {
            from = lisa_ui_scr_stack_pop(&from_data);
            if (from == NULL) {
                LISA_UI_LOGE("nav_to: failed to pop stack to id=%d", unique_id);
                return -1;
            }

            if (from->pause) {
                from->pause(from, from_data);
            }

            if (from->close) {
                from->close(from, from_data);
            }
        }

        to = lisa_ui_scr_stack_top_get(&to_data);
        if (to == NULL) {
            LISA_UI_LOGE("nav_to: target scr not found after pop, id=%d", unique_id);
            return -1;
        }
        if (to->resume) {
            to->resume(to, to_data);
        }
        return 0;
    }

    if (to->open == NULL) {
        return -1;
    }

    if (from->pause) {
        from->pause(from, from_data);
    }

    if (to->open(to, &to_data) != 0) {
        if (from->resume) {
            from->resume(from, from_data);
        }
        return -1;
    }

    if (lisa_ui_scr_stack_push(to, to_data) != 0) {
        if (to->close) {
            to->close(to, to_data);
        }
        if (from->resume) {
            from->resume(from, from_data);
        }
        return -1;
    }

    if (to->show) {
        to->show(to, to_data);
    }

    return 0;
}

int lisa_ui_nav_scr_nav_back()
{
    void *from_data = NULL;
    void *to_data = NULL;

    if (!scr_stack_top) {
        LISA_UI_LOGE("nav_back: stack is empty");
        return -1;
    }

    if (!scr_stack_top->next) {
        LISA_UI_LOGW("nav_back: only one node in stack, cannot go back");
        return -1;
    }

    const struct lisa_ui_nav_scr *from = lisa_ui_scr_stack_pop(&from_data);
    const struct lisa_ui_nav_scr *to = lisa_ui_scr_stack_top_get(&to_data);

    if (from == NULL) {
        LISA_UI_LOGE("nav_back: from scr not found");
        return -1;
    }

    if (to == NULL) {
        LISA_UI_LOGE("nav_back: to scr not found");
        return -1;
    }

    if (from->pause) {
        from->pause(from, from_data);
    }

    if (to->resume) {
        to->resume(to, to_data);
    }

    if (from->close) {
        from->close(from, from_data);
    }

    return 0;
}

int lisa_ui_nav_scr_nav_default(void)
{
    void *to_data = NULL;

    if (!scr_stack_top) {
        LISA_UI_LOGE("nav_default: stack is empty");
        return -1;
    }

    if (!scr_stack_top->next) {
        LISA_UI_LOGW("nav_default: only one node in stack, already at default");
        return 0;
    }

    while (scr_stack_top && scr_stack_top->next) {
        void *temp_data = NULL;
        const struct lisa_ui_nav_scr *temp_scr = lisa_ui_scr_stack_pop(&temp_data);
        if (temp_scr && temp_scr->pause) {
            temp_scr->pause(temp_scr, temp_data);
        }
        if (temp_scr && temp_scr->close) {
            temp_scr->close(temp_scr, temp_data);
        }
    }

    const struct lisa_ui_nav_scr *to = lisa_ui_scr_stack_top_get(&to_data);
    if (to == NULL) {
        LISA_UI_LOGE("nav_default: default scr not found");
        return -1;
    }

    if (to->resume) {
        to->resume(to, to_data);
    }

    return 0;
}

int lisa_ui_nav_scr_default_set(const struct lisa_ui_nav_scr *scr)
{
    if (scr == NULL) {
        LISA_UI_LOGE("default_set: scr is NULL");
        return -1;
    }

    while (scr_stack_top) {
        void *temp_data = NULL;
        const struct lisa_ui_nav_scr *temp_scr = lisa_ui_scr_stack_pop(&temp_data);
        if (temp_scr && temp_scr->close) {
            temp_scr->close(temp_scr, temp_data);
        }
    }

    void *data = NULL;

    if (scr->open) {
        scr->open(scr, &data);
    }

    if (lisa_ui_scr_stack_push(scr, data) != 0) {
        LISA_UI_LOGE("default_set: failed to push scr to stack");
        return -1;
    }

    if (scr->show) {
        scr->show(scr, data);
    }

    return 0;
}

int lisa_ui_nav_scr_default_set_by_id(int id)
{
    return lisa_ui_nav_scr_default_set(lisa_ui_scr_find_by_id(id));
}

int lisa_ui_nav_scr_add(const struct lisa_ui_nav_scr *scr)
{
    if (!scr) {
        LISA_UI_LOGE("scr is NULL");
        return -1;
    }

    struct lisa_ui_scr_node *node = malloc(sizeof(struct lisa_ui_scr_node));
    if (!node) {
        LISA_UI_LOGE("malloc failed for scr_node");
        return -1;
    }

    node->scr = scr;
    node->next = scr_list_head;
    scr_list_head = node;

    return 0;
}

int lisa_ui_nav_scr_get_top_id(void)
{
    const struct lisa_ui_nav_scr *scr = lisa_ui_scr_stack_top_get(NULL);
    if (!scr) {
        LISA_UI_LOGE("get_top_id: stack is empty");
        return -1;
    }

    return scr->unique_id;
}
