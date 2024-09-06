import sys
from PIL import Image
import qoi
import numpy as np
from qoi import QOIColorSpace

def png_to_qoi(png_path, qoi_path):

    # help(qoi.encode)
    with Image.open(png_path) as img:
        img = img.convert("RGBA")
        rgb_data = np.array(img)

        qoi_data = qoi.encode(rgb_data, colorspace=QOIColorSpace.SRGB)
        
        with open(qoi_path, "wb") as f:
            f.write(qoi_data)
        print(f"Successfully converted {png_path} to {qoi_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python png_to_qoi.py <input.png> <output.qoi>")
        sys.exit(1)

    png_to_qoi(sys.argv[1], sys.argv[2])
