#ifndef __POSTINGS_H__
#define __POSTINGS_H__

#include "wiser.h"

int fetch_postings(const wiser_env *env, const int token_id,
                   postings_list **postings, int *postings_len);
void merge_inverted_index(inverted_index_hash *base,
                          inverted_index_hash *to_be_added);
void update_postings(const wiser_env *env, inverted_index_hash *p);
void dump_postings_list(const postings_list *postings);
void free_postings_list(postings_list *pl);
void dump_inverted_index(wiser_env *env, inverted_index_hash *ii);
void free_inverted_index(inverted_index_hash *ii);

#endif /* __POSTINGS_H__ */
