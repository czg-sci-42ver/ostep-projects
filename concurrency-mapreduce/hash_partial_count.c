/*
https://www.digitalocean.com/community/tutorials/hash-table-in-c-plus-plus
*/
// Defines the HashTable item.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define SORT_KEY_ASCEND
// #define CHECK_CREATE_ITEM

/*
from paper 4.1, multiple keys maybe to one partition.
*/
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

LinkedList *allocate_list()
{
    // Allocates memory for a LinkedList pointer.
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    return list;
}

/*
next is always NULL
*/
LinkedList *linkedlist_insert(LinkedList *list, Ht_item *item)
{
    // Inserts the item onto the LinkedList.
    /*
    This maybe duplicate, because `if (head == NULL)`
    */
    #ifdef SORT_KEY_ASCEND
    // int insert_index = 0;
    LinkedList *prev=NULL,*tmp=list; // similar to ht_delete
    assert(tmp!=NULL);
    int cmp=0,find_key=0;
    for (; tmp!=NULL; tmp=tmp->next) {
      /*
      until current key larger than the key to insert.
      this with O(n^2) complexity is not recommended, here is only for representation.
      */
      cmp=strcmp(tmp->item->key, item->key);
      if (cmp>0) {
        #ifdef CMP_LOG
        printf("%s > %s\n",tmp->item->key,item->key);
        #endif
        break;
      }else if (cmp==0){
        find_key=1;
        break;
      }
      prev = tmp;
    }
    if (find_key) {
      append_item_value(item->value[0],tmp->item);
      /*
      1. check all return paths to fix the valgrind errors.
      2. TODO better to change the API to avoid the frequent free and malloc of this appended thing.
      */
      free_item(item);
      // printf("append item: %p\n",tmp->item);
    }else {
      LinkedList *mid = allocate_list();
      if (prev) {
        mid->item = item;
        mid->next = tmp;
        // printf("finish mid assignment with mid ptr: %p\n",mid);
        fflush(stdout);
        prev->next=mid;
        #ifdef CHECK_CREATE_ITEM
        printf("insert item in the mid: %p\n",item);
        #endif
      }else {
        /*
        insert at the beginning of overflow_buckets
        here tmp=list
        */
        *mid=*tmp;
        tmp->item=item;
        tmp->next=mid;
      }
    }
    #else
    if (!list)
    {
        LinkedList *head = allocate_list();
        head->item = item;
        head->next = NULL;
        list = head;
        return list;
    }
    else if (list->next == NULL)
    {
        LinkedList *node = allocate_list();
        node->item = item;
        node->next = NULL;
        /*
        here each LinkedList only stores one item.
        */
        list->next = node;
        return list;
    }

    LinkedList *temp = list;

    while (temp->next->next)
    {
        temp = temp->next;
    }

    LinkedList *node = allocate_list();
    node->item = item;
    node->next = NULL;
    temp->next = node;
    #endif
    return list;
}

Ht_item *linkedlist_remove(LinkedList *list)
{
    // Removes the head from the LinkedList.
    // Returns the item of the popped element.
    if (!list)
        return NULL;
    
    if (!list->next)
        return NULL;

    LinkedList *node = list->next;
    LinkedList *temp = list;
    temp->next = NULL;
    list = node;
    Ht_item *it = NULL;
    memcpy(temp->item, it, sizeof(Ht_item));
    free(temp->item->key);
    free(temp->item->value);
    free(temp->item);
    free(temp);
    return it;
}

void free_linkedlist(LinkedList *list)
{
    LinkedList *temp = list;

    while (list)
    {
        temp = list;
        list = list->next;
        free_item(temp->item);
        free(temp);
    }
}

LinkedList **create_overflow_buckets(HashTable *table)
{
    // Create the overflow buckets; an array of LinkedLists.
    LinkedList **buckets = (LinkedList **)calloc(table->size, sizeof(LinkedList *));

    for (int i = 0; i < table->size; i++)
        buckets[i] = NULL;

    return buckets;
}

void free_overflow_buckets(HashTable *table)
{
    // Free all the overflow bucket lists.
    LinkedList **buckets = table->overflow_buckets;

    for (int i = 0; i < table->size; i++)
        free_linkedlist(buckets[i]);

    free(buckets);
}

void append_item_value(char *value,Ht_item *item)
{
  item->value[item->value_list_len] = (char *)malloc(strlen(value) + 1);
  strcpy(item->value[item->value_list_len++], value);
}

Ht_item *copy_item(Ht_item *target_item)
{
    // Creates a pointer to a new HashTable item.
    Ht_item *item = (Ht_item *)malloc(sizeof(Ht_item));
    // item->value_list_len=target_item->value_list_len;
    // item->key = (char *)malloc(strlen(key) + 1);
    // strcpy(item->key, key);
    // item->value = value;
    *item = *target_item;
    return item;
}

Ht_item *create_item(char *key, char *value,int store_to_overflow_buckets)
{
    // Creates a pointer to a new HashTable item.
    Ht_item *item = (Ht_item *)malloc(sizeof(Ht_item));
    item->value_list_len=0;
    item->store_to_overflow_buckets=store_to_overflow_buckets;
    item->key = (char *)malloc(strlen(key) + 1);
    strcpy(item->key, key);
    item->value = (char **)calloc(VALUE_SIZE,sizeof(char*));
    append_item_value(value,item);
    return item;
}

HashTable *create_table(int size)
{
    // Creates a new HashTable.
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    table->size = size;
    table->count = 0;
    table->items = (Ht_item **)calloc(table->size, sizeof(Ht_item *));

    for (int i = 0; i < table->size; i++)
        table->items[i] = NULL;

    table->overflow_buckets = create_overflow_buckets(table);

    return table;
}

void free_item(Ht_item *item)
{
    // Frees an item.
    #ifdef CHECK_CREATE_ITEM
    printf("free item: %p with size:%ld\n",item,sizeof(Ht_item));
    #endif
    free(item->key);
    for (int i=0; i<item->value_list_len; i++) {
      free(item->value[i]);
    }
    free(item->value);
    free(item);
}

void free_table(HashTable *table)
{
    // Frees the table.
    for (int i = 0; i < table->size; i++)
    {
        Ht_item *item = table->items[i];

        if (item != NULL)
            free_item(item);
    }

    // Free the overflow bucket lists and its items.
    free_overflow_buckets(table);
    free(table->items);
    free(table);
}

void handle_collision(HashTable *table, unsigned long index, Ht_item *item)
{
    LinkedList *head = table->overflow_buckets[index];

    if (head == NULL)
    {
        // Creates the list.
        head = allocate_list();
        head->item = item;
        #ifdef CHECK_CREATE_ITEM
        printf("handle_collision insert head: %p\n",item);
        #endif
        head->next = NULL; // self-added
        table->overflow_buckets[index] = head;
        return;
    }
    else
    {
        // #ifdef SORT_KEY_ASCEND
        // if (strcmp(head->item->key, item->key)>0) {
        //   LinkedList *tmp = head;
        //   head->item=item;
        // }
        // #endif
        // Insert to the list.
        table->overflow_buckets[index] = linkedlist_insert(head, item);
        return;
    }
}

void ht_insert(HashTable *table, char *key, char *value)
{
    // Computes the index.
    int index = MR_DefaultHashPartition(key,table->size);

    Ht_item *current_item = table->items[index];

    /*
    totally empty, insert first elem
    */
    if (current_item == NULL)
    {
        // Creates the item.
        Ht_item *item = create_item(key, value,0);
        #ifdef CHECK_CREATE_ITEM
        printf("insert at the head: %p\n",item);
        #endif
        // Key does not exist.
        if (table->count == table->size)
        {
            // HashTable is full.
            printf("Insert Error: Hash Table is full\n");
            free_item(item);
            return;
        }

        // Insert directly.
        table->items[index] = item;
        table->count++;
    }
    else
    {
        int cmp = strcmp(current_item->key, key);
        // Scenario 1: Update the value.
        if (cmp == 0)
        {
            append_item_value(value,current_item);
            return;
        }
        else
        {
            // Scenario 2: Handle the collision.
            /*
            here inserted different key from `table->items[index]->key` to `overflow_buckets`.
            */
            #ifdef SORT_KEY_ASCEND
            Ht_item *item = create_item(key, value,0);
            #ifdef CHECK_CREATE_ITEM
            printf("create_item: %p\n",item);
            #endif
            // int find_key = 0;
            /*
            here search to 
            */
            if (cmp>0) {
              /*
              store_to_overflow_buckets not use.
              */
              // item->store_to_overflow_buckets=0;
              #ifdef CMP_LOG
              printf("%s > %s\n",current_item->key,key);
              #endif
              /*
              https://stackoverflow.com/questions/2302351/assign-one-struct-to-another-in-c#comment111331700_2302370
              not make them point to same addr, otherwise undefined -> sometimes `SIGSEGV` while sometimes not.
              */
              // Ht_item *tmp=current_item;
              Ht_item tmp;
              #ifdef CHECK_CREATE_ITEM
              printf("before swap: %p %p\n",current_item,item);
              #endif
              tmp=*current_item;
              *current_item=*item;
              *item=tmp;
              #ifdef CHECK_CREATE_ITEM
              printf("after swap: %p %p\n",current_item,item);
              #endif
            }else {
              // item->store_to_overflow_buckets=1;
            }
            #else
            Ht_item *item = create_item(key, value);
            #endif
            handle_collision(table, index,item);
            return;
        }
    }
}

char *ht_search(HashTable *table, char *key)
{
    // Searches for the key in the HashTable.
    // Returns NULL if it doesn't exist.
    int index = MR_DefaultHashPartition(key,table->size);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    // Provide only non-NULL values.
    if (item != NULL)
    {
        if (strcmp(item->key, key) == 0)
            /*
            only return first
            */
            return item->value[0];

        if (head == NULL)
            return NULL;

        item = head->item;
        head = head->next;
    }

    return NULL;
}

void ht_delete(HashTable *table, char *key)
{
    // Deletes an item from the table.
    int index = MR_DefaultHashPartition(key,table->size);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    if (item == NULL)
    {
        // Does not exist.
        return;
    }
    else
    {
        if (head == NULL && strcmp(item->key, key) == 0)
        {
            // Collision chain does not exist.
            // Remove the item.
            // Set table index to NULL.
            table->items[index] = NULL;
            free_item(item);
            table->count--;
            return;
        }
        else if (head != NULL)
        {
            // Collision chain exists.
            if (strcmp(item->key, key) == 0)
            {
                // Remove this item.
                // Set the head of the list as the new item.
                free_item(item);
                LinkedList *node = head;
                head = head->next;
                node->next = NULL;
                table->items[index] = copy_item(node->item);
                free_linkedlist(node);
                table->overflow_buckets[index] = head;
                return;
            }

            LinkedList *curr = head;
            LinkedList *prev = NULL;

            /*
            shift the left except the first which has been done.
            */

            while (curr)
            {
                if (strcmp(curr->item->key, key) == 0)
                {
                    if (prev == NULL)
                    {
                        // First element of the chain.
                        // Remove the chain.
                        free_linkedlist(head);
                        table->overflow_buckets[index] = NULL;
                        return;
                    }
                    else
                    {
                        // This is somewhere in the chain.
                        prev->next = curr->next;
                        curr->next = NULL;
                        free_linkedlist(curr);
                        table->overflow_buckets[index] = head;
                        return;
                    }
                }

                curr = curr->next;
                prev = curr;
            }
        }
    }
}

// void print_search(HashTable *table, char *key)
// {
//     char *val;

//     if ((val = ht_search(table, key)) == NULL)
//     {
//         printf("Key:%s does not exist\n", key);
//         return;
//     }
//     else
//     {
//         printf("Key:%s, Value:%s\n", key, val);
//     }
// }

void print_table(HashTable *table)
{
    printf("\nHash Table size: %d\n-------------------\n",table->size);

    for (int i = 0; i < table -> size; i++)
    {
        if (table -> items[i])
        {
            printf("Index:%d, Key:%s, Value:%s Len:%d\n",\
             i, table -> items[i] -> key, table -> items[i] -> value[0],table->items[i]->value_list_len);
        }
        LinkedList *tmp=table->overflow_buckets[i];
        if (tmp!=NULL) {
            printf("overflow_buckets:\n%10s %10s %10s\n","key","value","len");
            do{
                printf("%10s %10s %10d\n",tmp->item->key,tmp->item->value[0],tmp->item->value_list_len);
            }while((tmp=tmp->next)!=NULL);
        }
    }

    printf("-------------------\n\n");
}

// int main()
// {
//     HashTable *ht = create_table(CAPACITY);
//     ht_insert(ht, (char *)"1", (char *)"First address");
//     ht_insert(ht, (char *)"2", (char *)"Second address");
//     ht_insert(ht, (char *)"Hel", (char *)"Third address");
//     ht_insert(ht, (char *)"Cau", (char *)"Fourth address");
//     print_search(ht, (char *)"1");
//     print_search(ht, (char *)"2");
//     print_search(ht, (char *)"3");
//     print_search(ht, (char *)"Hel");
//     print_search(ht, (char *)"Cau"); // Collision!
//     print_table(ht);
//     ht_delete(ht, (char *)"1");
//     ht_delete(ht, (char *)"Cau");
//     print_table(ht);
//     free_table(ht);
//     return 0;
// }