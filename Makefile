BIN := musicled
SRCS := $(wildcard *.cpp)

OBJDIR := .objs
DEPDIR := .deps
OBJS := $(patsubst %,$(OBJDIR)/%.o,$(basename $(SRCS)))
DEPS := $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS)))

$(shell mkdir -p $(dir $(OBJS)) >/dev/null)
$(shell mkdir -p $(dir $(DEPS)) >/dev/null)

CXX := g++
LD := g++

CXXFLAGS := -std=c++11
CPPFLAGS := -Wall -Wextra -g -O3 -ffast-math
LDFLAGS := -L/usr/local/lib -pthread
LDLIBS := -lasound -lm -lfftw3 -lX11
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.d

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/%.o: %.cpp
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

all: $(BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(BIN)
