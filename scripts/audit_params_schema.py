import sys
from pathlib import Path
import yaml

ALLOWED_KEYS = {'grid', 'time', 'ccz4', 'io', 'initial_data', 'black_holes', 'amr'}
CONCEPT_KEYS = {'scenario', 'stars', 'physics', 'expected_results', 'compute_estimate', 'evolution', 'output'}

def audit_schema():
    benchmarks_dir = Path('benchmarks')
    if not benchmarks_dir.exists():
        print("No benchmarks directory found.")
        return 0

    failed = False
    for yaml_file in benchmarks_dir.rglob('*.yaml'):
        if yaml_file.name == 'validation_tests.yaml':
            continue
        try:
            with open(yaml_file, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
            
            if not isinstance(data, dict):
                continue
                
            for key in data.keys():
                if key in ALLOWED_KEYS:
                    continue
                elif key in CONCEPT_KEYS:
                    print(f"[WARN] {yaml_file}: Concept-only key '{key}' found.")
                else:
                    print(f"[FAIL] {yaml_file}: Unknown key '{key}' found.")
                    failed = True
        except Exception as e:
            print(f"[FAIL] {yaml_file}: Error parsing YAML - {e}")
            failed = True
            
    if failed:
        print("Schema audit failed.")
        sys.exit(1)
    else:
        print("Schema audit passed.")
        return 0

if __name__ == '__main__':
    sys.exit(audit_schema())
