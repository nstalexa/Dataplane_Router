#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct TrieNode TrieNode;
struct TrieNode {
    struct route_table_entry * rtable;
    int end_of_word; //0 if no 1 if yes
    TrieNode** children;
};

TrieNode * create_node();
void insert_node (TrieNode *root, int addr, struct route_table_entry *rtable);
struct route_table_entry * longest_prefix_match (TrieNode *root, int addr);