#!/bin/bash

# COMPUTING_ID=your_id <--- do this before running the script

LAB=Lab2

# Function to print warning messages in red
print_warning() {
    echo -e "\e[31mWarning: $1\e[0m"
}

# Function to check if any file is larger than 50MB
check_file_size() {
    local file_size_limit=$((50 * 1024 * 1024)) # 50MB in bytes
    find . -type f | while read -r file; do
        file_size=$(stat -c%s "$file")
        if [ "$file_size" -gt "$file_size_limit" ]; then
            print_warning "File larger than 50MB - $file"
            all_conditions_met=false
        fi
    done
}

# Function to check if DELIVERABLE/ directory exists
check_deliverable_dir() {
    if [ ! -d "DELIVERABLE" ]; then
        print_warning "DELIVERABLE directory does not exist"
        all_conditions_met=false
    fi
}

# Function to check if any file in DELIVERABLE is empty
check_deliverable_files() {
    if [ -d "DELIVERABLE" ]; then
        find DELIVERABLE -type f | while read -r file; do
            if [ ! -s "$file" ] && [ "$(basename "$file")" != ".gitkeep" ]; then
                print_warning "Empty file found in DELIVERABLE - $file"
                all_conditions_met=false
            fi
        done
    fi
}

# Function to check if image files in DELIVERABLE have at least 1024x1024 resolution
check_image_resolution() {
    if ! command -v identify &> /dev/null; then
        print_warning "'identify' command not found. Please install ImageMagick using 'sudo apt-get install imagemagick'."
        all_conditions_met=false
        return
    fi

    if [ -d "DELIVERABLE" ]; then
        for image_file in $(find DELIVERABLE -type f \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.bmp" \)); do
            resolution=$(identify -format "%w %h" "$image_file" 2>/dev/null)
            if [ -n "$resolution" ]; then
                width=$(echo "$resolution" | awk '{print $1}')
                height=$(echo "$resolution" | awk '{print $2}')
                if [ "$width" -lt 1024 ] || [ "$height" -lt 1024 ]; then
                    print_warning "Image resolution is less than 1024x1024 - $image_file"
                    all_conditions_met=false
                fi
            fi
        done
    fi
}

# Function to check if video files in DELIVERABLE have resolution larger than 1024x1024 and duration between 5 and 20 seconds
check_video_files() {
    if ! command -v ffprobe &> /dev/null; then
        print_warning "'ffprobe' command not found. Please install FFmpeg using 'sudo apt-get install ffmpeg'."
        all_conditions_met=false
        return
    fi

    if [ -d "DELIVERABLE" ]; then
        for video_file in $(find DELIVERABLE -type f \( -iname "*.mp4" -o -iname "*.mov" -o -iname "*.avi" -o -iname "*.mkv" -o -iname "*.gif" \)); do
            resolution=$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 "$video_file")
            duration=$(ffprobe -v error -select_streams v:0 -show_entries format=duration -of csv=p=0 "$video_file")
            if [ -n "$resolution" ] && [ -n "$duration" ]; then
                width=$(echo "$resolution" | cut -d',' -f1)
                height=$(echo "$resolution" | cut -d',' -f2)
                duration=${duration%.*} # Convert duration to integer (seconds)
                if [ "$width" -le 1024 ] || [ "$height" -le 1024 ] || [ "$duration" -lt 5 ] || [ "$duration" -gt 20 ]; then
                    print_warning "Video resolution $width x $height is less than 1024x1024, or duration $duration is not between 5 and 20 seconds - $video_file"
                    all_conditions_met=false
                fi
            fi
        done
    fi
}


# Run the checks
all_conditions_met=true

# Check for any file larger than 50MB
check_file_size

# Check if DELIVERABLE directory exists
check_deliverable_dir

# Check if any file in DELIVERABLE is empty
check_deliverable_files

# Check if image files in DELIVERABLE have at least 1024x1024 resolution
check_image_resolution

# Check if video files in DELIVERABLE have resolution larger than 1024x1024 and duration between 5 and 20 seconds
check_video_files

# Ask user if they want to continue if any check fails
if [ "$all_conditions_met" = false ]; then
    read -p "One or more checks failed. You can still proceed if you believe the files are good. Continue? (y/N) " user_response
    if [[ ! "$user_response" =~ ^[Yy]$ ]]; then
        echo "Exiting script."
        exit 1
    fi
else
        echo -e "\e[32mAll conditions met.\e[0m"
fi

# Check if COMPUTING_ID is set
if [ -z "$COMPUTING_ID" ] || [[ "$COMPUTING_ID" =~ ^[[:space:]]*$ ]]; then
        print_warning "COMPUTING_ID environment variable is not set or contains only spaces. Please set it using export COMPUTING_ID=your_id."
        echo "Exiting script."
        exit 1
fi

# Replace spaces in COMPUTING_ID with dashes
COMPUTING_ID=$(echo "$COMPUTING_ID" | tr ' ' '-')

OUTFILE="/tmp/${LAB}-${COMPUTING_ID}.tar.gz"

# Create tarball excluding specific contents
tar --exclude='.git' \
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
        --exclude='.vscode' \
        --exclude='.github' \
        --exclude='docs' \
        --exclude='.gdb_history' \
        -czf $OUTFILE .

# Print completion message
echo "Tarball created at"
ls -lh $OUTFILE

# Check if OUTFILE size is 0 or larger than 200MB
outfile_size=$(stat -c%s "$OUTFILE")
if [ "$outfile_size" -eq 0 ]; then
        print_warning "The tarball size is 0 bytes."
elif [ "$outfile_size" -gt $((200 * 1024 * 1024)) ]; then
        print_warning "The tarball size is larger than 200MB."
fi

echo "Now double check the content of $OUTFILE, and submit it to Canvas."