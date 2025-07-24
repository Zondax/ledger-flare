# Fuzzer Find and Fix Command

This command helps identify and fix errors using the fuzzer tool in Ledger applications. It follows a systematic approach to run fuzzing tests, identify issues, and implement fixes.

**CRITICAL REQUIREMENTS:**
- Always use commands from `deps/ledger-zxlib/dockerized_build.mk` - never execute direct Python commands
- Always remain in the root path of the project for commands to execute correctly
- Never modify code that is part of a Git submodule

## Step 1: Verify Fuzzing Environment

First, check if the fuzzing environment is properly set up:

```bash
# Check if fuzzing directory exists
if [ ! -d "./deps/ledger-zxlib/fuzzing" ]; then
    echo "Fuzzing directory not found. Updating submodules..."
    git submodule update
fi

# Verify fuzzing directory exists after update
if [ ! -d "./deps/ledger-zxlib/fuzzing" ]; then
    echo "ERROR: Fuzzing directory still not found after submodule update"
    exit 1
fi
```

## Step 2: Validate Fuzz Configuration

Check if the project has the required fuzzer configuration:

```bash
# Check if fuzz_config.py exists
if [ ! -f "./fuzz/fuzz_config.py" ]; then
    echo "ERROR: fuzz_config.py not found in ./fuzz directory"
    echo "The fuzzer must be updated for this project."
    echo "Please run the command described in fuzz-update.md"
    exit 1
fi

echo "Fuzz configuration found. Proceeding with fuzzing process..."
```

## Step 3: Configure Quick Test Parameters

Modify the fuzz_config.py file to enable a quick test run:

```bash
# Backup original configuration
cp ./fuzz/fuzz_config.py ./fuzz/fuzz_config.py.backup

# Set quick test parameters
sed -i.tmp 's/MAX_SECONDS = .*/MAX_SECONDS = 120/' ./fuzz/fuzz_config.py
sed -i.tmp 's/FUZZER_JOBS = .*/FUZZER_JOBS = 4/' ./fuzz/fuzz_config.py

echo "Configuration updated for quick test run:"
echo "- MAX_SECONDS = 120"
echo "- FUZZER_JOBS = 4"
```

## Step 4: Clean and Run Initial Fuzzer Test

Clean previous fuzzing artifacts and run the fuzzer:

```bash
# Clean all fuzzing files
echo "Cleaning fuzzing artifacts..."
make fuzz_clean

# Run the fuzzer
echo "Starting fuzzer with quick test configuration..."
make fuzz
```

## Step 5: Handle Fuzzer Results

Check the fuzzer execution result:

```bash
if [ $? -eq 0 ]; then
    echo "Fuzzer completed successfully with quick test."
    echo "Updating MAX_SECONDS to 300 for extended testing..."
    sed -i.tmp 's/MAX_SECONDS = .*/MAX_SECONDS = 300/' ./fuzz/fuzz_config.py
    echo "Running extended fuzzer test..."
    make fuzz
else
    echo "Fuzzer encountered errors. Analyzing crash logs..."
    make fuzz_crash
fi
```

## Step 6: Analyze Crash Logs

When errors occur, analyze the generated crash logs:

```bash
# Check for crash logs
if [ -d "./fuzz/logs" ]; then
    crash_files=$(ls ./fuzz/logs/crash_* 2>/dev/null)
    if [ -n "$crash_files" ]; then
        echo "Crash logs found:"
        for crash_file in $crash_files; do
            echo "=== $crash_file ==="
            cat "$crash_file"
            echo ""
        done
    else
        echo "No crash files found in ./fuzz/logs"
    fi
else
    echo "No logs directory found"
fi
```

## Step 7: Code Analysis and Classification

Analyze the code to determine where changes are needed:

**App Code Classification:**
Code within these defines belongs to the app:
```c
#if defined(TARGET_NANOS) || defined(TARGET_NANOX) || defined(TARGET_NANOS2) || defined(TARGET_STAX) || defined(LEDGER_SPECIFIC)
// This is APP CODE
#else
// This is CPP_TESTS CODE
#endif
```

**Implementation Strategy:**
- **App Code Issues**: Require careful analysis and proper fixes
- **CPP Tests Issues**: Can use wrapper solutions with `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` flag
- **Never modify code that belongs to Git submodules**

## Step 8: Implement Fixes (Iterative Process)

For each identified issue, implement fixes and validate:

```bash
# After implementing fixes, test with crash detection
echo "Testing fixes with crash detection..."
make fuzz_crash

# This step should be repeated up to 5 times
# Track attempt count to prevent infinite loops
attempt_count=1
max_attempts=5

while [ $attempt_count -le $max_attempts ]; do
    echo "Fix attempt $attempt_count of $max_attempts"
    make fuzz_crash
    
    if [ $? -eq 0 ]; then
        echo "No crashes detected. Proceeding to next step."
        break
    else
        echo "Crashes still detected. Analyzing and implementing additional fixes..."
        if [ $attempt_count -eq $max_attempts ]; then
            echo "ERROR: Maximum attempts reached. Could not resolve all fuzzer issues."
            echo "Please review the crash logs and implement manual fixes."
            exit 1
        fi
    fi
    
    ((attempt_count++))
done
```

## Step 9: Validate CPP Tests

Ensure that fixes don't break existing functionality:

```bash
echo "Validating CPP tests after fixes..."
rm -rf build
make cpp_test

if [ $? -ne 0 ]; then
    echo "ERROR: CPP tests failed after implementing fixes."
    echo "Reverting changes and trying different approach..."
    
    # Restore backup if available
    if [ -f "./fuzz/fuzz_config.py.backup" ]; then
        cp ./fuzz/fuzz_config.py.backup ./fuzz/fuzz_config.py
    fi
    
    echo "Please review the fixes and ensure they don't break existing functionality."
    exit 1
fi

echo "CPP tests passed successfully."
```

## Step 10: Configure and Run Zemu Tests

Modify Zemu tests to run only on nanosp target:

```bash
# Backup original common.ts
cp ./tests_zemu/tests/common.ts ./tests_zemu/tests/common.ts.backup

# Comment out other targets, keeping only nanosp
sed -i.tmp 's/{ name: "nanox", prefix: "X", path: APP_PATH_X },/\/\/ { name: "nanox", prefix: "X", path: APP_PATH_X },/' ./tests_zemu/tests/common.ts
sed -i.tmp 's/{ name: "stax", prefix: "ST", path: APP_PATH_ST },/\/\/ { name: "stax", prefix: "ST", path: APP_PATH_ST },/' ./tests_zemu/tests/common.ts
sed -i.tmp 's/{ name: "flex", prefix: "FL", path: APP_PATH_FL },/\/\/ { name: "flex", prefix: "FL", path: APP_PATH_FL },/' ./tests_zemu/tests/common.ts

echo "Modified common.ts to run tests only on nanosp target"
```

## Step 11: Execute Zemu Tests

Run the Zemu tests to validate the fixes:

```bash
echo "Running Zemu tests..."
make clean
make buildS2
make zemu_test

if [ $? -ne 0 ]; then
    echo "ERROR: Zemu tests failed."
    echo "Please review the fixes and ensure compatibility with Zemu tests."
    
    # Restore common.ts backup
    if [ -f "./tests_zemu/tests/common.ts.backup" ]; then
        cp ./tests_zemu/tests/common.ts.backup ./tests_zemu/tests/common.ts
    fi
    
    exit 1
fi

echo "Zemu tests passed successfully."

# IMPORTANT: If fixes were implemented in Steps 6-8, return to Step 4 for verification
# This ensures that the fixes don't introduce new fuzzer issues
echo "IMPORTANT: After implementing fixes and passing Zemu tests,"
echo "you must return to Step 4 to run the fuzzer again and verify"
echo "that your fixes didn't introduce new issues."
echo ""
echo "==> Go back to Step 4: Clean and Run Initial Fuzzer Test"
echo "==> This creates a feedback loop to ensure fix quality"
```

## Step 12: Final Extended Fuzzer Test

Run the final extended fuzzer test:

```bash
echo "Running final extended fuzzer test..."
sed -i.tmp 's/MAX_SECONDS = .*/MAX_SECONDS = 300/' ./fuzz/fuzz_config.py
make fuzz_clean
make fuzz

if [ $? -eq 0 ]; then
    echo "SUCCESS: All fuzzing tests completed without errors."
    echo "Fuzzer find and fix process completed successfully."
else
    echo "ERROR: Extended fuzzer test failed."
    echo "Please review the implementation and crash logs."
    exit 1
fi
```

## Step 13: Cleanup and Restore

Restore original configurations:

```bash
echo "Restoring original configurations..."

# Restore fuzz_config.py if backup exists
if [ -f "./fuzz/fuzz_config.py.backup" ]; then
    cp ./fuzz/fuzz_config.py.backup ./fuzz/fuzz_config.py
    rm ./fuzz/fuzz_config.py.backup
fi

# Restore common.ts if backup exists
if [ -f "./tests_zemu/tests/common.ts.backup" ]; then
    cp ./tests_zemu/tests/common.ts.backup ./tests_zemu/tests/common.ts
    rm ./tests_zemu/tests/common.ts.backup
fi

echo "Cleanup completed. Original configurations restored."
echo "Fuzzer find and fix process completed successfully!"
```

## Usage Notes

1. **Always run from project root**: All commands must be executed from the project root directory
2. **Use dockerized commands**: Never execute direct Python commands; always use make targets from dockerized_build.mk
3. **Iterative fixing**: The crash detection and fixing process may require multiple iterations
4. **Backup configurations**: Always backup configuration files before modifying them
5. **Validate changes**: Ensure CPP tests and Zemu tests pass after implementing fixes
6. **Submodule safety**: Never modify code that belongs to Git submodules
7. **Feedback loop**: After implementing fixes and passing Zemu tests (Step 11), always return to Step 4 to verify that fixes don't introduce new fuzzer issues

## Error Handling

- If fuzzing directory is missing after submodule update, the fuzzer setup is incomplete
- If fuzz_config.py is missing, the fuzzer needs to be updated (see fuzz-update.md)
- If fixes fail after 5 attempts, manual intervention is required
- If CPP tests fail after fixes, the changes need to be reviewed and corrected
- If Zemu tests fail, compatibility issues need to be addressed

## Expected Outcomes

- Successful fuzzer execution without crashes
- All CPP tests passing
- Zemu tests running successfully on nanosp target
- Clean fuzzer run with extended 300-second timeout
- Proper code fixes that maintain functionality while resolving fuzzer issues