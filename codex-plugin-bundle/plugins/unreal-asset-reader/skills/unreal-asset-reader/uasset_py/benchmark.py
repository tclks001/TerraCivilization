#!/usr/bin/env python3
"""
uasset_py Benchmark: Python+orjson vs Python+json（可选对比历史 C# exe）

Usage:
    cd <unreal-asset-reader dir>
    python uasset_py/benchmark.py

说明：
- 自动扫描 Content 目录，找 DT_Small / DT_Medium / BP_Small 代表性资产
- 每个命令重复 REPEATS 次取最小值，排除进程启动抖动
- 输出 Markdown 格式对比表
- Python+json：通过 UASSET_DISABLE_ORJSON=1 环境变量强制禁用 orjson
- Python+orjson：默认路径（orjson 已安装时生效）
- C# exe（可选）：若 bin_cli/UAssetCLI.exe 存在则自动纳入对比
"""
from __future__ import annotations
import subprocess
import time
import os
import sys
from pathlib import Path

# ─── 配置 ───────────────────────────────────────────────────────────────────

PYTHON_PKG  = "uasset_py"
CS_CLI      = Path(__file__).parent.parent / "bin_cli" / "UAssetCLI.exe"
CONTENT_DIR = Path(r"H:\work\maple_jeffe-PC3_5325\Client\Projects\Content")
ENGINE_VER  = "VER_UE5_3"
REPEATS     = 3   # 每个命令重复次数，取最小值

# Python subprocess 内部不经过 bash 路径转换，必须使用 Windows 绝对路径
TEMP_DIR = os.environ.get("TEMP", r"C:\Users\jeffefang\AppData\Local\Temp").replace("/", "\\")
OUTPUT_FILE = os.path.join(TEMP_DIR, "bench_output.json")

# 每种资产类型支持的命令
ASSET_COMMANDS: dict[str, list[str]] = {
    "DT_Small":  ["toc", "refs", "tojson", "dt-rows", "dt-schema"],
    "DT_Medium": ["toc", "refs", "tojson", "dt-rows", "dt-schema"],
    "BP_Small":  ["toc", "refs", "tojson", "unlua"],
}

# ─── 资产发现 ────────────────────────────────────────────────────────────────

def find_assets() -> dict[str, Path]:
    """扫描 Content 目录，找代表性资产（DT_Small / DT_Medium / BP_Small）"""
    assets: dict[str, Path] = {}

    if not CONTENT_DIR.exists():
        print(f"[WARN] Content 目录不存在: {CONTENT_DIR}", file=sys.stderr)
        return assets

    dt_small = dt_medium = bp_small = None
    for f in CONTENT_DIR.rglob("*.uasset"):
        name = f.name
        try:
            size = f.stat().st_size
        except OSError:
            continue

        if name.startswith("DT_"):
            if dt_small is None and size < 50_000:
                dt_small = f
            elif dt_medium is None and 50_000 <= size < 500_000:
                dt_medium = f
        elif name.startswith("BP_"):
            if bp_small is None and size < 200_000:
                bp_small = f

        if dt_small and dt_medium and bp_small:
            break

    if dt_small:   assets["DT_Small"]  = dt_small
    if dt_medium:  assets["DT_Medium"] = dt_medium
    if bp_small:   assets["BP_Small"]  = bp_small

    return assets


# ─── 命令执行 ────────────────────────────────────────────────────────────────

def run_timed(cmd: list[str], timeout: int = 60,
              extra_env: dict[str, str] | None = None) -> float | None:
    """
    执行命令 REPEATS 次，返回最小耗时（毫秒）。
    若所有次均失败，返回 None。

    extra_env：向子进程追加/覆盖的环境变量（用于 UASSET_DISABLE_ORJSON=1）。
    """
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)

    times: list[float] = []
    printed_error = False
    for _ in range(REPEATS):
        t0 = time.perf_counter()
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                timeout=timeout,
                env=env,
            )
        except (subprocess.TimeoutExpired, FileNotFoundError):
            continue
        t1 = time.perf_counter()
        if result.returncode == 0:
            times.append((t1 - t0) * 1000.0)
        else:
            if not printed_error:
                err = result.stderr.decode("utf-8", errors="replace").strip()
                if err:
                    print(f"  [stderr] {err[:120]}", file=sys.stderr)
                printed_error = True
    return min(times) if times else None


def benchmark_asset(
    asset_path: Path, command: str
) -> tuple[float | None, float | None, float | None]:
    """
    对一个资产运行一个命令，返回 (orjson_ms, json_ms, cs_ms)。

    - orjson_ms：默认 Python 路径（orjson 生效）
    - json_ms：  设置 UASSET_DISABLE_ORJSON=1 强制用标准库 json
    - cs_ms：    C# exe（若存在）
    """
    src = str(asset_path)
    dst = OUTPUT_FILE

    py_base_cmd = [sys.executable, "-m", PYTHON_PKG, command, src, dst, ENGINE_VER]

    # 1. Python + orjson（默认）
    orjson_ms = run_timed(py_base_cmd)

    # 2. Python + json（禁用 orjson）
    json_ms = run_timed(py_base_cmd, extra_env={"UASSET_DISABLE_ORJSON": "1"})

    # 3. C# exe
    cs_ms: float | None = None
    if CS_CLI.exists():
        cs_cmd = [str(CS_CLI), command, src, dst, ENGINE_VER]
        cs_ms = run_timed(cs_cmd)

    return orjson_ms, json_ms, cs_ms


# ─── 标签辅助 ────────────────────────────────────────────────────────────────

def speedup_label(fast: float, slow: float) -> str:
    """返回加速比标签，fast 是更快的数值。"""
    ratio = slow / fast
    return f"{ratio:.2f}x"


def vs_cs_label(py_ms: float, cs_ms: float) -> str:
    ratio = py_ms / cs_ms
    tag = "✅" if ratio <= 3 else ("⚠️" if ratio <= 5 else "🐢")
    return f"{tag} {ratio:.1f}x"


# ─── 主程序 ──────────────────────────────────────────────────────────────────

def main() -> None:
    # 强制 stdout/stderr 使用 UTF-8
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")

    # 切换到 unreal-asset-reader 目录，保证 `-m uasset_py` 能找到包
    root = Path(__file__).parent.parent
    os.chdir(root)

    py_ver = subprocess.run(
        [sys.executable, "--version"], capture_output=True, text=True
    ).stdout.strip() or subprocess.run(
        [sys.executable, "--version"], capture_output=True, text=True
    ).stderr.strip()

    # 检测 orjson 是否可用
    orjson_available = False
    orjson_ver = "未安装"
    try:
        import orjson
        orjson_available = True
        orjson_ver = orjson.__version__
    except ImportError:
        pass

    print("# uasset_py Benchmark: Python+orjson vs Python+json（可选 vs C#）\n")
    print(f"- **Python**: `{py_ver}`")
    print(f"- **orjson**: `{orjson_ver}` ({'✅ 已启用' if orjson_available else '❌ 未安装，两路 Python 数据将相同'})")
    print(f"- **C# CLI**: `{'存在' if CS_CLI.exists() else '未找到（跳过 C# 对比）'}`")
    print(f"- **Repeats**: {REPEATS}（取最小值，排除进程冷启动抖动）")
    print(f"- **Content**: `{CONTENT_DIR}`\n")

    assets = find_assets()
    if not assets:
        print("❌ 未找到任何测试资产，请检查 CONTENT_DIR 配置。")
        return

    print("## 发现的测试资产\n")
    for name, path in assets.items():
        size_kb = path.stat().st_size // 1024
        print(f"- `{name}`: `{path.name}` ({size_kb} KB)")
    print()

    # ── 表头
    print("## 性能对比\n")
    header = "| 资产 | 文件 | 大小 | 命令 | C# (ms) | Py+json (ms) | Py+orjson (ms) | json→orjson加速 | vs C# (orjson) |"
    sep    = "|------|------|------|------|---------|--------------|----------------|-----------------|----------------|"
    print(header)
    print(sep)

    results: list[dict] = []

    for asset_name, asset_path in assets.items():
        commands = ASSET_COMMANDS.get(asset_name, [])
        size_kb  = asset_path.stat().st_size // 1024
        fname    = asset_path.name[:32]

        for cmd in commands:
            print(f"  [RUN] {asset_name} / {cmd} ...", end="", flush=True)
            orjson_ms, json_ms, cs_ms = benchmark_asset(asset_path, cmd)
            print(
                f" orjson={f'{orjson_ms:.1f}' if orjson_ms else 'FAIL'} ms"
                f"  json={f'{json_ms:.1f}' if json_ms else 'FAIL'} ms",
                flush=True,
            )

            orjson_str = f"{orjson_ms:.1f}" if orjson_ms else "**FAIL**"
            json_str   = f"{json_ms:.1f}"   if json_ms   else "**FAIL**"
            cs_str     = f"{cs_ms:.1f}"     if cs_ms     else ("N/A" if CS_CLI.exists() else "—")

            # json → orjson 加速比
            if orjson_ms and json_ms:
                accel_str = speedup_label(orjson_ms, json_ms)
            elif not orjson_available:
                accel_str = "N/A (无orjson)"
            else:
                accel_str = "N/A"

            # vs C# (用 orjson 路径对比)
            if orjson_ms and cs_ms:
                vs_cs_str = vs_cs_label(orjson_ms, cs_ms)
            elif orjson_ms:
                vs_cs_str = "N/A (无C#)"
            else:
                vs_cs_str = "FAIL"

            row = {
                "asset_name": asset_name,
                "file":       fname,
                "size_kb":    size_kb,
                "cmd":        cmd,
                "orjson_ms":  orjson_ms,
                "json_ms":    json_ms,
                "cs_ms":      cs_ms,
            }
            results.append(row)

            print(f"| {asset_name} | `{fname}` | {size_kb} KB | `{cmd}` | {cs_str} | {json_str} | {orjson_str} | {accel_str} | {vs_cs_str} |")

    # ── 汇总统计
    print()
    print("## 汇总\n")

    # orjson vs json 加速比（仅 tojson，JSON 体积大时差异最显著）
    tojson_pairs = [
        (r["orjson_ms"], r["json_ms"])
        for r in results
        if r["cmd"] == "tojson" and r["orjson_ms"] and r["json_ms"]
    ]
    all_pairs = [
        (r["orjson_ms"], r["json_ms"])
        for r in results
        if r["orjson_ms"] and r["json_ms"]
    ]

    if all_pairs:
        all_speedups = [j / o for o, j in all_pairs]
        avg_speedup = sum(all_speedups) / len(all_speedups)
        max_speedup = max(all_speedups)
        print(f"### orjson vs json\n")
        print(f"- 有效对比组数（全命令）：{len(all_pairs)}")
        print(f"- 平均加速比：**{avg_speedup:.2f}x**")
        print(f"- 最大加速比：**{max_speedup:.2f}x**")
        if tojson_pairs:
            tojson_speedups = [j / o for o, j in tojson_pairs]
            print(f"- tojson 命令平均加速：**{sum(tojson_speedups)/len(tojson_speedups):.2f}x**")
        print()

    # orjson vs C#
    valid_cs = [
        (r["orjson_ms"], r["cs_ms"])
        for r in results
        if r["orjson_ms"] and r["cs_ms"]
    ]
    fail_count = sum(1 for r in results if not r["orjson_ms"])

    print(f"### Python+orjson vs C#\n")
    if valid_cs:
        ratios = [p / c for p, c in valid_cs]
        avg_r  = sum(ratios) / len(ratios)
        max_r  = max(ratios)
        print(f"- 有效对比组数：{len(valid_cs)}")
        print(f"- 平均 Ratio（Python+orjson / C#）：**{avg_r:.2f}x**")
        print(f"- 最大 Ratio：**{max_r:.2f}x**")
    if fail_count:
        print(f"- ⚠️ FAIL 项：{fail_count}（见表格）")
    if not CS_CLI.exists():
        print("- ℹ️  未找到 C# exe，仅记录 Python 绝对耗时")
    print()

    print("## 已内建优化\n")
    print("| 优化 | 位置 | 效果 |")
    print("|------|------|------|")
    print("| `orjson` 序列化 | `cli.py` `_dumps_json()` | JSON 输出加速，大文件效果显著 |")
    print("| `struct.Struct` 预编译 | `core/reader.py` L9-19 | 避免每次调用重新解析格式字符串 |")
    print("| `memoryview` 零拷贝 | `core/reader.py` BinaryReader | 大文件读取无额外内存拷贝 |")
    print("| `__slots__` 全覆盖 | 所有 Property/Export/FName 类 | 降低每实例内存开销，减少 GC 压力 |")
    print("| Lazy export parsing | `core/uasset.py` + `cli.py` | `toc`/`refs`/`dt-rows`/`dt-schema`/`unlua` 跳过 export body |")


if __name__ == "__main__":
    main()
