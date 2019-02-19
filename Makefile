
EXE := build/gatt_outa_here
OBJS = $(patsubst ./src/%.c,./build/%.o,$(wildcard ./src/*.c))
OBJS += $(patsubst ./src/%.cpp,./build/%.opp,$(wildcard ./src/*.cpp))
INCLUDE += ./include


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
	@$(CXX) $(COMMON_FLAGS) $(CXX_FLAGS) -I$(INCLUDE) -c -o "$@" "$<" 
	@echo 'Built $@'
	@echo

# Build c
%.o: %.c 
	@echo 'Building source file $<...'
	@$(CC) $(COMMON_FLAGS) ($CFLAGS) -I$(INCLUDE) -c -o "$@" "$<"
	@echo 'Built $@'
	@echo

clean:
	@rm -rf build/*
	@echo "Squeeky clean!"

echo:
	@echo "Echo!"
