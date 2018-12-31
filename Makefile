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
CPPFLAGS := -Wall -Wextra -g -O3
LDFLAGS := -L/usr/local/lib
LDLIBS := -lpthread -lasound -lm -lfftw3 -ltinfo -lX11
DEPFLAGS = -MT $@ -MD -MP -MF $(DEPDIR)/$*.Td

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) $(LDLIBS) -o $@ $^

$(OBJDIR)/%.o: %.cpp
$(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.d
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

.PRECIOUS = $(DEPDIR)/%.d
$(DEPDIR)/%.d: ;

-include $(DEPS)

all: $(BIN)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(BIN)
