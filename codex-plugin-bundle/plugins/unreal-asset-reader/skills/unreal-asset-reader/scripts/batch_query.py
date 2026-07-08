"""
Batch query template for Tier 2 Commandlet execution.
Run via: UnrealEditor-Cmd.exe ... -run=pythonscript -script=<this_file>

Accepts one optional argument: path to a JSON file containing a list of queries.
Each query: {"action": "list"|"info", "path": "/Game/..."}
Writes results to %TEMP%\\ue_batch_output.json (or env TEMP).

If no argument, runs a minimal example and writes to ue_batch_output.json in TEMP.
"""
import json
import os
import sys

try:
    import unreal
except ImportError:
    print(json.dumps({"error": "unreal module not available (not running in UE?)"}))
    sys.exit(1)

def run_list(path, recursive=True):
    assets = unreal.EditorAssetLibrary.list_assets(path, recursive=recursive)
    return [str(a) for a in assets]

def run_info(path):
    try:
        obj = unreal.EditorAssetLibrary.load_asset(path)
        if obj is None:
            return {"error": "load_asset returned None"}
        return {"class": type(obj).__name__, "path": path}
    except Exception as e:
        return {"error": str(e), "path": path}

def main():
    output_path = os.path.join(os.environ.get("TEMP", "."), "ue_batch_output.json")
    results = {}

    if len(sys.argv) > 1:
        query_file = sys.argv[1]
        if os.path.isfile(query_file):
            with open(query_file, "r", encoding="utf-8") as f:
                queries = json.load(f)
            for i, q in enumerate(queries):
                action = q.get("action", "list")
                path = q.get("path", "/Game/")
                key = path if path != "/Game/" else f"query_{i}"
                if action == "list":
                    results[key] = run_list(path, q.get("recursive", True))
                elif action == "info":
                    results[key] = run_info(path)
                else:
                    results[key] = {"error": f"unknown action: {action}"}
        else:
            results["error"] = f"query file not found: {query_file}"
    else:
        try:
            registry = unreal.AssetRegistryHelpers.get_asset_registry()
            ar_filter = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "DataTable")], package_paths=[unreal.DirectoryPath("/Game")], recursive_paths=True)
            assets = registry.get_assets(ar_filter)
            results["datatable_sample"] = [str(a.package_name) for a in list(assets)[:10]]
        except Exception as e:
            results["error"] = str(e)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    unreal.log(f"[Skill] Batch output written to {output_path}")
    print(output_path)

if __name__ == "__main__":
    main()
