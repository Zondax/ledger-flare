# Fuzzer Infrastructure Upgrade Command

This command automatically upgrades the fuzzing infrastructure from the legacy implementation to the modern common fuzzing library architecture.

## Usage

Run this command to upgrade your fuzzing infrastructure:
```bash
claude --file .claude/commands/fuzz-update.md --update
```

Run this command to execute automated fuzzing with crash analysis:
```bash
claude --file .claude/commands/fuzz-update.md --execute
```

## What this command does

When you run this command with `--update`, it will:

1. **Analyze current fuzzing setup** - Detect existing fuzzer implementations in `/fuzz` directory
2. **Create configuration** - Generate `fuzz/fuzz_config.py` with all detected fuzzers
3. **Replace run-fuzzers.py** - Convert to wrapper that uses common fuzzing library
4. **Add crash analyzer** - Create `fuzz/run-fuzz-crashes.py` wrapper
5. **Update CMakeLists.txt** - Integrate common fuzzing macros and add all detected targets
6. **Add formatting support** - Integrate Python formatting infrastructure

When you run this command with `--execute`, it will:

1. **Configure fuzzing parameters** - Set MAX_SECONDS=120 and FUZZER_JOBS=4 in fuzz_config.py
2. **Clean and run initial fuzzing** - Execute `make fuzz_clean` then `make fuzz`
3. **Escalate to longer session** - If successful, increase MAX_SECONDS=300 and repeat
4. **Analyze crashes** - If errors found, run `make fuzz_crash` to generate crash files
5. **Fix crashes automatically** - Analyze crash logs and attempt code fixes (up to 5 attempts)
6. **Validate fixes** - Re-run crash analysis to verify fixes
7. **Complete cycle** - Continue until all errors are resolved or max attempts reached

## Implementation

```python
import os
import subprocess
import sys

def upgrade_fuzzer_infrastructure():
    """Upgrade fuzzing infrastructure to use common library"""
    
    project_root = os.getcwd()
    fuzz_dir = os.path.join(project_root, "fuzz")
    
    if not os.path.exists(fuzz_dir):
        print("❌ No fuzz directory found. This command requires an existing /fuzz directory.")
        return False
    
    print("🚀 Upgrading fuzzing infrastructure...")
    
    # 1. Detect existing fuzzer files
    fuzzer_files = []
    print("🔍 Detecting existing fuzzer files...")
    for file in os.listdir(fuzz_dir):
        if file.endswith('.cpp'):
            fuzzer_name = file.replace('.cpp', '')
            fuzzer_files.append(fuzzer_name)
            print(f"   Found: {file}")
    
    if not fuzzer_files:
        print("⚠️  No .cpp fuzzer files found in /fuzz directory")
        return False
    
    # 2. Create fuzz_config.py with detected fuzzers
    config_path = os.path.join(fuzz_dir, "fuzz_config.py")
    print("📝 Creating/updating fuzz_config.py...")
    
    # Generate fuzzer configurations
    fuzzer_configs = []
    for fuzzer_name in fuzzer_files:
        if 'parser' in fuzzer_name:
            comment = f"# Tests {fuzzer_name.replace('_', ' ')} functionality"
        else:
            comment = f"# Auto-detected from {fuzzer_name}.cpp"
        fuzzer_configs.append(f'        FuzzConfig(name="{fuzzer_name}", max_len=17000),  {comment}')
    
    config_content = f'''#!/usr/bin/env python3
"""
Project-specific fuzzing configuration for ledger-filecoin
"""

import sys
import os

# Configuration constants
MAX_SECONDS = "3600"  # 1 hour by default
FUZZER_JOBS = 8  # Number of parallel jobs for fuzzing

# Add the common fuzzing module to path
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "deps", "ledger-zxlib", "fuzzing"))

try:
    from run_fuzzers import FuzzConfig
except ImportError:
    # Fallback if common module is not available
    class FuzzConfig:
        def __init__(self, name: str, max_len: int = 17000):
            self.name = name
            self.max_len = max_len


def get_fuzzer_configs():
    """Get project-specific fuzzer configurations"""
    return [
{chr(10).join(fuzzer_configs)}
        # Add more fuzzers here as needed
        # Example:
        # FuzzConfig(
        #     name='custom_fuzzer',
        #     max_len=1000
        # ),
    ]


if __name__ == "__main__":
    # Show available configurations
    configs = get_fuzzer_configs()
    print("Available fuzzer configurations:")
    for config in configs:
        print(f"  - {{config.name}}: max_len={{config.max_len}}")
'''
    with open(config_path, 'w') as f:
        f.write(config_content)
    
    # 3. Replace run-fuzzers.py with wrapper
    run_fuzzers_path = os.path.join(fuzz_dir, "run-fuzzers.py")
    print("🔄 Updating run-fuzzers.py to use common library...")
    wrapper_content = '''#!/usr/bin/env python3
"""
Local convenience wrapper for running fuzzers in ledger-filecoin project
"""

import os
import sys

# Add the common fuzzing module to path
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(current_dir)
common_fuzzing_path = os.path.join(project_root, "deps", "ledger-zxlib", "fuzzing")

sys.path.insert(0, common_fuzzing_path)

try:
    from run_fuzzers import main
    from fuzz_config import MAX_SECONDS, FUZZER_JOBS

    # Override default arguments to point to this project root
    if "--project-root" not in sys.argv:
        sys.argv.extend(["--project-root", project_root])

    # Override max-seconds if not provided
    if "--max-seconds" not in sys.argv:
        sys.argv.extend(["--max-seconds", MAX_SECONDS])

    # Override jobs if not provided
    if "--jobs" not in sys.argv:
        sys.argv.extend(["--jobs", str(FUZZER_JOBS)])

    # Run the common fuzzer
    sys.exit(main())

except ImportError as e:
    print(f"Error: Cannot import common fuzzing module: {e}")
    print(f"Make sure deps/ledger-zxlib/fuzzing/run_fuzzers.py exists")
    sys.exit(1)
'''
    with open(run_fuzzers_path, 'w') as f:
        f.write(wrapper_content)
    os.chmod(run_fuzzers_path, 0o755)
    
    # 4. Create crash analyzer wrapper
    crash_analyzer_path = os.path.join(fuzz_dir, "run-fuzz-crashes.py")
    print("🔍 Creating/updating run-fuzz-crashes.py...")
    crash_content = '''#!/usr/bin/env python3
"""
Local convenience wrapper for analyzing fuzzer crashes in ledger-filecoin project
"""

import os
import sys

# Add the common fuzzing module to path
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(current_dir)
common_fuzzing_path = os.path.join(project_root, "deps", "ledger-zxlib", "fuzzing")

sys.path.insert(0, common_fuzzing_path)

try:
    from analyze_crashes import main

    # Override default arguments to point to this project root
    if "--project-root" not in sys.argv:
        sys.argv.extend(["--project-root", project_root])

    # Run the common crash analyzer
    sys.exit(main())

except ImportError as e:
    print(f"Error: Cannot import common fuzzing module: {e}")
    print(f"Make sure deps/ledger-zxlib/fuzzing/analyze_crashes.py exists")
    sys.exit(1)
'''
    with open(crash_analyzer_path, 'w') as f:
        f.write(crash_content)
    os.chmod(crash_analyzer_path, 0o755)
    
    # 5. Update .gitignore for Python bytecode
    gitignore_path = os.path.join(project_root, ".gitignore")
    if os.path.exists(gitignore_path):
        print("📝 Updating .gitignore for Python support...")
        with open(gitignore_path, 'r') as f:
            gitignore_content = f.read()
        
        # Add Python bytecode entries if not present
        python_entries = [
            "# Python bytecode",
            "__pycache__/",
            "*.pyc",
            "*.pyo"
        ]
        
        # Add fuzzing-related entries if not present
        fuzzing_entries = [
            "# Fuzzing artifacts",
            "fuzz/coverage/",
            "fuzz/logs/",
            "fuzz-*.log"
        ]
        
        needs_python_entries = any(entry not in gitignore_content for entry in python_entries[1:])
        needs_fuzzing_entries = any(entry not in gitignore_content for entry in fuzzing_entries[1:])
        
        if needs_python_entries or needs_fuzzing_entries:
            entries_to_add = []
            
            if needs_fuzzing_entries:
                entries_to_add.extend(fuzzing_entries)
                
            if needs_python_entries:
                if entries_to_add:  # Add separator if we already have fuzzing entries
                    entries_to_add.append("")
                entries_to_add.extend(python_entries)
            
            # Find a good place to insert (after existing fuzz entries if they exist, otherwise at end)
            if "fuzz/" in gitignore_content:
                # Insert after existing fuzz section
                gitignore_content = gitignore_content.rstrip() + "\\n\\n" + "\\n".join(entries_to_add) + "\\n"
            else:
                # Append at end
                gitignore_content = gitignore_content.rstrip() + "\\n\\n" + "\\n".join(entries_to_add) + "\\n"
            
            with open(gitignore_path, 'w') as f:
                f.write(gitignore_content)
    
    # 6. Update CMakeLists.txt
    cmake_path = os.path.join(project_root, "CMakeLists.txt")
    if os.path.exists(cmake_path):
        print("⚙️  Updating CMakeLists.txt...")
        with open(cmake_path, 'r') as f:
            cmake_content = f.read()
        
        # Check if common fuzzing is already included
        if "include(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)" not in cmake_content:
            # Find the enable_testing() line and add the include after it
            cmake_content = cmake_content.replace(
                "enable_testing()",
                "enable_testing()\\n\\n# Include common fuzzing configuration\\ninclude(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)"
            )
        
        # Remove duplicate fuzzing configuration that's now handled by common library
        lines = cmake_content.split('\\n')
        new_lines = []
        skip_fuzzing_config = False
        
        for i, line in enumerate(lines):
            # Skip duplicate options that are in FuzzingCommon.cmake
            if (line.strip().startswith('option(ENABLE_FUZZING') or 
                line.strip().startswith('option(ENABLE_COVERAGE') or 
                line.strip().startswith('option(ENABLE_SANITIZERS')):
                # Check if common library is included before this line
                preceding_content = '\\n'.join(lines[:i])
                if "include(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)" in preceding_content:
                    continue  # Skip duplicate option
            
            # Skip fuzzing configuration blocks that duplicate FuzzingCommon.cmake
            if line.strip().startswith('if(ENABLE_FUZZING)') and not line.strip().endswith('# Fuzz Targets'):
                # Check if this is the configuration block (not the targets block)
                preceding_content = '\\n'.join(lines[:i])
                if "include(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)" in preceding_content:
                    skip_fuzzing_config = True
                    continue
            elif skip_fuzzing_config and line.strip().startswith('endif()'):
                skip_fuzzing_config = False
                continue
            elif skip_fuzzing_config:
                continue
            
            # Skip duplicate string(APPEND CMAKE_*_FLAGS) that are in FuzzingCommon.cmake  
            if (line.strip().startswith('string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer') or
                line.strip().startswith('string(APPEND CMAKE_CXX_FLAGS " -fno-omit-frame-pointer') or  
                line.strip().startswith('string(APPEND CMAKE_LINKER_FLAGS " -fno-omit-frame-pointer')):
                preceding_content = '\\n'.join(lines[:i])
                if "include(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)" in preceding_content:
                    continue  # Skip duplicate flags
            
            new_lines.append(line)
        
        cmake_content = '\\n'.join(new_lines)
        
        # Update fuzzing section to use common macros and properly handle unit tests
        if "ENABLE_FUZZING" in cmake_content and "add_fuzz_target" not in cmake_content:
            # Find the fuzzing section and unit tests sections
            lines = cmake_content.split('\\n')
            new_lines = []
            in_fuzz_section = False
            in_unit_tests_section = False
            unit_tests_lines = []
            indent = ""
            
            # First pass: collect unit test lines
            for i, line in enumerate(lines):
                if line.strip().startswith("file(GLOB_RECURSE TESTS_SRC"):
                    in_unit_tests_section = True
                    unit_tests_lines.append(line)
                elif in_unit_tests_section:
                    unit_tests_lines.append(line)
                    if line.strip().startswith("set_tests_properties"):
                        # End of unit tests section
                        in_unit_tests_section = False
            
            # Second pass: rebuild CMakeLists with proper structure
            for line in lines:
                if "if(ENABLE_FUZZING)" in line:
                    in_fuzz_section = True
                    indent = line[:line.index('if')]
                    new_lines.append(f"{indent}if(ENABLE_FUZZING) # Fuzz Targets")
                    new_lines.append(f"{indent}    # Setup fuzzing directories")
                    new_lines.append(f"{indent}    setup_fuzz_directories()")
                    new_lines.append(f"{indent}    ")
                    new_lines.append(f"{indent}    # Add fuzz targets using the common macro")
                    
                    # Add all detected fuzzer files
                    for fuzzer_name in fuzzer_files:
                        new_lines.append(f"{indent}    add_fuzz_target({fuzzer_name} ${{CMAKE_CURRENT_SOURCE_DIR}}/fuzz/{fuzzer_name}.cpp app_lib)")
                    
                    new_lines.append(f"{indent}    ")
                    new_lines.append(f"{indent}else()")
                    
                    # Add unit tests in the else block
                    for unit_line in unit_tests_lines:
                        new_lines.append(unit_line)
                    
                    new_lines.append(f"{indent}endif()")
                    
                elif in_fuzz_section and line.strip().startswith("endif()"):
                    in_fuzz_section = False
                    # Skip the endif, we already added it
                    continue
                elif in_fuzz_section:
                    # Skip old fuzzing implementation lines
                    continue
                elif line.strip().startswith("file(GLOB_RECURSE TESTS_SRC"):
                    # Skip unit tests lines, they're now in the else block
                    in_unit_tests_section = True
                    continue
                elif in_unit_tests_section:
                    if line.strip().startswith("set_tests_properties"):
                        in_unit_tests_section = False
                    continue
                else:
                    new_lines.append(line)
            
            cmake_content = '\\n'.join(new_lines)
        
        with open(cmake_path, 'w') as f:
            f.write(cmake_content)
    
    # 7. Add formatting support to dockerized_build.mk if it exists
    dockerized_mk_path = os.path.join(project_root, "deps", "ledger-zxlib", "dockerized_build.mk")
    if os.path.exists(dockerized_mk_path):
        print("🎨 Adding Python formatting support...")
        
        with open(dockerized_mk_path, 'r') as f:
            mk_content = f.read()
        
        if "fuzz_fmt" not in mk_content:
            # Add fuzz_fmt target
            fuzz_fmt_content = '''
.PHONY: fuzz_fmt
fuzz_fmt:
\t@echo "🎨 Formatting Python files in fuzzing infrastructure..."
\t@$(MAKE) -C $(CURDIR)/deps/ledger-zxlib/fuzzing format
'''
            mk_content += fuzz_fmt_content
            
            with open(dockerized_mk_path, 'w') as f:
                f.write(mk_content)
    
    print("✅ Fuzzing infrastructure upgrade completed!")
    print(f"\\n📊 Summary:")
    print(f"   • Detected {len(fuzzer_files)} fuzzer files: {', '.join(fuzzer_files)}")
    print(f"   • Created fuzz_config.py with all detected fuzzers")
    print(f"   • Updated CMakeLists.txt with {len(fuzzer_files)} add_fuzz_target entries")
    
    # 8. Test the fuzzing infrastructure
    print("\\n🧪 Testing fuzzing infrastructure...")
    
    # Clean any existing build artifacts
    print("   • Cleaning build artifacts...")
    subprocess.run(["make", "fuzz_clean"], capture_output=True, cwd=project_root)
    
    # Temporarily modify config for testing
    print("   • Setting test configuration (10s, 4 jobs)...")
    with open(config_path, 'r') as f:
        original_config = f.read()
    
    test_config = original_config.replace('MAX_SECONDS = "3600"', 'MAX_SECONDS = "10"')
    test_config = test_config.replace('FUZZER_JOBS = 8', 'FUZZER_JOBS = 4')
    
    with open(config_path, 'w') as f:
        f.write(test_config)
    
    # Test fuzzing
    print("   • Running test fuzzing session...")
    original_cwd = os.getcwd()  # Store current directory
    
    try:
        result = subprocess.run(["make", "fuzz"], capture_output=True, text=True, cwd=project_root)
        
        if result.returncode == 0:
            print("   ✅ Fuzzing test successful!")
            
            # Clean and format (ensure we're in project root)
            print("   • Cleaning up...")
            subprocess.run(["make", "fuzz_clean"], capture_output=True, cwd=project_root)
            
            print("   • Formatting Python code...")
            subprocess.run(["make", "fuzz_fmt"], capture_output=True, cwd=project_root)
            
            # Restore original config
            print("   • Restoring original configuration...")
            with open(config_path, 'w') as f:
                f.write(original_config)
            
            print("\\n🎉 All tests passed! Fuzzing infrastructure is ready.")
        else:
            print("   ❌ Fuzzing test failed!")
            print(f"   Error output: {result.stderr}")
            
            # Restore original config even on failure
            with open(config_path, 'w') as f:
                f.write(original_config)
            
            print("   Configuration restored. Please check the build system.")
            return False
    finally:
        # Always restore working directory
        os.chdir(original_cwd)
    
    print("\\n📋 Next steps:")
    print("1. Review and adjust settings in fuzz/fuzz_config.py as needed")
    print("2. Run fuzzers: make fuzz")
    print("3. Analyze crashes: make fuzz_crash")
    print("4. Format Python code: make fuzz_fmt")
    
    return True

def execute_fuzzer_session():
    """Show instructions for running fuzzing with make targets"""
    
    project_root = os.getcwd()
    fuzz_dir = os.path.join(project_root, "fuzz")
    config_path = os.path.join(fuzz_dir, "fuzz_config.py")
    
    if not os.path.exists(fuzz_dir):
        print("❌ No fuzz directory found. Run with --update first to setup fuzzing infrastructure.")
        return False
    
    if not os.path.exists(config_path):
        print("❌ No fuzz_config.py found. Run with --update first to setup fuzzing infrastructure.")
        return False
    
    print("🚀 Fuzzing Infrastructure Ready!")
    print("=" * 50)
    print()
    print("📋 To run the fuzzer, you MUST use the make targets:")
    print()
    print("🎯 PRIMARY COMMAND:")
    print("   make fuzz")
    print("   └── This is the ONLY correct way to run fuzzing!")
    print()
    print("🔧 ADDITIONAL COMMANDS:")
    print("   make fuzz_clean   # Clean fuzzing artifacts before running")
    print("   make fuzz_crash   # Analyze any crashes found")  
    print("   make fuzz_fmt     # Format Python fuzzing code")
    print()
    print("⚠️  CRITICAL: DO NOT run Python scripts directly!")
    print("   ❌ ./fuzz/run-fuzzers.py     # WRONG - Don't do this")
    print("   ✅ make fuzz                 # CORRECT - Always use this")
    print()
    print("🏁 Quick Start:")
    print("   1. make fuzz_clean    # Clean previous runs")
    print("   2. make fuzz          # Start fuzzing")
    print("   3. make fuzz_crash    # Analyze any crashes (if needed)")
    print()
    print("📊 Configuration:")
    print(f"   • Config file: {config_path}")
    print("   • Modify MAX_SECONDS and FUZZER_JOBS as needed")
    print()
    return True

if __name__ == "__main__":
    if "--update" in sys.argv:
        success = upgrade_fuzzer_infrastructure()
        sys.exit(0 if success else 1)
    elif "--execute" in sys.argv:
        success = execute_fuzzer_session()
        sys.exit(0 if success else 1)
    else:
        print("❌ This command requires either --update or --execute flag.")
        print()
        print("Usage:")
        print("  claude --file .claude/commands/fuzz-update.md --update   # Upgrade fuzzing infrastructure")
        print("  claude --file .claude/commands/fuzz-update.md --execute  # Run automated fuzzing with crash fixes")
        print()
        print("Flags:")
        print("  --update   : Upgrade the fuzzing infrastructure to use common library")
        print("  --execute  : Show instructions for running fuzzing with make targets")
        print()
        print("The --execute flag will show you how to:")
        print("  1. Run fuzzing using 'make fuzz' (REQUIRED)")
        print("  2. Analyze crashes using 'make fuzz_crash'")
        print("  3. Clean artifacts using 'make fuzz_clean'")
        print("  4. Format code using 'make fuzz_fmt'")
        print()
        print("⚠️  IMPORTANT: Always use 'make fuzz' - never call Python scripts directly!")
        sys.exit(1)
```

## Requirements

- The project must have a `/fuzz` directory with existing `.cpp` fuzzer implementations
- The common fuzzing library must be available at `/deps/ledger-zxlib/fuzzing/`
- CMake-based build system using CMakeLists.txt

## Benefits after upgrade

- **Auto-detection** - Automatically finds and configures all `.cpp` files in `/fuzz`
- **Adaptive resource management** - Automatic CPU/memory allocation based on system
- **Better logging** - Job-specific logs with crash consolidation  
- **Cross-platform support** - Works on Linux and macOS
- **Configurable settings** - Easy to adjust via fuzz_config.py
- **Python formatting** - Automated code formatting with `make fuzz_fmt`
- **Crash analysis** - Built-in crash analysis tools
- **Sanitizer optimization** - Configured for embedded/static allocation apps