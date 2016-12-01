#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

typedef uint32_t
UTF32Char; /* 经过UTF-32编码的Unicode字符串 */
#define MAX_UTF8_SIZE 4 /* 用UTF-8表示1个Unicode字符最多需要多少个字节 */

typedef struct {
  char *head;       /* 指向缓冲区的开头 */
  char *curr;       /* 指向缓冲区中的当前位置 */
  const char *tail; /* 指向缓冲区的结尾 */
  int bit;          /* 缓冲区的当前位置（以比特为单位） */
} buffer;

#define BUFFER_PTR(b) ((b)->head) /* 返回指向缓冲区开头的指针 */
#define BUFFER_SIZE(b) ((b)->curr - (b)->head) /* 返回缓冲区的大小 */

int print_error(const char *format, ...);
buffer *alloc_buffer(void);
int append_buffer(buffer *buf, const void *data,
                  unsigned int data_size);
void free_buffer(buffer *buf);
void append_buffer_bit(buffer *buf, int bit);
int uchar2utf8_size(const UTF32Char *ustr, int ustr_len);
char *utf32toutf8(const UTF32Char *ustr, int ustr_len, char *str,
                  int *str_size);
int utf8toutf32(const char *str, int str_size, UTF32Char **ustr,
                int *ustr_len);
void print_time_diff(void);

#endif /* __UTIL_H__ */
