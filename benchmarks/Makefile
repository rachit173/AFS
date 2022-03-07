all: read_test write_test
	echo "DONE"

read_test: read.cpp
	g++ read.cpp -o read_test

clean:
	rm -rf read_test write_test

write_test: write_test.cpp
	g++ write_test.cpp -o write_test