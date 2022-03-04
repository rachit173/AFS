
#include <iostream>
#include <memory>
#include <string>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

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
using afs::FileSystemRemovedirRequest;
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
  std::string root;

public:
  FileSystemImpl(std::string root) : FileSystem::Service() {
    this->root = root;
  }

  Status Readdir(ServerContext* context, const FileSystemReaddirRequest* request,
        FileSystemReaddirResponse *reply) override {
    std::string path = this->root + "/" + request->path().c_str();
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
      if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        reply->add_filename(dp->d_name);
    }

    reply->set_status(errno);

    return Status::OK;
  }

  Status Makedir(ServerContext* context, const FileSystemMakedirRequest* request,
                  FileSystemResponse *reply) override {
    std::string path = this->root + "/" + request->path().c_str();
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
    std::string path = this->root + "/" + request->path().c_str();
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
    TimeSpec *lastAccess;
    TimeSpec *lastModification;
    TimeSpec *lastStatusChange;
    struct stat buf;
    std::string path = this->root + "/" + request->path().c_str();
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

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Must supply a root directory to store the file system" << std::endl;
    return -1;
  }
  std::string root = argv[1];

  RunServer(root);

  return 0;
}
