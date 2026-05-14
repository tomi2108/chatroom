include settings.mk

# Compiler
CC = gcc

# Source files
CLIENT_SRC = src/client.c
SERVER_SRC = src/server.c

# Static library dependencies
DEPS := $(foreach STL,$(STATIC_LIBPATHS),$(STL:%=%/bin/lib$(notdir $(STL)).a))

# Include directories
IDIRS += $(addsuffix /src,$(STATIC_LIBPATHS) .)

# Library directories
LIBDIRS := $(addsuffix /bin,$(STATIC_LIBPATHS))

# Main object files
CLIENT_OBJ := obj/client.o
SERVER_OBJ := obj/server.o

# Output binaries
CLIENT_BIN := bin/client
SERVER_BIN := bin/server

BINS := $(CLIENT_BIN) $(SERVER_BIN)

# ------------------------------------------------------------------------------

.PHONY: all
all: debug

.PHONY: debug
debug: CFLAGS = $(CDEBUG)
debug: $(BINS)

.PHONY: release
release: CFLAGS = $(CRELEASE)
release: $(BINS)

# ------------------------------------------------------------------------------

.PHONY: clean
clean:
	-rm -rfv obj bin

# ------------------------------------------------------------------------------

# Client executable
$(CLIENT_BIN): $(CLIENT_OBJ) 
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o "$@" $^ \
	$(IDIRS:%=-I%) \
	$(LIBDIRS:%=-L%) \
	$(LIBS:%=-l%)

# Server executable
$(SERVER_BIN): $(SERVER_OBJ) 
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o "$@" $^ \
	$(IDIRS:%=-I%) \
	$(LIBDIRS:%=-L%) \
	$(LIBS:%=-l%)

# ------------------------------------------------------------------------------

# Compile shared src/ files
obj/%.o: src/%.c $(SRCS_H) $(DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o "$@" $< $(IDIRS:%=-I%)

# Compile client.c
$(CLIENT_OBJ): $(CLIENT_SRC) $(SRCS_H) $(DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o "$@" $< $(IDIRS:%=-I%)

# Compile server.c
$(SERVER_OBJ): $(SERVER_SRC) $(SRCS_H) $(DEPS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o "$@" $< $(IDIRS:%=-I%)

# ------------------------------------------------------------------------------

.SECONDEXPANSION:

$(DEPS): $$(shell find $$(patsubst %bin/,%src/,$$(dir $$@)) -iname "*.c" -or -iname "*.h")
	make --no-print-directory -C $(patsubst %bin/,%,$(dir $@)) \
	3>&1 1>&2 2>&3 | \
	sed -E 's,(src/)[^ ]+\.(c|h)\:,$(patsubst %bin/,%,$(dir $@))&,' \
	3>&2 2>&1 1>&3

# ------------------------------------------------------------------------------

.PHONY: client
client: $(CLIENT_BIN)

.PHONY: server
server: $(SERVER_BIN)

# ------------------------------------------------------------------------------

.PHONY: start-client
start-client: debug
	valgrind --tool=none ./$(CLIENT_BIN) $(ARGS)

.PHONY: start-server
start-server: debug
	valgrind --tool=none ./$(SERVER_BIN) $(ARGS)

# ------------------------------------------------------------------------------

.PHONY: memcheck-client
memcheck-client: debug
	valgrind --leak-check=full $(MEMCHECK_FLAGS) ./$(CLIENT_BIN) $(ARGS)

.PHONY: memcheck-server
memcheck-server: debug
	valgrind --leak-check=full $(MEMCHECK_FLAGS) ./$(SERVER_BIN) $(ARGS)

# ------------------------------------------------------------------------------

.PHONY: helgrind-client
helgrind-client: debug
	valgrind --tool=helgrind $(HELGRIND_FLAGS) ./$(CLIENT_BIN) $(ARGS)

.PHONY: helgrind-server
helgrind-server: debug
	valgrind --tool=helgrind $(HELGRIND_FLAGS) ./$(SERVER_BIN) $(ARGS)
