CXXFLAGS=-Wall -std=c++11
LDFLAGS=-lz
PREFIX?=/usr/local

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SRCS))
TRGT=psxwadgen

$(TRGT): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

clean:
	rm -f $(TRGT) $(OBJS) *.pke *.str

install: psxwadgen
	install -d $(DESTDIR)/$(PREFIX)/bin/
	install $(TRGT) $(DESTDIR)/$(PREFIX)/bin/
