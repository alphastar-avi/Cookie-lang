#!/bin/bash

# Cookie Language Compiler Wrapper
# This script compiles Cookie source code to native executables

set -e

# Default settings
INPUT_FILE=""
OUTPUT_FILE="a.out"
VERBOSE=false
KEEP_INTERMEDIATE=false
RUNTIME_PATH="$(dirname "$0")/../lib/libruntime.a"

# Help function
show_help() {
    echo "Cookie Language Compiler"
    echo "Usage: $0 [OPTIONS] input.cook"
    echo ""
    echo "Options:"
    echo "  -o FILE     Output executable name (default: a.out)"
    echo "  -v          Verbose output"
    echo "  -k          Keep intermediate files"
    echo "  -h          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 program.cook"
    echo "  $0 -o myprogram program.cook"
    echo "  $0 -v -k program.cook"
}

# Parse command line arguments
while getopts "o:vkh" opt; do
    case $opt in
        o)
            OUTPUT_FILE="$OPTARG"
            ;;
        v)
            VERBOSE=true
            ;;
        k)
            KEEP_INTERMEDIATE=true
            ;;
        h)
            show_help
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            show_help
            exit 1
            ;;
    esac
done

# Get the input file
shift $((OPTIND-1))
if [ $# -eq 0 ]; then
    echo "Error: No input file specified" >&2
    show_help
    exit 1
fi

INPUT_FILE="$1"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found" >&2
    exit 1
fi

# Check if Cookie compiler exists
COOKIE_COMPILER="$(dirname "$0")/cookie"
if [ ! -f "$COOKIE_COMPILER" ]; then
    echo "Error: Cookie compiler not found at '$COOKIE_COMPILER'" >&2
    echo "Please run 'make' to build the compiler first" >&2
    exit 1
fi

# Check if runtime library exists
if [ ! -f "$RUNTIME_PATH" ]; then
    echo "Error: Runtime library not found at '$RUNTIME_PATH'" >&2
    echo "Please run 'make' to build the runtime library first" >&2
    exit 1
fi

# Verbose output function
log() {
    if [ "$VERBOSE" = true ]; then
        echo "[INFO] $1"
    fi
}

# Clean up intermediate files
cleanup() {
    if [ "$KEEP_INTERMEDIATE" = false ]; then
        rm -f output.ll output.o
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Start compilation process
log "Compiling Cookie source: $INPUT_FILE"

# Step 1: Compile Cookie source to LLVM IR
log "Generating LLVM IR..."
if ! cat "$INPUT_FILE" | "$COOKIE_COMPILER" > output.ll 2>&1; then
    echo "Error: Failed to compile Cookie source to LLVM IR" >&2
    exit 1
fi

# Step 2: Compile LLVM IR to object file
log "Compiling LLVM IR to object code..."
if ! llc -filetype=obj output.ll -o output.o 2>&1; then
    echo "Error: Failed to compile LLVM IR to object code" >&2
    echo "Make sure 'llc' is installed and in your PATH" >&2
    exit 1
fi

# Step 3: Link with runtime library to create executable
log "Linking with runtime library..."
RUNTIME_DIR="$(dirname "$RUNTIME_PATH")"
if ! gcc output.o "$RUNTIME_PATH" -Wl,-rpath,"$RUNTIME_DIR" -o "$OUTPUT_FILE" 2>&1; then
    echo "Error: Failed to link object code with runtime library" >&2
    echo "Make sure gcc is installed and runtime library is built" >&2
    exit 1
fi

# Make executable
chmod +x "$OUTPUT_FILE"

echo "Successfully compiled '$INPUT_FILE' to '$OUTPUT_FILE'"

# Show file info if verbose
if [ "$VERBOSE" = true ]; then
    log "Output file info:"
    ls -la "$OUTPUT_FILE"
    log "File type:"
    file "$OUTPUT_FILE"
fi
