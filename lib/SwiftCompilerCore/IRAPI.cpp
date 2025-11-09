//===--- IRAPI.cpp - IR Generation API Implementation ----------*- C++ -*-===//
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
#include "swift/IRGen/IRGenPublic.h"
#include "swift/IRGen/IRGenSILPasses.h"
#include "swift/Subsystems.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <system_error>

using namespace swift;
using namespace swift::embedded;

//===----------------------------------------------------------------------===//
// IR Generation Implementation
//===----------------------------------------------------------------------===//

swift_status_t CompilerInstanceImpl::generateLLVMIR(llvm::Module **out_llvm_module) {
  if (!SILMod) {
    setLastError("No SIL module available for IR generation");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  if (SILMod->getStage() != SILStage::Lowered) {
    setLastError("SIL module must be in Lowered stage for IR generation");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  try {
    // Initialize IRGen options
    IRGenOptions irgenOpts;
    initializeIRGenOptions(irgenOpts);

    // Get the module name
    const llvm::PrimarySpecificPaths PSPs =
        llvm::PrimarySpecificPaths("", ModuleName);

    // Generate IR
    auto generatedModule = performIRGeneration(
        MainModule, irgenOpts, TBDGenOptions(), std::move(SILMod),
        ModuleName, PSPs, ArrayRef<std::string>());

    if (!generatedModule.getModule()) {
      setLastError("Failed to generate LLVM IR");
      return SWIFT_ERROR_IR_GEN_ERROR;
    }

    // Take ownership of the LLVM module
    std::unique_ptr<llvm::Module> llvmMod(generatedModule.getModule());
    generatedModule.release();

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Generated LLVM IR module\n";
    }

    *out_llvm_module = llvmMod.get();
    LLVMModules.push_back(std::move(llvmMod));

    return SWIFT_SUCCESS;

  } catch (...) {
    setLastError("Internal error during IR generation");
    return SWIFT_ERROR_INTERNAL;
  }
}

//===----------------------------------------------------------------------===//
// C API Implementations
//===----------------------------------------------------------------------===//

swift_status_t swift_generate_llvm_ir(swift_compiler_t compiler,
                                      swift_sil_module_t sil_module,
                                      swift_llvm_module_t *out_llvm_module) {
  if (!compiler || !compiler->impl || !sil_module || !sil_module->mod ||
      !out_llvm_module)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  llvm::Module *llvmMod = nullptr;
  swift_status_t status = compiler->impl->generateLLVMIR(&llvmMod);

  if (status != SWIFT_SUCCESS)
    return status;

  auto *handle = new swift_llvm_module_s();
  handle->mod = llvmMod;
  *out_llvm_module = handle;

  return SWIFT_SUCCESS;
}

swift_status_t swift_llvm_ir_to_string(swift_llvm_module_t llvm_module,
                                       char **out_string) {
  if (!llvm_module || !llvm_module->mod || !out_string)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  try {
    std::string result;
    llvm::raw_string_ostream OS(result);

    llvm_module->mod->print(OS, nullptr);
    OS.flush();

    *out_string = allocateCString(result);
    return SWIFT_SUCCESS;

  } catch (...) {
    return SWIFT_ERROR_INTERNAL;
  }
}

swift_status_t swift_llvm_ir_write_to_file(swift_llvm_module_t llvm_module,
                                           const char *filepath) {
  if (!llvm_module || !llvm_module->mod || !filepath)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  try {
    std::error_code EC;
    llvm::raw_fd_ostream OS(filepath, EC, llvm::sys::fs::OF_None);

    if (EC) {
      return SWIFT_ERROR_INTERNAL;
    }

    llvm_module->mod->print(OS, nullptr);
    OS.flush();

    return SWIFT_SUCCESS;

  } catch (...) {
    return SWIFT_ERROR_INTERNAL;
  }
}

void swift_llvm_module_destroy(swift_llvm_module_t llvm_module) {
  // Note: We don't delete the actual LLVM Module as it's owned by CompilerInstance
  // We only delete the handle
  if (llvm_module)
    delete llvm_module;
}
