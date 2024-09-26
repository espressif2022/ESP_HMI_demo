# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import io
import os
import argparse
import json
import shutil
import math
import sys
import time
import numpy as np

sys.dont_write_bytecode = True

from PIL import Image
from datetime import datetime
from qoi_conv import replace_extension, Qoi

def generate_header_filename(path):
    asset_name = os.path.basename(path)

    header_filename = f'mmap_generate_{asset_name}.h'
    return header_filename

def compute_checksum(data):
    checksum = sum(data) & 0xFFFF
    return checksum

def sort_key(filename):
    basename, extension = os.path.splitext(filename)
    return extension, basename

def split_image(im, block_size, input_dir, ext, convert_to_qoi):
    """Splits the image into blocks based on the block size."""
    width, height = im.size

    if block_size:
        splits = math.ceil(height / block_size)
    else:
        splits = 1

    for i in range(splits):
        if i < splits - 1:
            crop = im.crop((0, i * block_size, width, (i + 1) * block_size))
        else:
            crop = im.crop((0, i * block_size, width, height))

        output_path = os.path.join(input_dir, str(i) + ext)
        crop.save(output_path, quality=100)

        if convert_to_qoi:
            with Image.open(output_path) as img:
                out_path = replace_extension(output_path, 'qoi')
                new_image = Qoi().save(out_path, np.asarray(img))
                os.remove(output_path)


    return width, height, splits

def create_header(width, height, splits, split_height, lenbuf, ext):
    """Creates the header for the output file based on the format."""
    header = bytearray()

    if ext.lower() == '.jpg':
        header += bytearray('_SJPG__'.encode('UTF-8'))
    elif ext.lower() == '.png':
        header += bytearray('_SPNG__'.encode('UTF-8'))
    elif ext.lower() == '.qoi':
        header += bytearray('_SQOI__'.encode('UTF-8'))

    # 6 BYTES VERSION
    header += bytearray(('\x00V1.00\x00').encode('UTF-8'))

    # WIDTH 2 BYTES
    header += width.to_bytes(2, byteorder='little')

    # HEIGHT 2 BYTES
    header += height.to_bytes(2, byteorder='little')

    # NUMBER OF ITEMS 2 BYTES
    header += splits.to_bytes(2, byteorder='little')

    # SPLIT HEIGHT 2 BYTES
    header += split_height.to_bytes(2, byteorder='little')

    for item_len in lenbuf:
        # LENGTH 2 BYTES
        header += item_len.to_bytes(2, byteorder='little')

    return header

def save_image(output_file_path, header, split_data):
    """Saves the image with the constructed header and split data."""
    with open(output_file_path, 'wb') as f:
        if header is not None:
            f.write(header + split_data)
        else:
            f.write(split_data)

def process_image(input_file, height_str, output_extension, convert_to_qoi=False):
    """Main function to process the image and save it as .sjpg, .spng, or .sqoi."""
    try:
        SPLIT_HEIGHT = int(height_str)
        if SPLIT_HEIGHT < 0:
            raise ValueError('Height must be a positive integer')
    except ValueError as e:
        print('Error: Height must be a positive integer')
        sys.exit(1)

    input_dir, input_filename = os.path.split(input_file)
    base_filename, ext = os.path.splitext(input_filename)
    OUTPUT_FILE_NAME = base_filename

    try:
        im = Image.open(input_file)
    except Exception as e:
        print('Error:', e)
        sys.exit(0)

    width, height, splits = split_image(im, SPLIT_HEIGHT, input_dir, ext, convert_to_qoi)

    split_data = bytearray()
    lenbuf = []

    if convert_to_qoi:
        ext = '.qoi'

    for i in range(splits):
        with open(os.path.join(input_dir, str(i) + ext), 'rb') as f:
            a = f.read()
        split_data += a
        lenbuf.append(len(a))
        os.remove(os.path.join(input_dir, str(i) + ext))

    header = None
    if splits == 1 and convert_to_qoi:
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + ext)
    else:
        header = create_header(width, height, splits, SPLIT_HEIGHT, lenbuf, ext)
        output_file_path = os.path.join(input_dir, OUTPUT_FILE_NAME + output_extension)

    save_image(output_file_path, header, split_data)

    print('Completed, ', input_filename, '->', os.path.basename(output_file_path))

def convert_image_to_qoi(input_file, height_str):
    process_image(input_file, height_str, '.sqoi', convert_to_qoi=True)

def convert_image_to_simg(input_file, height_str):
    input_dir, input_filename = os.path.split(input_file)
    _, ext = os.path.splitext(input_filename)
    output_extension = '.sjpg' if ext.lower() == '.jpg' else '.spng'
    process_image(input_file, height_str, output_extension, convert_to_qoi=False)

def pack_models(model_path, assets_c_path, out_file, assets_path, max_name_len):
    merged_data = bytearray()
    file_info_list = []

    file_list = sorted(os.listdir(model_path), key=sort_key)
    for filename in file_list:
        file_path = os.path.join(model_path, filename)
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)

        try:
            img = Image.open(file_path)
            width, height = img.size
        except Exception as e:
            # print("Error:", e)
            _, file_extension = os.path.splitext(file_path)
            if file_extension.lower() in ['.sjpg', '.spng', '.sqoi']:
                offset = 14
                with open(file_path, 'rb') as f:
                    f.seek(offset)
                    width_bytes = f.read(2)
                    height_bytes = f.read(2)
                    width = int.from_bytes(width_bytes, byteorder='little')
                    height = int.from_bytes(height_bytes, byteorder='little')
            else:
                width, height = 0, 0

        file_info_list.append((file_name, len(merged_data), file_size, width, height))
        # Add 0x5A5A prefix to merged_data
        merged_data.extend(b'\x5A' * 2)

        with open(file_path, 'rb') as bin_file:
            bin_data = bin_file.read()

        merged_data.extend(bin_data)

    total_files = len(file_info_list)

    mmap_table = bytearray()
    for file_name, offset, file_size, width, height in file_info_list:
        if len(file_name) > int(max_name_len):
            print(f'\033[1;33mWarn:\033[0m "{file_name}" exceeds {max_name_len} bytes and will be truncated.')
        fixed_name = file_name.ljust(int(max_name_len), '\0')[:int(max_name_len)]
        mmap_table.extend(fixed_name.encode('utf-8'))
        mmap_table.extend(file_size.to_bytes(4, byteorder='little'))
        mmap_table.extend(offset.to_bytes(4, byteorder='little'))
        mmap_table.extend(width.to_bytes(2, byteorder='little'))
        mmap_table.extend(height.to_bytes(2, byteorder='little'))

    combined_data = mmap_table + merged_data
    combined_checksum = compute_checksum(combined_data)
    combined_data_length = len(combined_data).to_bytes(4, byteorder='little')
    header_data = total_files.to_bytes(4, byteorder='little') + combined_checksum.to_bytes(4, byteorder='little')
    final_data = header_data + combined_data_length + combined_data

    with open(out_file, 'wb') as output_bin:
        output_bin.write(final_data)

    os.makedirs(assets_c_path, exist_ok=True)
    current_year = datetime.now().year

    asset_name = os.path.basename(assets_path)
    file_path = os.path.join(assets_c_path, f'mmap_generate_{asset_name}.h')
    with open(file_path, 'w') as output_header:
        output_header.write('/*\n')
        output_header.write(' * SPDX-FileCopyrightText: 2022-{} Espressif Systems (Shanghai) CO LTD\n'.format(current_year))
        output_header.write(' *\n')
        output_header.write(' * SPDX-License-Identifier: Apache-2.0\n')
        output_header.write(' */\n\n')
        output_header.write('/**\n')
        output_header.write(' * @file\n')
        output_header.write(" * @brief This file was generated by esp_mmap_assets, don't modify it\n")
        output_header.write(' */\n\n')
        output_header.write('#pragma once\n\n')
        output_header.write("#include \"esp_mmap_assets.h\"\n\n")
        output_header.write(f'#define MMAP_{asset_name.upper()}_FILES           {total_files}\n')
        output_header.write(f'#define MMAP_{asset_name.upper()}_CHECKSUM        0x{combined_checksum:04X}\n\n')
        output_header.write(f'enum MMAP_{asset_name.upper()}_LISTS {{\n')

        for i, (file_name, _, _, _, _) in enumerate(file_info_list):
            enum_name = file_name.replace('.', '_')
            output_header.write(f'    MMAP_{asset_name.upper()}_{enum_name.upper()} = {i},        /*!< {file_name} */\n')

        output_header.write('};\n')

    print(f'All bin files have been merged into {out_file}')


def copy_assets_to_build(assets_path, target_path, spng_enable, sjpg_enable, qoi_enable, sqoi_enable, support_format, split_height):
    """
    Copy assets to target_path based on sdkconfig
    """
    format_string = support_format

    format_list = format_string.split(',')
    format_tuple = tuple(format_list)
    for filename in os.listdir(assets_path):
        if any(filename.endswith(suffix) for suffix in format_tuple):
            shutil.copyfile(os.path.join(assets_path, filename), os.path.join(target_path, filename))
            if filename.endswith('.jpg') and sjpg_enable:
                convert_image_to_simg(os.path.join(target_path, filename), split_height)
                os.remove(os.path.join(target_path, filename))
            elif filename.endswith('.png') and spng_enable:
                convert_image_to_simg(os.path.join(target_path, filename), split_height)
                os.remove(os.path.join(target_path, filename))
            elif filename.endswith('.png') and qoi_enable:
                convert_image_to_qoi(os.path.join(target_path, filename), split_height)
                os.remove(os.path.join(target_path, filename))
            elif filename.endswith('.jpg') and qoi_enable:
                convert_image_to_qoi(os.path.join(target_path, filename), split_height)
                os.remove(os.path.join(target_path, filename))
        else:
            print(f'No match found for file: {filename}, format_tuple: {format_tuple}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Move and Pack assets.")
    parser.add_argument('--config', required=True, help='Path to the configuration file')
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = json.load(f)

    project_dir = config['project_dir']
    main_path = config['main_path']
    assets_path = config['assets_path']
    assets_size = config['assets_size']
    image_file = config['image_file']

    project_dir = config['project_dir']
    name_length = config['name_length']
    split_height = config['split_height']

    support_format = config['support_format']
    support_spng = config['support_spng']
    support_sjpg = config['support_sjpg']
    support_qoi = config['support_qoi']
    support_sqoi = config['support_sqoi']

    print('--support_format:',  support_format)
    print('--support_spng:',  support_spng)
    print('--support_sjpg:',  support_sjpg)
    print('--support_qoi:',  support_qoi)
    if support_sqoi:
        print('--support_sqoi:',  support_sqoi)
    if support_spng or support_sjpg or support_sqoi:
        print('--split_height:', split_height)

    image_file = image_file
    target_path = os.path.dirname(image_file)

    if os.path.exists(target_path):
        shutil.rmtree(target_path)
    os.makedirs(target_path)

    copy_assets_to_build(assets_path, target_path, support_spng, support_sjpg, support_qoi, support_sqoi, support_format, split_height)
    pack_models(target_path, main_path, image_file, assets_path, name_length)

    total_size = os.path.getsize(os.path.join(target_path, image_file))
    recommended_size = int(math.ceil(total_size/1024))
    partition_size = int(math.ceil(int(assets_size, 16)/1024))

    if int(assets_size, 16) <= total_size:
        print('Given assets partition size: %dK' % (partition_size))
        print('Recommended assets partition size: %dK' % (recommended_size))
        print('\033[1;31mError:\033[0m assets partition size is smaller than recommended.')
        sys.exit(1)
