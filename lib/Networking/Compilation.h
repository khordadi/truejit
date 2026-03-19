#pragma once

#include "Networking/protocol.grpc.pb.h"
#include <Compiler/CompilerServer/CompilerServer.h> // TODO: make it only the message

using namespace grpc;

// compiler
#define COMPILER_PORT "50051"
namespace transport {
// connection
using Compiler = networking::Compiler;
using NetConnectRequest = networking::ConnectRequest;
using NetConnectResponse = networking::ConnectResponse;
using NetDisconnectRequest = networking::DisconnectRequest;
using NetDisconnectResponse = networking::DisconnectResponse;
// compilation
using NetCompilationRequest = networking::CompilationRequest;
using NetCompilationResponse = networking::CompilationResponse;

// TODO: replace New with constructor
static NetConnectRequest New(const CompilerServer::ConnectRequest &CR) {
  NetConnectRequest TCR;
  TCR.set_application(CR.Application);
  return TCR;
}

static CompilerServer::ConnectRequest New(const NetConnectRequest &TCR) {
  CompilerServer::ConnectRequest CR;
  CR.Application = TCR.application();
  return CR;
}

// TODO: replace New with constructor
static CompilerServer::ConnectResponse New(const NetConnectResponse &TCR) {
  CompilerServer::ConnectResponse CR;
  CR.SessionID = TCR.session_id();
  return CR;
}

static NetDisconnectRequest New(const CompilerServer::DisconnectRequest &DR) {
  auto TDR = NetDisconnectRequest{};
  TDR.set_session_id(DR.SessionID);
  return TDR;
}

static CompilerServer::DisconnectRequest New(const NetDisconnectRequest &TDR) {
  auto DR = CompilerServer::DisconnectRequest{};
  DR.SessionID = TDR.session_id();
  return DR;
}

static NetDisconnectResponse New(const CompilerServer::DisconnectResponse &DR) {
  return NetDisconnectResponse{};
}

static CompilerServer::DisconnectResponse
New(const NetDisconnectResponse &TDR) {
  return CompilerServer::DisconnectResponse{};
}

static NetCompilationRequest New(const CompilerServer::Request &CR) {
  auto TCR = NetCompilationRequest{};
  TCR.set_session_id(CR.SessionID);
  TCR.set_request_id(CR.RequestID);
  for (const auto &H : CR.History)
    TCR.add_history(H);
  return TCR;
}

static CompilerServer::Request New(const NetCompilationRequest &TCR) {
  auto SessionID = TCR.session_id();
  auto RequestID = TCR.request_id();
  auto History = std::vector<u32>{TCR.history().begin(), TCR.history().end()};
  return CompilerServer::Request{SessionID, RequestID, std::move(History)};
}

static NetCompilationResponse New(const CompilerServer::Response &CR) {
  auto TCR = NetCompilationResponse{};
  for (const auto &Func : CR.Responses) {
    auto TCRF = TCR.add_functions();
    TCRF->set_function_index(Func.Fn);
    TCRF->set_code(Func.Code.data(), Func.Code.size());
  }
  return TCR;
}

static CompilerServer::Response New(const NetCompilationResponse &TCR) {
  CompilerServer::Response CR;
  for (auto &TF : TCR.functions()) {
    CR.Responses.emplace_back(
        TF.function_index(),
        std::vector<char>{TF.code().begin(), TF.code().end()});
  }
  return CR;
}

static void WriteTo(const CompilerServer::Response &CR,
                    NetCompilationResponse *TCR) {
  for (const auto &Func : CR.Responses) {
    auto TCRF = TCR->add_functions();
    TCRF->set_function_index(Func.Fn);
    TCRF->set_code(Func.Code.data(), Func.Code.size());
  }
}

static void WriteTo(const CompilerServer::ConnectResponse &CR,
                    NetConnectResponse *TCR) {
  TCR->set_session_id(CR.SessionID);
}
} // namespace transport
