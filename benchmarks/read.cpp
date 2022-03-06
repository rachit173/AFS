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

#define MOUNT_DIR "/users/bijan/mount/"
#define CACHE_DIR "/tmp/afs_prototype/"

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
    uint64_t uncached_total = 0;
    uint64_t cached_total = 0;
    uint64_t filesize;
    std::string filename;
    struct stat statbuf;
    char *buf;
    int ret;
    int fd;
    int iterations = 50;

    if (argc < 2) {
        filename = "file";
    } else {
        filename = std::string(argv[1]);
    }

    std::string file_path = MOUNT_DIR + filename;
    std::string cache_path = CACHE_DIR + filename;

    ret = stat(file_path.c_str(), &statbuf);
    std::cout << file_path << std::endl;
    if (ret == -1) {
        std::cout << "Error stating file " << strerror(errno) << std::endl;
    }
    filesize = statbuf.st_size;

    buf = (char *)malloc(sizeof(char) * filesize);

    for (int i = 0; i < iterations; i++) {
        // First, read the file when it is uncached
        uint64_t time = read_file(file_path, filesize, buf);
        if (time == -1) {
            return -1;
        }
        uncached_total += time;

        // Clear the page cache
        system("sync; echo 3 > /proc/sys/vm/drop_caches");

        // Now, read it again while it is still cached
        time = read_file(file_path, filesize, buf);
        if (time == -1) {
            return -1;
        }
        cached_total += time;

        // Clear the page cache
        system("sync; echo 3 > /proc/sys/vm/drop_caches");

        // Delete the file from the cache so we can record the time again
        ret = remove(cache_path.c_str());
    }

    std::cout << "Average uncached read time " << uncached_total / iterations << std::endl;
    std::cout << "Average cached read time " << cached_total / iterations << std::endl;

    return 0;
}
