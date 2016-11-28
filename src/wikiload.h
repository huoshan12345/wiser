#ifndef __WIKILOAD_H__
#define __WIKILOAD_H__

#include "wiser.h"

typedef void (*add_document_callback)(wiser_env *env,
                                      const char *title,
                                      const char *body);

int load_wikipedia_dump(wiser_env *env, const char *path,
                        add_document_callback func, int max_article_count);

#endif /* __WIKILOAD_H__ */
