import numpy as np
import qoi

# Get your image as a numpy array (OpenCV, Pillow, etc. but here we just create a bunch of noise). Note: HWC ordering
rgb = np.random.randint(low=0, high=255, size=(224, 244, 3)).astype(np.uint8)

# Write it:
_ = qoi.write("/tmp/img.qoi", rgb)

# Read it and check it matches (it should, as we're lossless)
rgb_read = qoi.read("/tmp/img.qoi")
assert np.array_equal(rgb, rgb_read)

# Likewise for encode/decode to/from bytes:
bites = qoi.encode(rgb)
rgb_decoded = qoi.decode(bites)
assert np.array_equal(rgb, rgb_decoded)

# Benchmarking
from qoi.benchmark import benchmark
benchmark()  # Check out the arguments if you're interested