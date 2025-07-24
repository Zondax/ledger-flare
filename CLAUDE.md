# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Ledger hardware wallet application for Flare blockchain that supports:
- Flare native transactions (P-chain and C-chain)
- EVM-compatible transactions
- Multiple Ledger devices (Nano S+, Nano X, Stax, Flex)
- Both development and production builds

The project consists of multiple components:
- **C application** (`app/`) - Core Ledger firmware application
- **JavaScript library** (`js/`) - Client library for interacting with the Ledger app
- **Integration tests** (`tests_zemu/`) - Zemu-based device emulation tests
- **Unit tests** (`tests/`) - C++ unit tests for core functionality

## Build Commands

### Building the app
```bash
make                    # Build the app using dockerized environment
make clean              # Clean build artifacts
```

### Testing
```bash
make test_all          # Run all tests (unit, integration, zemu)
make rust_test         # Run Rust unit tests
make cpp_test          # Run C++ unit tests
make zemu_test         # Run Zemu integration tests
make zemu_install      # Install Zemu test dependencies
```

### Device-specific builds (when BOLOS_SDK is set)
```bash
make buildS2           # Build for Nano S+
make loadS2            # Build and load to Nano S+ device
```

### JavaScript library (in js/ directory)
```bash
yarn build             # Compile TypeScript to JavaScript
yarn test              # Run tests
yarn lint              # Run ESLint
yarn lint:fix          # Fix ESLint issues
yarn format            # Format code with Prettier
```

### Integration tests (in tests_zemu/ directory)  
```bash
yarn test              # Run Zemu integration tests
yarn clean             # Clean up docker containers
```

## Architecture

### Core Application Structure
- `app/src/` - Main C application code
  - `common/` - Shared core functionality (main.c, actions.c, tx.c)
  - `evm/` - EVM-specific transaction parsing
  - `addr.c/h` - Address generation and validation
  - `crypto.c/h` - Cryptographic operations
  - `parser*.c/h` - Transaction parsing logic
  - `tx_cchain.c/h` - C-chain (EVM) transaction handling
  - `tx_pchain.c/h` - P-chain (Avalanche consensus) transaction handling

### JavaScript Client Library
- `js/src/index.ts` - Main API entry point
- `js/src/types.ts` - TypeScript type definitions
- `js/src/helper.ts` - Utility functions
- `js/src/consts.ts` - Constants and configuration

### Test Structure
- `tests/` - C++ unit tests for core parsing logic
- `tests_zemu/tests/` - Integration tests using Zemu device emulator
- `js/tests/` - JavaScript library unit tests

## Key Development Notes

### Transaction Types
The app handles multiple transaction types:
- **Flare P-chain**: Native Avalanche consensus transactions (validators, delegators, transfers)
- **Flare C-chain**: EVM-compatible transactions (transfers, contract calls, ERC-20/721)
- **Cross-chain**: Import/export transactions between P-chain and C-chain

### Parser Architecture
The transaction parser uses a two-stage approach:
1. **Common parsing** (`parser_impl_common.c`) - Basic transaction structure
2. **Specific parsing** (`parser_impl_evm_specific.c`) - EVM transaction details

### Network Support
- **Flare Mainnet**: Chain ID 14, network ID 1
- **Coston Testnet**: Chain ID 16, network ID 7

### Testing Strategy
- Use `make test_all` for comprehensive testing before commits
- Zemu tests provide end-to-end validation with device emulation
- Unit tests cover transaction parsing edge cases
- Integration tests validate the JavaScript library against the app

## Development Workflow

1. Make changes to C code in `app/src/`
2. Run `make` to build the app
3. Test with `make cpp_test` for unit tests
4. Test with `make zemu_test` for integration tests
5. For JavaScript changes, work in `js/` directory and use `yarn` commands
6. Always run `make test_all` before committing changes

## Important Files

- `Makefile` - Main build configuration
- `ledger_app.toml` - Ledger app manifest
- `app/Makefile` - Application-specific build rules
- `deps/ledger-zxlib/` - Zondax Ledger development framework (submodule)