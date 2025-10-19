#!/bin/bash

# Define source and destination directories
SRC_DIR="$(pwd)"
DEST_DIR="/tmp/prep-push"

# Get the current date in MMDDYY format
DATE=$(date +%b%d-%Y)

# Get the latest git commit hash
GIT_COMMIT=$(git rev-parse --short HEAD)

# Check if the --no-code argument is passed
if [[ "$*" == *--no-code* ]]; then
    EXCLUDES="--exclude=kernel --exclude=usr --exclude=scripts"
    # Insert a new line after the 1st line in docs/README.md
    sed -i '1a ## To UVA students: the code will be available after the submission opens' docs/README.md
else
    EXCLUDES=""
fi

rm -rf $DEST_DIR

# Mirror the current directory to /tmp/prep-push with exclusions
rsync -av --delete \
    --exclude='*.zip' \
    --exclude='*.tgz' \
    --exclude='*.rar' \
    --exclude='*.7z' \
    --exclude='staged' \
    --exclude='*.o' \
    --exclude='*.bin' \
    --exclude='*.img' \
    --exclude='*.elf' \
    --exclude='*.d' \
    --exclude='*.sym' \
    --exclude='*.log' \
    --exclude='*.list' \
    --exclude='*.a' \
    --exclude='.github' \
    --exclude='docs/archived' \
    --exclude='.gdb_history' \
    $EXCLUDES \
    "$SRC_DIR/" "$DEST_DIR/"

    # --exclude='.vscode' \


# Change to the destination directory
cd "$DEST_DIR" || exit

# Delete existing branch "student" if it exists
git branch -D student 2>/dev/null

# Initialize a new orphan branch and commit the files
git checkout --orphan student
git add .
git commit -m "Date: $DATE, Commit: $GIT_COMMIT"

# Calculate total file sizes excluding .git and ask for confirmation
TOTAL_SIZE=$(du -sh --exclude=.git . | cut -f1)
echo "Total file size (excluding .git): $TOTAL_SIZE"
read -p "Proceed with push? (y/N): " confirm
if [[ $confirm != "y" ]]; then
    echo "Push aborted."
    exit 1
fi

# Add the remote repository and push the branch
git remote add uva-os git@github.com:fxlin/uva-os-world2.git
git push -u uva-os student --force

# Print the GitHub URL
echo "Push completed. Repository URL: https://github.com/fxlin/uva-os-world2/tree/student"
