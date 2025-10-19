#!/bin/bash

# Pull common files from a different "lab" repo
# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --from) LAB="$2"; shift ;;
        --file) FILE="$2"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

if [ -z "$LAB" ] || [ -z "$FILE" ]; then
    echo "Usage: $0 --from <lab> --file <relative_path>"
    exit 1
fi

# Set the repository URL
REPO_URL="https://raw.githubusercontent.com/fxlin/p1-kernel-${LAB}/main"
GIT_REPO_URL="https://github.com/fxlin/p1-kernel-${LAB}.git"


# Construct the full URL for the remote file
REMOTE_URL="${REPO_URL}/${FILE}"

# Check if the remote file exists in the repository
response_code=$(curl -s -o /dev/null -w "%{http_code}" "$REMOTE_URL")
if [ "$response_code" -ne 200 ]; then
    echo "Error: Remote file '$REMOTE_URL' not found."
    exit 1
fi

# Fetch the file and overwrite the local file
echo "Fetching '$FILE' to overwrite '$FILE'..."
curl -s "$REMOTE_URL" -o "$FILE"

if [ $? -eq 0 ]; then
    echo "Successfully fetched and updated '$FILE'."
else
    echo "Error: Failed to fetch '$FILE'."
fi

echo "Done."
