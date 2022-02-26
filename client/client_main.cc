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

// #include <config.h>

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
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
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

static off_t fill_dir_plus = 0;
static void *xmp_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
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

static int xmp_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
	(void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
	// DIR *dp;
	// struct dirent *de;

	// (void) offset;
	// (void) fi;
	// (void) flags;

	// dp = opendir(path);
	// if (dp == NULL)
	// 	return -errno;

	// while ((de = readdir(dp)) != NULL) {
	// 	struct stat st;
	// 	memset(&st, 0, sizeof(st));
	// 	st.st_ino = de->d_ino;
	// 	st.st_mode = de->d_type << 12;
	// 	if (filler(buf, de->d_name, &st, 0, fill_dir_plus))
	// 		break;
	// }

	// closedir(dp);
	return 0;
}

// static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
// {
// 	int res;

// 	res = mknod_wrapper(AT_FDCWD, path, NULL, mode, rdev);
// 	if (res == -1)
// 		return -errno;

// 	return 0;
// }

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

// static int xmp_symlink(const char *from, const char *to)
// {
// 	int res;

// 	res = symlink(from, to);
// 	if (res == -1)
// 		return -errno;

// 	return 0;
// }

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
	int res;

	if (flags)
		return -EINVAL;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

// static int xmp_link(const char *from, const char *to)
// {
// 	int res;

// 	res = link(from, to);
// 	if (res == -1)
// 		return -errno;

// 	return 0;
// }

static int xmp_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
	(void) fi;
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
	(void) fi;
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
	int res;

	if (fi != NULL)
		res = ftruncate(fi->fh, size);
	else
		res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
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

static int xmp_write(const char *path, const char *buf, size_t size,
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

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	close(fi->fh);
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

fuse_operations xmp_oper_new() {
  fuse_operations ops;
  ops.init = xmp_init;
  ops.getattr = xmp_getattr;
  ops.access = xmp_access;
  ops.readdir = xmp_readdir;
  // ops.readlink = xmp_readlink;
  // ops.mknod = xmp_mknod;
  ops.mkdir = xmp_mkdir;
  ops.rmdir = xmp_rmdir;
  ops.rename = xmp_rename;
  ops.chmod = xmp_chmod;
  ops.chown = xmp_chown;
  ops.truncate = xmp_truncate;
  ops.open = xmp_open;
  ops.create = xmp_create;
  ops.read = xmp_read;
  ops.write = xmp_write;
  ops.release = xmp_release;
  ops.fsync = xmp_fsync;
  return ops;
}

static const fuse_operations xmp_oper = xmp_oper_new();

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
  if ((argc < 2)) {
    fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
    return 1;
  }

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
  std::string target_str = "localhost:50051";
  GreeterClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = greeter.SayHello(user);
  std::cout << "Greeter received: " << reply << std::endl; 
  return fuse_main(argc, argv, &xmp_oper, NULL); 
  // ret = fuse_main(argc, argv, &hello_oper, NULL);
}