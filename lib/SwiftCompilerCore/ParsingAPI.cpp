//===--- ParsingAPI.cpp - Parsing API Implementation -----------*- C++ -*-===//
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
#include "swift/AST/ASTWalker.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Stmt.h"
#include "swift/Parse/Parser.h"
#include "swift/Subsystems.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include <system_error>

using namespace swift;
using namespace swift::embedded;

//===----------------------------------------------------------------------===//
// Parsing Implementation
//===----------------------------------------------------------------------===//

swift_status_t CompilerInstanceImpl::parseSource(const char *source_code,
                                                 size_t source_length,
                                                 const char *filename,
                                                 SourceFile **out_source_file) {
  if (!source_code || !out_source_file) {
    setLastError("Invalid arguments to parseSource");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  if (!Context || !MainModule) {
    setLastError("Compiler not initialized");
    return SWIFT_ERROR_INTERNAL;
  }

  try {
    // Create a memory buffer
    std::string fname = filename ? filename : "<input>";
    std::unique_ptr<llvm::MemoryBuffer> buffer =
        llvm::MemoryBuffer::getMemBufferCopy(
            llvm::StringRef(source_code, source_length), fname);

    // Add buffer to source manager
    unsigned bufferID = SourceMgr->addNewSourceBuffer(std::move(buffer));

    // Create source file
    auto sourceFileKind = EnableWMO ? SourceFileKind::Library
                                    : SourceFileKind::Main;

    auto *sourceFile = new (*Context) SourceFile(
        *MainModule, sourceFileKind, bufferID, ImplicitImportInfo{},
        /*isPrimary=*/true);

    // Add to module
    MainModule->addFile(*sourceFile);
    SourceFiles.push_back(sourceFile);

    // Parse the file
    bool hadError = performParseOnly(*sourceFile);

    if (hadError) {
      setLastError("Parse errors encountered");
      return SWIFT_ERROR_PARSE_ERROR;
    }

    *out_source_file = sourceFile;

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Parsed source file '" << fname
                   << "' (" << source_length << " bytes)\n";
    }

    return SWIFT_SUCCESS;

  } catch (const std::bad_alloc &) {
    setLastError("Out of memory during parsing");
    return SWIFT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    setLastError("Internal error during parsing");
    return SWIFT_ERROR_INTERNAL;
  }
}

swift_status_t CompilerInstanceImpl::parseFile(const char *filepath,
                                               SourceFile **out_source_file) {
  if (!filepath || !out_source_file) {
    setLastError("Invalid arguments to parseFile");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  try {
    // Read the file
    auto fileOrErr = llvm::MemoryBuffer::getFile(filepath);
    if (!fileOrErr) {
      setLastError(std::string("Failed to read file: ") + filepath);
      return SWIFT_ERROR_INVALID_ARGUMENT;
    }

    std::unique_ptr<llvm::MemoryBuffer> buffer = std::move(*fileOrErr);
    size_t length = buffer->getBufferSize();
    const char *data = buffer->getBufferStart();

    // Parse using the buffer
    return parseSource(data, length, filepath, out_source_file);

  } catch (const std::bad_alloc &) {
    setLastError("Out of memory reading file");
    return SWIFT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    setLastError("Internal error reading file");
    return SWIFT_ERROR_INTERNAL;
  }
}

//===----------------------------------------------------------------------===//
// Type Checking Implementation
//===----------------------------------------------------------------------===//

swift_status_t CompilerInstanceImpl::typecheckSourceFile(SourceFile *SF) {
  if (!SF) {
    setLastError("Invalid source file");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  if (!Context) {
    setLastError("Compiler not initialized");
    return SWIFT_ERROR_INTERNAL;
  }

  try {
    // Perform type checking
    performTypeChecking(*SF);

    // Check if there were errors
    if (Context->hadError()) {
      setLastError("Type check errors encountered");
      return SWIFT_ERROR_TYPE_CHECK_ERROR;
    }

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Type checked source file\n";
    }

    return SWIFT_SUCCESS;

  } catch (...) {
    setLastError("Internal error during type checking");
    return SWIFT_ERROR_INTERNAL;
  }
}

swift_status_t CompilerInstanceImpl::typecheckModule() {
  if (!Context || !MainModule) {
    setLastError("Compiler not initialized");
    return SWIFT_ERROR_INTERNAL;
  }

  try {
    // Type check all source files
    for (auto *SF : SourceFiles) {
      swift_status_t status = typecheckSourceFile(SF);
      if (status != SWIFT_SUCCESS) {
        return status;
      }
    }

    // Perform module-wide type checking
    performWholeModuleTypeChecking(*MainModule);

    if (Context->hadError()) {
      setLastError("Module type check errors encountered");
      return SWIFT_ERROR_TYPE_CHECK_ERROR;
    }

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Type checked module '" << ModuleName
                   << "'\n";
    }

    return SWIFT_SUCCESS;

  } catch (...) {
    setLastError("Internal error during module type checking");
    return SWIFT_ERROR_INTERNAL;
  }
}

//===----------------------------------------------------------------------===//
// C API Implementations
//===----------------------------------------------------------------------===//

swift_status_t swift_parse_source(swift_compiler_t compiler,
                                  const char *source_code, size_t source_length,
                                  const char *filename,
                                  swift_source_file_t *out_source_file) {
  if (!compiler || !compiler->impl)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  SourceFile *sf = nullptr;
  swift_status_t status =
      compiler->impl->parseSource(source_code, source_length, filename, &sf);

  if (status == SWIFT_SUCCESS && sf) {
    auto *handle = new swift_source_file_s();
    handle->sf = sf;
    *out_source_file = handle;
  }

  return status;
}

swift_status_t swift_parse_file(swift_compiler_t compiler,
                                const char *filepath,
                                swift_source_file_t *out_source_file) {
  if (!compiler || !compiler->impl)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  SourceFile *sf = nullptr;
  swift_status_t status = compiler->impl->parseFile(filepath, &sf);

  if (status == SWIFT_SUCCESS && sf) {
    auto *handle = new swift_source_file_s();
    handle->sf = sf;
    *out_source_file = handle;
  }

  return status;
}

void swift_source_file_destroy(swift_source_file_t source_file) {
  // Note: We don't delete the actual SourceFile as it's owned by ASTContext
  // We only delete the handle
  if (source_file)
    delete source_file;
}

swift_status_t swift_typecheck_source_file(swift_compiler_t compiler,
                                           swift_source_file_t source_file) {
  if (!compiler || !compiler->impl || !source_file || !source_file->sf)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  return compiler->impl->typecheckSourceFile(source_file->sf);
}

swift_status_t swift_typecheck_module(swift_compiler_t compiler) {
  if (!compiler || !compiler->impl)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  return compiler->impl->typecheckModule();
}

//===----------------------------------------------------------------------===//
// AST Walking Implementation
//===----------------------------------------------------------------------===//

namespace {

/// AST walker that calls C callback
class CASTWalker : public ASTWalker {
  swift_ast_visitor_t Visitor;
  void *UserData;
  SourceManager &SM;

public:
  CASTWalker(swift_ast_visitor_t visitor, void *userData, SourceManager &sm)
      : Visitor(visitor), UserData(userData), SM(sm) {}

  PreWalkAction walkToDeclPre(Decl *D) override {
    swift_ast_node_t node = {};

    // Set kind
    if (isa<FuncDecl>(D))
      node.kind = SWIFT_AST_DECL_FUNC;
    else if (isa<StructDecl>(D))
      node.kind = SWIFT_AST_DECL_STRUCT;
    else if (isa<ClassDecl>(D))
      node.kind = SWIFT_AST_DECL_CLASS;
    else if (isa<EnumDecl>(D))
      node.kind = SWIFT_AST_DECL_ENUM;
    else if (isa<ProtocolDecl>(D))
      node.kind = SWIFT_AST_DECL_PROTOCOL;
    else if (isa<VarDecl>(D))
      node.kind = SWIFT_AST_DECL_VAR;
    else if (isa<ImportDecl>(D))
      node.kind = SWIFT_AST_DECL_IMPORT;
    else if (isa<ExtensionDecl>(D))
      node.kind = SWIFT_AST_DECL_EXTENSION;
    else if (isa<TypeAliasDecl>(D))
      node.kind = SWIFT_AST_DECL_TYPEALIAS;
    else if (isa<ConstructorDecl>(D))
      node.kind = SWIFT_AST_DECL_INIT;
    else if (isa<DestructorDecl>(D))
      node.kind = SWIFT_AST_DECL_DEINIT;
    else if (isa<SubscriptDecl>(D))
      node.kind = SWIFT_AST_DECL_SUBSCRIPT;
    else
      return Action::Continue();

    // Set name
    if (auto *named = dyn_cast<ValueDecl>(D)) {
      if (named->hasName()) {
        node.name = named->getName().str().data();
      }
    }

    // Set location
    auto loc = D->getLoc();
    if (loc.isValid()) {
      auto bufferID = SM.findBufferContainingLoc(loc);
      if (bufferID.isValid()) {
        auto lineAndCol = SM.getLineAndColumnInBuffer(loc, bufferID);
        node.line = lineAndCol.first;
        node.column = lineAndCol.second;

        auto *buffer = SM.getLLVMSourceMgr().getMemoryBuffer(bufferID);
        if (buffer) {
          node.filename = buffer->getBufferIdentifier().data();
        }
      }
    }

    node.opaque_decl = D;

    // Call visitor
    bool shouldContinue = Visitor(&node, UserData);
    return shouldContinue ? Action::Continue() : Action::Stop();
  }
};

} // anonymous namespace

void swift_ast_walk_source_file(swift_source_file_t source_file,
                                swift_ast_visitor_t visitor, void *user_data) {
  if (!source_file || !source_file->sf || !visitor)
    return;

  SourceFile *SF = source_file->sf;
  SourceManager &SM = SF->getASTContext().SourceMgr;

  CASTWalker walker(visitor, user_data, SM);
  SF->walk(walker);
}

swift_status_t swift_ast_dump_to_string(swift_source_file_t source_file,
                                        char **out_string) {
  if (!source_file || !source_file->sf || !out_string)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  try {
    std::string result;
    llvm::raw_string_ostream OS(result);

    source_file->sf->dump(OS);
    OS.flush();

    *out_string = allocateCString(result);
    return SWIFT_SUCCESS;

  } catch (...) {
    return SWIFT_ERROR_INTERNAL;
  }
}
