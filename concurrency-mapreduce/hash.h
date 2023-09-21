#ifndef HASH_H
#define HASH_H
#include <assert.h>
#define CAPACITY 200 // Size of the HashTable.
// Defines the HashTable item.
#define VALUE_SIZE 100
typedef struct Ht_item
{
    char *key;
    char **value;
    int value_list_len;
    int store_to_overflow_buckets;
    int get_next_index;
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
void free_item(Ht_item *item);
void ht_insert(HashTable *table, char *key, char *value);
void free_table(HashTable *table);
void print_table(HashTable *table);
void append_item_value(char *value,Ht_item *item);
char *ht_search_with_get_next(HashTable *table, char *key);
char *ht_search_with_get_next_partition(HashTable *table, char *key, int index);
#endif