# Makefile

#CLANG_FLAGS = -target bpf -O2 -Wall -g
CLANG_FLAGS = -target bpf -O2 -fno-jump-tables -Wall -g

#CFLAGS = -O2 -Wall -g
CFLAGS = -O2 -Wall

LIBBPF = -lbpf
#LIBBPF = -lelf ~/dev/linux/tools/lib/bpf/libbpf.a

OBJS =	pass_kern.o \
	drop_kern.o \
	print_kern.o \
	ethertype_kern.o \
	map_array_kern.o \
	live_kern.o \
	ait_kern.o \
	link_kern.o \
	multi_kern.o

TEST =	code

PROGS =	$(TEST) \
	ait_user \
	link_user \
	nprocs

ALL =	$(PROGS) $(OBJS)

all: $(ALL)

clean:
	rm -f $(ALL) *.ll

test: $(TEST)
	./code

pass_kern.o: pass_kern.c
	clang $(CLANG_FLAGS) -c -o pass_kern.o pass_kern.c

drop_kern.o: drop_kern.c
	clang $(CLANG_FLAGS) -c -emit-llvm -o drop_kern.ll drop_kern.c
	llc -march=bpf -filetype=obj -o drop_kern.o drop_kern.ll

print_kern.o: print_kern.c
	clang $(CLANG_FLAGS) -c -emit-llvm -o print_kern.ll print_kern.c
	llc -march=bpf -filetype=obj -o print_kern.o print_kern.ll

ethertype_kern.o: ethertype_kern.c
	clang $(CLANG_FLAGS) -c -o ethertype_kern.o ethertype_kern.c

map_array_kern.o: map_array_kern.c
	clang $(CLANG_FLAGS) -c -o map_array_kern.o map_array_kern.c

live_kern.o: live_kern.c code.c
	clang $(CLANG_FLAGS) -c -o live_kern.o live_kern.c

ait_kern.o: ait_kern.c code.c
	clang $(CLANG_FLAGS) -c -o ait_kern.o ait_kern.c

ait_user: ait_user.c
	cc $(CFLAGS) -o ait_user ait_user.c $(LIBBPF)

link_kern.o: link_kern.c code.c
	clang $(CLANG_FLAGS) -c -o link_kern.o link_kern.c

link_user: link_user.c
	cc $(CFLAGS) -o link_user link_user.c $(LIBBPF)

multi_kern.o: multi_kern.c
	clang $(CLANG_FLAGS) -c -o multi_kern.o multi_kern.c

code: code.c
	cc $(CFLAGS) -DTEST_MAIN -o code code.c

nprocs: nprocs.c

ifndef LINK
    $(warning "Warning: `LINK' not set in environment, default LINK=enp6s0 for carol")
    LINK:=enp6s0
endif

define link
    sudo ip l set dev $(1) $(2) $(3)
endef


netinit: ; @$(call link,$(LINK),up) && $(call link,$(LINK),multicast,off) && $(call link,$(LINK),allmulticast,off) && echo "$(LINK) ready for transactions"

e1000e:
	@+ make -C e1000e-3.8.4/src CFLAGS_EXTRA="-DE1000E_NO_NAPI"
	@+ sudo rmmod e1000e || true
	@+ sudo insmod e1000e-3.8.4/src/e1000e.ko && echo "e1000e loaded"

