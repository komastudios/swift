//===--- CompilerAPI.h - Swift Compiler C API -------------------*- C -*-===//
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
// This file defines the C API for the embeddable Swift compiler library.
//
// This API provides a minimal, stable interface for embedding the Swift
// compiler in developer tooling, allowing:
// - Parsing Swift source code to AST
// - Type checking and semantic analysis
// - SIL (Swift Intermediate Language) generation and optimization
// - LLVM IR generation for various backends
//
// Design principles:
// - C89 compatible for maximum portability
// - Opaque handle types for ABI stability
// - Explicit memory management (create/destroy)
// - Thread-safe (each compiler instance is independent)
// - Zero-copy where possible (string views)
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_C_COMPILER_API_H
#define SWIFT_C_COMPILER_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//===----------------------------------------------------------------------===//
// Version Information
//===----------------------------------------------------------------------===//

#define SWIFT_COMPILER_API_VERSION_MAJOR 1
#define SWIFT_COMPILER_API_VERSION_MINOR 0
#define SWIFT_COMPILER_API_VERSION_PATCH 0

/// Get the API version at runtime
void swift_compiler_api_version(int *major, int *minor, int *patch);

/// Get the Swift compiler version string
const char *swift_compiler_version(void);

//===----------------------------------------------------------------------===//
// Opaque Handle Types
//===----------------------------------------------------------------------===//

/// Opaque handle to a compiler instance
typedef struct swift_compiler_s *swift_compiler_t;

/// Opaque handle to an AST context
typedef struct swift_ast_context_s *swift_ast_context_t;

/// Opaque handle to a parsed source file
typedef struct swift_source_file_s *swift_source_file_t;

/// Opaque handle to a SIL module
typedef struct swift_sil_module_s *swift_sil_module_t;

/// Opaque handle to an LLVM module
typedef struct swift_llvm_module_s *swift_llvm_module_t;

//===----------------------------------------------------------------------===//
// Status Codes and Error Handling
//===----------------------------------------------------------------------===//

/// Status codes returned by API functions
typedef enum {
  SWIFT_SUCCESS = 0,
  SWIFT_ERROR_INVALID_ARGUMENT = 1,
  SWIFT_ERROR_OUT_OF_MEMORY = 2,
  SWIFT_ERROR_PARSE_ERROR = 3,
  SWIFT_ERROR_TYPE_CHECK_ERROR = 4,
  SWIFT_ERROR_SIL_GEN_ERROR = 5,
  SWIFT_ERROR_IR_GEN_ERROR = 6,
  SWIFT_ERROR_INTERNAL = 99
} swift_status_t;

/// Diagnostic severity levels
typedef enum {
  SWIFT_DIAG_ERROR = 0,
  SWIFT_DIAG_WARNING = 1,
  SWIFT_DIAG_NOTE = 2,
  SWIFT_DIAG_REMARK = 3
} swift_diagnostic_level_t;

/// Diagnostic information
typedef struct {
  swift_diagnostic_level_t level;
  const char *message;
  const char *filename;
  unsigned line;
  unsigned column;
} swift_diagnostic_t;

/// Callback function type for diagnostic reporting
typedef void (*swift_diagnostic_handler_t)(const swift_diagnostic_t *diag,
                                           void *user_data);

//===----------------------------------------------------------------------===//
// Compiler Configuration
//===----------------------------------------------------------------------===//

/// Optimization levels
typedef enum {
  SWIFT_OPT_NONE = 0,    ///< No optimization
  SWIFT_OPT_SIZE = 1,    ///< Optimize for size
  SWIFT_OPT_SPEED = 2,   ///< Optimize for speed
  SWIFT_OPT_SPEED_FULL = 3 ///< Aggressive optimization
} swift_optimization_level_t;

/// Compiler configuration
typedef struct {
  /// Target triple (e.g., "arm64-apple-macosx", "wasm32-unknown-wasi")
  /// If NULL, uses host target
  const char *target_triple;

  /// Array of include paths for module search
  const char **include_paths;
  size_t include_path_count;

  /// Array of framework search paths (macOS/iOS)
  const char **framework_paths;
  size_t framework_path_count;

  /// Optimization level
  swift_optimization_level_t optimization_level;

  /// Enable embedded Swift mode (minimal runtime, no reflection, etc.)
  bool enable_embedded_mode;

  /// Enable whole module optimization
  bool enable_whole_module_optimization;

  /// Enable testing (enables @testable import)
  bool enable_testing;

  /// Module name (if NULL, uses "main")
  const char *module_name;

  /// Additional compiler flags (forwarded to Clang for C/C++ imports)
  const char **clang_flags;
  size_t clang_flag_count;
} swift_compiler_config_t;

/// Initialize a config structure with default values
void swift_compiler_config_init(swift_compiler_config_t *config);

//===----------------------------------------------------------------------===//
// Compiler Lifecycle
//===----------------------------------------------------------------------===//

/// Create a new compiler instance
///
/// \param config Compiler configuration (copied, can be freed after call)
/// \param out_compiler Receives the compiler handle on success
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_compiler_create(const swift_compiler_config_t *config,
                                     swift_compiler_t *out_compiler);

/// Destroy a compiler instance and free all associated resources
///
/// \param compiler Compiler handle (must not be NULL)
void swift_compiler_destroy(swift_compiler_t compiler);

/// Set a diagnostic handler for the compiler
///
/// \param compiler Compiler handle
/// \param handler Diagnostic callback (NULL to disable)
/// \param user_data User data passed to callback
void swift_compiler_set_diagnostic_handler(swift_compiler_t compiler,
                                           swift_diagnostic_handler_t handler,
                                           void *user_data);

//===----------------------------------------------------------------------===//
// Source Parsing
//===----------------------------------------------------------------------===//

/// Parse Swift source code into an AST
///
/// \param compiler Compiler handle
/// \param source_code Swift source code (UTF-8 encoded, need not be null-terminated)
/// \param source_length Length of source_code in bytes
/// \param filename Virtual filename for diagnostics (can be NULL)
/// \param out_source_file Receives the source file handle on success
/// \returns SWIFT_SUCCESS or SWIFT_ERROR_PARSE_ERROR
swift_status_t swift_parse_source(swift_compiler_t compiler,
                                  const char *source_code,
                                  size_t source_length,
                                  const char *filename,
                                  swift_source_file_t *out_source_file);

/// Parse Swift source code from a file
///
/// \param compiler Compiler handle
/// \param filepath Path to Swift source file
/// \param out_source_file Receives the source file handle on success
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_parse_file(swift_compiler_t compiler,
                                const char *filepath,
                                swift_source_file_t *out_source_file);

/// Destroy a source file handle
///
/// Note: Source files are also destroyed when the compiler is destroyed
///
/// \param source_file Source file handle
void swift_source_file_destroy(swift_source_file_t source_file);

//===----------------------------------------------------------------------===//
// Type Checking
//===----------------------------------------------------------------------===//

/// Perform type checking on a parsed source file
///
/// \param compiler Compiler handle
/// \param source_file Source file to type check
/// \returns SWIFT_SUCCESS or SWIFT_ERROR_TYPE_CHECK_ERROR
swift_status_t swift_typecheck_source_file(swift_compiler_t compiler,
                                           swift_source_file_t source_file);

/// Perform whole-module type checking
///
/// This must be called after all source files in a module have been parsed
///
/// \param compiler Compiler handle
/// \returns SWIFT_SUCCESS or SWIFT_ERROR_TYPE_CHECK_ERROR
swift_status_t swift_typecheck_module(swift_compiler_t compiler);

//===----------------------------------------------------------------------===//
// AST Access
//===----------------------------------------------------------------------===//

/// Get the AST context from a compiler instance
///
/// \param compiler Compiler handle
/// \returns AST context handle (valid until compiler is destroyed)
swift_ast_context_t swift_get_ast_context(swift_compiler_t compiler);

/// AST node kinds
typedef enum {
  SWIFT_AST_DECL_IMPORT = 0,
  SWIFT_AST_DECL_EXTENSION = 1,
  SWIFT_AST_DECL_STRUCT = 2,
  SWIFT_AST_DECL_CLASS = 3,
  SWIFT_AST_DECL_ENUM = 4,
  SWIFT_AST_DECL_PROTOCOL = 5,
  SWIFT_AST_DECL_FUNC = 6,
  SWIFT_AST_DECL_VAR = 7,
  SWIFT_AST_DECL_INIT = 8,
  SWIFT_AST_DECL_DEINIT = 9,
  SWIFT_AST_DECL_SUBSCRIPT = 10,
  SWIFT_AST_DECL_TYPEALIAS = 11,
  SWIFT_AST_DECL_OPERATOR = 12,
  SWIFT_AST_DECL_PRECEDENCE_GROUP = 13,
  SWIFT_AST_EXPR = 100,
  SWIFT_AST_STMT = 200,
  SWIFT_AST_TYPE = 300
} swift_ast_node_kind_t;

/// AST node descriptor (passed to visitors)
typedef struct {
  swift_ast_node_kind_t kind;
  const char *name; ///< Name of declaration (NULL for unnamed nodes)
  const char *filename;
  unsigned line;
  unsigned column;
  void *opaque_decl; ///< Opaque pointer to internal Decl* (for advanced use)
} swift_ast_node_t;

/// AST visitor callback function
///
/// \param node AST node information
/// \param user_data User data passed to walk function
/// \returns true to continue visiting, false to stop
typedef bool (*swift_ast_visitor_t)(const swift_ast_node_t *node,
                                    void *user_data);

/// Walk the AST for a source file
///
/// \param source_file Source file to walk
/// \param visitor Visitor callback
/// \param user_data User data passed to visitor
void swift_ast_walk_source_file(swift_source_file_t source_file,
                                swift_ast_visitor_t visitor,
                                void *user_data);

/// Dump the AST to a string (for debugging)
///
/// \param source_file Source file to dump
/// \param out_string Receives allocated string (must be freed with swift_string_free)
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_ast_dump_to_string(swift_source_file_t source_file,
                                        char **out_string);

//===----------------------------------------------------------------------===//
// SIL Generation
//===----------------------------------------------------------------------===//

/// SIL stages
typedef enum {
  SWIFT_SIL_STAGE_RAW = 0,      ///< Raw SIL (after SILGen)
  SWIFT_SIL_STAGE_CANONICAL = 1, ///< Canonical SIL (after mandatory passes)
  SWIFT_SIL_STAGE_LOWERED = 2    ///< Lowered SIL (prepared for IRGen)
} swift_sil_stage_t;

/// Generate SIL for the current module
///
/// This must be called after type checking
///
/// \param compiler Compiler handle
/// \param out_sil_module Receives the SIL module handle
/// \returns SWIFT_SUCCESS or SWIFT_ERROR_SIL_GEN_ERROR
swift_status_t swift_generate_sil(swift_compiler_t compiler,
                                  swift_sil_module_t *out_sil_module);

/// Get the current stage of a SIL module
///
/// \param sil_module SIL module handle
/// \returns The current stage
swift_sil_stage_t swift_sil_get_stage(swift_sil_module_t sil_module);

/// Run mandatory SIL passes (transforms Raw SIL to Canonical SIL)
///
/// \param compiler Compiler handle
/// \param sil_module SIL module to transform
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_sil_run_mandatory_passes(swift_compiler_t compiler,
                                              swift_sil_module_t sil_module);

/// SIL optimization pass categories (can be combined with bitwise OR)
typedef enum {
  SWIFT_SIL_OPT_NONE = 0,
  SWIFT_SIL_OPT_PERF = 1 << 0,     ///< Performance optimizations
  SWIFT_SIL_OPT_SIZE = 1 << 1,     ///< Size optimizations
  SWIFT_SIL_OPT_SIMPLIFY = 1 << 2, ///< Simplification passes
  SWIFT_SIL_OPT_ALL = 0xFFFF       ///< All optimization passes
} swift_sil_opt_flags_t;

/// Run SIL optimization passes
///
/// \param compiler Compiler handle
/// \param sil_module SIL module to optimize
/// \param opt_flags Optimization pass flags
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_sil_optimize(swift_compiler_t compiler,
                                  swift_sil_module_t sil_module,
                                  swift_sil_opt_flags_t opt_flags);

/// Lower SIL to preparation for IRGen (transforms to Lowered SIL stage)
///
/// \param compiler Compiler handle
/// \param sil_module SIL module to lower
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_sil_lower(swift_compiler_t compiler,
                               swift_sil_module_t sil_module);

/// Export SIL module to a string
///
/// \param sil_module SIL module to export
/// \param out_string Receives allocated string (must be freed with swift_string_free)
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_sil_to_string(swift_sil_module_t sil_module,
                                   char **out_string);

/// Destroy a SIL module handle
///
/// Note: SIL modules are also destroyed when the compiler is destroyed
///
/// \param sil_module SIL module handle
void swift_sil_module_destroy(swift_sil_module_t sil_module);

//===----------------------------------------------------------------------===//
// LLVM IR Generation
//===----------------------------------------------------------------------===//

/// Generate LLVM IR from SIL
///
/// \param compiler Compiler handle
/// \param sil_module SIL module (must be in Lowered stage)
/// \param out_llvm_module Receives the LLVM module handle
/// \returns SWIFT_SUCCESS or SWIFT_ERROR_IR_GEN_ERROR
swift_status_t swift_generate_llvm_ir(swift_compiler_t compiler,
                                      swift_sil_module_t sil_module,
                                      swift_llvm_module_t *out_llvm_module);

/// Export LLVM IR module to a string
///
/// \param llvm_module LLVM module to export
/// \param out_string Receives allocated string (must be freed with swift_string_free)
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_llvm_ir_to_string(swift_llvm_module_t llvm_module,
                                       char **out_string);

/// Write LLVM IR to a file
///
/// \param llvm_module LLVM module to write
/// \param filepath Path to output file
/// \returns SWIFT_SUCCESS or an error code
swift_status_t swift_llvm_ir_write_to_file(swift_llvm_module_t llvm_module,
                                           const char *filepath);

/// Destroy an LLVM module handle
///
/// Note: LLVM modules are also destroyed when the compiler is destroyed
///
/// \param llvm_module LLVM module handle
void swift_llvm_module_destroy(swift_llvm_module_t llvm_module);

//===----------------------------------------------------------------------===//
// Memory Management Utilities
//===----------------------------------------------------------------------===//

/// Free a string allocated by the API
///
/// \param string String to free (can be NULL)
void swift_string_free(char *string);

//===----------------------------------------------------------------------===//
// Utility Functions
//===----------------------------------------------------------------------===//

/// Get the last error message for a compiler instance
///
/// \param compiler Compiler handle
/// \returns Error message string (valid until next API call or compiler destroyed)
const char *swift_compiler_get_last_error(swift_compiler_t compiler);

/// Enable or disable verbose output
///
/// \param compiler Compiler handle
/// \param enable True to enable verbose output
void swift_compiler_set_verbose(swift_compiler_t compiler, bool enable);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SWIFT_C_COMPILER_API_H
