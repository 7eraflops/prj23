#!/usr/bin/env bash
# Generates a local .clangd file with the correct include paths from the ESP-IDF toolchain.
# This solves the issue where clangd cannot find C++ standard library headers.

# Try to find the compiler from compile_commands.json first
if [ -f "firmware/build/compile_commands.json" ]; then
    COMPILER=$(grep -o '"command": "[^"]*xtensa-[^-]*-elf-g++' firmware/build/compile_commands.json | head -n 1 | awk -F'"' '{print $4}')
fi

# Fallback to PATH
if [ -z "$COMPILER" ] || [ ! -x "$COMPILER" ]; then
    COMPILER=$(which xtensa-esp32s3-elf-g++ 2>/dev/null)
fi

if [ -z "$COMPILER" ] || [ ! -x "$COMPILER" ]; then
    echo "ESP32 C++ compiler not found."
    echo "Please source your ESP-IDF export.sh first, or run 'idf.py build' to generate compile_commands.json."
    exit 1
fi

echo "Found compiler: $COMPILER"
echo "Generating .clangd..."

cat <<INNER_EOF > .clangd
CompileFlags:
  Add:
    - "--target=xtensa-esp-elf"
INNER_EOF

# Extract system include paths correctly
"$COMPILER" -E -v -x c++ /dev/null 2>&1 | awk '/#include <...>/ {flag=1; next} /End of search list./ {flag=0} flag {print $1}' | while read -r inc; do
    # Resolve absolute path
    abs_inc=$(readlink -f "$inc" || echo "$inc")
    echo "    - \"-isystem\"" >> .clangd
    echo "    - \"$abs_inc\"" >> .clangd
done

cat <<INNER_EOF >> .clangd
  Remove:
    - "-mlongcalls"
    - "-fno-tree-switch-conversion"
    - "-fno-shrink-wrap"
    - "-fstrict-volatile-bitfields"
    - "-fno-jump-tables"
INNER_EOF

echo ".clangd generated successfully."
