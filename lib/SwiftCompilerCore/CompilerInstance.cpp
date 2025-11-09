//===--- CompilerInstance.cpp - Compiler Instance Implementation -*- C++ -*-===//
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
#include "swift/AST/DiagnosticsFrontend.h"
#include "swift/AST/SearchPathOptions.h"
#include "swift/Basic/LangOptions.h"
#include "swift/Basic/SourceManager.h"
#include "swift/ClangImporter/ClangImporter.h"
#include "swift/Subsystems.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include <system_error>

using namespace swift;
using namespace swift::embedded;

//===----------------------------------------------------------------------===//
// DiagnosticConsumer Implementation
//===----------------------------------------------------------------------===//

void CompilerInstanceImpl::DiagnosticConsumer::handleDiagnostic(
    swift::SourceManager &SM, const swift::DiagnosticInfo &Info) {

  // If no handler is set, just record errors
  if (!Impl.DiagnosticHandler) {
    if (Info.Kind == DiagnosticKind::Error) {
      std::string message;
      llvm::raw_string_ostream OS(message);
      DiagnosticEngine::formatDiagnosticText(OS, Info.FormatString,
                                            Info.FormatArgs);
      Impl.LastError = message;
    }
    return;
  }

  // Build diagnostic info for C API
  swift_diagnostic_t diag;
  diag.level = convertDiagnosticLevel(Info.Kind);

  // Format the message
  std::string message;
  llvm::raw_string_ostream OS(message);
  DiagnosticEngine::formatDiagnosticText(OS, Info.FormatString,
                                        Info.FormatArgs);
  OS.flush();
  diag.message = message.c_str();

  // Get location information
  if (Info.Loc.isValid()) {
    auto bufferID = SM.findBufferContainingLoc(Info.Loc);
    if (bufferID.isValid()) {
      auto lineAndCol = SM.getLineAndColumnInBuffer(Info.Loc, bufferID);
      diag.line = lineAndCol.first;
      diag.column = lineAndCol.second;

      auto *buffer = SM.getLLVMSourceMgr().getMemoryBuffer(bufferID);
      if (buffer) {
        diag.filename = buffer->getBufferIdentifier().data();
      } else {
        diag.filename = "<unknown>";
      }
    } else {
      diag.filename = "<unknown>";
      diag.line = 0;
      diag.column = 0;
    }
  } else {
    diag.filename = "<unknown>";
    diag.line = 0;
    diag.column = 0;
  }

  // Call the user's handler
  Impl.DiagnosticHandler(&diag, Impl.DiagnosticUserData);
}

//===----------------------------------------------------------------------===//
// CompilerInstanceImpl Implementation
//===----------------------------------------------------------------------===//

CompilerInstanceImpl::CompilerInstanceImpl() {
  // Initialize LLVM targets
  static bool LLVMInitialized = false;
  if (!LLVMInitialized) {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
    LLVMInitialized = true;
  }
}

CompilerInstanceImpl::~CompilerInstanceImpl() {
  // Clean up in reverse order
  LLVMModules.clear();
  SILMod.reset();
  MainModule = nullptr;
  SourceFiles.clear();
  Context.reset();
  Diags.reset();
  DiagConsumer.reset();
  SourceMgr.reset();
}

swift_status_t CompilerInstanceImpl::initialize(
    const swift_compiler_config_t *config) {

  try {
    // Store configuration
    if (config->target_triple) {
      TargetTriple = config->target_triple;
    }

    if (config->module_name) {
      ModuleName = config->module_name;
    } else {
      ModuleName = "main";
    }

    OptimizationLevel = config->optimization_level;
    EnableEmbeddedMode = config->enable_embedded_mode;
    EnableWMO = config->enable_whole_module_optimization;
    EnableTesting = config->enable_testing;

    // Copy include paths
    for (size_t i = 0; i < config->include_path_count; ++i) {
      IncludePaths.push_back(config->include_paths[i]);
    }

    // Copy framework paths
    for (size_t i = 0; i < config->framework_path_count; ++i) {
      FrameworkPaths.push_back(config->framework_paths[i]);
    }

    // Copy Clang flags
    for (size_t i = 0; i < config->clang_flag_count; ++i) {
      ClangFlags.push_back(config->clang_flags[i]);
    }

    // Create Source Manager
    SourceMgr = std::make_unique<SourceManager>();

    // Setup diagnostics
    setupDiagnostics();

    // Initialize language options
    LangOptions langOpts;
    initializeLangOptions(langOpts);

    // Initialize type checker options
    TypeCheckerOptions typeckOpts;
    initializeTypeCheckerOptions(typeckOpts);

    // Initialize search path options
    SearchPathOptions searchPathOpts;
    initializeSearchPathOptions(searchPathOpts);

    // Initialize SIL options
    SILOptions silOpts;
    initializeSILOptions(silOpts);

    // Create ASTContext
    Context = ASTContext::get(langOpts, typeckOpts, searchPathOpts, *SourceMgr,
                             *Diags);

    // Create main module
    MainModule = ModuleDecl::create(Context->getIdentifier(ModuleName), *Context);
    Context->addLoadedModule(MainModule);

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Initialized compiler for module '"
                   << ModuleName << "'\n";
      if (!TargetTriple.empty()) {
        llvm::errs() << "  Target: " << TargetTriple << "\n";
      }
      llvm::errs() << "  Embedded mode: " << (EnableEmbeddedMode ? "yes" : "no")
                   << "\n";
    }

    return SWIFT_SUCCESS;

  } catch (const std::bad_alloc &) {
    setLastError("Out of memory during initialization");
    return SWIFT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    setLastError("Internal error during initialization");
    return SWIFT_ERROR_INTERNAL;
  }
}

void CompilerInstanceImpl::setupDiagnostics() {
  DiagConsumer = std::make_unique<DiagnosticConsumer>(*this);
  Diags = std::make_unique<DiagnosticEngine>(*SourceMgr);
  Diags->addConsumer(*DiagConsumer);
}

void CompilerInstanceImpl::initializeLangOptions(LangOptions &opts) {
  // Set target triple if specified
  if (!TargetTriple.empty()) {
    opts.Target = llvm::Triple(TargetTriple);
  }

  // Enable Embedded Swift mode if requested
  if (EnableEmbeddedMode) {
    opts.Features.insert(Feature::Embedded);
  }

  // Enable testing if requested
  opts.EnableTesting = EnableTesting;

  // Always enable module import for now
  opts.EnableObjCInterop = false; // Disable for embedded/minimal builds

  // Set language version (Swift 6)
  opts.EffectiveLanguageVersion = version::Version::getCurrentLanguageVersion();
}

void CompilerInstanceImpl::initializeTypeCheckerOptions(TypeCheckerOptions &opts) {
  // Enable debugging constraints if verbose
  opts.DebugConstraintSolver = Verbose;
}

void CompilerInstanceImpl::initializeSILOptions(SILOptions &opts) {
  // Set optimization mode based on configuration
  switch (OptimizationLevel) {
  case SWIFT_OPT_NONE:
    opts.OptMode = OptimizationMode::NoOptimization;
    break;
  case SWIFT_OPT_SIZE:
    opts.OptMode = OptimizationMode::ForSize;
    break;
  case SWIFT_OPT_SPEED:
    opts.OptMode = OptimizationMode::ForSpeed;
    break;
  case SWIFT_OPT_SPEED_FULL:
    opts.OptMode = OptimizationMode::ForSpeed;
    break;
  }

  // Embedded mode specific settings
  if (EnableEmbeddedMode) {
    opts.EnableOSSAModules = true;
    opts.StripOwnershipAfterSerialization = false;
  }

  // Verification settings
  opts.VerifyAll = Verbose;
}

void CompilerInstanceImpl::initializeIRGenOptions(IRGenOptions &opts) {
  // Set optimization level
  switch (OptimizationLevel) {
  case SWIFT_OPT_NONE:
    opts.Optimize = false;
    opts.OptMode = OptimizationMode::NoOptimization;
    break;
  case SWIFT_OPT_SIZE:
    opts.Optimize = true;
    opts.OptMode = OptimizationMode::ForSize;
    break;
  case SWIFT_OPT_SPEED:
  case SWIFT_OPT_SPEED_FULL:
    opts.Optimize = true;
    opts.OptMode = OptimizationMode::ForSpeed;
    break;
  }

  // Embedded mode settings
  if (EnableEmbeddedMode) {
    opts.UseJIT = false;
    opts.ForceLoadSymbolName.clear();
  }
}

void CompilerInstanceImpl::initializeSearchPathOptions(SearchPathOptions &opts) {
  // Add include paths
  for (const auto &path : IncludePaths) {
    opts.ImportSearchPaths.push_back(path);
  }

  // Add framework paths
  for (const auto &path : FrameworkPaths) {
    opts.FrameworkSearchPaths.push_back({path, /*isSystem=*/false});
  }

  // Disable runtime library import for embedded mode
  if (EnableEmbeddedMode) {
    opts.DisableModulesValidateSystemHeaders = true;
  }
}
