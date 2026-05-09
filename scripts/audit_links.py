import sys
import re
from pathlib import Path
import urllib.parse

def audit_links():
    root = Path('.')
    md_files = list(root.rglob('*.md'))
    
    # Exclude internal/temp directories
    excludes = ['wiki_staging', '.gemini', 'scratch', 'venv', '.git', 'audit', '.github']
    md_files = [f for f in md_files if not any(part in f.parts for part in excludes)]
    md_files = [f for f in md_files if f.name != 'GRANITE_ANTIGRAVITY_PROMPTS.md']

    failed = False
    
    # Regex for standard markdown links [text](target) avoiding spaces
    link_pattern = re.compile(r'\[[^\]]+\]\(([^)\s]+)\)')
    
    for md_file in md_files:
        try:
            content = md_file.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            continue
            
        # 1. Check for stale Granite links
        stale_matches = re.finditer(r'github\.com/LiranOG/Granite(?![-\w])', content)
        for m in stale_matches:
            print(f"[FAIL] {md_file}: Stale link found '{m.group(0)}'")
            failed = True
            
        # 2. Check local relative markdown links
        links = link_pattern.findall(content)
        for link in links:
            if link.startswith(('http://', 'https://', 'mailto:', '#')):
                continue
                
            # Strip anchors for local file check: file.md#section -> file.md
            target_path_str = urllib.parse.unquote(link.split('#')[0])
            if not target_path_str:
                continue # was just an anchor #something
                
            # Resolve relative to the current markdown file's directory
            target_path = (md_file.parent / target_path_str).resolve()
            
            # Check if it exists
            if not target_path.exists():
                print(f"[FAIL] {md_file}: Dead local link '{link}' (resolved to {target_path})")
                failed = True

    if failed:
        print("Link audit failed.")
        sys.exit(1)
    else:
        print("Link audit passed.")
        return 0

if __name__ == '__main__':
    sys.exit(audit_links())
