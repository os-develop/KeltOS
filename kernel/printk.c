#include "kernel/args.h"
#include "kernel/printk.h"
#include "kernel/types.h"
#include "kernel/defs.h"
#include "drivers/semihosting.h"

enum { MAX_LEN_U32 = 10, MAX_LEN_S32 = 11, MAX_LEN_PTR = 10, MAX_LEN_XU32 = 8 };
static const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

void puts(const char* str)
{
    smhost_printz(str);
}

size_t bprintu32(char* buf, u32 a, int base) {
    size_t i;
    size_t bytes;

    if (a == 0) {
        buf[0] = '0';
        return 1;
    }
    for (bytes = 0; a > 0; bytes++) {
        buf[bytes] = digits[a % base];
        a /= base;
    }
    for (i = 0; i < bytes / 2; i++) {
        buf[i] ^= buf[bytes - i - 1];
        buf[bytes - i - 1] ^= buf[i];
        buf[i] ^= buf[bytes - i - 1];
    }
    return bytes;
}

size_t bprints32(char* buf, s32 a, int base) {
    if (a < 0) {
        *(buf++) = '-';
        return 1 + bprintu32(buf, -a, base);
    }
    return bprintu32(buf, a, base);
}

size_t bprintstr(char* buf, const char* str) {
    const char* cur = str;
    size_t length = 0;
    while (*cur) {
        buf[length++] = *(cur++);
    }
    return length;
}

size_t bprintptr(char* buf, void* ptr) {
    size_t bytes;
    if (ptr == NULL) {
        return bprintstr(buf, "(nil)");
    }
    bytes = bprintstr(buf, "0x");
    return bytes + bprintu32(buf + bytes, (u32) ptr, 16); 
}

void printu32(u32 a)
{
    char buf[MAX_LEN_U32 + 1];
    size_t bytes = bprintu32(buf, a, 10);
    buf[bytes] = '\0';
    puts(buf);
}

size_t __max_length(const char* fmt) {
    size_t length = 1; /* Reserved for \0 */
    const char* cursor = fmt;
    while (*cursor) {
        switch (*cursor) {
        case '%':
            switch (*(++cursor)) {
            case 'u':
                length += MAX_LEN_U32;
                break;
            case 'x':
                length += MAX_LEN_XU32;
                break;
            case 'd':
                length += MAX_LEN_S32;
                break;
            case 'p':
                length += MAX_LEN_PTR;
                break;
            default:
                break;
            }
            ++cursor;
            break;
        default:
            ++cursor;
            ++length;
            break;
        }
    }
    return length;
}

void printk(const char* fmt, ...) {
    const char* cursor = fmt;
    int idle = 1;
    va_list args;
    u32 u32value;
    s32 s32value;
    void* ptrvalue;
    char buf[__max_length(fmt)];
    char* b_cursor = buf;

    va_start(args, fmt);
    while (*cursor) {
        if (idle) {
            if (*cursor != '%') {
                *(b_cursor++) = *cursor;
            } else {
                idle = 0;
            }
        } else {
            switch (*cursor) {
            case 'u':
                u32value = va_arg(args, u32);
                b_cursor += bprintu32(b_cursor, u32value, 10);
                break;
            case 'x':
                u32value = va_arg(args, u32);
                b_cursor += bprintu32(b_cursor, u32value, 16);
                break;
            case 'd':
                s32value = va_arg(args, s32);
                b_cursor += bprints32(b_cursor, s32value, 10);
                break;
            case 'p':
                ptrvalue = va_arg(args, void*);
                b_cursor += bprintptr(b_cursor, ptrvalue);
                break;
            default:
                break;
            }
            idle = 1;
        }
        ++cursor;
    }
    va_end(args);
    
    *b_cursor = '\0';
    puts(buf);
}

