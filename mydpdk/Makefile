# binary name
APP = mydpdk

# all source are stored in SRCS-y
SRCS-y := mydpdk.c

PKGCONF ?= pkg-config

export PKG_CONFIG_PATH=/home/zxy/workspace/30Gsda5/mydpdk/dpdk-stable-25.11.1/build/meson-private

export C_INCLUDE_PATH ?=
export C_INCLUDE_PATH := $(C_INCLUDE_PATH):/home/zxy/workspace/30Gsda5/mydpdk/dpdk-stable-25.11.1/build/include

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif


all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	echo $(PKG_CONFIG_PATH)
	echo $(C_INCLUDE_PATH)
static: build/$(APP)-static

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

CFLAGS += -I. -DALLOW_EXPERIMENTAL_API -D_GNU_SOURCE

OBJS := $(patsubst %.c,build/%.o,$(SRCS-y))

build/%.o: %.c Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(APP)-shared: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP)* build/*.o
	test -d build && rmdir -p build || true
