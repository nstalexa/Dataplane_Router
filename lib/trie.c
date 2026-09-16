#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"
#include "protocols.h"
#include <string.h>
#include "trie.h"

//Function to create a node for a trie.
TrieNode * create_node() {
    TrieNode *node = (TrieNode *) malloc(sizeof(TrieNode));
    node->end_of_word = 0;
    node->rtable = NULL;

    node->children = (TrieNode **) malloc(2 * sizeof(TrieNode *));
    node->children[0] = NULL;
    node->children[1] = NULL;

    return node;
}


void insert_node (TrieNode *root, int addr, struct route_table_entry *rtable) {
    TrieNode *curr = root;
    int pref_len = 0;

    for(int i = 31; i >= 0; i--) {
        if ((ntohl(rtable->mask) >> i) & 1)
            pref_len++;
        else
            break;
    }

    for(int i = 31; i >= 32 - pref_len; i--) {
        int bit = (addr >> i) & 1;

        if(curr->children[bit] == NULL) {
            curr->children[bit] = create_node();
        }

        curr = curr->children[bit];

        //We iterate through each bit adding a child on the correct bit
        //position.
    }
    curr -> end_of_word = 1;
    curr->rtable = rtable;

    //Adding a node means inserting an address to the trie, 
    //in order to ease each search for any address.

}

struct route_table_entry * longest_prefix_match (TrieNode *root, int addr) {

    struct route_table_entry * match = NULL;
    TrieNode * curr = root;

    for(int i = 31; i >= 0; i--) {
        int bit = (addr >> i) & 1;        

        if(curr->children[bit] == NULL) {
            return match;
        }


        curr = curr->children[bit];
        if(curr->rtable != NULL) {
            match = curr->rtable;
        }
        
    }
    return match;

    //Longest prefix match algorithm, we search for
    //the longest sequence of bits in the trie that match the
    //given address and return the match.

}