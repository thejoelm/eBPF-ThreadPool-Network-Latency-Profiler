APP := profiler
BPF_SRC := $(APP).bpf.c
BPF_OBJ := $(APP).bpf.o
SKEL := $(APP).skel.h
USER_SRC := main.cpp


SOURCE = ${TARGETS:=.ebpf.c}

CLANG ?= clang
CXX ?= g++
BPFTOOL ?= bpftool

.PHONY: all clean
all: $(APP)

# Step 1: compile kernel eBPF code into bytecode
$(BPF_OBJ): $(BPF_SRC)
	$(CLANG) -g -O2 -target bpf -c $(BPF_SRC) -o $(BPF_OBJ) -D __TARGET_ARCH_arm64

# Step 2: generate C++ skeleton blueprint
$(SKEL): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $(BPF_OBJ) > $(SKEL)

# Step 3: Compile C++ User Space app
$(APP): $(SKEL) $(USER_SRC)
	$(CXX) -g -O2 -Wall $(USER_SRC) -o $(APP) -lbpf -lelf

clean:
	rm -f $(APP) $(BPF_OBJ) $(SKEL)


