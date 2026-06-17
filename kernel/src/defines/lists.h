#pragma once

// #include "io.h"


#include <stddef.h>
struct list_head {
	struct list_head *next, *prev;
};


struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos ; pos = pos->next)
	
#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h){
	h->next = NULL;
	h->pprev = NULL;
}

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != head; pos = pos->next)



static inline void hlist_add_head(struct hlist_node *node, struct hlist_head *head)
{
	struct hlist_node *first = head->first;
	node->next = first;
	if (first)
		first->pprev = &node->next;
	head->first = node;
	node->pprev = &head->first;
    
}

static inline void hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	*pprev = next;
	if (next) next->pprev = pprev;
	n->next = NULL;
	n->pprev = NULL;
}