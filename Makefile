SRCDIR := src
TESTDIR := test
OBJDIR := .objs
DEPDIR := .deps

BIN := musicled

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:$(SRCDIR)/%.cpp=$(DEPDIR)/%.d)

LIB_OBJS := $(filter-out $(OBJDIR)/$(BIN).o, $(OBJS))

TEST_SRCS := $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJS := $(TEST_SRCS:$(TESTDIR)/%.cpp=$(OBJDIR)/%.o)
TEST_BIN := $(TEST_SRCS:$(TESTDIR)/%.cpp=%)
TEST_DEPS := $(TEST_SRCS:$(TESTDIR)/%.cpp=$(DEPDIR)/%.d)

$(shell mkdir -p $(dir $(OBJS)) >/dev/null)
$(shell mkdir -p $(dir $(DEPS)) >/dev/null)

CXX := g++
LD := g++

CXXFLAGS := -std=c++11
CPPFLAGS := -Wall -Wextra -g -O3 -ffast-math -fdata-sections -ffunction-sections
LDFLAGS := -L/usr/local/lib -pthread -Wl,--gc-sections
LDLIBS := -lasound -lm -lfftw3f -lX11 -lcurl -lpulse -lpulse-simple
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.d

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(TEST_OBJS): $(OBJDIR)/%.o : $(TESTDIR)/%.cpp
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(TEST_BIN): % : $(OBJDIR)/%.o $(LIB_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

-include $(DEPS) $(TEST_DEPS)

all: $(BIN)

test: $(TEST_BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(BIN) $(TEST_BIN)
