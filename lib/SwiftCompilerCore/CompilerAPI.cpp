//===--- CompilerAPI.cpp - Swift Compiler C API Implementation -*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2024 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "CompilerAPIInternal.h"
#include "swift/AST/DiagnosticConsumer.h"
#include "swift/Basic/Version.h"
#include <cstring>

using namespace swift;
using namespace swift::embedded;

//===----------------------------------------------------------------------===//
// Version Information
//===----------------------------------------------------------------------===//

void swift_compiler_api_version(int *major, int *minor, int *patch) {
  if (major)
    *major = SWIFT_COMPILER_API_VERSION_MAJOR;
  if (minor)
    *minor = SWIFT_COMPILER_API_VERSION_MINOR;
  if (patch)
    *patch = SWIFT_COMPILER_API_VERSION_PATCH;
}

const char *swift_compiler_version(void) {
  return swift::version::getSwiftFullVersion().c_str();
}

//===----------------------------------------------------------------------===//
// Configuration
//===----------------------------------------------------------------------===//

void swift_compiler_config_init(swift_compiler_config_t *config) {
  if (!config)
    return;

  memset(config, 0, sizeof(*config));
  config->optimization_level = SWIFT_OPT_NONE;
  config->enable_embedded_mode = false;
  config->enable_whole_module_optimization = true;
  config->enable_testing = false;
}

//===----------------------------------------------------------------------===//
// Compiler Lifecycle
//===----------------------------------------------------------------------===//

swift_status_t swift_compiler_create(const swift_compiler_config_t *config,
                                     swift_compiler_t *out_compiler) {
  if (!config || !out_compiler)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  try {
    // Allocate compiler instance
    auto *compiler = new swift_compiler_s();
    compiler->impl = new CompilerInstanceImpl();

    // Initialize with configuration
    swift_status_t status = compiler->impl->initialize(config);
    if (status != SWIFT_SUCCESS) {
      delete compiler->impl;
      delete compiler;
      return status;
    }

    *out_compiler = compiler;
    return SWIFT_SUCCESS;

  } catch (const std::bad_alloc &) {
    return SWIFT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return SWIFT_ERROR_INTERNAL;
  }
}

void swift_compiler_destroy(swift_compiler_t compiler) {
  if (!compiler)
    return;

  delete compiler->impl;
  delete compiler;
}

void swift_compiler_set_diagnostic_handler(swift_compiler_t compiler,
                                           swift_diagnostic_handler_t handler,
                                           void *user_data) {
  if (!compiler || !compiler->impl)
    return;

  compiler->impl->DiagnosticHandler = handler;
  compiler->impl->DiagnosticUserData = user_data;
}

//===----------------------------------------------------------------------===//
// AST Access
//===----------------------------------------------------------------------===//

swift_ast_context_t swift_get_ast_context(swift_compiler_t compiler) {
  if (!compiler || !compiler->impl || !compiler->impl->Context)
    return nullptr;

  auto *ctx = new swift_ast_context_s();
  ctx->ctx = compiler->impl->Context.get();
  return ctx;
}

//===----------------------------------------------------------------------===//
// Memory Management
//===----------------------------------------------------------------------===//

void swift_string_free(char *string) {
  if (string)
    free(string);
}

//===----------------------------------------------------------------------===//
// Utility Functions
//===----------------------------------------------------------------------===//

const char *swift_compiler_get_last_error(swift_compiler_t compiler) {
  if (!compiler || !compiler->impl)
    return nullptr;

  return compiler->impl->getLastError();
}

void swift_compiler_set_verbose(swift_compiler_t compiler, bool enable) {
  if (!compiler || !compiler->impl)
    return;

  compiler->impl->Verbose = enable;
}

//===----------------------------------------------------------------------===//
// Utilities Implementation
//===----------------------------------------------------------------------===//

namespace swift {
namespace embedded {

char *allocateCString(const std::string &str) {
  char *result = (char *)malloc(str.size() + 1);
  if (result) {
    memcpy(result, str.c_str(), str.size() + 1);
  }
  return result;
}

char *allocateCString(llvm::StringRef str) {
  char *result = (char *)malloc(str.size() + 1);
  if (result) {
    memcpy(result, str.data(), str.size());
    result[str.size()] = '\0';
  }
  return result;
}

swift_diagnostic_level_t convertDiagnosticLevel(swift::DiagnosticKind kind) {
  switch (kind) {
  case swift::DiagnosticKind::Error:
    return SWIFT_DIAG_ERROR;
  case swift::DiagnosticKind::Warning:
    return SWIFT_DIAG_WARNING;
  case swift::DiagnosticKind::Note:
    return SWIFT_DIAG_NOTE;
  case swift::DiagnosticKind::Remark:
    return SWIFT_DIAG_REMARK;
  }
  return SWIFT_DIAG_ERROR;
}

} // namespace embedded
} // namespace swift
