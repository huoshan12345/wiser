#include "util.h"
#include "token.h"
#include "postings.h"
#include "database.h"

#include <stdio.h>

/**
 * 检查输入的字符（UTF-32）是否不属于索引对象
 * @param[in] ustr 输入的字符（UTF-32）
 * @return 是否是空白字符
 * @retval 0 不是空白字符
 * @retval 1 是空白字符
 */
static int
wiser_is_ignored_char(const UTF32Char ustr)
{
  switch (ustr) {
  case ' ': case '\f': case '\n': case '\r': case '\t': case '\v':
  case '!': case '"': case '#': case '$': case '%': case '&':
  case '\'': case '(': case ')': case '*': case '+': case ',':
  case '-': case '.': case '/':
  case ':': case ';': case '<': case '=': case '>': case '?': case '@':
  case '[': case '\\': case ']': case '^': case '_': case '`':
  case '{': case '|': case '}': case '~':
  case 0x3000: /* 全角空格 */
  case 0x3001: /* 、 */
  case 0x3002: /* 。 */
  case 0xFF08: /* （ */
  case 0xFF09: /* ） */
  case 0xFF01: /* ！ */
  case 0xFF0C: /* ， */
  case 0xFF1A: /* ： */
  case 0xFF1B: /* ； */
  case 0xFF1F: /* ? */
    return 1;
  default:
    return 0;
  }
}

/**
 * 将输入的字符串分割为N-gram
 * @param[in] ustr 输入的字符串（UTF-8）
 * @param[in] ustr_end 输入的字符串中最后一个字符的位置
 * @param[in] n N-gram中N的取值。建议将其设为大于1的值
 * @param[out] start 词元的起始位置
 * @return 分割出来的词元的长度
 */
static int
ngram_next(const UTF32Char *ustr, const UTF32Char *ustr_end,
           unsigned int n, const UTF32Char **start)
{
  int i;
  const UTF32Char *p;

  /* 读取时跳过文本开头的空格等字符 */
  for (; ustr < ustr_end && wiser_is_ignored_char(*ustr); ustr++) {
  }

  /* 不断取出最多包含n个字符的词元，直到遇到不属于索引对象的字符或到达了字符串的尾部 */
  for (i = 0, p = ustr; i < n && p < ustr_end
       && !wiser_is_ignored_char(*p); i++, p++) {
  }

  *start = ustr;
  return p - ustr;
}

/**
 * 为inverted_index_value分配存储空间并对其进行初始化
 * @param[in] token_id 词元编号
 * @param[in] docs_count 包含该词元的文档数
 * @return 生成的inverted_index_value
 */
static inverted_index_value *
create_new_inverted_index(int token_id, int docs_count)
{
  inverted_index_value *ii_entry;

  ii_entry = malloc(sizeof(inverted_index_value));
  if (!ii_entry) {
    print_error("cannot allocate memory for an inverted index.");
    return NULL;
  }
  ii_entry->positions_count = 0;
  ii_entry->postings_list = NULL;
  ii_entry->token_id = token_id;
  ii_entry->docs_count = docs_count;

  return ii_entry;
}

/**
 * 为倒排列表分配存储空间并对其进行并初始化
 * @param[in] document_id 文档编号
 * @return 生成的倒排列表
 */
static postings_list *
create_new_postings_list(int document_id)
{
  postings_list *pl;

  pl = malloc(sizeof(postings_list));
  if (!pl) {
    print_error("cannot allocate memory for a postings list.");
    return NULL;
  }
  pl->document_id = document_id;
  pl->positions_count = 1;
  utarray_new(pl->positions, &ut_int_icd);

  return pl;
}

/**
 * 为传入的词元创建倒排列表
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] document_id 文档编号
 * @param[in] token 词元（UTF-8）
 * @param[in] token_size 词元的长度（以字节为单位）
 * @param[in] position 词元出现的位置
 * @param[in,out] postings 倒排列表的数组
 * @retval 0 成功
 * @retval -1 失败
 */
int
token_to_postings_list(wiser_env *env,
                       const int document_id, const char *token,
                       const unsigned int token_size,
                       const int position,
                       inverted_index_hash **postings)
{
  postings_list *pl;
  inverted_index_value *ii_entry;
  int token_id, token_docs_count;

  token_id = db_get_token_id(
               env, token, token_size, document_id, &token_docs_count);
  if (*postings) {
    HASH_FIND_INT(*postings, &token_id, ii_entry);
  } else {
    ii_entry = NULL;
  }
  if (ii_entry) {
    pl = ii_entry->postings_list;
    pl->positions_count++;
  } else {
    ii_entry = create_new_inverted_index(token_id,
                                         document_id ? 1 : token_docs_count);
    if (!ii_entry) { return -1; }
    HASH_ADD_INT(*postings, token_id, ii_entry);

    pl = create_new_postings_list(document_id);
    if (!pl) { return -1; }
    LL_APPEND(ii_entry->postings_list, pl);
  }
  /* 存储位置信息 */
  utarray_push_back(pl->positions, &position);
  ii_entry->positions_count++;
  return 0;
}

/**
 * 为构成文档内容的字符串建立倒排列表的集合
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] document_id 文档编号。为0时表示把要查询的关键词作为处理对象
 * @param[in] text 输入的字符串
 * @param[in] text_len 输入的字符串的长度
 * @param[in] n N-gram中N的取值
 * @param[in,out] postings 倒排列表的数组（也可视作是指向小倒排索引的指针）。若传入的指针指向了NULL，
 *                         则表示要新建一个倒排列表的数组（小倒排索引）。若传入的指针指向了之前就已经存在的倒排列表的数组，
 *                         则表示要添加元素
 * @retval 0 成功
 * @retval -1 失败
 */
int
text_to_postings_lists(wiser_env *env,
                       const int document_id, const UTF32Char *text,
                       const unsigned int text_len,
                       const int n, inverted_index_hash **postings)
{
  /* FIXME: now same document update is broken. */
  int t_len, position = 0;
  const UTF32Char *t = text, *text_end = text + text_len;

  inverted_index_hash *buffer_postings = NULL;

  for (; (t_len = ngram_next(t, text_end, n, &t)); t++, position++) {
    /* 检索时，忽略掉由t中长度不足N-gram的最后几个字符构成的词元 */
    if (t_len >= n || document_id) {
      int retval, t_8_size;
      char t_8[n * MAX_UTF8_SIZE];

      utf32toutf8(t, t_len, t_8, &t_8_size);

      retval = token_to_postings_list(env, document_id, t_8, t_8_size,
                                      position, &buffer_postings);
      if (retval) { return retval; }
    }
  }

  if (*postings) {
    merge_inverted_index(*postings, buffer_postings);
  } else {
    *postings = buffer_postings;
  }

  return 0;
}

/**
 * 打印指定的词元
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] token_id 词元编号
 */
void
dump_token(wiser_env *env, int token_id)
{
  int token_len;
  const char *token;

  db_get_token(env, token_id, &token, &token_len);
  printf("token: %.*s (id: %d)\n", token_len, token, token_id);
}
