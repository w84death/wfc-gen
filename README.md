# Wave Function Collapse with RayLib

A simple and clean implementation of the Wave Function Collapse (WFC) algorithm in C using RayLib for visualization. This program generates procedural patterns from a sample input image.

## Features

- Live visualization of the WFC generation process
- Side-by-side display of input image and generated output
- Interactive controls for step-by-step or automatic generation
- Overlapping pattern extraction from input images
- Weighted pattern selection based on frequency in the input

## Requirements

- C compiler (gcc/clang)
- RayLib library
- Make (optional, for using the Makefile)

## Installation

### Install RayLib

On Ubuntu/Debian:
```bash
sudo apt-get install libraylib-dev
```

On macOS with Homebrew:
```bash
brew install raylib
```

On Arch Linux:
```bash
sudo pacman -S raylib
```

## Building

Using Make:
```bash
make
```

Or compile directly:
```bash
gcc -Wall -O2 wfc.c -o wfc -lraylib -lm -lpthread -ldl -lrt -lX11 -lGL
```

## Usage

Run with default input image:
```bash
./wfc
```

Run with custom input image:
```bash
./wfc your_image.png
```

## Controls

- **SPACE** - Toggle automatic generation (runs at maximum speed)
- **S** - Single step generation
- **R** - Reset and start over
- **ESC** - Exit program

## How It Works

1. **Pattern Extraction**: The algorithm extracts all unique NxN (default 3x3) patterns from the input image
2. **Frequency Analysis**: Counts how often each pattern appears in the input
3. **Adjacency Rules**: Determines which patterns can be placed next to each other based on overlapping pixels
4. **Wave Function Collapse**: 
   - Starts with all cells in superposition (all patterns possible)
   - Finds the cell with lowest entropy (fewest possibilities)
   - Collapses it to a single pattern (weighted by frequency)
   - Propagates constraints to neighboring cells
   - Repeats until all cells are collapsed

## Parameters

You can modify these constants in `wfc.c`:

- `PATTERN_SIZE` - Size of patterns to extract (default: 3x3)
- `OUTPUT_WIDTH` - Width of output grid (default: 50)
- `OUTPUT_HEIGHT` - Height of output grid (default: 50)
- `SCALE` - Display scale factor (default: 8)
- `MAX_PATTERNS` - Maximum number of unique patterns (default: 256)

## Tips for Good Input Images

- Use small, tileable textures with clear patterns
- Images with limited color palettes work best
- Ensure patterns have clear structure and repetition
- Avoid very noisy or random images
- Small input images (16x16 to 64x64) are often sufficient

## Example Images

The project includes sample images:
- `simple_simple_brick.png` - Simple brick pattern
- `test_circuit.png` - Circuit board pattern
- `godot.png` - Another sample pattern

## Troubleshooting

If generation fails (red cells appear):
- The input might be too complex
- Try reducing pattern size
- Use a simpler input image
- Increase MAX_PATTERNS if needed

## License

This is a simple educational implementation. Feel free to use and modify as needed.