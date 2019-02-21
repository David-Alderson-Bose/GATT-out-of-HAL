
EXE := build/gatt_outa_here
OBJS = $(patsubst ./src/%.c,./build/%.o,$(wildcard ./src/*.c))
OBJS += $(patsubst ./src/%.cpp,./build/%.opp,$(wildcard ./src/*.cpp))
INCLUDE = -I./include \
	-I$(CASTLE_INCLUDE)/hardware

#0(error $(INCLUDE))

.PHONY: all clean echo
all: $(EXE)

# Link executable
$(EXE): $(OBJS)
	@echo 'Linking source file(s) $(OBJS) together into $@...'
	@$(CXX) $(COMMON_FLAGS) -o "$@" $(OBJS)
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

echo:
	@echo "Echo!"
