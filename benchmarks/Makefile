all: read_test read_local write_test write_local
	echo "DONE"

read_test: read.cpp
	g++ read.cpp -o read_test

read_local: read_local.cpp
	g++ read_local.cpp -o read_local

clean:
	rm -rf read_test read_local write_test

write_test: write_test.cpp
	g++ write_test.cpp -o write_test

write_local: write_local.cpp
	g++ write_local.cpp -o write_local
