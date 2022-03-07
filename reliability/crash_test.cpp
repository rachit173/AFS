//
// Created by Yifei Yang on 3/6/22.
//

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

const std::string CrashTestMountDir = "/users/yyf123/AFS/mountdir/";
const std::string CrashTestServerDir = "/users/yyf123/AFS/reliability/crash_mountdir/";
const std::string CrashTestFileName = "crash_test_file";
const std::string CrashTestOriginalFileName = "crash_test_original_file";
const std::string CrashTestFileOriginalContent = "old content";
const std::string CrashTestFileWriteContent = "new content";
long retry_gap_ms = 1000;

void restore_original_test_file() {
  std::string crash_test_original_file_path = CrashTestServerDir + CrashTestOriginalFileName;
  std::string crash_test_file_path = CrashTestServerDir + CrashTestFileName;
  std::string rmCmd = "rm -r " + crash_test_file_path;
  std::string cpCmd = "cp " + crash_test_original_file_path + " " + crash_test_file_path;
  system(rmCmd.c_str());
  system(cpCmd.c_str());
}

int read_file(std::string filename, char* buf, uint64_t size) {
  int fd = open(filename.c_str(), O_RDONLY | O_CREAT, 0644);
  if (fd == -1) {
    std::cout << "Error opening uncached file " << strerror(errno) << std::endl;
    return -1;
  }

  int ret = read(fd, buf, size);
  if (ret == -1) {
    std::cout << "Error reading file " << strerror(errno) << std::endl;
    return -1;
  }

  close(fd);
  return 0;
}

int write_file(std::string filename, const char* buf, uint64_t size) {
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
  if (fd == -1) {
    std::cout << "Error opening uncached file " << strerror(errno) << std::endl;
    return -1;
  }

  int ret = write(fd, buf, size);
  if (ret == -1) {
    std::cout << "Error writing file " << strerror(errno) << std::endl;
    return -1;
  }

  close(fd);
  return 0;
}

void crash_open_read_test() {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;
  char *buf = (char *)malloc(sizeof(char) * CrashTestFileOriginalContent.size());

  // first read should fail due to crash
  int ret = read_file(crash_test_file_path, buf, CrashTestFileOriginalContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "pass: " << "first read failed due to crash" << std::endl;
  } else {
    std::cout << "[CHECK] " << "failed: " << "first read should fail due to crash" << std::endl;
  }

  usleep(1000 * retry_gap_ms);

  // second read to check correctness
  ret = read_file(crash_test_file_path, buf, CrashTestFileOriginalContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "failed: " << "second read should succeed" << std::endl;
  } else {
    if (std::string(buf) == CrashTestFileOriginalContent) {
      std::cout << "[CHECK] " << "pass: " << "second read succeed, file content is correct" << std::endl;
    } else {
      printf("%s\n", buf);
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong" << std::endl;
    }
  }
}

void crash_write_test() {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;

  // first write should fail due to crash
  int ret = write_file(crash_test_file_path, CrashTestFileWriteContent.c_str(), CrashTestFileWriteContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "pass: " << "first write failed due to crash" << std::endl;
  } else {
    std::cout << "[CHECK] " << "failed: " << "first write should fail due to crash" << std::endl;
  }

  usleep(1000 * retry_gap_ms);

  // second write should succeed
  ret = write_file(crash_test_file_path, CrashTestFileWriteContent.c_str(), CrashTestFileWriteContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "failed: " << "second write should succeed" << std::endl;
  } else {
    std::cout << "[CHECK] " << "pass: " << "second write succeeds" << std::endl;
  }

  // check correctness
  char *read_buf = (char *)malloc(CrashTestFileWriteContent.size());
  ret = read_file(crash_test_file_path, read_buf, CrashTestFileWriteContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "failed: " << "read should succeed" << std::endl;
  } else {
    if (std::string(read_buf) == CrashTestFileWriteContent) {
      std::cout << "[CHECK] " << "pass: " << "read succeed, file content is correct" << std::endl;
    } else {
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong" << std::endl;
    }
  }
}

void crash_flush_test() {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;

  // write should succeed because crash on flush won't affect write
  int ret = write_file(crash_test_file_path, CrashTestFileWriteContent.c_str(), CrashTestFileWriteContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "failed: " << "write should succeed" << std::endl;
  } else {
    std::cout << "[CHECK] " << "pass: " << "write succeeds" << std::endl;
  }

  // check correctness
  char *read_buf = (char *)malloc(CrashTestFileWriteContent.size());
  ret = read_file(crash_test_file_path, read_buf, CrashTestFileWriteContent.size());
  if (ret == -1) {
    std::cout << "[CHECK] " << "failed: " << "read should succeed" << std::endl;
  } else {
    if (std::string(read_buf) == CrashTestFileWriteContent) {
      std::cout << "[CHECK] " << "pass: " << "read succeed, file content is correct" << std::endl;
    } else {
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong" << std::endl;
    }
  }
}

/// set crash type before running this
int main(int argc, char* argv[]) {
  if ((argc < 2)) {
    fprintf(stderr, "Usage: %s <crash_test_type>\n", argv[0]);
    return 1;
  }

  int crash_test_type = std::stoi(argv[1]);
  switch (crash_test_type) {
    case 1: {
      crash_open_read_test();
      break;
    }
    case 2: {
      crash_write_test();
      break;
    }
    case 3: {
      crash_flush_test();
      break;
    }
  }

  return 0;
}