


# Resulting executable
EXE := build/gatt_outa_here

# Make it easy to scoop up new source files
OBJS = $(patsubst ./src/%.c,./build/%.o,$(wildcard ./src/*.c))
OBJS += $(patsubst ./src/%.cpp,./build/%.opp,$(wildcard ./src/*.cpp))

# Header locations
INCLUDE = -I./include \
	-I$(CASTLE_INCLUDE)/hardware

# Functions to help put "whole archive" modifiers around libraries
whole_archive = -Wl,--whole-archive,${CASTLE_LIB}/$(1),--no-whole-archive
all_whole_archives = $(foreach lib,$(1),$(call whole_archive,$(lib)))



# Libraries getting "whole archive"d
HW_LIBS := \
	libbtcore.a \
	libbt-hci.a \
	libbtbta.a \
	libbtif.a \
	libbtstack.a \
	libbtutils.a \
	libbtosi.a \
	libbtdevice.a \
	libhardware.a
HW_LIB_INVOKATION = $(call all_whole_archives,$(HW_LIBS))

#SHARED_LIBS_FLAGS = \
#	-lpthread \ 
#	-lz \
#	-ldl \
#	-lrt \
#	-lresolv \
#	-lbluetoothdefault \
#	-laudioutils \
#	-llog
SHARED_LIBS_FLAGS = -lpthread -lz -ldl -lrt -lresolv -lbluetoothdefault -laudioutils -llog -lcutils


.PHONY: all clean echo
all: $(EXE)

# Link executable
$(EXE): $(OBJS)
	@echo 'Linking source file(s) $(OBJS) together into $@...'
	@$(CXX) $(OBJS) $(COMMON_FLAGS) -rdynamic ${HW_LIB_INVOKATION} ${SHARED_LIBS_FLAGS} -o "$@" 
	@echo "Built $@"

# Build cpp
build/%.opp: src/%.cpp 
	@echo 'Building source file $<...'
	@$(CXX) $(COMMON_FLAGS) $(CXX_FLAGS) $(INCLUDE) -c -o "$@" "$<" 
	@echo 'Built $@'
	@echo

# Build c
%.o: %.c 
	@echo 'Building source file $<...'
	@$(CC) $(COMMON_FLAGS) ($CFLAGS) $(INCLUDE) -c -o "$@" "$<"
	@echo 'Built $@'
	@echo

clean:
	@rm -rf build/*
	@echo "Squeeky clean!"

test:
	@echo ${HW_LIB_INVOKATION}
