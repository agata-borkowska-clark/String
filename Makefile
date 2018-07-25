include config.mk

.PHONY: run
run: test
	./test

test: src/Test.cpp src/String.cpp
	${CXX} ${CPPFLAGS} ${CXXFLAGS} ${LDFLAGS} ${LDLIBS} $^ -o $@

.PHONY: clean
clean:
	rm -f test
