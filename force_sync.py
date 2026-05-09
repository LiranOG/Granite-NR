import re
import io
import sys

# Ensure UTF-8 output
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def sledgehammer_pass():
    print("=== EXECUTING SLEDGEHAMMER REGEX PASS ===\n")

    # 1. README.md Fixes
    with open('README.md', 'r', encoding='utf-8') as f:
        readme_content = f.read()

    # Known Limitations Header
    print("--- README.md: Known Limitations ---")
    limitations_pattern = re.compile(r'(##.*Known Limitations.*)\(v0\.6\.7(?:\.2)?\)')
    match = limitations_pattern.search(readme_content)
    if match:
        print(f"BEFORE: {repr(match.group(0))}")
        readme_content = limitations_pattern.sub(r'\g<1>(v0.6.8)', readme_content)
        new_match = re.search(r'##.*Known Limitations.*\(v0\.6\.8\)', readme_content)
        print(f"AFTER:  {repr(new_match.group(0))}\n")

    # Version History Table
    print("--- README.md: Version History Table ---")
    table_pattern = re.compile(
        r'\|\s*\*\*\`v0\.6\.7\.2\`\*\*\s*\|\s*\*\*([^\*]+)\*\*\s*\|\s*(.*?)\s*\*\*Current\*\*\s*\|\s*\*\*([^\*]+)\*\*\s*\|'
    )
    match = table_pattern.search(readme_content)
    if match:
        print(f"BEFORE ROW: {repr(match.group(0))}")
        date_str = match.group(1).strip()
        check_mark = match.group(2).strip()
        desc = match.group(3).strip()
        
        demoted_row = f"| `v0.6.7.2` | {date_str} | {check_mark} | {desc} |"
        new_row = f"| **`v0.6.8`** | **2026-05-09** | ✅ **Current** | **Architecture & Stability Release** |"
        
        replacement = f"{demoted_row}\n{new_row}"
        readme_content = readme_content[:match.start()] + replacement + readme_content[match.end():]
        print(f"AFTER ROWS:\n{repr(demoted_row)}\n{repr(new_row)}\n")
    else:
        print("WARNING: Could not find the v0.6.7.2 row in the Version History table.")

    # Write changes back to README.md
    with open('README.md', 'w', encoding='utf-8', newline='') as f:
        f.write(readme_content)

    # 2. Globally replace in VALIDATION_STATUS.md and SOURCE_OF_TRUTH.md
    docs = ['docs/VALIDATION_STATUS.md', 'docs/SOURCE_OF_TRUTH.md']
    replace_pattern = re.compile(r'v0\.6\.7\.2|v0\.6\.7\.x|0\.6\.7\.2')
    
    for doc in docs:
        print(f"--- {doc}: Global Replacements ---")
        try:
            with open(doc, 'r', encoding='utf-8') as f:
                content = f.read()
            
            matches = list(replace_pattern.finditer(content))
            if matches:
                for m in matches:
                    start_idx = max(0, m.start() - 20)
                    end_idx = min(len(content), m.end() + 20)
                    context = content[start_idx:end_idx].replace('\n', ' ')
                    print(f"FOUND:  ...{context}...")

                new_content = replace_pattern.sub('v0.6.8', content)
                
                with open(doc, 'w', encoding='utf-8', newline='') as f:
                    f.write(new_content)
                print(f"SUCCESS: Replaced {len(matches)} occurrences in {doc}\n")
            else:
                print(f"CLEAN: No occurrences found in {doc}\n")
        except FileNotFoundError:
            print(f"ERROR: {doc} not found.\n")

if __name__ == '__main__':
    sledgehammer_pass()
