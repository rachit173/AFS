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
#include <utime.h>
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
using afs::FileSystemMakedirRequest;
using afs::FileSystemRemoveRequest;
using afs::FileSystemRemovedirRequest;
using afs::FileSystemRenameRequest;
using afs::FileSystemCreateRequest;
using afs::FileSystemStoreRequest;
using afs::FileSystemStoreResponse;

#define CACHE_DIR "/tmp/afs_prototype"
#define CACHE_VERSION_DIR "/tmp/afs_prototype.version_file"
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
                    errno = reply.status();
                    return -1;
                }
            } else {
                errno = ETIMEDOUT;
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
            char **file_list;

            Status status = stub_->Readdir(&context, request, &reply);
            file_list =
                (char **)calloc(reply.filename_size(), sizeof(char *));

            // handle server response.
            if (status.ok()) {

                // check for server side function call error
                if (reply.status() != 0) {
                    errno = reply.status();
                    return NULL;
                }

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
                errno = ETIMEDOUT;
                return NULL;
            }
        }

        int mkdir(const char *path) {
            FileSystemMakedirRequest request;
            request.set_path(path);
            FileSystemResponse response;
            ClientContext context;

            Status status = stub_->Makedir(&context, request, &response);

            if (status.ok()) {
                return 1;
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        int unlink(const char *path) {
            FileSystemRemoveRequest request;
            request.set_path(path);
            FileSystemResponse response;
            ClientContext context;

            Status status = stub_->Remove(&context, request, &response);

            if (status.ok()) {
                return 1;
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        int rmdir(const char *path) {
            FileSystemRemovedirRequest request;
            request.set_path(path);
            FileSystemResponse response;
            ClientContext context;

            Status status = stub_->Removedir(&context, request, &response);

            if (status.ok()) {
                return -response.status();
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        int rename(const char *fromPath, const char *toPath) {
            FileSystemRenameRequest request;
            request.set_frompath(fromPath);
            request.set_topath(toPath);
            FileSystemResponse response;
            ClientContext context;

            Status status = stub_->Rename(&context, request, &response);

            if (status.ok()) {
                return response.status();
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        int create(const char *path, mode_t mode) {
            FileSystemCreateRequest request;
            request.set_path(path);
            request.set_mode(mode);
            FileSystemResponse response;
            ClientContext context;

            Status status = stub_->Create(&context, request, &response);

            if (status.ok()) {
                return response.status();
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        int store(const char *path, const char *data) {
            FileSystemStoreRequest request;
            request.set_path(path);
            request.set_data(data);
            FileSystemStoreResponse response;
            ClientContext context;

            Status status = stub_->Store(&context, request, &response);

            if (status.ok()) {
                errno = response.status();
                return errno;
            } else {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        /**
         * Store data at given path, and create the version file
         */
        int fetch(const char *path, afs::FileSystemFetchResponse *response) {
            afs::FileSystemFetchRequest request;
            request.set_path(path);
            ClientContext context;

            Status status = stub_->Fetch(&context, request, response);

            if (status.ok()) {
                if (response->status() != 0) {
                    errno = response->status();
                    return -1;
                } else {
                    return 0;
                }
            } else {
                std::cout << status.error_code() << ": " << status.error_message()
                          << std::endl;
                errno = ETIMEDOUT;
                return -1;
            }
        }

    private:
        std::unique_ptr<FileSystem::Stub> stub_;
        void populateStatStruct(FileSystemStatResponse reply, struct stat *st) {
            timespec atime, mtime, ctime;
            atime.tv_sec = reply.lastaccess().sec();
            atime.tv_nsec = reply.lastaccess().nsec();
            mtime.tv_sec = reply.lastmodification().sec();
            mtime.tv_nsec = reply.lastmodification().nsec();
            ctime.tv_sec = reply.laststatuschange().sec();
            ctime.tv_nsec = reply.laststatuschange().nsec();
            
            st->st_uid = reply.uid();
            st->st_gid = reply.gid(); // group of the file
            st->st_atim = atime; // last access time
            st->st_mtim = mtime; // last modification time
            st->st_ctim = ctime; // last status change time
            st->st_size = reply.size();
            st->st_mode = reply.mode();
            st->st_nlink = reply.nlink();
            st->st_dev = reply.dev();
            st->st_rdev = reply.rdev();
            st->st_ino = reply.inodenum();
            st->st_blksize = reply.blksize();
            st->st_blocks = reply.blocksnum();
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
static char *get_cache_name(const char *path) {
    int path_length = strlen(path);
    int dir_length = strlen(CACHE_DIR);
    
    char *cache_name = (char*)calloc(path_length + dir_length + 1, sizeof(char));
    strcat(cache_name, CACHE_DIR);
    strcat(cache_name, path);
    return cache_name;
}

/**
 * Get the path to the meta data for cache for a given file/directory path
 * The caller needs to free the memory for the return
 */
static char *get_cache_version_name(const char *path) {
    int path_length = strlen(path);
    int dir_length = strlen(CACHE_VERSION_DIR);
    
    char *cache_version_name = (char*)calloc(path_length + dir_length + 1, sizeof(char));
    strcat(cache_version_name, CACHE_VERSION_DIR);
    strcat(cache_version_name, path);
    return cache_version_name;
}

/**
 * Compare timestamp 1 and timestamp 2.
 * If timestamp 1 is newer, return 1
 * If timestamp 2 is newer, return 2
 * If equal return 0
 */
static int time_cmp(struct timespec time1, struct timespec time2) {
    if (time1.tv_sec > time2.tv_sec) {
        return 1;
    } else if (time1.tv_sec == time2.tv_sec) {
        if (time1.tv_nsec > time2.tv_nsec) return 1;
        else if (time1.tv_nsec == time2.tv_nsec) return 0;
        else return 2;
    } else {
        return 2;
    }
}

/**
 * Make an rpc call to check if the cache for path is valid
 * Return 1 if valid, 0 for invalid, negative for errors
 */
static int is_cache_valid(const char *path) {
    struct stat st_server_file = {};
    struct stat st_cache_version = {};
    FileSystemClient client(channel);
    if (-1 == client.getStat(path, &st_server_file)){
		return -errno;
    }

    // get the status of the version file for the cache
    // where the last modified time is the version timestamp of the cache
    char *cached_file_version = get_cache_version_name(path);
    if (-1 == lstat(cached_file_version, &st_cache_version)) {
        free(cached_file_version); 
		return -errno;
    }
    free(cached_file_version); 

    // compare the last modified time
    int ret = time_cmp(st_server_file.st_mtim, st_cache_version.st_mtim);
    if (ret == 1) {
        // server has a newer version
        return 0;
    } else {
        // client has a newer or equivalent version
        return 1;
    }
}

/**
 * Check if the local file chache is dirty
 * Return 1 if dirty, 0 for not dirty, negative for errors
 */
static int is_cache_dirty(const char *path) {
    struct stat st_cache = {};
    struct stat st_cache_version = {};

    char *cached_file = get_cache_name(path);
    if (-1 == lstat(cached_file, &st_cache)) {
        free(cached_file); 
		return -errno;
    }
    free(cached_file); 

    // get the status of the version file for the cache
    // where the last modified time is the version timestamp of the cache
    char *cached_file_version = get_cache_version_name(path);
    if (-1 == lstat(cached_file_version, &st_cache_version)) {
        free(cached_file_version);
		return -errno;
    }
    free(cached_file_version); 

    // compare the last modified time
    int ret = time_cmp(st_cache.st_mtim, st_cache_version.st_mtim);
    if (ret == 1) {
        // cache is dirty
        return 1;
    } else {
        // cache is not dirty
        return 0;
    }
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
    DIR* meta_dir = opendir(CACHE_VERSION_DIR);

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

    if (meta_dir) {
        closedir(meta_dir);
    } else if (ENOENT == errno) {
        // meta data directory for cache file does not exist
        if (-1 == mkdir(CACHE_VERSION_DIR, 0700)) {
            perror("Failed to initialize cache metadata directory");
            exit(EXIT_FAILURE);
        } 
    } else {
        perror("Failed to initialize cache metadata directory");
        exit(EXIT_FAILURE);
    }
	
    return NULL;
}
/**
 * Return the attribute of the file by calling the server
 * The attribute is stored in stbuf
 */
static int afs_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
    (void) fi;

    FileSystemClient client(channel);
    if (client.getStat(path, stbuf) < 0){
        return -errno;
    }

    return 0;
}

/**
 * Display the content of a directory.
 */
static int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    FileSystemClient client(channel);
    struct dir_structure *dir_entries = client.readDir(path);
    printf("error code is %d", errno);
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

/**
 * Make a directory on server
 */
static int afs_mkdir(const char *path, mode_t mode)
{
    int res;

    // server-side
    FileSystemClient client(channel);
    if (-1 == client.mkdir(path)){
        return -errno;
    }

    return 0;
}

/**
 * Remove a file from both server and cache.
 */
static int afs_unlink(const char *path)
{
    int res;

    // unlink cache copy
    std::string cachePath = std::string(CACHE_DIR) + "/" + std::string(path);
    std::string cacheVersionPath = std::string(CACHE_VERSION_DIR) + "/" + std::string(path);

    res = unlink(cachePath.c_str());
    res = unlink(cacheVersionPath.c_str());
    // Don't error out if the cached copy doesn't exist because we might want
    // to still delete a file if we don't have it cached.

    // server-side
    FileSystemClient client(channel);
    if (-1 == client.unlink(path)){
        return -errno;
    }

    return 0;
}

/**
 * Remove a directory on server
 */
static int afs_rmdir(const char *path)
{
    int res;

    // server-side
    FileSystemClient client(channel);
    if (client.rmdir(path) < 0){
       return -errno;
    }

    return 0;
}

/**
 * Rename the file on both server and cache.
 */
static int afs_rename(const char *from, const char *to, unsigned int flags)
{
    int res;

    if (flags)
      return -EINVAL;

    // rename cache copy
    std::string cacheFromPath = std::string(CACHE_DIR) + "/" + std::string(from);
    std::string cacheToPath = std::string(CACHE_DIR) + "/" + std::string(to);
    res = rename(cacheFromPath.c_str(), cacheToPath.c_str());
    // No error check
    // We may still want to rename the file even if we don't have it in the cache

    // server-side
    FileSystemClient client(channel);
    if (-1 == client.rename(from, to)){
        return -errno;
    }

    return 0;
}

/*
 * Create an empty file on both server and cache, then open the cache copy.
 */
static int afs_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
    int res;

    // create file on server-side
    FileSystemClient client(channel);
    if (0 > client.create(path, mode)){
        return -errno;
    }

    // create cache copy
    std::string cachePath = std::string(CACHE_DIR) + "/" + std::string(path);
    res = open(cachePath.c_str(), fi->flags, mode);
    if (res == -1)
        return -errno;

    fi->fh = res;

    // TODO will need to create the version file too, where the last_modfied time stamp is 
    // retrived from the server. 

    return 0;
}

/*
 * Open a file. If has a valid local cache, use it.
 * Otherwise retrieve a copy from the server and store in local cache
 * If file not exist, throw error 
 */
static int afs_open(const char *path, struct fuse_file_info *fi) {
    printf("============open file %s\n", path);
    // check if a cache exist
    int get_new_file = 1;
    char *cache_name = get_cache_name(path);
    int fd;
    int ret;

    if(access(cache_name, F_OK ) == 0) {
        // file exists
        int ret = is_cache_valid(path);
        printf("============is cache valid %d\n", ret);
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
        // create a version file for the cache using the server returned timestamp
        // flags should also be passed to the server(?)
        // If there is any error the errono should be returned
        printf("============file %s is retrived from the server\n", path);
        char *cache_version_name;
        afs::FileSystemFetchResponse response;
        FileSystemClient client(channel);

        // Get the file
        ret = client.fetch(path, &response);
        if (ret < 0) {
            return -errno;
        }

        // Write the program data to a tmp file first to ensure that if
        // the client crashes in the middle of the transfer, the client
        // wont have an invalid entry in its cache
        std::string tmp_file = std::string("/tmp/") + path + ".tmp";
        fd = open(tmp_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0777);
        if (fd == -1) {
            return -errno;
        }
        ret = write(fd, response.data().c_str(), response.size());
        if (ret == -1) {
            return -errno;
        }
        ret = fsync(fd);
        if (ret == -1) {
            return -errno;
        }
        close(fd);
        ret = rename(tmp_file.c_str(), cache_name);
        if (ret == -1) {
            return -errno;
        }

        // modify last modified time for the cache version file
        // This should be safe to do unatomically because if this
        // crashes before we update the modtime, we will just use
        // the old modtime which should be older
        cache_version_name = get_cache_version_name(path);

        // If the cache version file does not exist, create it
        ret = access(cache_version_name, F_OK);
        if (ret == -1 && errno == ENOENT) {
            fd = open(cache_version_name, O_CREAT | O_WRONLY, 0777);
            if (fd == -1) {
                return -errno;
            }
            close(fd);
        } else if (ret == -1) {
            return -errno;
        }

        struct timespec times[2];
        times[0].tv_sec = times[1].tv_sec = response.lastmodification().sec();
        times[0].tv_nsec = times[1].tv_nsec = response.lastmodification().nsec();
        ret = utimensat(0, cache_version_name, times, 0);
        if (ret == -1) {
            return -errno;
        }
        free(cache_version_name);
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
 * Read from a file's cached copy.
 */
static int afs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
    char *cache_name = get_cache_name(path);

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
 * Write to the the cached copy
 */
static int afs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
    char *cache_name = get_cache_name(path);

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
 * File flushed to server on close if the cache has been modified
 */
static int afs_flush(const char *path, struct fuse_file_info *fi)
{
    int ret = is_cache_dirty(path);
    if (ret == 1) {
        // only flush to server if the file is dirty
        // call the server call store
        char *cache_name = get_cache_name(path);
        FILE* f = fopen(cache_name, "r");
        free(cache_name);

        // Determine file size
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);

        char* data = new char[size];
        rewind(f);
        fread(data, sizeof(char), size, f);
        
        FileSystemClient client(channel);
        if (client.store(path, data) < 0) return -errno;
    } else if (0 > ret){
        return ret;
    }

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
