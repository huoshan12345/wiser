#include <stdio.h>

#include "util.h"
#include "database.h"

/**
 * 从字节序列中还原出倒排列表
 * @param[in] postings_e 待还原的倒排列表（字节序列）
 * @param[in] postings_e_size 待还原的倒排列表（字节序列）中的元素数
 * @param[out] postings 还原后的倒排列表
 * @param[out] postings_len 还原后的倒排列表中的元素数
 * @retval 0 成功
 *
 */
static int
decode_postings_none(const char *postings_e, int postings_e_size,
                     postings_list **postings, int *postings_len)
{
  const int *p, *pend;

  *postings = NULL;
  *postings_len = 0;
  for (p = (const int *)postings_e,
       pend = (const int *)(postings_e + postings_e_size); p < pend;) {
    postings_list *pl;
    int document_id, positions_count;

    document_id = *(p++);
    positions_count = *(p++);
    if ((pl = malloc(sizeof(postings_list)))) {
      int i;
      pl->document_id = document_id;
      pl->positions_count = positions_count;
      utarray_new(pl->positions, &ut_int_icd);
      LL_APPEND(*postings, pl);
      (*postings_len)++;

      /* decode positions */
      for (i = 0; i < positions_count; i++) {
        utarray_push_back(pl->positions, p);
        p++;
      }
    } else {
      p += positions_count;
    }
  }
  return 0;
}

/**
 * 将倒排列表转换成字节序列
 * @param[in] postings 倒排列表
 * @param[in] postings_len 倒排列表中的元素数
 * @param[out] postings_e 转换后的倒排列表
 * @retval 0 成功
 */
static int
encode_postings_none(const postings_list *postings,
                     const int postings_len,
                     buffer *postings_e)
{
  const postings_list *p;
  LL_FOREACH(postings, p) {
    int *pos = NULL;
    append_buffer(postings_e, (void *)&p->document_id, sizeof(int));
    append_buffer(postings_e, (void *)&p->positions_count, sizeof(int));
    while ((pos = (int *)utarray_next(p->positions, pos))) {
      append_buffer(postings_e, (void *)pos, sizeof(int));
    }
  }
  return 0;
}

/**
 * 从数据中的指定位置读取1个比特
 * @param[in,out] buf 数据的开头
 * @param[in] buf_end 数据的结尾
 * @param[in,out] bit 从变量buf的哪个位置读取1个比特
 * @return 读取出的比特值
 */
static inline int
read_bit(const char **buf, const char *buf_end, unsigned char *bit)
{
  int r;
  if (*buf >= buf_end) { return -1; }
  r = (**buf & *bit) ? 1 : 0;
  *bit >>= 1;
  if (!*bit) {
    *bit = 0x80;
    (*buf)++;
  }
  return r;
}

/**
 * 根据Golomb编码中的参数m，计算出编码和解码过程中所需的参数b和参数t
 * @param[in] m Golomb编码中的参数m
 * @param[out] b Golomb编码中的参数b。ceil(log2(m))
 * @param[out] t pow2(b) - m
 */
static void
calc_golomb_params(int m, int *b, int *t)
{
  int l;
  assert(m > 0);
  for (*b = 0, l = 1; m > l; (*b)++, l <<= 1) {}
  *t = l - m;
}

/**
 * 用Golomb编码对1个数值进行解码
 * @param[in] m Golomb编码中的参数m
 * @param[in] b Golomb编码中的参数b。ceil(log2(m))
 * @param[in] t pow2(b) - m
 * @param[in,out] buf 待解码的数据
 * @param[in] buf_end 待解码数据的结尾
 * @param[in,out] bit 待解码数据的起始比特
 * @return 解码后的数值
 */
static inline int
golomb_decoding(int m, int b, int t,
                const char **buf, const char *buf_end, unsigned char *bit)
{
  int n = 0;

  /* decode (n / m) with unary code */
  while (read_bit(buf, buf_end, bit) == 1) {
    n += m;
  }
  /* decode (n % m) */
  if (m > 1) {
    int i, r = 0;
    for (i = 0; i < b - 1; i++) {
      int z = read_bit(buf, buf_end, bit);
      if (z == -1) { print_error("invalid golomb code"); break; }
      r = (r << 1) | z;
    }
    if (r >= t) {
      int z = read_bit(buf, buf_end, bit);
      if (z == -1) {
        print_error("invalid golomb code");
      } else {
        r = (r << 1) | z;
        r -= t;
      }
    }
    n += r;
  }
  return n;
}

/**
 * 用Golomb编码对1个数值进行编码
 * @param[in] m Golomb编码中的参数m
 * @param[in] b Golomb编码中的参数b。ceil(log2(m))
 * @param[in] t pow2(b) - m
 * @param[in] n 待编码的数值
 * @param[in] buf 编码后的数据
 */
static inline void
golomb_encoding(int m, int b, int t, int n, buffer *buf)
{
  int i;
  /* encode (n / m) with unary code */
  for (i = n / m; i; i--) { append_buffer_bit(buf, 1); }
  append_buffer_bit(buf, 0);
  /* encode (n % m) */
  if (m > 1) {
    int r = n % m;
    if (r < t) {
      for (i = 1 << (b - 2); i; i >>= 1) {
        append_buffer_bit(buf, r & i);
      }
    } else {
      r += t;
      for (i = 1 << (b - 1); i; i >>= 1) {
        append_buffer_bit(buf, r & i);
      }
    }
  }
}

/**
 * 对经过Golomb编码的倒排列表进行解码
 * @param[in] postings_e 经过Golomb编码的倒排列表
 * @param[in] postings_e_size 经过Golomb编码的倒排列表中的元素数
 * @param[out] postings 解码后的倒排列表
 * @param[out] postings_len 解码后的倒排列表中的元素数
 * @retval 0 成功
 */
static int
decode_postings_golomb(const char *postings_e, int postings_e_size,
                       postings_list **postings, int *postings_len)
{
  const char *pend;
  unsigned char bit;

  pend = postings_e + postings_e_size;
  bit = 0x80;
  *postings = NULL;
  *postings_len = 0;
  {
    int i, docs_count;
    postings_list *pl;
    {
      int m, b, t, pre_document_id = 0;

      docs_count = *((int *)postings_e);
      postings_e += sizeof(int);
      m = *((int *)postings_e);
      postings_e += sizeof(int);
      calc_golomb_params(m, &b, &t);
      for (i = 0; i < docs_count; i++) {
        int gap = golomb_decoding(m, b, t, &postings_e, pend, &bit);
        if ((pl = malloc(sizeof(postings_list)))) {
          pl->document_id = pre_document_id + gap + 1;
          utarray_new(pl->positions, &ut_int_icd);
          LL_APPEND(*postings, pl);
          (*postings_len)++;
          pre_document_id = pl->document_id;
        } else {
          print_error("memory allocation failed.");
        }
      }
    }
    if (bit != 0x80) { postings_e++; bit = 0x80; }
    for (i = 0, pl = *postings; i < docs_count; i++, pl = pl->next) {
      int j, mp, bp, tp, position = -1;

      pl->positions_count = *((int *)postings_e);
      postings_e += sizeof(int);
      mp = *((int *)postings_e);
      postings_e += sizeof(int);
      calc_golomb_params(mp, &bp, &tp);
      for (j = 0; j < pl->positions_count; j++) {
        int gap = golomb_decoding(mp, bp, tp, &postings_e, pend, &bit);
        position += gap + 1;
        utarray_push_back(pl->positions, &position);
      }
      if (bit != 0x80) { postings_e++; bit = 0x80; }
    }
  }
  return 0;
}

/**
 * 对倒排列表进行Golomb编码
 * @param[in] documents_count 文档总数
 * @param[in] postings 待编码的倒排列表
 * @param[in] postings_len 待编码的倒排列表中的元素数
 * @param[in] postings_e 编码后的倒排列表
 * @retval 0 成功
 */
static int
encode_postings_golomb(int documents_count,
                       const postings_list *postings, const int postings_len,
                       buffer *postings_e)
{
  const postings_list *p;

  append_buffer(postings_e, &postings_len, sizeof(int));
  if (postings && postings_len) {
    int m, b, t;
    m = documents_count / postings_len;
    append_buffer(postings_e, &m, sizeof(int));
    calc_golomb_params(m, &b, &t);
    {
      int pre_document_id = 0;

      LL_FOREACH(postings, p) {
        int gap = p->document_id - pre_document_id - 1;
        golomb_encoding(m, b, t, gap, postings_e);
        pre_document_id = p->document_id;
      }
    }
    append_buffer(postings_e, NULL, 0);
  }
  LL_FOREACH(postings, p) {
    append_buffer(postings_e, &p->positions_count, sizeof(int));
    if (p->positions && p->positions_count) {
      const int *pp;
      int mp, bp, tp, pre_position = -1;

      pp = (const int *)utarray_back(p->positions);
      mp = (*pp + 1) / p->positions_count;
      calc_golomb_params(mp, &bp, &tp);
      append_buffer(postings_e, &mp, sizeof(int));
      pp = NULL;
      while ((pp = (const int *)utarray_next(p->positions, pp))) {
        int gap = *pp - pre_position - 1;
        golomb_encoding(mp, bp, tp, gap, postings_e);
        pre_position = *pp;
      }
      append_buffer(postings_e, NULL, 0);
    }
  }
  return 0;
}

/**
 * 对倒排列表进行还原或解码
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] postings_e 待还原或解码前的倒排列表
 * @param[in] postings_e_size 待还原或解码前的倒排列表中的元素数
 * @param[out] postings 还原或解码后的倒排列表
 * @param[out] postings_len 还原或解码后的倒排列表中的元素数
 * @retval 0 成功
 */
static int
decode_postings(const wiser_env *env,
                const char *postings_e, int postings_e_size,
                postings_list **postings, int *postings_len)
{
  switch (env->compress) {
  case compress_none:
    return decode_postings_none(postings_e, postings_e_size,
                                postings, postings_len);
  case compress_golomb:
    return decode_postings_golomb(postings_e, postings_e_size,
                                  postings, postings_len);
  default:
    abort();
  }
}

/**
 * 对倒排列表进行转换或编码
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] postings 待转换或编码前的倒排列表
 * @param[in] postings_len 待转换或编码前的倒排列表中的元素数
 * @param[out] postings_e 转换或编码后的倒排列表
 * @retval 0 成功
 */
static int
encode_postings(const wiser_env *env,
                const postings_list *postings, const int postings_len,
                buffer *postings_e)
{
  switch (env->compress) {
  case compress_none:
    return encode_postings_none(postings, postings_len, postings_e);
  case compress_golomb:
    return encode_postings_golomb(db_get_document_count(env),
                                  postings, postings_len, postings_e);
  default:
    abort();
  }
}

/**
 * 从数据库中获取关联到指定词元上的倒排列表
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] token_id 词元编号
 * @param[out] postings 获取到的倒排列表
 * @param[out] postings_len 获取到的倒排列表中的元素数
 * @retval 0 成功
 * @retval -1 失败
 */
int
fetch_postings(const wiser_env *env, const int token_id,
               postings_list **postings, int *postings_len)
{
  char *postings_e;
  int postings_e_size, docs_count, rc;

  rc = db_get_postings(env, token_id, &docs_count, (void **)&postings_e,
                       &postings_e_size);
  if (!rc && postings_e_size) {
    /* 只有当倒排列表非空时，才进行解码 */
    int decoded_len;
    if (decode_postings(env, postings_e, postings_e_size, postings,
                        &decoded_len)) {
      print_error("postings list decode error");
      rc = -1;
    } else if (docs_count != decoded_len) {
      print_error("postings list decode error: stored:%d decoded:%d.\n",
                  *postings_len, decoded_len);
      rc = -1;
    }
    if (postings_len) { *postings_len = decoded_len; }
  } else {
    *postings = NULL;
    if (postings_len) { *postings_len = 0; }
  }
  return rc;
}

/**
 * 获取将两个倒排列表合并后得到的倒排列表
 * @param[in] pa 要合并的倒排列表
 * @param[in] pb 要合并的倒排列表
 *
 * @return 合并后的倒排列表
 *
 * @attention 若base和to_be_added（参见函数merge_inverted_index）中的任意一个被破坏了，
 *            或者二者中含有相同的文档编号，则该函数的行为不可预知
 */
static postings_list *
merge_postings(postings_list *pa, postings_list *pb)
{
  postings_list *ret = NULL, *p;
  /* 用pa和pb分别遍历base和to_be_added（参见函数merge_inverted_index）中的倒排列表中的元素， */
  /* 将二者连接成按文档编号升序排列的链表 */
  while (pa || pb) {
    postings_list *e;
    if (!pb || (pa && pa->document_id <= pb->document_id)) {
      e = pa;
      pa = pa->next;
    } else if (!pa || pa->document_id >= pb->document_id) {
      e = pb;
      pb = pb->next;
    } else {
      abort();
    }
    e->next = NULL;
    if (!ret) {
      ret = e;
    } else {
      p->next = e;
    }
    p = e;
  }
  return ret;
}

/**
 * 将内存上（小倒排索引中）的倒排列表与存储器上的倒排列表合并后存储到数据库中
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] p 含有倒排列表的倒排索引中的索引项
 */
void
update_postings(const wiser_env *env, inverted_index_value *p)
{
  int old_postings_len;
  postings_list *old_postings;

  if (!fetch_postings(env, p->token_id, &old_postings,
                      &old_postings_len)) {
    buffer *buf;
    if (old_postings_len) {
      p->postings_list = merge_postings(old_postings, p->postings_list);
      p->docs_count += old_postings_len;
    }
    if ((buf = alloc_buffer())) {
      encode_postings(env, p->postings_list, p->docs_count, buf);
      db_update_postings(env, p->token_id, p->docs_count,
                         BUFFER_PTR(buf), BUFFER_SIZE(buf));
      free_buffer(buf);
    }
  } else {
    print_error("cannot fetch old postings list of token(%d) for update.",
                p->token_id);
  }
}

/**
 * 合并两个倒排索引
 * @param[in] base 合并后其中的元素会增多的倒排索引（合并目标）
 * @param[in] to_be_added 合并后就被释放的倒排索引（合并源）
 *
 */
void
merge_inverted_index(inverted_index_hash *base,
                     inverted_index_hash *to_be_added)
{
  inverted_index_value *p, *temp;

  HASH_ITER(hh, to_be_added, p, temp) {
    inverted_index_value *t;
    HASH_DEL(to_be_added, p);
    HASH_FIND_INT(base, &p->token_id, t);
    if (t) {
      t->postings_list = merge_postings(t->postings_list, p->postings_list);
      t->docs_count += p->docs_count;
      free(p);
    } else {
      HASH_ADD_INT(base, token_id, p);
    }
  }
}

/**
 * 打印倒排列表中的内容。用于调试
 * @param[in] postings 待打印的倒排列表
 */
void
dump_postings_list(const postings_list *postings)
{
  const postings_list *pl;
  LL_FOREACH(postings, pl) {
    printf("doc_id %d (", pl->document_id);
    if (pl->positions) {
      const int *p = NULL;
      while ((p = (const int *)utarray_next(pl->positions, p))) {
        printf("%d ", *p);
      }
    }
    printf(")\n");
  }
}

/**
 * 释放倒排列表
 * @param[in] pl 待释放的倒排列表中的首元素
 */
void
free_postings_list(postings_list *pl)
{
  postings_list *a, *a2;
  LL_FOREACH_SAFE(pl, a, a2) {
    if (a->positions) {
      utarray_free(a->positions);
    }
    LL_DELETE(pl, a);
    free(a);
  }
}

/**
 * 输出倒排索引的内容
 * @param[in] ii 指向倒排索引的指针
 */
void
dump_inverted_index(wiser_env *env, inverted_index_hash *ii)
{
  inverted_index_value *it;
  for (it = ii; it != NULL; it = it->hh.next) {
    int token_len;
    const char *token;

    if (it->token_id) {
      db_get_token(env, it->token_id, &token, &token_len);
      printf("TOKEN %d.%.*s(%d):\n", it->token_id, token_len, token,
             it->docs_count);
    } else {
      puts("TOKEN NONE:");
    }
    if (it->postings_list) {
      printf("POSTINGS: [\n  ");
      dump_postings_list(it->postings_list);
      puts("]");
    }
  }
}

/**
 * 释放倒排索引
 * @param[in] ii 指向倒排索引的指针
 */
void
free_inverted_index(inverted_index_hash *ii)
{
  inverted_index_value *cur;
  while (ii) {
    cur = ii;
    HASH_DEL(ii, cur);
    if (cur->postings_list) {
      free_postings_list(cur->postings_list);
    }
    free(cur);
  }
}
