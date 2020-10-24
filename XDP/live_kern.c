/*
 * live_kern.c -- XDP in-kernel eBPF filter
 *
 * Implement link-liveness protocol in XDP
 */
#include <stddef.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include "bpf_helpers.h"
#include "bpf_endian.h"

#define USE_BPF_MAPS 1  // monitor/control from userspace

#if USE_BPF_MAPS
#include <iproute2/bpf_elf.h>

struct bpf_elf_map liveness_map SEC("maps") = {
    .type       = BPF_MAP_TYPE_ARRAY,
    .size_key   = sizeof(__u32),
    .size_value = sizeof(__u64),
    .pinning    = PIN_GLOBAL_NS,
    .max_elem   = 4,
};
#endif /* USE_BPF_MAPS */

#define USE_CODE_C 1  // include encode/decode helpers

#if USE_CODE_C
#include "code.c"  // data encoding/decoding
#else
#include "code.h"  // data encoding/decoding
#endif

#define PERMISSIVE 1  // allow non-protocol packets to pass through

#define ETH_P_DALE (0xDA1E)

static void copy_mac(void *dst, void *src)
{
    __u16 *d = dst;
    __u16 *s = src;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

static void swap_mac_addrs(void *ethhdr)
{
    __u16 tmp[3];
    __u16 *eth = ethhdr;

    copy_mac(tmp, eth);
    copy_mac(eth, eth + 3);
    copy_mac(eth + 3, tmp);
}

static int
fwd_state(int state)
{
    switch (state) {
    case 0:  return 1;
    case 1:  return 2;
    case 2:  return 1;
    default: return 0;
    }
}

#if 0
static int
rev_state(int state)
{
    switch (state) {
    case 0:  return 0;
    case 1:  return 2;
    case 2:  return 1;
    default: return 0;
    }
}
#endif

static int next_seq_num(int seq_num)
{
#if USE_BPF_MAPS
    __u32 key;
    __u64 *value_ptr;

    key = 3;
    value_ptr = bpf_map_lookup_elem(&liveness_map, &key);
    if (value_ptr) {
        __sync_fetch_and_add(value_ptr, 1);
        seq_num = *value_ptr;
    } else {
        ++seq_num;
    }
#else
    ++seq_num;
#endif
    return seq_num;
}

static int handle_message(struct xdp_md *ctx)
{
    __u8 *msg_base = (void *)(long)ctx->data;
    __u8 *msg_start = msg_base + ETH_HLEN;
    __u8 *msg_limit = (void *)(long)ctx->data_end;

    __u8 *msg_cursor = msg_start;
    __u8 *msg_end = msg_limit;
    int size = 0;
    int count = -1;
#if USE_CODE_C
    int n;
#endif

    if (msg_cursor >= msg_end) return XDP_DROP;  // out of bounds
    __u8 b = *msg_cursor++;
    if (b == array) {
        // get array size (in bytes)
        if (msg_cursor >= msg_end) return XDP_DROP;  // out of bounds
        b = *msg_cursor++;
        size = SMOL2INT(b);
        if ((size < SMOL_MIN) || (size > SMOL_MAX)) {
            return XDP_DROP;  // bad size
        }
        msg_end = msg_cursor + size;  // limit to array contents
        if (msg_end > msg_limit) return XDP_DROP;  // out of bounds
    } else {
        return XDP_DROP;  // bad message type
    }

    __u8 *msg_content = msg_cursor;  // start of array elements
//    bpf_printk("array size=%d count=%d\n", size, count);

    // get `state` field
    if (msg_cursor >= msg_end) return XDP_DROP;  // out of bounds
    b = *msg_cursor++;
    int state = SMOL2INT(b);
    if ((state < 0) || (state > 2)) {
        return XDP_DROP;  // bad state
    }

    // get `other` field
    if (msg_cursor >= msg_end) return XDP_DROP;  // out of bounds
    b = *msg_cursor++;
    int other = SMOL2INT(b);
    if ((other < 0) || (other > 2)) {
        return XDP_DROP;  // bad other
    }

    // get `seq_num` field
#if USE_CODE_C
    int seq_num;
    n = parse_int(msg_cursor, msg_end, &seq_num);
    if (n <= 0) return XDP_DROP;  // parse error
    msg_cursor += n;
#else
    if (msg_cursor >= msg_end) return XDP_DROP;  // out of bounds
    b = *msg_cursor++;
    int seq_num = SMOL2INT(b);
#endif

    bpf_printk("%d,%d #%d <--\n", state, other, seq_num);

    // calculate new state
    state = other;  // swap self <-> other
    other = fwd_state(state);
    seq_num = next_seq_num(seq_num);

    // prepare reply message
    swap_mac_addrs(msg_base);
    msg_content[0] = INT2SMOL(state);
    msg_content[1] = INT2SMOL(other);
#if USE_CODE_C
    n = code_int16(msg_content + 2, msg_end, seq_num);
    if (n <= 0) return XDP_DROP;  // coding error
#else
    msg_content[2] = INT2SMOL(seq_num);
#endif

    bpf_printk("%d,%d #%d <--\n", state, other, seq_num);

    return XDP_TX;  // send updated frame out on same interface
}

SEC("prog")
int xdp_filter(struct xdp_md *ctx)
{
//    __u32 data_len = ctx->data_end - ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    if (data + ETH_ZLEN > data_end) {
        return XDP_DROP;  // frame too small
    }
    struct ethhdr *eth = data;
    __u16 eth_proto = bpf_ntohs(eth->h_proto);
    if (eth_proto != ETH_P_DALE) {
#if PERMISSIVE
        return XDP_PASS;  // pass frame on to networking stack
#else
        return XDP_DROP;  // wrong protocol
#endif
    }

    int rc = handle_message(ctx);
//    bpf_printk("proto=0x%x len=%lu rc=%d\n", eth_proto, data_len, rc);

    return rc;
}

char __license[] SEC("license") = "GPL";

