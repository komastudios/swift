//===--- CompilerAPIInternal.h - Internal API Implementation ----*- C++ -*-===//
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
//
// Internal header for the Swift Compiler C API implementation.
// This file bridges between the C API and the C++ Swift compiler internals.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_COMPILER_API_INTERNAL_H
#define SWIFT_COMPILER_API_INTERNAL_H

#include "swift-c/CompilerAPI.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/DiagnosticEngine.h"
#include "swift/AST/Module.h"
#include "swift/AST/SourceFile.h"
#include "swift/Basic/LangOptions.h"
#include "swift/Basic/SourceManager.h"
#include "swift/Parse/Parser.h"
#include "swift/Sema/ConstraintSystem.h"
#include "swift/SIL/SILModule.h"
#include "swift/Subsystems.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>
#include <vector>

namespace swift {
namespace embedded {

//===----------------------------------------------------------------------===//
// Internal Compiler Instance
//===----------------------------------------------------------------------===//

/// Internal implementation of the compiler instance
class CompilerInstanceImpl {
public:
  /// Configuration
  std::string TargetTriple;
  std::vector<std::string> IncludePaths;
  std::vector<std::string> FrameworkPaths;
  std::vector<std::string> ClangFlags;
  swift_optimization_level_t OptimizationLevel;
  bool EnableEmbeddedMode;
  bool EnableWMO;
  bool EnableTesting;
  std::string ModuleName;

  /// Swift compiler components (owned)
  std::unique_ptr<swift::SourceManager> SourceMgr;
  std::unique_ptr<swift::DiagnosticEngine> Diags;
  std::unique_ptr<swift::ASTContext> Context;
  swift::ModuleDecl *MainModule = nullptr;

  /// Source files (owned by ASTContext)
  std::vector<swift::SourceFile *> SourceFiles;

  /// SIL module (optional, created on demand)
  std::unique_ptr<swift::SILModule> SILMod;

  /// LLVM modules (optional, created on demand)
  std::vector<std::unique_ptr<llvm::Module>> LLVMModules;

  /// Diagnostics
  swift_diagnostic_handler_t DiagnosticHandler = nullptr;
  void *DiagnosticUserData = nullptr;
  std::string LastError;
  bool Verbose = false;

  /// Consumer for diagnostic engine
  class DiagnosticConsumer;
  std::unique_ptr<DiagnosticConsumer> DiagConsumer;

  CompilerInstanceImpl();
  ~CompilerInstanceImpl();

  /// Initialize the compiler with the given configuration
  swift_status_t initialize(const swift_compiler_config_t *config);

  /// Parse source code
  swift_status_t parseSource(const char *source_code, size_t source_length,
                            const char *filename,
                            swift::SourceFile **out_source_file);

  /// Parse a file
  swift_status_t parseFile(const char *filepath,
                          swift::SourceFile **out_source_file);

  /// Type check a source file
  swift_status_t typecheckSourceFile(swift::SourceFile *SF);

  /// Type check the whole module
  swift_status_t typecheckModule();

  /// Generate SIL
  swift_status_t generateSIL();

  /// Run SIL passes
  swift_status_t runSILPasses(swift_sil_stage_t target_stage,
                             swift_sil_opt_flags_t opt_flags);

  /// Generate LLVM IR
  swift_status_t generateLLVMIR(llvm::Module **out_llvm_module);

  /// Set last error message
  void setLastError(const std::string &error) { LastError = error; }

  /// Get last error message
  const char *getLastError() const {
    return LastError.empty() ? nullptr : LastError.c_str();
  }

private:
  /// Initialize language options
  void initializeLangOptions(swift::LangOptions &opts);

  /// Initialize type checker options
  void initializeTypeCheckerOptions(swift::TypeCheckerOptions &opts);

  /// Initialize SIL options
  void initializeSILOptions(swift::SILOptions &opts);

  /// Initialize IRGen options
  void initializeIRGenOptions(swift::IRGenOptions &opts);

  /// Initialize search path options
  void initializeSearchPathOptions(swift::SearchPathOptions &opts);

  /// Create a diagnostic consumer
  void setupDiagnostics();
};

//===----------------------------------------------------------------------===//
// Diagnostic Consumer
//===----------------------------------------------------------------------===//

/// Custom diagnostic consumer that forwards to C API callbacks
class CompilerInstanceImpl::DiagnosticConsumer
    : public swift::DiagnosticConsumer {
  CompilerInstanceImpl &Impl;

public:
  DiagnosticConsumer(CompilerInstanceImpl &impl) : Impl(impl) {}

  void handleDiagnostic(swift::SourceManager &SM,
                       const swift::DiagnosticInfo &Info) override;
};

//===----------------------------------------------------------------------===//
// Opaque Handle Implementations
//===----------------------------------------------------------------------===//

/// Opaque compiler handle (points to CompilerInstanceImpl)
struct swift_compiler_s {
  CompilerInstanceImpl *impl;
};

/// Opaque AST context handle (points to swift::ASTContext)
struct swift_ast_context_s {
  swift::ASTContext *ctx;
};

/// Opaque source file handle (points to swift::SourceFile)
struct swift_source_file_s {
  swift::SourceFile *sf;
};

/// Opaque SIL module handle (points to swift::SILModule)
struct swift_sil_module_s {
  swift::SILModule *mod;
};

/// Opaque LLVM module handle (points to llvm::Module)
struct swift_llvm_module_s {
  llvm::Module *mod;
};

//===----------------------------------------------------------------------===//
// Utilities
//===----------------------------------------------------------------------===//

/// Convert Swift diagnostic level to C API level
swift_diagnostic_level_t convertDiagnosticLevel(swift::DiagnosticKind kind);

/// Allocate and copy a C string
char *allocateCString(const std::string &str);
char *allocateCString(llvm::StringRef str);

} // namespace embedded
} // namespace swift

#endif // SWIFT_COMPILER_API_INTERNAL_H
