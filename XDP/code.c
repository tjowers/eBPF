/*
 * code.c -- data encoding/decoding
 *
 * NOTE: This file is meant to be #include'd, not separately compiled.
 *       With -DTEST_MAIN, it builds a standalone unit-test program.
 */
#include "code.h"

static int u64_to_bytes(__u8 *data, __u8 *end, __u64 num)
{
    if (data + 8 > end) return 0;
    data[0] = num;
    num >>= 8;
    data[1] = num;
    num >>= 8;
    data[2] = num;
    num >>= 8;
    data[3] = num;
    num >>= 8;
    data[4] = num;
    num >>= 8;
    data[5] = num;
    num >>= 8;
    data[6] = num;
    num >>= 8;
    data[7] = num;
    return 8;
}

static __u64 bytes_to_u64(__u8 *data, __u8 *end)
{
    __u64 num = 0;

    if (data + 8 > end) return 0;
    num |= data[7];
    num <<= 8;
    num |= data[6];
    num <<= 8;
    num |= data[5];
    num <<= 8;
    num |= data[4];
    num <<= 8;
    num |= data[3];
    num <<= 8;
    num |= data[2];
    num <<= 8;
    num |= data[1];
    num <<= 8;
    num |= data[0];
    return num;
}

static int parse_int(__u8 *data, __u8 *end, int *int_ptr)
{
    int offset = 0;
    int i = 0;
    __u8 b;

    if (data + offset >= end) return 0;  // out of bounds
    b = data[offset++];
    switch (b) {
        case m_int_0: i = -1;  /* FALL-THRU */
        case p_int_0: {
            if (data + offset >= end) return 0;  // out of bounds
            b = data[offset++];
            int n = SMOL2INT(b);  // size (in bytes)
            if (n > sizeof(int)) return 0;  // too big for int
            if (data + offset + n > end) return 0;  // out of bounds
            switch (n) {
                case 8:  i = (i << 8) | data[offset + 7];  /* FALL-THRU */
                case 7:  i = (i << 8) | data[offset + 6];  /* FALL-THRU */
                case 6:  i = (i << 8) | data[offset + 5];  /* FALL-THRU */
                case 5:  i = (i << 8) | data[offset + 4];  /* FALL-THRU */
                case 4:  i = (i << 8) | data[offset + 3];  /* FALL-THRU */
                case 3:  i = (i << 8) | data[offset + 2];  /* FALL-THRU */
                case 2:  i = (i << 8) | data[offset + 1];  /* FALL-THRU */
                case 1:  i = (i << 8) | data[offset + 0];  /* FALL-THRU */
                case 0:  break;
                default: return 0;  // range error
            }
            offset += n;
            break;
        }
        default: {
            i = SMOL2INT(b);
            if ((i < SMOL_MIN) || (i > SMOL_MAX)) return 0;  // range error
            break;
        }
    }
    *int_ptr = i;  // store int value
    return offset;
}

static int parse_int16(__u8 *data, __u8 *end, __s16 *ip)
{
    __s16 i = 0;

    if (data + 4 > end) return 0;  // out of bounds
    if (data[0] == m_int_0) {
        i = -1;
    } else if (data[0] != p_int_0) {
        return 0;  // require +/- Int pad=0
    }
    if (data[1] != n_2) return 0;  // require size=2
    i <<= 8;
    i |= data[2];
    i <<= 8;
    i |= data[3];
    *ip = i;
    return 4;
}

static int code_int16(__u8 *data, __u8 *end, __s16 i)
{
    if (data + 4 > end) return 0;  // out of bounds
    data[0] = (i < 0) ? m_int_0 : p_int_0;  // +/- Int, pad = 0
    data[1] = n_2;  // size = 2
    data[2] = i;  // lsb
    data[3] = i >> 8;  // msb
    return 4;
}


#ifdef TEST_MAIN
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#if (INT_MAX == 0x7FFF)
#define INT_BIT 16
#else
#if (INT_MAX == 0x7FFFFFFF)
#define INT_BIT 32
#else
#if (INT_MAX == 0x7FFFFFFFFFFFFFFF)
#define INT_BIT 64
#else
#error `int` must be 16, 32, or 64 bits!
#endif
#endif
#endif

void
test_int()
{
    __u8 buf[32];
    __u8 buf_0[] = { n_0 };
    __u8 buf_1[] = { null };
    __u8 buf_2[] = { p_int_0, n_0 };
    __u8 buf_3[] = { m_int_0, n_0 };
    __u8 buf_4[] = { p_int_0, n_2, 0xFE, 0xFF };
    __u8 buf_5[] = { m_int_0, n_2, 0xFE, 0xFF };
    __u8 buf_6[] = { n_126 };
    __u8 buf_7[] = { n_m64 };
#if (INT_BIT == 32)
    __u8 buf_8[] = { p_int_0, n_4, 0x98, 0xBA, 0xDC, 0xFE };
#endif
#if (INT_BIT == 64)
    __u8 buf_8[] = { p_int_0, n_8,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE };
#endif
    __u8 buf_9[] = { p_int_0, n_4, 0x10, 0x32, 0x54, 0x76 };
    __u8 buf_10[] = { p_int_0, n_10,
        'N', 'o', 't', 'A', 'N', 'u', 'm', 'b', 'e', 'r' };
    __u8 buf_11[] = { octets, n_16,
        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
        0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
        0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8,
        0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0 };
    __u64 u;
    int n;
    int i;

    printf("sizeof(int)=%d INT_BIT=%d\n", sizeof(int), INT_BIT);

    i = 0xDEAD;
    n = parse_int(buf_0, buf_0 + sizeof(buf_0), &i);
    printf("buf_0: n=%d i=%d\n", n, i);
    assert(n == 1);
    assert(i == 0);

    i = 0xDEAD;
    n = parse_int(buf_1, buf_1 + sizeof(buf_1), &i);
    printf("buf_1: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 0);
    assert(i == 0xDEAD);

    i = 0xDEAD;
    n = parse_int(buf_2, buf_2 + sizeof(buf_2), &i);
    printf("buf_2: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 2);
    assert(i == 0);

    i = 0xDEAD;
    n = parse_int(buf_3, buf_3 + sizeof(buf_3), &i);
    printf("buf_3: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 2);
    assert(i == -1);

    i = 0xDEAD;
    n = parse_int(buf_4, buf_4 + sizeof(buf_4), &i);
    printf("buf_4: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 4);
    assert(i == 65534);

    i = 0xDEAD;
    n = parse_int(buf_5, buf_5 + sizeof(buf_5), &i);
    printf("buf_5: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 4);
    assert(i == -2);

    i = 0xDEAD;
    n = parse_int(buf_6, buf_6 + sizeof(buf_6), &i);
    printf("buf_6: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 1);
    assert(i == SMOL_MAX);

    i = 0xDEAD;
    n = parse_int(buf_7, buf_7 + sizeof(buf_7), &i);
    printf("buf_7: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 1);
    assert(i == SMOL_MIN);

    i = 0xDEAD;
    n = parse_int(buf_8, buf_8 + sizeof(buf_8), &i);
    printf("buf_8: n=%d i=%d (0x%x)\n", n, i, i);
#if (INT_BIT == 32)
    assert(n == 6);
    n = 4275878552;
    assert(i == n);
#endif
#if (INT_BIT == 64)
    assert(n == 10);
    n = 1147797409030816545;
    assert(i == n);
#endif

    i = 0xDEAD;
    n = parse_int(buf_9, buf_9 + sizeof(buf_9), &i);
    printf("buf_9: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 6);
    assert(i == 1985229328);

    i = 0xDEAD;
    n = parse_int(buf_10, buf_10 + sizeof(buf_10), &i);
    printf("buf_10: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 0);
    assert(i == 0xDEAD);

    n = code_int16(buf, buf + sizeof(buf), -12345);
    assert(n == 4);
    i = 0xDEAD;
    n = parse_int(buf, buf + sizeof(buf), &i);
    printf("buf: n=%d i=%d (0x%x)\n", n, i, i);
    assert(n == 4);
    assert(i == -12345);

    u = bytes_to_u64(buf_11 + 2, buf_11 + 10);
    u64_to_bytes(buf, buf + sizeof(buf), u);
}

int
main()
{
    test_int();
    exit(EXIT_SUCCESS);
}
#endif /* TEST_MAIN */
