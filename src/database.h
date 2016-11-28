#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <sqlite3.h>

#include "wiser.h"

int init_database(wiser_env *env, const char *db_path);
void fin_database(wiser_env *env);
int db_get_document_id(const wiser_env *env,
                       const char *title, unsigned int title_size);
int db_get_document_title(const wiser_env *env, int document_id,
                          const char **const title, int *title_size);
int db_add_document(const wiser_env *env,
                    const char *title, unsigned int title_size,
                    const char *body, unsigned int body_size);
int db_get_token_id(const wiser_env *env,
                    const char *str, unsigned int str_size, int insert,
                    int *docs_count);
int db_get_token(const wiser_env *env,
                 const int token_id,
                 const char **const token, int *token_size);
int db_get_postings(const wiser_env *env, int token_id,
                    int *docs_count, void **postings, int *postings_size);
int db_update_postings(const wiser_env *env, int token_id,
                       int docs_count,
                       void *postings, int postings_size);
int db_get_settings(const wiser_env *env, const char *key,
                    int key_size,
                    const char **value, int *value_size);
int db_replace_settings(const wiser_env *env, const char *key,
                        int key_size,
                        const char *value, int value_size);
int db_get_document_count(const wiser_env *env);
int begin(const wiser_env *env);
int commit(const wiser_env *env);
int rollback(const wiser_env *env);

#endif /* __DATABASE_H__ */
