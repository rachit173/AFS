#include <iostream>
#include <string>
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <grpcpp/grpcpp.h>

#include <ctype.h>
#include <libgen.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifdef BAZEL_BUILD
#else
#include "protos/afs.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using afs::Greeter;
using afs::HelloRequest;
using afs::HelloReply;

/**
 * Do stuff on mounting. 
 */
static void *afs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
	(void) conn;
	cfg->use_ino = 1;

	/* Pick up changes from lower filesystem right away. This is
	   also necessary for better hardlink support. When the kernel
	   calls the unlink() handler, it does not know the inode of
	   the to-be-removed entry and can therefore not invalidate
	   the cache of the associated inode - resulting in an
	   incorrect st_nlink value being reported for any remaining
	   hardlinks to this inode. */
	cfg->entry_timeout = 0;
	cfg->attr_timeout = 0;
	cfg->negative_timeout = 0;

	return NULL;
}
/**
 * Return the attribute of the file by calling the server. The attribute is stored in stbuf
 * Use te stat for the local cached file, not the server's TODO
 */
static int afs_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
	(void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

/**
 * Optional, display the content of a directory. TODO
 * Maybe use a file .dir in each cahced diretory path to store the directory content?  
 */
static int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	(void) flags;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0, (fuse_fill_dir_flags)0))
			break;
	}

	closedir(dp);
	return 0;
}

/*
 * Call the server to make a directory. TODO
 */
static int afs_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

/**
 * Remove a file from both server and cache TODO
 */
static int afs_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

/**
 * Simply redirect call to the server to remove a directory, and remove it from cache. TODO
 * I think this should only succed if the directory is empty?
 */
static int afs_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

/**
 * I believe this is an optional operation? set aside for now
 */
static int afs_rename(const char *from, const char *to, unsigned int flags)
{
	int res;

	if (flags)
		return -EINVAL;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

/*
 * Create an empty file on the server right away TODO
 */
static int afs_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

/*
 * Open a file. If has a valid local cache, use it. TODO
 * Otherwise retrieve a copy from the server and store in local cache
 * If file not exist, throw error 
 */
static int afs_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

/*
 * Read from a file's cached copy. TODO
 * If no cached copy exists this should get a copy from server.
 */
static int afs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	if(fi == NULL)
		fd = open(path, O_RDONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

/*
 * Write to the file's cached copy.
 * If no cached copy exist it should retrieve a copy from the server. TODO
 */
static int afs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	if(fi == NULL)
		fd = open(path, O_WRONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

/**
 * File flushed to server on flose is the cache has been modified. TODO
 */
static int afs_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

fuse_operations afs_oper_new() {
  fuse_operations ops;
  ops.init = afs_init; // initialization. Some behavior related to crash may happen here. Flush all the dirty cached files? 
  ops.getattr = afs_getattr; // stat()
  ops.readdir = afs_readdir; // read a directory
  ops.unlink = afs_unlink; // remove a file/directory
  ops.mkdir = afs_mkdir;
  ops.rmdir = afs_rmdir;
  ops.open = afs_open; // open an existing file, get from server or check if local copy is valid
  ops.create = afs_create; // create a new cached file
  ops.read = afs_read;// read a opened file
  ops.write = afs_write; // write to an opened file
  ops.flush = afs_flush; // called once for system call close(), flush change to server
  ops.rename = afs_rename;
  return ops;
}

static const fuse_operations afs_oper = afs_oper_new();

class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

struct State {
  FILE* logfile;
  std::string rootdir;
};

int main(int argc, char* argv[]) {
  if ((argc < 2)) {
    fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
    return 1;
  }
  umask(0);
  if ((getuid() == 0) || (geteuid() == 0)) {
      fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
      return 1;
  }
  auto data = new State();
  data->rootdir = argv[1];

  std::string target_str = "localhost:50051";
  GreeterClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl; 
  auto fuse_stat =  fuse_main(argc, argv, &afs_oper, nullptr);
  return fuse_stat;
}
