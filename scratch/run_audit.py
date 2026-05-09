import subprocess
import os
from pathlib import Path

def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return f'\n$ {cmd}\n' + (res.stdout + res.stderr).strip()
    except Exception as e:
        return f'\n$ {cmd}\nERROR: {str(e)}'

out = []
out.append('# IMMEDIATE-10: Final Validation Scan\n')

# 1. Stale link check
# Using git grep since it's Windows-compatible and clean
out.append('## 1. Stale Link Check')
out.append(run_cmd(r'git grep -E "github\.com/LiranOG/Granite([^-\w]|$)" README.md docs .github benchmarks scripts viz CHANGELOG.md CITATION.cff pyproject.toml CMakeLists.txt'))

# 2. Tooling checks
out.append('\n## 2. Tooling Checks')
res_links = run_cmd('python scripts/audit_links.py')
if "can't open file" in res_links or "No such file" in res_links:
    out.append('\n$ python scripts/audit_links.py\nNot yet created')
else:
    out.append(res_links)

res_schema = run_cmd('python scripts/audit_params_schema.py')
if "can't open file" in res_schema or "No such file" in res_schema:
    out.append('\n$ python scripts/audit_params_schema.py\nNot yet created')
else:
    out.append(res_schema)

# 3. Build check
out.append('\n## 3. Build Check (Expect HDF5 Failure)')
out.append(run_cmd('python scripts/run_granite.py build --build-type Release'))

# 4. Test check
out.append('\n## 4. Python Test Suite (Expect 8-test baseline)')
# Use set PYTHONPATH on Windows
out.append(run_cmd('set PYTHONPATH=python&& pytest -q tests/python'))

# 5. Git State
out.append('\n## 5. Git State')
out.append(run_cmd('git log --oneline main..HEAD'))
out.append(run_cmd('git diff main..HEAD --stat'))

# 6. Wiki Diff Sizes
out.append('\n## 6. Wiki Diff Sizes')
try:
    with open('audit/wiki-home-changes.diff', 'r', encoding='utf-16') as f:
        l1 = len(f.readlines())
    out.append(f'\n$ wc -l audit/wiki-home-changes.diff\n{l1} audit/wiki-home-changes.diff')
except FileNotFoundError:
    out.append('\n$ wc -l audit/wiki-home-changes.diff\nFile not found.')
except UnicodeDecodeError:
    with open('audit/wiki-home-changes.diff', 'r', encoding='utf-8') as f:
        l1 = len(f.readlines())
    out.append(f'\n$ wc -l audit/wiki-home-changes.diff\n{l1} audit/wiki-home-changes.diff')

try:
    with open('audit/wiki-param-ref-changes.diff', 'r', encoding='utf-16') as f:
        l2 = len(f.readlines())
    out.append(f'\n$ wc -l audit/wiki-param-ref-changes.diff\n{l2} audit/wiki-param-ref-changes.diff')
except FileNotFoundError:
    out.append('\n$ wc -l audit/wiki-param-ref-changes.diff\nFile not found.')
except UnicodeDecodeError:
    with open('audit/wiki-param-ref-changes.diff', 'r', encoding='utf-8') as f:
        l2 = len(f.readlines())
    out.append(f'\n$ wc -l audit/wiki-param-ref-changes.diff\n{l2} audit/wiki-param-ref-changes.diff')

Path('audit/IMMEDIATE-complete.md').write_text('\n'.join(out), encoding='utf-8')
