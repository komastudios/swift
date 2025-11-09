# SwiftCompilerCore - Embeddable Swift Compiler Library

SwiftCompilerCore is a C API library that provides programmatic access to the Swift compiler, allowing you to embed Swift compilation capabilities in your developer tooling.

## Features

- **Parse Swift Source**: Convert Swift source code to AST
- **Type Checking**: Perform semantic analysis and type checking
- **SIL Generation**: Generate Swift Intermediate Language (SIL)
- **LLVM IR Generation**: Produce LLVM IR for backend code generation
- **AST Traversal**: Walk and inspect the Swift AST
- **Diagnostics**: Receive detailed error, warning, and note messages
- **Minimal Dependencies**: Designed for embedding in Web, Mobile, and Embedded platforms

## API Design

The library provides a stable C API with opaque handle types for ABI stability:

```c
// Create compiler
swift_compiler_config_t config;
swift_compiler_config_init(&config);
config.enable_embedded_mode = true;

swift_compiler_t compiler;
swift_compiler_create(&config, &compiler);

// Parse source
swift_source_file_t source_file;
swift_parse_source(compiler, source_code, length, "file.swift", &source_file);

// Type check
swift_typecheck_source_file(compiler, source_file);

// Generate SIL
swift_sil_module_t sil;
swift_generate_sil(compiler, &sil);

// Generate LLVM IR
swift_llvm_module_t ir;
swift_generate_llvm_ir(compiler, sil, &ir);

// Cleanup
swift_compiler_destroy(compiler);
```

## Building

SwiftCompilerCore is built as part of the Swift project:

```bash
# Configure
cmake -G Ninja \
  -DSWIFT_BUILD_EMBEDDABLE_LIB=ON \
  -DSWIFT_EMBEDDABLE_STATIC=ON \
  -S . -B build

# Build
ninja -C build SwiftCompilerCore
```

## Examples

See the `examples/simple-compiler/` directory for a complete example demonstrating:

- Compiler initialization
- Source parsing
- AST walking
- Type checking
- SIL generation and optimization
- LLVM IR generation

## Use Cases

- **LSP Servers**: Build language servers with full Swift support
- **Code Analysis Tools**: Perform structural and semantic analysis
- **JIT Compilers**: Compile and execute Swift code at runtime
- **Cross-Platform Development**: Compile Swift for embedded, web, or mobile platforms
- **Educational Tools**: Create interactive Swift learning environments

## Documentation

- [Full Implementation Plan](../../EMBEDDABLE_COMPILER_PLAN.md)
- [Implementation Roadmap](../../IMPLEMENTATION_ROADMAP.md)
- [API Reference](../../include/swift-c/CompilerAPI.h)

## Current Status

**Phase 1 - Foundation (Complete)**

- ✅ C API design and header
- ✅ Compiler lifecycle (create/destroy)
- ✅ Source parsing
- ✅ Type checking
- ✅ AST traversal
- ✅ SIL generation and passes
- ✅ LLVM IR generation
- ✅ Diagnostic handling
- ✅ Example program

**Phase 2 - SIL Pipeline (In Progress)**

- Advanced SIL optimization configuration
- SIL introspection APIs
- Custom optimization pass integration

**Future Phases**

- C/C++ interoperability
- Platform-specific builds (WebAssembly, Embedded devices)
- GPU/SPIR-V backend

## License

Licensed under Apache License v2.0 with Runtime Library Exception.
See https://swift.org/LICENSE.txt for details.
