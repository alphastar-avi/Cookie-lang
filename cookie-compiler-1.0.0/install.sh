#!/bin/bash

# Cookie Compiler Installation Script

set -e

PREFIX="/usr/local"
INSTALL_BIN="$PREFIX/bin"
INSTALL_LIB="$PREFIX/lib"

echo "Installing Cookie Compiler..."

# Check if running as root for system-wide installation
if [ "$EUID" -ne 0 ]; then
    echo "Note: Running without sudo. Installing to ~/.local/"
    PREFIX="$HOME/.local"
    INSTALL_BIN="$PREFIX/bin"
    INSTALL_LIB="$PREFIX/lib"
    mkdir -p "$INSTALL_BIN" "$INSTALL_LIB"
fi

# Copy files
cp bin/cookie "$INSTALL_BIN/"
cp bin/cookie_compiler.sh "$INSTALL_BIN/"
cp lib/libruntime.a "$INSTALL_LIB/"

# Make executable
chmod +x "$INSTALL_BIN/cookie"
chmod +x "$INSTALL_BIN/cookie_compiler.sh"

echo "Cookie Compiler installed successfully!"
echo "Installed to: $INSTALL_BIN"
echo ""
echo "Usage: cookie_compiler.sh [OPTIONS] input.cook"
echo ""
echo "Add $INSTALL_BIN to your PATH if not already present:"
echo "export PATH=\$PATH:$INSTALL_BIN"
