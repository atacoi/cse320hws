# PNG Homework

## Overview

A homework dedicated to understanding and manipulating PNG images.

## Features

The program is a command-line utility with the following features:

- Printing chunk summary
- Printing palette summary
- Printing IHDR fields
- Encoding a message and writing it to an output file
- Decoding and printing a hidden message
- Overlaying a smaller PNG over a larger PNG and writing that output to a file

## Running the Code

1. Compile the code by running: `make clean all`, or `make clean debug` to enable debug flags.
2. Run the program `bin/png` with desired flags (run `-h` for more info).

## Limitations

- Due to how the steganography is performed on color type 3 png images, any such image with more than 128 unique entries will fail to encode the given message.

- Overlay does not support alpha blending only direct pixel copying.

![](https://raw.githubusercontent.com/atacoi/cse320hws/main/PNG_HW/preview.png)
