import re
import sys
import os

def convert_ripes_to_clean_format(input_text):
    clean_lines = []
    
    # Regular expression to match instruction lines (skipping labels)
    instr_pattern = re.compile(r'^\s*[0-9a-fA-F]+:\s+([0-9a-fA-F]{8})\s+(.+)$')
    
    # Pattern to remove label references in branch instructions
    label_pattern = re.compile(r'\s+<[^>]+>')
    
    for line in input_text.strip().split('\n'):
        # Skip label lines
        if '<' in line and '>:' in line:
            continue
            
        # Match and process instruction lines
        match = instr_pattern.match(line)
        if match:
            instruction = match.group(1)
            assembly = match.group(2)
            
            # Remove any label references in the assembly
            assembly = label_pattern.sub('', assembly)
            
            # Format the line in the desired way
            clean_line = f"0x{instruction} {assembly}"
            clean_lines.append(clean_line)
    
    return '\n'.join(clean_lines)

def main():
    # Define input file directory
    input_files_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "inputfiles")
    
    # Ensure inputfiles directory exists
    if not os.path.exists(input_files_dir):
        os.makedirs(input_files_dir)
    
    # Determine input source and output destination
    if len(sys.argv) == 1:
        # No arguments: read from stdin, save to default
        print("Please paste your RIPES output (press Ctrl+D when finished):")
        input_text = sys.stdin.read()
        output_file = os.path.join(input_files_dir, "ripes_input.txt")
    elif len(sys.argv) == 2:
        # One argument: read from stdin, save to specified file in inputfiles
        print("Please paste your RIPES output (press Ctrl+D when finished):")
        input_text = sys.stdin.read()
        output_file = os.path.join(input_files_dir, sys.argv[1])
    else:
        # Two or more arguments: read from first file, save to second
        with open(sys.argv[1], 'r') as f:
            input_text = f.read()
        output_file = sys.argv[2]

    # Convert the input
    output = convert_ripes_to_clean_format(input_text)
    
    # Save to file
    with open(output_file, 'w') as f:
        f.write(output)
    
    print(f"Output saved to {output_file}")
    print(output)  # Also display the output in terminal

if __name__ == "__main__":
    main()