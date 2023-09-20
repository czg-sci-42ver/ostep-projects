#ifndef HASH_H
#define HASH_H
#define CAPACITY 200 // Size of the HashTable.
// Defines the HashTable item.
typedef struct Ht_item
{
    char *key;
    char *value;
} Ht_item;

// Defines the LinkedList.
typedef struct linkedList
{
    Ht_item *item;
    struct linkedList *next;
} LinkedList;

// Defines the HashTable.
typedef struct HashTable
{
    // Contains an array of pointers to items.
    Ht_item **items;
    LinkedList **overflow_buckets;
    int size;
    int count;
} HashTable;
HashTable *create_table(int size);
#endif