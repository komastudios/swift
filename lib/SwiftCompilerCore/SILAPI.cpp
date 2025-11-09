//===--- SILAPI.cpp - SIL Generation API Implementation --------*- C++ -*-===//
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
#include "swift/SIL/SILModule.h"
#include "swift/SILOptimizer/PassManager/Passes.h"
#include "swift/Subsystems.h"

using namespace swift;
using namespace swift::embedded;

//===----------------------------------------------------------------------===//
// SIL Generation Implementation
//===----------------------------------------------------------------------===//

swift_status_t CompilerInstanceImpl::generateSIL() {
  if (!Context || !MainModule) {
    setLastError("Compiler not initialized");
    return SWIFT_ERROR_INTERNAL;
  }

  if (Context->hadError()) {
    setLastError("Cannot generate SIL: previous errors encountered");
    return SWIFT_ERROR_SIL_GEN_ERROR;
  }

  try {
    // Initialize SIL options
    SILOptions silOpts;
    initializeSILOptions(silOpts);

    // Create SIL module
    SILMod = performASTLowering(*MainModule, silOpts);

    if (!SILMod) {
      setLastError("Failed to generate SIL");
      return SWIFT_ERROR_SIL_GEN_ERROR;
    }

    if (Verbose) {
      llvm::errs() << "SwiftCompilerCore: Generated SIL module\n";
    }

    return SWIFT_SUCCESS;

  } catch (...) {
    setLastError("Internal error during SIL generation");
    return SWIFT_ERROR_INTERNAL;
  }
}

swift_status_t CompilerInstanceImpl::runSILPasses(
    swift_sil_stage_t target_stage, swift_sil_opt_flags_t opt_flags) {

  if (!SILMod) {
    setLastError("No SIL module available");
    return SWIFT_ERROR_INVALID_ARGUMENT;
  }

  try {
    // Run mandatory passes if we're in Raw stage
    if (SILMod->getStage() == SILStage::Raw &&
        target_stage >= SWIFT_SIL_STAGE_CANONICAL) {
      runSILDiagnosticPasses(*SILMod);

      if (SILMod->getASTContext().hadError()) {
        setLastError("Errors during mandatory SIL passes");
        return SWIFT_ERROR_SIL_GEN_ERROR;
      }

      SILMod->setStage(SILStage::Canonical);

      if (Verbose) {
        llvm::errs() << "SwiftCompilerCore: Ran mandatory SIL passes\n";
      }
    }

    // Run optimization passes if requested
    if (opt_flags != SWIFT_SIL_OPT_NONE &&
        SILMod->getStage() == SILStage::Canonical) {
      runSILOptimizationPasses(*SILMod);

      if (Verbose) {
        llvm::errs() << "SwiftCompilerCore: Ran SIL optimization passes\n";
      }
    }

    // Run lowering passes if needed
    if (target_stage == SWIFT_SIL_STAGE_LOWERED &&
        SILMod->getStage() == SILStage::Canonical) {
      runSILLoweringPasses(*SILMod);
      SILMod->setStage(SILStage::Lowered);

      if (Verbose) {
        llvm::errs() << "SwiftCompilerCore: Ran SIL lowering passes\n";
      }
    }

    return SWIFT_SUCCESS;

  } catch (...) {
    setLastError("Internal error during SIL passes");
    return SWIFT_ERROR_INTERNAL;
  }
}

//===----------------------------------------------------------------------===//
// C API Implementations
//===----------------------------------------------------------------------===//

swift_status_t swift_generate_sil(swift_compiler_t compiler,
                                  swift_sil_module_t *out_sil_module) {
  if (!compiler || !compiler->impl || !out_sil_module)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  swift_status_t status = compiler->impl->generateSIL();
  if (status != SWIFT_SUCCESS)
    return status;

  auto *handle = new swift_sil_module_s();
  handle->mod = compiler->impl->SILMod.get();
  *out_sil_module = handle;

  return SWIFT_SUCCESS;
}

swift_sil_stage_t swift_sil_get_stage(swift_sil_module_t sil_module) {
  if (!sil_module || !sil_module->mod)
    return SWIFT_SIL_STAGE_RAW;

  switch (sil_module->mod->getStage()) {
  case SILStage::Raw:
    return SWIFT_SIL_STAGE_RAW;
  case SILStage::Canonical:
    return SWIFT_SIL_STAGE_CANONICAL;
  case SILStage::Lowered:
    return SWIFT_SIL_STAGE_LOWERED;
  }

  return SWIFT_SIL_STAGE_RAW;
}

swift_status_t swift_sil_run_mandatory_passes(swift_compiler_t compiler,
                                              swift_sil_module_t sil_module) {
  if (!compiler || !compiler->impl || !sil_module || !sil_module->mod)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  return compiler->impl->runSILPasses(SWIFT_SIL_STAGE_CANONICAL,
                                     SWIFT_SIL_OPT_NONE);
}

swift_status_t swift_sil_optimize(swift_compiler_t compiler,
                                  swift_sil_module_t sil_module,
                                  swift_sil_opt_flags_t opt_flags) {
  if (!compiler || !compiler->impl || !sil_module || !sil_module->mod)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  return compiler->impl->runSILPasses(SWIFT_SIL_STAGE_CANONICAL, opt_flags);
}

swift_status_t swift_sil_lower(swift_compiler_t compiler,
                               swift_sil_module_t sil_module) {
  if (!compiler || !compiler->impl || !sil_module || !sil_module->mod)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  return compiler->impl->runSILPasses(SWIFT_SIL_STAGE_LOWERED,
                                     SWIFT_SIL_OPT_NONE);
}

swift_status_t swift_sil_to_string(swift_sil_module_t sil_module,
                                   char **out_string) {
  if (!sil_module || !sil_module->mod || !out_string)
    return SWIFT_ERROR_INVALID_ARGUMENT;

  try {
    std::string result;
    llvm::raw_string_ostream OS(result);

    sil_module->mod->print(OS);
    OS.flush();

    *out_string = allocateCString(result);
    return SWIFT_SUCCESS;

  } catch (...) {
    return SWIFT_ERROR_INTERNAL;
  }
}

void swift_sil_module_destroy(swift_sil_module_t sil_module) {
  // Note: We don't delete the actual SILModule as it's owned by CompilerInstance
  // We only delete the handle
  if (sil_module)
    delete sil_module;
}
