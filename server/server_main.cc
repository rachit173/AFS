
#include <iostream>
#include <memory>
#include <string>


#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

  Status Readdir(ServerContext* context, const FileSystemReaddirRequest* request,
    FileSystemReaddirResponse *reply) override {
      // TODO this is a mock
      reply->add_filename("test_file");
      reply->add_filename("test_directory");

      return Status::OK;
  }

  Status Makedir(ServerContext* context, const FileSystemMakedirRequest* request,
                  FileSystemResponse *reply) override {
    int ret = mkdir(request->path().c_str(), 0777);

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
    int ret = rmdir(request->path().c_str());

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
    int ret = stat(request->path().c_str(), &buf);

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

void RunServer(const std::string& targetdir) {
  std::string server_address("0.0.0.0:50051");
  GreeterServiceImpl service;
  FileSystemImpl afs_service;

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
