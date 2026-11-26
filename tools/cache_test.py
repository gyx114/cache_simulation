#!/usr/bin/env python3
"""Build and run the simulator with a selected cache configuration."""

import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CACHE_HEADER = ROOT / "src" / "cache.h"
RESULTS_DIR = ROOT / "results"
PARAMETER_MAP = RESULTS_DIR / "cache_parameter_map.csv"
PARAMETERS = {
    "i_group_bits": "I_CACHE_GROUP_BITS",
    "i_ways": "I_CACHE_WAYS",
    "i_offset_bits": "I_CACHE_OFFSET_BITS",
    "d_group_bits": "D_CACHE_GROUP_BITS",
    "d_ways": "D_CACHE_WAYS",
    "d_offset_bits": "D_CACHE_OFFSET_BITS",
}
DEFAULT_PARAMETERS = {
    "i_group_bits": 6,
    "i_ways": 4,
    "i_offset_bits": 5,
    "d_group_bits": 8,
    "d_ways": 8,
    "d_offset_bits": 5,
}
PARAMETER_PROMPTS = {
    "i_group_bits": "I-cache 组索引位数",
    "i_ways": "I-cache 路数",
    "i_offset_bits": "I-cache 块内偏移位数",
    "d_group_bits": "D-cache 组索引位数",
    "d_ways": "D-cache 路数",
    "d_offset_bits": "D-cache 块内偏移位数",
}
MAP_COLUMNS = [
    "test_name",
    "result_file",
    "inputs",
    *PARAMETERS,
]


def read_cache_header():
    with CACHE_HEADER.open(encoding="utf-8", newline="") as header_file:
        return header_file.read()


def update_header(header, values):
    for option, macro in PARAMETERS.items():
        header, replacements = re.subn(
            rf"(?m)^(#define\s+{macro}\s+)\d+(\s*)$",
            rf"\g<1>{values[option]}\g<2>",
            header,
        )
        if replacements != 1:
            raise ValueError(f"Expected exactly one definition for {macro} in {CACHE_HEADER}")
    return header


def update_parameter_map(record):
    rows = []
    if PARAMETER_MAP.exists():
        with PARAMETER_MAP.open(newline="", encoding="utf-8") as mapping:
            rows = [
                row
                for row in csv.DictReader(mapping)
                if row.get("test_name") != record["test_name"]
            ]
    rows.append(record)
    with PARAMETER_MAP.open("w", newline="", encoding="utf-8") as mapping:
        writer = csv.DictWriter(mapping, fieldnames=MAP_COLUMNS)
        writer.writeheader()
        writer.writerows(rows)


def validate_parameter(option, value):
    if option.endswith("group_bits") and not 0 <= value <= 30:
        raise ValueError(f"{option} must be between 0 and 30")
    if option.endswith("offset_bits") and not 2 <= value <= 30:
        raise ValueError(f"{option} must be between 2 and 30")
    if option.endswith("ways") and not 1 <= value <= 0xFFFFFFFF:
        raise ValueError(f"{option} must be between 1 and 4294967295")


def interactive_args():
    test_name = input("测试名称 [test1]：").strip() or "test1"
    values = {}
    for option, prompt in PARAMETER_PROMPTS.items():
        while True:
            response = input(f"{prompt} [{DEFAULT_PARAMETERS[option]}]：").strip()
            if not response:
                values[option] = DEFAULT_PARAMETERS[option]
                break
            try:
                value = int(response)
                validate_parameter(option, value)
            except ValueError as error:
                print(f"输入无效：{error}")
                continue
            values[option] = value
            break
    inputs = input("测试文件（空格分隔；直接回车运行全部测试）：").split()
    return argparse.Namespace(test_name=test_name, inputs=inputs), values


def parse_args():
    if len(sys.argv) == 1:
        return interactive_args()

    parser = argparse.ArgumentParser(
        description="Set cache parameters, rebuild the simulator, and save its test output."
    )
    parser.add_argument("test_name", help="result name, for example test1")
    parser.add_argument("inputs", nargs="*", help="optional .x input files; defaults to ./run's full suite")
    for option in PARAMETERS:
        parser.add_argument(
            f"--{option.replace('_', '-')}",
            type=int,
            default=None,
            help=f"override {option} (default: original value {DEFAULT_PARAMETERS[option]})",
        )
    args = parser.parse_args()
    values = {
        option: getattr(args, option)
        if getattr(args, option) is not None
        else DEFAULT_PARAMETERS[option]
        for option in PARAMETERS
    }
    return args, values


def main():
    try:
        args, values = parse_args()
        if re.fullmatch(r"[A-Za-z0-9_-]+", args.test_name) is None:
            raise ValueError("test_name may contain only letters, numbers, underscores, and hyphens")
        for option, value in values.items():
            validate_parameter(option, value)

        inputs = args.inputs
        for input_file in inputs:
            if not (ROOT / input_file).is_file():
                raise ValueError(f"Input file not found: {input_file}")

        with CACHE_HEADER.open(encoding="utf-8", newline="") as header_file:
            header = header_file.read()
        with CACHE_HEADER.open("w", encoding="utf-8", newline="") as header_file:
            header_file.write(update_header(header, values))
        subprocess.run(["make", "clean"], cwd=ROOT, check=True)
        subprocess.run(["make"], cwd=ROOT, check=True)

        RESULTS_DIR.mkdir(exist_ok=True)
        result_path = RESULTS_DIR / f"{args.test_name}.txt"
        with result_path.open("w", encoding="utf-8") as output:
            subprocess.run(["./run", *inputs], cwd=ROOT, stdout=output, check=True)

        record = {
            "test_name": args.test_name,
            "result_file": str(result_path.relative_to(ROOT)),
            "inputs": ";".join(inputs) if inputs else "inputs/*/*.x",
            **{option: str(value) for option, value in values.items()},
        }
        update_parameter_map(record)
        print(f"Test output: {result_path.relative_to(ROOT)}")
        print(f"Parameter map: {PARAMETER_MAP.relative_to(ROOT)}")
    except (EOFError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"cache_test: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
