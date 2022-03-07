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

void crash_open_read_test(bool is_client_crash) {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;
  char *buf = (char *)malloc(sizeof(char) * CrashTestFileOriginalContent.size());

  // client crash: first read should fail due to crash
  // server crash: first read should succeed because of retrying
  int ret = read_file(crash_test_file_path, buf, CrashTestFileOriginalContent.size());
  if (is_client_crash) {
    if (ret == -1) {
      std::cout << "[CHECK] (client crash) " << "pass: " << "first read failed due to crash" << std::endl;
    } else {
      std::cout << "[CHECK] (client crash) " << "failed: " << "first read should fail due to crash" << std::endl;
    }
  } else {
    if (ret == -1) {
      std::cout << "[CHECK] (server crash) " << "failed: " << "first read should succeed" << std::endl;
    } else {
      std::cout << "[CHECK] (server crash) " << "pass: " << "first read succeeds" << std::endl;
    }
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
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong, content: " << std::endl;
      printf("%s\n", buf);
    }
  }
}

void crash_write_test(bool is_client_crash) {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;

  // client crash: first write should fail due to crash
  // server crash: first write should succeed because of retrying
  int ret = write_file(crash_test_file_path, CrashTestFileWriteContent.c_str(), CrashTestFileWriteContent.size());
  if (is_client_crash) {
    if (ret == -1) {
      std::cout << "[CHECK] (client crash) " << "pass: " << "first write failed due to crash" << std::endl;
    } else {
      std::cout << "[CHECK] (client crash) " << "failed: " << "first write should fail due to crash" << std::endl;
    }
  } else {
    if (ret == -1) {
      std::cout << "[CHECK] (server crash) " << "failed: " << "first write should succeed" << std::endl;
    } else {
      std::cout << "[CHECK] (server crash) " << "pass: " << "first write succeeds" << std::endl;
    }
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
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong, content: " << std::endl;
      printf("%s\n", read_buf);
    }
  }
}

void crash_flush_test(bool is_client_crash) {
  restore_original_test_file();

  std::string crash_test_file_path = CrashTestMountDir + CrashTestFileName;

  // client crash: write should succeed because crash on flush won't affect write
  // server crash: write should succeed because of retrying
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
      std::cout << "[CHECK] " << "failed: " << "second read succeed, file content is wrong, content" << std::endl;
      printf("%s\n", read_buf);
    }
  }
}

/// set crash type before running this
int main(int argc, char* argv[]) {
  if ((argc < 3)) {
    fprintf(stderr, "Usage: %s <crash_test_type> <is_client_crash>\n", argv[0]);
    return 1;
  }

  int is_client_crash = (std::stoi(argv[1]) == 1);

  int crash_test_type = std::stoi(argv[2]);
  switch (crash_test_type) {
    case 1: {
      crash_open_read_test(is_client_crash);
      break;
    }
    case 2: {
      crash_write_test(is_client_crash);
      break;
    }
    case 3: {
      crash_flush_test(is_client_crash);
      break;
    }
  }

  return 0;
}