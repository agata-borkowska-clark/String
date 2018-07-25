include config.mk

.PHONY: all opt debug run clean

all: test
opt: all
debug: all

run: all
	./test

clean:
	rm -f test

test: src/Test.cpp src/String.cpp
	${CXX} ${CPPFLAGS} ${CXXFLAGS} ${LDFLAGS} ${LDLIBS} $^ -o $@
