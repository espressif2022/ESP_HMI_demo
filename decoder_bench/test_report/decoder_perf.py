# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import re
from typing import List, Tuple, Dict

def find_log_files(base_path: str, filename: str = 'dut.log') -> List[str]:
    log_files = []
    for root, _, files in os.walk(base_path):
        if filename in files:
            log_files.append(os.path.join(root, filename))
    return log_files

def parse_log_file(log_file: str) -> List[tuple]:
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as file:
        log = file.read()

    target_pattern = re.compile(r'Perf ctr target: (\w+)')
    target_match = target_pattern.search(log)
    target = target_match.group(1) if target_match else 'unknown'

    data_pattern = re.compile(r'Perf ctr\[0\], \[\s*(.*?)\s*\]\[\s*(.*?)\s*\]: (\d+\.\d+) ms')
    data = data_pattern.findall(log)

    parsed_data = [(config, type_, time_, target) for config, type_, time_ in data]

    return parsed_data

def process_log_files(base_path: str, filename: str = 'dut.log') -> Dict[str, List[Tuple[str, str, str]]]:
    log_files = find_log_files(base_path, filename)

    if not log_files:
        print('No log files found')
        return {}

    all_data = {}
    for log_file in log_files:
        data = parse_log_file(log_file)
        for config, type_, time_, target in data:
            if target not in all_data:
                all_data[target] = []
            all_data[target].append((config, type_, time_))

    return all_data

def print_data(data: Dict[str, List[Tuple[str, str, str]]]):
    targets = sorted(data.keys())
    header = f"{'Configuration':<15} {'Type':<15} {'':<10} " + ' '.join([f'{target:<10}' for target in targets])
    print(header)
    print('-' * len(header))

    combined_data = {}
    for target in targets:
        for config, type_, time_ in data[target]:
            if (config, type_) not in combined_data:
                combined_data[(config, type_)] = {}
            combined_data[(config, type_)][target] = time_

    for (config, type_), times in combined_data.items():
        row = f"{config:<15} {type_:<15} {'':<10} " + ' '.join([f"{times.get(target, ''):<10}" for target in targets])
        print(row)

def save_data_as_html(data: Dict[str, List[Tuple[str, str, str]]], output_file: str):
    targets = sorted(data.keys())
    header = f'<th>Configuration</th><th>Type</th>' + ''.join([f'<th>{target}</th>' for target in targets])

    combined_data = {}
    for target in targets:
        for config, type_, time_ in data[target]:
            if (config, type_) not in combined_data:
                combined_data[(config, type_)] = {}
            combined_data[(config, type_)][target] = time_

    # Generate the main table
    main_rows = ''
    for (config, type_), times in combined_data.items():
        row = f'<tr><td>{config}</td><td>{type_}</td>' + ''.join([f"<td>{times.get(target, '')}</td>" for target in targets]) + '</tr>'
        main_rows += row

    # Generate tables for each type
    type_tables = ''
    types = set(type_ for _, type_ in combined_data.keys())
    for type_ in sorted(types):
        type_header = f'<th>Configuration</th>' + ''.join([f'<th>{target}</th>' for target in targets])
        type_rows = ''
        for (config, type_key), times in combined_data.items():
            if type_key == type_:
                row = f'<tr><td>{config}</td>' + ''.join([f"<td>{times.get(target, '')}</td>" for target in targets]) + '</tr>'
                type_rows += row
        type_tables += f"""
        <h2>Type: {type_}</h2>
        <table border="1">
            <thead>
                <tr>{type_header}</tr>
            </thead>
            <tbody>
                {type_rows}
            </tbody>
        </table>
        """

    html_content = f"""
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>perf_benchmark Result</title>
    </head>
    <body>
        <h1>Performance Benchmark Result(unit:ms)</h1>
        <table border="1">
            <thead>
                <tr>{header}</tr>
            </thead>
            <tbody>
                {main_rows}
            </tbody>
        </table>
        {type_tables}
    </body>
    </html>
    """

    with open(output_file, 'w', encoding='utf-8') as file:
        file.write(html_content)

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 3:
        print('Usage: python decoder_perf.py <base_path> <output_file>')
        sys.exit(1)
    base_path = sys.argv[1]
    output_file = sys.argv[2]
    data = process_log_files(base_path)
    print_data(data)
    save_data_as_html(data, output_file)
