#!/bin/bash

# Pull common files from the "main" repo

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --to) LAB="$2"; shift ;;
        --file) FILE="$2"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

if [ -z "$LAB" ] || [ -z "$FILE" ]; then
    echo "Usage: $0 --to <lab> --file <relative_path>"
    exit 1
fi

# Set the repository URL
GIT_REPO_URL="https://github.com/fxlin/p1-kernel-${LAB}.git"

if [ "$LAB" == "main" ]; then
    GIT_REPO_URL="https://github.com/fxlin/uva-os-main"
fi

# Create a temporary directory
TEMP_DIR=$(mktemp -d)

# Clone the target repository into the temporary directory
echo "Cloning repository into temporary directory..."
git clone "$GIT_REPO_URL" "$TEMP_DIR"

# Check if clone was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to clone repository."
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Copy the local file to the cloned repository
echo "Copying file '$FILE' to the cloned repository..."
cp "$FILE" "$TEMP_DIR/$FILE"

# Add the file to the cloned repository
cd "$TEMP_DIR"
git add "$FILE"

# Commit the changes
REPO_NAME=$(basename -s .git `git config --get remote.origin.url`)
COMMIT_MSG="Got cherry-pushed $FILE from $REPO_NAME"
git commit -m "$COMMIT_MSG"

# Push the changes to the remote repository
echo "Pushing changes to the remote repository..."
git push origin main

if [ $? -ne 0 ]; then
    echo "Error: Failed to push changes to the remote repository."
    cd ..
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Clean up the temporary directory
cd ..
echo "Cleaning up temporary directory..."
rm -rf "$TEMP_DIR"

echo "Done."
