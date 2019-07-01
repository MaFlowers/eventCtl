#include <assert.h>
#include "list.h"
#include "memory.h"
#include "memtypes.h"
#include <stdio.h>
struct list *list_new()
{
	return (struct list *)XCALLOC(MTYPE_LIST, sizeof(struct list));
}

void list_free(struct list *l)
{
	XFREE(MTYPE_LIST, l);
}

struct listnode *listnode_new()
{
	return (struct listnode *)XCALLOC(MTYPE_LIST_NODE, sizeof(struct listnode));
}

void listnode_free(struct listnode *node)
{
	XFREE(MTYPE_LIST_NODE, node);
}

/*Add a node at the end of the list*/
void listnode_add(struct list *list, void *val)
{
	struct listnode *node = NULL;
	assert(val != NULL);

	node = listnode_new();

	node->prev = list->tail;
	node->data = val;

	if(list->head == NULL)
		list->head = node;
	else
		list->tail->next = node;
	list->tail = node;

	list->count++;
}

/*Add a node  in the link list head*/
void listnode_add_head(struct list *list, void *val)
{
	struct listnode *node = NULL;
	assert(val != NULL);

	node = listnode_new();

	node->next = list->head;
	node->data = val;

	if(list->tail == NULL)
		list->tail = node;
	else
		list->head->prev = node;
	list->head = node;

	list->count++;
}

/*According to the comparison function in the list, add node into the list by the sorting*/
void listnode_add_sort(struct list *list, void *val)
{
	struct listnode *n, *new;

	assert(val != NULL);

	new = listnode_new();
	new->data = val;

	if(list->cmp)
	{
		for(n = list->head; n; n = n->next)
		{
			if((*list->cmp)(val, n->data) < 0)
			{
				new->next = n;
				new->prev = n->prev;

				if(n->prev)
					n->prev->next = new;
				else
					list->head = new;
				n->prev = new;
				list->count++;
				return;
			}
		}
	}

	/*if no comparison function, add node at the end of the list*/
	new->prev = list->tail;
	if(list->tail)
		list->tail->next = new;
	else
		list->head = new;
	list->tail = new;
	list->count++;
}

/*Adding new nodes behind a node in the list, if pp is empty, the new node will be added to the list head*/
void listnode_add_after(struct list *list, struct listnode *pp, void *val)
{
	struct listnode *nn = NULL;
	assert(val != NULL);

	nn = listnode_new();
	nn->data = val;

	if(pp == NULL)
	{
		if(list->head)
			list->head->prev = nn;
		else
			list->tail = nn;
		nn->next = list->head;
		nn->prev = pp;
		list->head = nn;
	}
	else
	{
		if(pp->next)
			pp->next->prev = nn;
		else
			list->tail = nn;
		nn->next = pp->next;
		nn->prev = pp;
		pp->next = nn;
	}
	list->count++;
}

/* delete the corresponding node of data from the link list*/
void listnode_delete(struct list *list, void *val)
{
	struct listnode *node;
	assert(list);

	for(node = list->head; node; node = node->next)
	{
		if(node->data == val)
		{
			if(node->prev)
				node->prev->next = node->next;
			else
				list->head = node->next;
			if(node->next)
				node->next->prev = node->prev;
			else
				list->tail = node->prev;
			list->count--;
			listnode_free(node);
			return;
		}
	}
}

/*Delete the specified node from the list*/
void list_delete_node (struct list *list, struct listnode *node)
{
	if (node->prev)
		node->prev->next = node->next;
	else
		list->head = node->next;
	if (node->next)
		node->next->prev = node->prev;
	else
		list->tail = node->prev;
	list->count--;
	listnode_free (node);
}


/* Return first node's data if it is there.  */
void *listnode_head (struct list *list)
{
	struct listnode *node;

	assert(list);
	node = list->head;

	if (node)
		return node->data;
	return NULL;
}

void *listnode_tail (struct list *list)
{
	struct listnode *node;

	assert(list);
	node = list->tail;

	if (node)
		return node->data;
	return NULL;
}

/* Delete all the node from the list */
void list_delete_all_node (struct list *list)
{
	struct listnode *node;
	struct listnode *next;

	assert(list);
	for (node = list->head; node; node = next)
	{
		next = node->next;
		if (list->del)
		{
			//fprintf(stdout, "list_delete_all_node delfunc:%p\n", list->del);
			(*list->del) (node->data);
		}
		listnode_free (node);
	}
	list->head = list->tail = NULL;
	list->count = 0;
}

/*Delete the specified node from the list, and delete the link list*/
void list_delete(struct list *list)
{
	assert(list);
	list_delete_all_node(list);
	list_free(list);
}

/*Look up the corresponding node of the data from the link list*/
struct listnode *listnode_lookup(struct list *list, void *data)
{
	struct listnode *node;
	assert(list);
	for(node = listhead(list);node; node = listnextnode(node))		
		if (data == listgetdata (node))
			return node;
	return NULL;
}

/*Add all the node from the link list A to the link list B, but do not delete the nodes from the link list A*/
void list_add_list (struct list *l, struct list *m)
{
	struct listnode *n = NULL;
	for (n = listhead (m); n; n = listnextnode (n))
		listnode_add (l, n->data);
}

