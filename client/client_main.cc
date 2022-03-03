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
#include <time.h>
#include <stdint.h>
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

using afs::FileSystem;
using afs::FileSystemResponse;
using afs::FileSystemStatResponse;
using afs::FileSystemStatRequest;
using afs::FileSystemReaddirRequest;
using afs::FileSystemReaddirResponse;

#define CACHE_DIR "/tmp/afs_prototype"
enum file_type{File, Directory};
static std::shared_ptr<Channel> channel;

struct dir_structure {
    char **files;
    int length;
};

class FileSystemClient {
    public:
        FileSystemClient(std::shared_ptr<Channel> channel)
            : stub_(FileSystem::NewStub(channel)) {}

        int getStat(const char *path, struct stat *st) {
            FileSystemStatRequest request;
            request.set_path(path); 
            FileSystemStatResponse reply;
            ClientContext context;
            Status status = stub_->Stat(&context, request, &reply);
        
            if (status.ok()) {
                if (reply.status() == 0){
                    populateStatStruct(reply, st);
                    // reply
                    return 0;
                } else {
                    return -1;
                }
            } else {
                std::cout << status.error_code() << ": " << status.error_message()
                        << std::endl;
                return -1;
            }
        }

        /**
         * Read directory content. Return null if error occurs
         */
         struct dir_structure *readDir(const char *path) {
            FileSystemReaddirRequest request;
            request.set_path(path); 
            FileSystemReaddirResponse reply;
            ClientContext context;
            struct dir_structure *dir = (struct dir_structure *) calloc(1, sizeof(struct dir_structure));
            char **file_list =
                (char **)calloc(reply.filename_size(), sizeof(char *));

            Status status = stub_->Readdir(&context, request, &reply);

            // handle server response.
            if (status.ok()) {

                for (int i = 0; i < reply.filename_size(); i++) {
                    const char *fileName = reply.filename(i).c_str();
                    file_list[i] = (char *)calloc(1, strlen(fileName) + 1);
                    strncpy(file_list[i], fileName, strlen(fileName));
                    file_list[i][strlen(fileName)] = '\0';
                }
                dir->files = file_list;
                dir->length = reply.filename_size();
                return dir;
            } else {
                std::cout << status.error_code() << ": " << status.error_message()
                    << std::endl;
                return NULL;
            }
        }

    private:
        std::unique_ptr<FileSystem::Stub> stub_;
        void populateStatStruct(FileSystemStatResponse reply, struct stat *st) {
            std::timespec atime, mtime;
            atime.tv_sec = reply.lastaccess().sec();
            atime.tv_nsec = reply.lastaccess().nsec();
            mtime.tv_sec = reply.lastmodification().sec();
            mtime.tv_nsec = reply.lastmodification().nsec();
            
            st->st_uid = reply.uid();
            st->st_gid = reply.gid(); // group of the file
            st->st_atim = atime; // last access time
            st->st_mtim = mtime; // last modification time
            st->st_size = reply.size();
            if (reply.isdir()) {
                // specify the file as directroy and set permission bit
                st->st_mode = S_IFDIR | 0777;
            } else {// a file
                // specify the file as normal file and set permission bit
                st->st_mode = S_IFREG | 0777;
            }
        }
};

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

/**
 * Get the path of the cache for a given file/directory path
 * The caller needs to free the memory for the return
 */
static char *get_cache_name(const char *path, enum file_type type) {
    int path_length = strlen(path);
    int dir_length = strlen(CACHE_DIR);
    
    char *cache_name = (char*)calloc(path_length + dir_length + 1, sizeof(char));
    strcat(cache_name, CACHE_DIR);
    strcat(cache_name, path);
    return cache_name;
}

/**
 * Make an rpc call to check if the cache for path is valid
 * Return 1 if valid, 0 for invalid, negative for errors
 */
static int is_cache_valid(const char *path) {
    struct stat st_server_file = {};
    struct stat st_cached = {};
    FileSystemClient client(channel);
    if (-1 == client.getStat(path, &st_server_file)){
		return -errno;
    }

    char *cached_file = get_cache_name(path, File);
    if (-1 == lstat(cached_file, &st_cached)) {
        free(cached_file); 
		return -errno;
    }
    free(cached_file); 

    // compare the last modified time
    if (st_server_file.st_atime <= st_cached.st_atime) {
        return 1;    
    }
    return 0;
}

/**
 * Do stuff on mounting.
 * Initialize the cache directory if it doesn't exist 
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

    // initialize cache directory if not exist
    DIR* dir = opendir(CACHE_DIR);

    if (dir) {
        closedir(dir);
    } else if (ENOENT == errno) {
        // cache_dir does not exist
        if (-1 == mkdir(CACHE_DIR, 0700)) {
            perror("Failed to initialize cache directory");
            exit(EXIT_FAILURE);
        } 
    } else {
        perror("Failed to initialize cache directory");
        exit(EXIT_FAILURE);
    }
	
    return NULL;
}
/**
 * Return the attribute of the file by calling the server
 * The attribute is stored in stbuf
 */
static int afs_getattr(const char *path, struct stat *st,
		       struct fuse_file_info *fi)
{
    // TODO
    // mock so readdir work for now
    st->st_uid = getuid(); // owner of the file
    st->st_gid = getgid(); // group of the file
    st->st_atime = time( NULL ); // last access time
    st->st_mtime = time( NULL ); // last modification time

    if (strcmp( path, "/" ) == 0 || strcmp( path, "/test_directory" ) == 0 ) {
        st->st_mode = S_IFDIR | 0777;// specify the file as directroy and set permission bit
    } else if (strcmp( path, "/test_file" ) == 0) {
        st->st_mode = S_IFREG | 0777;// specify the file as normal file and set permission bit
        st->st_size = 1024;
    }
    return 0;
}

/**
 * Optional, display the content of a directory. TODO 
 */
static int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    FileSystemClient client(channel);
    struct dir_structure *dir_entries = client.readDir(path);
    if (NULL == dir_entries){
		return -errno;
    }

    char **file_names = dir_entries->files;
    for (int i = 0; i < dir_entries->length; i++) {
        char * entry = file_names[i];
        // fill in the directory
        filler(buf, entry, NULL, 0, (fuse_fill_dir_flags)0);
        free(entry);
    }
    free(file_names);
    free(dir_entries);
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
 * Create an empty file and open it on the server right away TODO
 */
static int afs_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
	int res;

	mode |= O_CREAT;

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
static int afs_open(const char *path, struct fuse_file_info *fi) {
    printf("============open file %s\n", path);
    // check if a cache exist
    int get_new_file = 1;
    char *cache_name = get_cache_name(path, File);
    if(access(cache_name, F_OK ) == 0) {
        // file exists
        int ret = is_cache_valid(path);
        if (1 == ret) {
            get_new_file = 0;
        } else if (0 > ret) {
            // TODO Need to deal with the case when the file does not exist on server
            // remove the cached file and return error
            // mock don't get new file for now
            get_new_file = 0;
        }
    }

    if (get_new_file == 1) {
        // retrive a new copy from server, and replcae the cached file TODO
        // flags should also be passed to the server
        // If there is any error the errono should be returned
        if (strcmp( path, "/test_file" ) == 0) {
            printf("============file %s is retrived from the server\n", path);
            char str[] = "haha this is a new file from server\n";
            FILE * f = fopen(cache_name, "w");
            for (int i = 0; str[i] != '\n'; i++) {
                /* write to file using fputc() function */
                fputc(str[i], f);
            }
            fclose(f);
        }
    }
    
	int res;
	res = open(cache_name, fi->flags);
    free(cache_name);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

/*
 * Read from a file's cached copy
 */
static int afs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
    char *cache_name = get_cache_name(path, File);

	if(fi == NULL)
		fd = open(cache_name, O_RDONLY);
	else
		fd = fi->fh;
    free(cache_name);
	
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
 * Write to the temporary file of the cached copy.
 * If no cached copy exist it should retrieve a copy from the server. TODO
 */
static int afs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
    char *cache_name = get_cache_name(path, File);

	(void) fi;
	if(fi == NULL)
		fd = open(cache_name, O_WRONLY);
	else
		fd = fi->fh;
    free(cache_name);

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
 * File flushed to server on close is the cache has been modified. TODO
 */
static int afs_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

fuse_operations afs_oper_new() {
    fuse_operations ops;
 
    ops.init = afs_init; 
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
    channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    GreeterClient greeter(channel);
    
    std::string user("world");
    std::string reply = greeter.SayHello(user);
    std::cout << "Greeter received: " << reply << std::endl; 
    auto fuse_stat =  fuse_main(argc, argv, &afs_oper, nullptr);
    return fuse_stat;
}
