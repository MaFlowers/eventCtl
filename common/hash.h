#ifndef __HASH_H__
#define __HASH_H__

/* Default hash table size.  */ 
#define HASHTABSIZE     1024

struct hash_backet
{
	/* Linked list.  */
	struct hash_backet *next;

	/* Hash key. */
	unsigned int key;

	/* Data.  */
	void *data;
};

struct hash
{
	/*Hash bucket*/
	struct hash_backet **index;

	/*The total length of hash bucket*/
	unsigned int size;

	/*calculate the hash key*/
	unsigned int (*hash_key) (void *);

	/*compare date A with data B in the hash*/
	int (*hash_cmp) (const void *, const void *);

	/*The counts of the data in the hash table*/
	unsigned long count;
};

extern struct hash *hash_create_size(unsigned int size, unsigned int (*hash_key)(void *),
										int (*hash_cmp)(const void *, const void *));
extern struct hash *hash_create(unsigned int (*hash_key)(void *), 
										int (*hash_cmp)(const void *, const void *));
extern void *hash_alloc_intern(void *arg);
extern void *hash_get(struct hash *hash, void *data, void *(*alloc_func)(void *));
extern void *hash_lookup(struct hash *hash, void *data);
extern void *hash_release(struct hash *hash, void *data);
extern void hash_iterate(struct hash *hash, 
	      void (*func)(struct hash_backet *, void *), void *arg);
extern void hash_clean(struct hash *hash, void (*free_func)(void *));
extern void hash_free(struct hash *hash);

#endif
