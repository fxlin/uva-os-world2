#!/usr/bin/env python3

import os
import argparse
import re
import sys

'''
    remove lines with keywords. examples: 

    --mode=remove

        .c, .h, .S files:
            (source...) // !STUDENT_WONT_SEE   
            becomes
            /* TODO: your code here */

            // !STUDENT_WONT_SEE_BEGIN
            (source...) 
            // !STUDENT_WONT_SEE_END
            becomes
            /* TODO: your code here */

            ventry	el1_irq	  //!STUDENT_WILL_SEE (ventry	irq_invalid_el1h)
            becomes 
            ventry	irq_invalid_el1h /* TODO: replace this */
        
        *Makefile: 
            (source ...) # !STUDENT_WONT_SEE
            becomes 
            # TODO: your code here

            XXXX # !STUDENT_WILL_SEE (YYYY)
            becomes 
            YYYY # TODO: replace this

            
            #!STUDENT_WILL_SEE(YYYY)
            becomes 
            YYYY # TODO: replace this

    --mode=comment:
    .c, .h, .S files:
            (source...) // !STUDENT_WONT_SEE
            becomes
            // (source...) // !STUDENT_WONT_SEE

            XXXX	  //!STUDENT_WILL_SEE (YYY)
            becomes 
            YYY /* TODO: replace this */ // !STUDENT_SHOULD_WRITE(XXXX)

    *Makefile:
        (source ...) # !STUDENT_WONT_SEE
        becomes 
        # TODO: your code here

        XXXX # !STUDENT_WILL_SEE (YYYY)
        becomes 
        YYYY # TODO: replace this !STUDENT_SHOULD_WRITE(XXXX)

    --mode=dryrun:
         just count the number of lines that would be removed,
        but do not actually remove them.

'''

# Keywords for line and block removal
STUDENT_WONT_SEE_KEYWORD = "!STUDENT_WONT_SEE"
STUDENT_WONT_SEE_BEGIN_KEYWORD = "!STUDENT_WONT_SEE_BEGIN"
STUDENT_WONT_SEE_END_KEYWORD = "!STUDENT_WONT_SEE_END"
REPLACE_LINE_PATTERN = r"(.*)\s*//!STUDENT_WILL_SEE\s*\((.*)\)"
MAKEFILE_DONOT_SEE_PATTERN = r"(.*)\s*#\s*!STUDENT_WONT_SEE"
MAKEFILE_REPLACE_PATTERN = r"(.*)\s*#\s*!STUDENT_WILL_SEE\s*\((.*)\)"

def process_file(file_path, mode, write_to_stdout=False):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    new_lines = []
    in_remove_block = False
    lines_removed = 0
    consecutive_STUDENT_WONT_SEE = False
    last_indent = ""

    is_makefile = file_path.endswith('Makefile')

    for line_num, line in enumerate(lines):
        # Handle Makefile-specific syntax
        if is_makefile:
            # Remove or comment lines with MAKEFILE_DONOT_SEE_PATTERN
            if re.search(MAKEFILE_DONOT_SEE_PATTERN, line):
                lines_removed += 1
                if mode == "comment" or mode == "remove":
                    new_lines.append(f"# TODO: your code here\n")
                continue

            # Replace or comment lines with MAKEFILE_REPLACE_PATTERN
            match = re.search(MAKEFILE_REPLACE_PATTERN, line)
            if match:
                original_text = match.group(1).strip()
                replacement_text = match.group(2).strip()
                if mode == "comment":
                    new_lines.append(f"{replacement_text} # TODO: replace this !STUDENT_SHOULD_WRITE({original_text})\n")
                else:
                    new_lines.append(f"{replacement_text} # TODO: replace this\n")
                lines_removed += 1
                continue

        # Check if we need to start removing a block
        if STUDENT_WONT_SEE_BEGIN_KEYWORD in line:
            if in_remove_block:
                print(f"Error: Nested or unmatched {STUDENT_WONT_SEE_BEGIN_KEYWORD} at line {line_num + 1} in {file_path}")
                return False, 0
            in_remove_block = True
            last_indent = re.match(r"\s*", line).group(0)
            lines_removed += 1
            if mode == "comment":
                new_lines.append(f"// {line}")                
            continue

        # Check if we need to end removing a block
        if STUDENT_WONT_SEE_END_KEYWORD in line:
            if not in_remove_block:
                print(f"Error: {STUDENT_WONT_SEE_END_KEYWORD} found without matching {STUDENT_WONT_SEE_BEGIN_KEYWORD} at line {line_num + 1} in {file_path}")
                return False, 0
            in_remove_block = False
            last_indent = re.match(r"\s*", line).group(0)
            lines_removed += 1
            if mode == "comment":
                new_lines.append(f"// {line}")
            new_lines.append(f"{last_indent}/* TODO: your code here */\n")
            continue

        # Skip lines in the remove block
        if in_remove_block:
            lines_removed += 1
            last_indent = re.match(r"\s*", line).group(0)
            if mode == "comment":
                new_lines.append(f"// {line}")
            continue

        # Remove single line containing the STUDENT_WONT_SEE_KEYWORD
        if STUDENT_WONT_SEE_KEYWORD in line:
            lines_removed += 1
            consecutive_STUDENT_WONT_SEE = True
            last_indent = re.match(r"\s*", line).group(0)
            if mode == "comment":
                new_lines.append(f"// {line}")
            continue

        # Replace line with REPLACE_LINE_PATTERN
        match = re.search(REPLACE_LINE_PATTERN, line)
        if match:
            original_text = match.group(1).strip()
            replacement_text = match.group(2).strip()
            last_indent = re.match(r"\s*", line).group(0)
            if mode == "comment":
                new_lines.append(f"{replacement_text} /* TODO: replace this */ // !STUDENT_SHOULD_WRITE({original_text})\n")
            else:
                new_lines.append(f"{replacement_text} /* TODO: replace this */\n")
            lines_removed += 1
            continue

        # Add TODO comment if consecutive lines with STUDENT_WONT_SEE_KEYWORD were removed
        if consecutive_STUDENT_WONT_SEE:
            new_lines.append(f"{last_indent}/* TODO: your code here */\n")
            consecutive_STUDENT_WONT_SEE = False

        # Otherwise, keep the line
        new_lines.append(line)

    # Check if we ended in a remove block without finding STUDENT_WONT_SEE_END_KEYWORD
    if in_remove_block:
        print(f"Error: {STUDENT_WONT_SEE_BEGIN_KEYWORD} without matching {STUDENT_WONT_SEE_END_KEYWORD} in {file_path} (started at line {line_num + 1})")
        return False, 0

    # Write the modified content back to the file if changes were made and not in dry-run mode
    if write_to_stdout:
        sys.stdout.writelines(new_lines)
    elif lines_removed > 0 and mode != "dry-run":
        with open(file_path, 'w') as file:
            file.writelines(new_lines)
    
    return True, lines_removed

# Command line argument parsing
parser = argparse.ArgumentParser(description="Process C and asm files to remove specific lines or blocks.")
parser.add_argument("--mode", choices=["dry-run", "comment", "remove"], required=True, help="Specify the mode of operation: dry-run, comment, or remove")
parser.add_argument("-i", type=str, help="Specify a single file to process.")
parser.add_argument("-d", type=str, help="Specify a directory to process.")
args = parser.parse_args()

# Check arguments
if not args.i and not args.d:
    parser.print_usage()
    exit(1)

# Summary information
files_processed = 0
files_changed = 0
lines_removed_summary = {}

# Process a specific file if -i is provided
if args.i:
    success, lines_removed = process_file(args.i, args.mode, write_to_stdout=True)
    if success:
        files_processed += 1
        if lines_removed > 0:
            files_changed += 1
            lines_removed_summary[args.i] = lines_removed
        print(f"Processed {args.i}, lines removed: {lines_removed}", file=sys.stderr)
    else:
        print(f"Processing failed for {args.i}", file=sys.stderr)

# Process each file in the directory if -d is provided
if args.d:
    for root, _, files in os.walk(args.d):
        for file_name in files:
            if file_name.endswith(('.c', '.h', '.S', 'Makefile', '.ld')):
                file_path = os.path.join(root, file_name)
                success, lines_removed = process_file(file_path, args.mode)
                if not success:
                    print(f"Processing failed for {file_path}", file=sys.stderr)
                    break
                files_processed += 1
                if lines_removed > 0:
                    files_changed += 1
                    lines_removed_summary[file_path] = lines_removed
                    print(f"Processed {file_path}, lines removed: {lines_removed}")

# Print summary to stderr
print("\nSummary:", file=sys.stderr)
print(f"Total files processed: {files_processed}", file=sys.stderr)
print(f"Total files changed: {files_changed}", file=sys.stderr)
total_lines_removed=sum(lines_removed_summary.values())
print(f"Total lines removed: {total_lines_removed}", file=sys.stderr)

if files_changed > 0:
    for file_path, lines_removed in lines_removed_summary.items():
        print(f"{file_path}: {lines_removed} lines removed", file=sys.stderr)
