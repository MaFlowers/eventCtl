#ifndef __LIST_H__
#define __LIST_H__

struct listnode
{
	struct listnode *next;	/*pointer to the next node, if there's no next node, it will pointer to null*/
	struct listnode *prev;	/*pointer to the previous node, if there's no previous node, it will pointer to null*/
	void *data;				/*Store the data*/
};

struct list
{
	struct listnode *head;	/*The head of the list, if list is empty, head will pointer to null.*/
	struct listnode *tail;	/*The end of the list.*/

	unsigned int count;		/*the length of the link list*/

	/*
	 * if val1 < val2, return -1;
	 * if val1 = val2, return  0;
	 * if val1 > val2, return  1;
	 */
	int (*cmp) (void *val1, void *val2);
	
	/*when you wanna delete all the node, free the data.*/
	void (*del) (void *val);
};

#define listnextnode(X) ((X)->next)
#define listhead(X) ((X)->head)
#define listtail(X) ((X)->tail)
#define listcount(X) ((X)->count)
#define list_isempty(X) ((X)->head == NULL && (X)->tail == NULL)
#define listgetdata(X) (assert((X)->data != NULL), (X)->data)

extern struct list *list_new();
extern void list_free(struct list *l);
extern struct listnode *listnode_new();
extern void listnode_free(struct listnode *node);
extern void listnode_add(struct list *list, void *val);
extern void listnode_add_head(struct list *list, void *val);
extern void listnode_add_sort(struct list *list, void *val);
extern void listnode_add_after(struct list *list, struct listnode *pp, void *val);
extern void listnode_delete(struct list *list, void *val);
extern void list_delete_node (struct list *list, struct listnode *node);
extern void *listnode_head (struct list *list);
extern void *listnode_tail (struct list *list);
extern void list_delete_all_node (struct list *list);
extern void list_delete(struct list *list);
extern struct listnode *listnode_lookup(struct list *list, void *val);
extern void list_add_list (struct list *l, struct list *m);

/* List iteration macro. 
 * Usage: for (ALL_LIST_ELEMENTS (...) { ... }
 * It is safe to delete the listnode using this macro.
 */
#define ALL_LIST_ELEMENTS(list,node,nextnode,data) \
	(node) = listhead(list); \
	(node) != NULL && \
		((data) = listgetdata(node),(nextnode) = listnextnode(node), 1); \
	(node) = (nextnode)

/* read-only list iteration macro.
 * Usage: as per ALL_LIST_ELEMENTS, but not safe to delete the listnode Only
 * use this macro when it is *immediately obvious* the listnode is not
 * deleted in the body of the loop. Does not have forward-reference overhead
 * of previous macro.
 */
#define ALL_LIST_ELEMENTS_RO(list,node,data) \
	(node) = listhead(list); \
	(node) != NULL && ((data) = listgetdata(node), 1); \
	(node) = listnextnode(node)

#endif
