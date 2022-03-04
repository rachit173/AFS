
#include <iostream>
#include <memory>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#ifdef BAZEL_BUILD
// #include "examples/protos/helloworld.grpc.pb.h"
#else
#include "protos/afs.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using afs::Greeter;
using afs::HelloRequest;
using afs::HelloReply;
using afs::FileSystem;
using afs::FileSystemMakedirRequest;
using afs::FileSystemRemoveRequest;
using afs::FileSystemCreateRequest;
using afs::FileSystemRenameRequest;
using afs::FileSystemRemovedirRequest;
using afs::FileSystemFetchRequest;
using afs::FileSystemFetchResponse;
using afs::FileSystemStoreRequest;
using afs::FileSystemStoreResponse;
using afs::FileSystemStatRequest;
using afs::FileSystemStatResponse;
using afs::FileSystemResponse;
using afs::TimeSpec;
using afs::FileSystemReaddirRequest;
using afs::FileSystemReaddirResponse;


// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }
};

class FileSystemImpl final : public FileSystem::Service {
private:
  // The root of the filesystem on our local filesystem
  std::string root_;

public:
  FileSystemImpl(std::string root) : FileSystem::Service(), root_(root) {}

  Status Remove(ServerContext* context, const FileSystemRemoveRequest* request,
                  FileSystemResponse *reply) override {
    std::string path = serverPath(request->path().c_str());

    int ret = unlink(path.c_str());
    if (ret != 0)
      ret = errno;

    reply->set_status(errno);

    return Status::OK;
  }

  Status Create(ServerContext* context, const FileSystemCreateRequest* request,
                  FileSystemResponse *reply) override {
    std::string path = serverPath(request->path());

    int ret = creat(path.c_str(), O_CREAT | 0777);
    if (ret != 0) {
      ret = errno;
    }

    reply->set_status(errno);

    return Status::OK;
  }

  Status Rename(ServerContext* context, const FileSystemRenameRequest* request,
                  FileSystemResponse *reply) override {
    std::string fromPath = serverPath(request->frompath());
    std::string toPath = serverPath(request->topath());

    int ret = rename(fromPath.c_str(), toPath.c_str());
    if (ret != 0) {
      ret = errno;
    }

    reply->set_status(errno);

    return Status::OK;
  };

  Status Readdir(ServerContext* context, const FileSystemReaddirRequest* request,
                  FileSystemReaddirResponse *reply) override {
    std::string path = serverPath(request->path());
    DIR *dirp;
    struct dirent *dp;

    if ((dirp = opendir(path.c_str())) == NULL) {
      reply->set_status(errno);
      return Status::OK;
    }

    errno = 0;
    while (true) {
      dp = readdir(dirp);
      if (dp == NULL)
        break;

      // For now . and .. are causing issues with ls, so ignore them
      // It looks like the need READDIRPLUS implemented
      reply->add_filename(dp->d_name);
    }

    reply->set_status(errno);

    return Status::OK;
  }

  Status Makedir(ServerContext* context, const FileSystemMakedirRequest* request,
                  FileSystemResponse *reply) override {
    std::string path = serverPath(request->path());
    int ret = mkdir(path.c_str(), 0777);

    //Mkdir return -1 on error and sets errno to error code
    if (ret == -1) {
      ret = errno;
    }
    reply->set_status(ret);
    reply->set_data("");

    return Status::OK;
  }

  Status Removedir(ServerContext* context, const FileSystemRemovedirRequest *request,
                  FileSystemResponse *reply) override {
    std::string path = serverPath(request->path());
    int ret = rmdir(path.c_str());

    //rmdir returns -1 on error and sets errno
    if (ret == -1) {
      ret = errno;
    }

    reply->set_status(ret);
    reply->set_data("");

    return Status::OK;
  }

  Status Stat(ServerContext* context, const FileSystemStatRequest *request,
                  FileSystemStatResponse *reply) override {
    std::string path = serverPath(request->path());
    TimeSpec *lastAccess;
    TimeSpec *lastModification;
    TimeSpec *lastStatusChange;
    struct stat buf;
    int ret = stat(path.c_str(), &buf);

    // returns -1 on error and sets errno
    if (ret == -1) {
      ret = errno;
    }

    reply->set_status(ret);
    reply->set_uid(buf.st_uid);
    reply->set_gid(buf.st_gid);
    reply->set_size(buf.st_size);
    reply->set_isdir(S_ISDIR(buf.st_mode));

    lastAccess = reply->mutable_lastaccess();
    lastModification = reply->mutable_lastmodification();
    lastStatusChange = reply->mutable_laststatuschange();
    lastAccess->set_sec(buf.st_atim.tv_sec);
    lastAccess->set_nsec(buf.st_atim.tv_nsec);
    lastModification->set_sec(buf.st_mtim.tv_sec);
    lastModification->set_nsec(buf.st_mtim.tv_nsec);
    lastStatusChange->set_sec(buf.st_ctim.tv_sec);
    lastStatusChange->set_nsec(buf.st_ctim.tv_nsec);

    return Status::OK;
  }
  Status Fetch(ServerContext* context, const FileSystemFetchRequest *request,
                  FileSystemFetchResponse *reply) override {
    std::string path = serverPath(request->path());
    int size = request->size();
    int offset = request->offset();
    std::string* data = reply->mutable_data();
    if (size < 0) {
      size = 64*1024*1024; // 64 MB default file size if not specified.
    }
    data->reserve(size+1);
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
      reply->set_status(errno);
    } else {
      int res = pread(fd, data, size, offset);
      reply->set_status(errno);
      struct stat buf;
      res = stat(path.c_str(), &buf);
      if (res == -1) {
        reply->set_status(errno);
      } else {
        auto lastmodification = reply->mutable_lastmodification();
        lastmodification->set_sec(buf.st_mtim.tv_sec);
        lastmodification->set_nsec(buf.st_mtim.tv_nsec);
      }
    }
    close(fd);
    return Status::OK;
  }
  Status Store(ServerContext* context, const FileSystemStoreRequest *request,
                  FileSystemStoreResponse *reply) override {
    std::string path = serverPath(request->path());
    std::string data = request->data();
    int fd = open(path.c_str(), O_WRONLY);
    if (fd == -1) {
      reply->set_status(errno);
    } else {
      std::cout << request->data().size() << std::endl;
      int res = pwrite(fd, data.c_str(), request->data().size(), 0);
      reply->set_status(errno);
    }

    close(fd);
    return Status::OK;
  }
  std::string serverPath(const std::string& relative_path) {
    return root_ + "/" + relative_path;
  }
};


void RunServer(std::string root) {
  std::string server_address("0.0.0.0:50051");
  GreeterServiceImpl service;
  FileSystemImpl afs_service(root);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  builder.RegisterService(&afs_service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  std::cout << "Mounted to " << root << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  if ((argc < 2)) {
    fprintf(stderr, "Usage: %s <targetdir>\n", argv[0]);
    return 1;
  }
  if ((getuid() == 0) || (geteuid() == 0)) {
      fprintf(stderr, "Running server as root can cause security issues.\n");
      return 1;
  }
  std::string targetdir(argv[1]);  
  RunServer(targetdir);

  return 0;
}
