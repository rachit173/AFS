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

// #define MOUNT_DIR "/users/rrt/afs/mountdir/"
#define CACHE_DIR "/tmp/afs_prototype/"

uint64_t write_file(std::string filename, uint64_t size, char* buf) {
    auto start = std::chrono::high_resolution_clock::now();
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        std::cout << "Error opening uncached file " << strerror(errno) << std::endl;
        return -1;
    }

    int ret = write(fd, buf, size);
    auto end = std::chrono::high_resolution_clock::now();
    
    if (ret == -1) {
        std::cout << "Error writing file " << strerror(errno) << std::endl;
        return -1;
    } else if (ret != size) {
        std::cout << "Wrote " << ret << " Expected to write " << size << std::endl;
        return -1;
    }
    close(fd);

    auto diff = end - start;
    return diff.count();
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cout << "Usage: " << argv[0] << "<mountdir> <filename> <filesize> <num_iterations>"  << std::endl;
    return -1;
  }
  std::string mount_dir = std::string(argv[1]);
  std::string filename = std::string(argv[2]);
  std::string file_path = mount_dir + filename;
  std::string cache_path = CACHE_DIR + filename;
  uint64_t filesize = atoi(argv[3]);
  struct stat statbuf;
  char* buf;
  int ret;
  int fd;
  int iterations = atoi(argv[4]);

  // ret = stat(file_path.c_str(), &statbuf);
  // std::cout << file_path << std::endl;
  // if (ret == -1) {
  //     std::cout << "Error stating file " << strerror(errno) << std::endl;
  // }  
  // Prepare 
  char* write_buf = (char*)malloc(filesize+1);
  for (int i = 0; i < filesize; i++) {
    write_buf[i] = 'a'+ i%26;
  }
  write_buf[filesize] = '\0';
  uint64_t uncached_total = 0;
  uint64_t cached_total = 0;
  for (int i = 0; i < iterations; i++) {
    unlink(filename.c_str());
    // Write file when it is uncached.
    uint64_t time = write_file(file_path, filesize, write_buf);
    if (time == -1) {
      std::cout << "Error writing file" << std::endl;
      return -1;
    }
    uncached_total += time;
    // Clear the page cache
    system("sync; echo 3 > /proc/sys/vm/drop_caches");

    // Now, write it again when it is cached.
    time = write_file(file_path, filesize, write_buf);
    if (time == -1) {
      std::cout << "Error writing file" << std::endl;
      return -1;
    }
    cached_total += time;
    // Clear the page cache
    system("sync; echo 3 > /proc/sys/vm/drop_caches");

    // Delete the file from the cache so we can record the time it takes to write it again.
    ret = remove(cache_path.c_str());
  }
  std::cout << "Average uncached write time (us) " << uncached_total / (iterations*1000.) << std::endl;
  std::cout << "Average cached write time (us) " << cached_total / (iterations*1000.) << std::endl;
  return 0;
}