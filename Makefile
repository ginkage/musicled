SRCDIR := src
OBJDIR := .objs
DEPDIR := .deps

BIN := musicled

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:$(SRCDIR)/%.cpp=$(DEPDIR)/%.d)

$(shell mkdir -p $(dir $(OBJS)) >/dev/null)
$(shell mkdir -p $(dir $(DEPS)) >/dev/null)

CXX := g++
LD := g++

CXXFLAGS := -std=c++11
CPPFLAGS := -Wall -Wextra -g -O3 -ffast-math -fdata-sections -ffunction-sections
LDFLAGS := -L/usr/local/lib -pthread -Wl,--gc-sections
LDLIBS := -lasound -lm -lfftw3 -lX11 -lcurl
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.d

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

all: $(BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(BIN)
