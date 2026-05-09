import re
import io
import sys
import os

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def sledgehammer_sync():
    # 1. README.md
    with open('README.md', 'r', encoding='utf-8') as f:
        readme_content = f.read()

    print("=== README.md REPLACEMENTS ===")
    
    # 1a. Known Limitations header
    old_known_lim = re.search(r'(##.*Known Limitations.*)\(v0\.6\.7.*\)', readme_content)
    if old_known_lim:
        print(f"BEFORE: {old_known_lim.group(0)}")
        readme_content = re.sub(r'(##.*Known Limitations.*)\(v0\.6\.7.*\)', r'\1(v0.6.8)', readme_content)
        new_known_lim = re.search(r'##.*Known Limitations.*\(v0\.6\.8\)', readme_content)
        print(f"AFTER:  {new_known_lim.group(0)}\n")

    # 1b. Version History Table
    old_row = re.search(r'\| \*\*`v0\.6\.7\.2`\*\*.*', readme_content)
    if old_row:
        print(f"BEFORE ROW: {old_row.group(0)}")
        
        # Demote v0.6.7.2 to Stable
        demoted_row = '| `v0.6.7.2` | 2026-04-27 | ✅ | 107 tests / 20 suites — 100% CI clean. Full repo seal. |'
        
        # Add new v0.6.8 row as Current
        new_row = '| **`v0.6.8`** | **2026-05-09** | ✅ **Current** | **Architecture & Stability Release — 4-sprint audit remediation.** |'
        
        replacement = f"{demoted_row}\n{new_row}"
        readme_content = readme_content.replace(old_row.group(0), replacement)
        
        print(f"AFTER ROWS:\n{demoted_row}\n{new_row}\n")

    # 1c. Any other lingering 0.6.7.x strings
    for m in re.finditer(r'v0\.6\.7\.x', readme_content):
        print(f"FOUND: {m.group(0)}")
    readme_content = re.sub(r'v0\.6\.7\.x', 'v0.6.8', readme_content)

    with open('README.md', 'w', encoding='utf-8') as f:
        f.write(readme_content)

    # 2. VALIDATION_STATUS.md
    print("=== VALIDATION_STATUS.md REPLACEMENTS ===")
    with open('docs/VALIDATION_STATUS.md', 'r', encoding='utf-8') as f:
        val_content = f.read()

    for m in re.finditer(r'v0\.6\.7(\.2)?', val_content):
        print(f"FOUND: {m.group(0)}")
        
    val_content = re.sub(r'v0\.6\.7(\.2)?', 'v0.6.8', val_content)
    
    with open('docs/VALIDATION_STATUS.md', 'w', encoding='utf-8') as f:
        f.write(val_content)

    # 3. SOURCE_OF_TRUTH.md
    print("=== SOURCE_OF_TRUTH.md REPLACEMENTS ===")
    with open('docs/SOURCE_OF_TRUTH.md', 'r', encoding='utf-8') as f:
        sot_content = f.read()

    for m in re.finditer(r'v0\.6\.7(\.[2x])?', sot_content):
        print(f"FOUND: {m.group(0)}")
        
    sot_content = re.sub(r'v0\.6\.7(\.[2x])?', 'v0.6.8', sot_content)

    with open('docs/SOURCE_OF_TRUTH.md', 'w', encoding='utf-8') as f:
        f.write(sot_content)

if __name__ == "__main__":
    sledgehammer_sync()
