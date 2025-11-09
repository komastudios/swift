# Embeddable Swift Compiler - Quick Start Roadmap

This is a quick reference guide for the implementation roadmap. See `EMBEDDABLE_COMPILER_PLAN.md` for the complete detailed plan.

## Project Vision

Build a stripped-down Swift Compiler as a static/shared library for embedding in developer tooling, supporting:
- AST and SIL access for analysis
- LLVM backend code generation
- Minimal dependencies for Web/Mobile/Embedded
- Efficient FFI and C++ interop
- Future GPU/SPIR-V/MLIR backends

## Timeline Overview (12 Months)

```
Month 1-2:  Foundation (C API, Basic Parsing)
Month 3-4:  SIL Pipeline (SIL Generation & Optimization)
Month 5:    LLVM IR Generation
Month 6:    Interoperability (C/C++ via ClangImporter)
Month 7-8:  Platform Support (WASM, Embedded, Mobile)
Month 9-12: GPU/SPIR-V Backend
```

## Quick Phase Reference

### Phase 1: Foundation (Months 1-2)
**Goal:** Basic C API with parsing capability

**Key Deliverables:**
- [ ] C API header (`include/swift-c/SwiftCompilerAPI.h`)
- [ ] Core libraries: swiftBasic, swiftAST, swiftParse, swiftSema
- [ ] Lifecycle functions: `swift_compiler_create/destroy`
- [ ] Parsing API: `swift_parse_source`
- [ ] CMake build system with options
- [ ] Unit tests

**Success:** Can parse Swift source and create AST

### Phase 2: SIL Pipeline (Months 3-4)
**Goal:** SIL generation and optimization

**Key Deliverables:**
- [ ] Integrate swiftSIL, swiftSILGen, swiftSILOptimizer
- [ ] SIL generation API: `swift_generate_sil`
- [ ] Configurable optimization passes: `swift_optimize_sil`
- [ ] SIL export: `swift_sil_to_string`
- [ ] AST visitor pattern: `swift_ast_walk`

**Success:** Can generate and optimize SIL, traverse AST

### Phase 3: LLVM IR Generation (Month 5)
**Goal:** Complete compilation to LLVM IR

**Key Deliverables:**
- [ ] Integrate swiftIRGen with minimal LLVM
- [ ] IR generation API: `swift_generate_llvm_ir`
- [ ] Multiple target triple support
- [ ] IR export: `swift_llvm_ir_to_string`

**Success:** Can generate valid LLVM IR for multiple targets

### Phase 4: Interoperability (Month 6)
**Goal:** C/C++ interop without runtime overhead

**Key Deliverables:**
- [ ] Integrate swiftClangImporter (optional)
- [ ] C header import support
- [ ] C++ header import support
- [ ] Module serialization (swiftSerialization)
- [ ] FFI documentation and examples

**Success:** Can import and use C/C++ libraries with zero runtime overhead

### Phase 5: Platform Support (Months 7-8)
**Goal:** Support Web, Mobile, Embedded platforms

**Key Deliverables:**
- [ ] WebAssembly target (< 2 MB gzipped)
- [ ] Embedded device examples (ARM Cortex-M, Raspberry Pi Pico)
- [ ] Mobile platform examples (iOS framework, Android NDK)
- [ ] Cross-compilation build system
- [ ] Embedded Swift mode integration
- [ ] Complete documentation

**Success:** Runs on all target platforms with minimal size

### Phase 6: GPU/SPIR-V (Months 9-12)
**Goal:** GPU code generation capability

**Key Deliverables:**
- [ ] LLVM IR → SPIR-V via llvm-spirv integration
- [ ] `@gpu` attribute support in AST/Sema
- [ ] Direct SIL → SPIR-V backend prototype
- [ ] Compute shader examples
- [ ] GPU execution examples (Vulkan, Metal)

**Success:** Can generate and execute GPU compute shaders

## Key Technical Decisions

### Library Structure
```
SwiftCompilerCore.a (or .so)
  ├── C API Layer (stable interface)
  ├── Core Components (~2.5 MB)
  │   ├── swiftBasic
  │   ├── swiftAST
  │   ├── swiftParse
  │   ├── swiftSema
  │   ├── swiftSIL + swiftSILGen
  │   ├── swiftSILOptimizer
  │   └── swiftIRGen
  └── Optional Components
      ├── swiftClangImporter (C/C++ interop)
      └── swiftSerialization (module I/O)

Dependencies:
  └── LLVM Minimal Subset (~5 MB)
      ├── LLVMCore
      ├── LLVMSupport
      ├── LLVMIRReader
      └── LLVMTarget
```

### API Design Philosophy
- **C API**: Stable, version-safe, maximum compatibility
- **C++ Wrapper**: Optional, for convenience
- **Opaque Handles**: Hide implementation details
- **Zero-Copy**: Where possible (string views, etc.)
- **Error Handling**: Return status codes, detailed diagnostics via callbacks

### GPU Strategy
1. **Near-term**: External SPIR-V translation (llvm-spirv)
2. **Mid-term**: Direct SIL → SPIR-V backend
3. **Long-term**: Evaluate MLIR for multi-backend support

## Core API Example

```c
// Initialize compiler
swift_compiler_config_t config = {
    .target_triple = "armv7-none-none-eabi",
    .optimization_level = 1,
    .enable_embedded_mode = true,
    .enable_whole_module_optimization = true
};
swift_compiler_t compiler = swift_compiler_create(&config);

// Parse source
const char* source = "func add(_ a: Int, _ b: Int) -> Int { a + b }";
swift_ast_t ast;
swift_parse_source(compiler, source, strlen(source), "example.swift", &ast);

// Type check
swift_typecheck_ast(compiler, ast);

// Generate SIL
swift_sil_module_t sil;
swift_generate_sil(compiler, ast, &sil);

// Optimize
swift_optimize_sil(compiler, sil, SWIFT_OPTIMIZE_SIZE);

// Generate LLVM IR
swift_llvm_module_t ir;
swift_generate_llvm_ir(compiler, sil, &ir);

// Export to string
const char* ir_string = swift_llvm_ir_to_string(ir);
printf("%s\n", ir_string);
swift_llvm_ir_free_string(ir_string);

// Cleanup
swift_compiler_destroy(compiler);
```

## Size Targets

| Configuration | Target Size | Notes |
|---------------|-------------|-------|
| Core Library (static) | < 5 MB | Without LLVM |
| Core + Minimal LLVM | < 10 MB | Stripped |
| WASM Build (gzipped) | < 2 MB | For web |
| Embedded Build | < 3 MB | With basic stdlib |

## Directory Structure (New)

```
swift/
├── EMBEDDABLE_COMPILER_PLAN.md        # This detailed plan
├── IMPLEMENTATION_ROADMAP.md           # This quick reference
├── include/
│   ├── swift-c/                        # New: C API
│   │   └── SwiftCompilerAPI.h
│   └── swift-c++/                      # New: C++ wrapper
│       └── SwiftCompiler.hpp
├── lib/
│   ├── SwiftCompilerCore/              # New: Main embeddable lib
│   │   ├── CMakeLists.txt
│   │   ├── CompilerAPI.cpp
│   │   └── CompilerAPIBridging.cpp
│   ├── SwiftCompilerCXX/               # New: C++ wrapper impl
│   └── SPIRVGen/                       # Future: SPIR-V backend
└── examples/                            # New: Usage examples
    ├── simple-compiler/
    ├── lsp-server/
    ├── c-interop/
    ├── wasm-compiler/
    ├── embedded-firmware/
    └── gpu-compute/
```

## Build Commands (Future)

```bash
# Configure with minimal build
cmake -G Ninja \
  -DSWIFT_BUILD_EMBEDDABLE_LIB=ON \
  -DSWIFT_EMBEDDABLE_STATIC=ON \
  -DSWIFT_EMBEDDABLE_MINIMAL_LLVM=ON \
  -DSWIFT_EMBEDDABLE_INCLUDE_CLANG_IMPORTER=ON \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -S . -B build

# Build the library
ninja -C build SwiftCompilerCore

# Build examples
ninja -C build swift-examples
```

## CMake Options Reference

```cmake
SWIFT_BUILD_EMBEDDABLE_LIB          # Build embeddable library
SWIFT_EMBEDDABLE_STATIC             # Build as static library
SWIFT_EMBEDDABLE_SHARED             # Build as shared library
SWIFT_EMBEDDABLE_MINIMAL_LLVM       # Use minimal LLVM subset
SWIFT_EMBEDDABLE_INCLUDE_CLANG_IMPORTER    # Include C/C++ interop
SWIFT_EMBEDDABLE_INCLUDE_OPTIMIZER  # Include SIL optimizer
SWIFT_EMBEDDABLE_INCLUDE_SPIRV_BACKEND     # Include SPIR-V backend
SWIFT_EMBEDDABLE_TARGET_WEB         # Enable WebAssembly
SWIFT_EMBEDDABLE_TARGET_EMBEDDED    # Enable embedded targets
SWIFT_EMBEDDABLE_TARGET_MOBILE      # Enable mobile targets
```

## Testing Strategy

```bash
# Unit tests (C API)
ninja -C build swift-compiler-core-tests
./build/unittests/SwiftCompilerCore/swift-compiler-core-tests

# Integration tests
ninja -C build check-swift-embeddable

# Platform tests
ninja -C build check-swift-embeddable-wasm
ninja -C build check-swift-embeddable-embedded
```

## Success Metrics

**Phase 1-3 (First 5 months):**
- [ ] Parse and type-check Swift code
- [ ] Generate SIL and LLVM IR
- [ ] Library size < 10 MB
- [ ] API is stable and documented
- [ ] Unit test coverage > 80%

**Phase 5 (8 months):**
- [ ] Runs on WASM, embedded devices, mobile
- [ ] WASM build < 2 MB gzipped
- [ ] C/C++ interop with zero overhead
- [ ] 3+ example projects completed

**Phase 6 (12 months):**
- [ ] GPU compute shaders working
- [ ] SPIR-V generation functional
- [ ] Documentation complete
- [ ] Community adoption started

## Resources and References

**Key Files to Study:**
- `lib/Frontend/Frontend.cpp` - Main compiler entry points
- `include/swift/Subsystems.h` - Compilation pipeline functions
- `lib/IRGen/IRGen.cpp` - LLVM IR generation
- `docs/EmbeddedSwift/UserManual.md` - Embedded Swift guide
- `lib/ClangImporter/ClangImporter.cpp` - C/C++ interop

**Existing Projects to Reference:**
- SourceKit - IDE features library
- swift-syntax - Swift parser library
- SwiftPM - Package manager (uses compiler APIs)

**External Dependencies:**
- LLVM: https://llvm.org/
- llvm-spirv: https://github.com/KhronosGroup/SPIRV-LLVM-Translator
- MLIR: https://mlir.llvm.org/

## Getting Help

- Swift Forums: https://forums.swift.org/
- Swift Evolution: https://github.com/swiftlang/swift-evolution
- Compiler Architecture: https://github.com/apple/swift/tree/main/docs

---

**Last Updated:** 2025-11-09
**Status:** Planning Phase
**Next Milestone:** Phase 1 - Foundation
