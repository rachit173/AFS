#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <string>

uint64_t read_file(std::string filename, uint64_t size, char* buf) {
    auto start = std::chrono::high_resolution_clock::now();
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cout << "Error opening uncached file " << strerror(errno) << std::endl;
        return -1;
    }

    int ret = read(fd, buf, size);
    auto end = std::chrono::high_resolution_clock::now();

    if (ret == -1) {
        std::cout << "Error reading file " << strerror(errno) << std::endl;
        return -1;
    } else if (ret != size) {
        std::cout << "Read " << ret << " Expected to read " << size << std::endl;
        return -1;
    }
    close(fd);

    auto diff = end - start;
    return diff.count();
}

int main(int argc, char *argv[]) {
    uint64_t total = 0;
    uint64_t filesize;
    std::string mount_dir;
    std::string filename;
    struct stat statbuf;
    char *buf;
    int ret;
    int fd;
    int iterations = 50;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file to read>" << std::endl;
        return -1;
    }

    filename = std::string(argv[1]);

    ret = stat(filename.c_str(), &statbuf);
    std::cout << filename << std::endl;
    if (ret == -1) {
        std::cout << "Error stating file " << strerror(errno) << std::endl;
    }
    filesize = statbuf.st_size;

    buf = (char *)malloc(sizeof(char) * filesize);

    for (int i = 0; i < iterations; i++) {
        uint64_t time = read_file(filename, filesize, buf);
        if (time == -1) {
            return -1;
        }
        total += time;

        // Clear the page cache
        system("sync; echo 3 > /proc/sys/vm/drop_caches");
    }

    std::cout << "Average read time " << total / iterations << std::endl;

    return 0;
}
