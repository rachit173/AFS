
#include <iostream>
#include <memory>
#include <string>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

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
using afs::FileSystemResponse;

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
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  GreeterServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
