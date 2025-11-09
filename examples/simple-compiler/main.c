//===--- main.c - Simple Compiler Example ----------------------*- C -*-===//
//
// This is a simple example demonstrating the Swift Compiler C API.
// It parses Swift source code, type checks it, and generates LLVM IR.
//
//===----------------------------------------------------------------------===//

#include "swift-c/CompilerAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example Swift source code
static const char *swift_source =
    "func add(_ a: Int, _ b: Int) -> Int {\n"
    "    return a + b\n"
    "}\n"
    "\n"
    "func main() {\n"
    "    let result = add(40, 2)\n"
    "    print(result)\n"
    "}\n";

// Diagnostic handler callback
static void diagnostic_handler(const swift_diagnostic_t *diag, void *user_data) {
    const char *level_str = "";
    switch (diag->level) {
    case SWIFT_DIAG_ERROR:
        level_str = "error";
        break;
    case SWIFT_DIAG_WARNING:
        level_str = "warning";
        break;
    case SWIFT_DIAG_NOTE:
        level_str = "note";
        break;
    case SWIFT_DIAG_REMARK:
        level_str = "remark";
        break;
    }

    fprintf(stderr, "%s:%u:%u: %s: %s\n",
            diag->filename, diag->line, diag->column,
            level_str, diag->message);
}

// AST visitor callback
static bool ast_visitor(const swift_ast_node_t *node, void *user_data) {
    const char *kind_str = "unknown";
    switch (node->kind) {
    case SWIFT_AST_DECL_FUNC:
        kind_str = "function";
        break;
    case SWIFT_AST_DECL_STRUCT:
        kind_str = "struct";
        break;
    case SWIFT_AST_DECL_CLASS:
        kind_str = "class";
        break;
    case SWIFT_AST_DECL_VAR:
        kind_str = "variable";
        break;
    default:
        return true; // Continue visiting
    }

    printf("Found %s", kind_str);
    if (node->name) {
        printf(": %s", node->name);
    }
    if (node->filename && node->line > 0) {
        printf(" at %s:%u:%u", node->filename, node->line, node->column);
    }
    printf("\n");

    return true; // Continue visiting
}

int main(int argc, char **argv) {
    swift_status_t status;
    swift_compiler_t compiler = NULL;
    swift_source_file_t source_file = NULL;
    swift_sil_module_t sil_module = NULL;
    swift_llvm_module_t llvm_module = NULL;
    char *ir_string = NULL;
    int exit_code = 0;

    // Print version info
    int major, minor, patch;
    swift_compiler_api_version(&major, &minor, &patch);
    printf("Swift Compiler API v%d.%d.%d\n", major, minor, patch);
    printf("Swift Compiler Version: %s\n\n", swift_compiler_version());

    // Initialize configuration
    swift_compiler_config_t config;
    swift_compiler_config_init(&config);

    // Use embedded mode for minimal dependencies
    config.enable_embedded_mode = true;
    config.enable_whole_module_optimization = true;
    config.optimization_level = SWIFT_OPT_NONE;
    config.module_name = "SimpleExample";

    // Create compiler instance
    printf("Creating compiler instance...\n");
    status = swift_compiler_create(&config, &compiler);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "Failed to create compiler: %d\n", status);
        exit_code = 1;
        goto cleanup;
    }

    // Set diagnostic handler
    swift_compiler_set_diagnostic_handler(compiler, diagnostic_handler, NULL);

    // Enable verbose output
    swift_compiler_set_verbose(compiler, true);

    // Parse source code
    printf("\nParsing Swift source code...\n");
    status = swift_parse_source(compiler, swift_source, strlen(swift_source),
                                "example.swift", &source_file);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "Parse failed: %d\n", status);
        const char *error = swift_compiler_get_last_error(compiler);
        if (error) {
            fprintf(stderr, "Error: %s\n", error);
        }
        exit_code = 1;
        goto cleanup;
    }

    printf("Parse successful!\n");

    // Walk the AST
    printf("\nWalking AST:\n");
    swift_ast_walk_source_file(source_file, ast_visitor, NULL);

    // Type check the source
    printf("\nType checking...\n");
    status = swift_typecheck_source_file(compiler, source_file);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "Type check failed: %d\n", status);
        const char *error = swift_compiler_get_last_error(compiler);
        if (error) {
            fprintf(stderr, "Error: %s\n", error);
        }
        exit_code = 1;
        goto cleanup;
    }

    printf("Type check successful!\n");

    // Generate SIL
    printf("\nGenerating SIL...\n");
    status = swift_generate_sil(compiler, &sil_module);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "SIL generation failed: %d\n", status);
        const char *error = swift_compiler_get_last_error(compiler);
        if (error) {
            fprintf(stderr, "Error: %s\n", error);
        }
        exit_code = 1;
        goto cleanup;
    }

    printf("SIL generated! Stage: %d\n", swift_sil_get_stage(sil_module));

    // Run mandatory SIL passes
    printf("\nRunning mandatory SIL passes...\n");
    status = swift_sil_run_mandatory_passes(compiler, sil_module);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "SIL passes failed: %d\n", status);
        exit_code = 1;
        goto cleanup;
    }

    printf("Mandatory passes complete! Stage: %d\n", swift_sil_get_stage(sil_module));

    // Lower SIL for IRGen
    printf("\nLowering SIL...\n");
    status = swift_sil_lower(compiler, sil_module);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "SIL lowering failed: %d\n", status);
        exit_code = 1;
        goto cleanup;
    }

    printf("SIL lowered! Stage: %d\n", swift_sil_get_stage(sil_module));

    // Generate LLVM IR
    printf("\nGenerating LLVM IR...\n");
    status = swift_generate_llvm_ir(compiler, sil_module, &llvm_module);
    if (status != SWIFT_SUCCESS) {
        fprintf(stderr, "LLVM IR generation failed: %d\n", status);
        const char *error = swift_compiler_get_last_error(compiler);
        if (error) {
            fprintf(stderr, "Error: %s\n", error);
        }
        exit_code = 1;
        goto cleanup;
    }

    printf("LLVM IR generated!\n");

    // Export IR to string
    printf("\nExporting LLVM IR:\n");
    printf("----------------------------------------\n");
    status = swift_llvm_ir_to_string(llvm_module, &ir_string);
    if (status == SWIFT_SUCCESS && ir_string) {
        printf("%s\n", ir_string);
        printf("----------------------------------------\n");
    }

    printf("\n✓ All compilation stages completed successfully!\n");

cleanup:
    // Clean up
    if (ir_string)
        swift_string_free(ir_string);
    if (llvm_module)
        swift_llvm_module_destroy(llvm_module);
    if (sil_module)
        swift_sil_module_destroy(sil_module);
    if (source_file)
        swift_source_file_destroy(source_file);
    if (compiler)
        swift_compiler_destroy(compiler);

    return exit_code;
}
