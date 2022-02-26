// Hello filesystem class implementation

#include <iostream>
#include <string>
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
// #include "examples/protos/helloworld.grpc.pb.h"
#else
#include "protos/afs.grpc.pb.h"
#endif
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using afs::Greeter;
using afs::HelloRequest;
using afs::HelloReply;
// include in one .cpp file
// #include "Fuse-impl.h"
// #include "Fuse.h"

// class HelloFS : public Fusepp::Fuse<HelloFS>
// {
// public:
//   HelloFS() {}

//   ~HelloFS() {}

//   static int getattr (const char *, struct stat *, struct fuse_file_info *);

//   static int readdir(const char *path, void *buf,
//                      fuse_fill_dir_t filler,
//                      off_t offset, struct fuse_file_info *fi,
//                      enum fuse_readdir_flags);
  
//   static int open(const char *path, struct fuse_file_info *fi);

//   static int read(const char *path, char *buf, size_t size, off_t offset,
//                   struct fuse_file_info *fi);
// };

// using namespace std;

// static const string root_path = "/";
// static const string hello_str = "Hello World!\n";
// static const string hello_path = "/hello";

// int HelloFS::getattr(const char *path, struct stat *stbuf, struct fuse_file_info *)
// {
// 	int res = 0;

// 	memset(stbuf, 0, sizeof(struct stat));
// 	if (path == root_path) {
// 		stbuf->st_mode = S_IFDIR | 0755;
// 		stbuf->st_nlink = 2;
// 	} else if (path == hello_path) {
// 		stbuf->st_mode = S_IFREG | 0444;
// 		stbuf->st_nlink = 1;
// 		stbuf->st_size = hello_str.length();
// 	} else
// 		res = -ENOENT;

// 	return res;
// }

// int HelloFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
// 			               off_t, struct fuse_file_info *, enum fuse_readdir_flags)
// {
// 	if (path != root_path)
// 		return -ENOENT;

// 	filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
// 	filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
// 	filler(buf, hello_path.c_str() + 1, NULL, 0, FUSE_FILL_DIR_PLUS);

// 	return 0;
// }


// int HelloFS::open(const char *path, struct fuse_file_info *fi)
// {
//   const char* client_cache_path = "/tmp/afs/client_cache";
// 	if (path != hello_path)
// 		return -ENOENT;

// 	if ((fi->flags & 3) != O_RDONLY)
// 		return -EACCES;

// 	return 0;
// }


// int HelloFS::read(const char *path, char *buf, size_t size, off_t offset,
// 		              struct fuse_file_info *)
// {
// 	if (path != hello_path)
// 		return -ENOENT;

// 	size_t len;
// 	len = hello_str.length();
// 	if ((size_t)offset < len) {
// 		if (offset + size > len)
// 			size = len - offset;
// 		memcpy(buf, hello_str.c_str() + offset, size);
// 	} else
// 		size = 0;

// 	return size;
// }

static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

int main(int argc, char* argv[]) {
  // char buf[1024*1024];
  // struct fuse_file_info fi;
  // fi.flags = O_RDONLY;
  // HelloFS::open(hello_path.c_str(), &fi);
  // int output = HelloFS::read(hello_path.c_str(), buf, 1024*1024, 0, &fi);
  // cout << "read output: " << output << endl;
  int ret;
 	struct fuse_args args = FUSE_ARGS_INIT(argc, argv); 
  struct fuse_operations hello_oper = {
  };
  // ret = fuse_main(argc, argv, &hello_oper, NULL);
}