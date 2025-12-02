# LSB Steganography - Image Message Hiding

A C-based steganography tool that hides encrypted messages inside PNG images using Least Significant Bit (LSB) embedding with intelligent low-contrast region selection.

## Features

- **LSB Steganography**: Hides messages in the least significant bits of RGB channels
- **Low-Contrast Region Detection**: Automatically finds optimal embedding locations using statistical analysis
- **Multi-threaded**: Parallel encryption and image analysis for faster processing
- **PNG Support**: Works with PNG, JPG, JPEG, and BMP images
- **Interactive Menu**: User-friendly command-line interface with numbered image selection
- **Simple XOR Encryption**: Basic encryption (for demonstration purposes, not cryptographically secure)

## How It Works

1. **Encryption**: Message is XOR-encrypted with a user-provided key
2. **Image Analysis**: Image is analyzed to find low-contrast regions (8×8 pixel blocks) where LSB changes are less noticeable
3. **Embedding**: Encrypted bits are embedded into the LSBs of pixels in selected low-contrast regions
4. **Extraction**: Same mask pattern is recomputed to extract and decrypt the hidden message

## File Structure

- `main.c` - Interactive menu system and orchestration
- `encryption.c/.h` - XOR-based encryption/decryption
- `image_analysis.c/.h` - Low-contrast region detection using histogram-based median calculation
- `embedding.c/.h` - LSB embedding and extraction with mask support
- `stb_image.h` / `stb_image_write.h` - Single-header image loading/saving libraries
- `CMakeLists.txt` - CMake build configuration

## Building

### Prerequisites

- CMake (3.10 or higher)
- C compiler (GCC or Clang)
- libpng development libraries
- pthread support

**macOS:**
```bash
brew install libpng cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get install libpng-dev cmake build-essential
```

### Build Steps

1. **Download STB headers** (one-time setup):
```bash
cd steganography
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o stb_image.h
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o stb_image_write.h
```

2. **Build the project:**
```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

This creates the `steg` executable in the `build/` directory.

## Usage

### Running the Program

```bash
cd build
./steg
```

### Encrypt Mode (Hide Message)

1. Select option `1` (Encrypt)
2. Choose an image from the `image/` folder (numbered list) or enter custom path
3. Enter your message (multi-line supported, press Enter twice to finish)
4. Enter encryption key (or press Enter for default)
5. Encrypted image is saved to `encrypted/` folder with `encrypted_` prefix

**Example:**
```
Select an option:
  1. Encrypt (hide message in image)
  2. Decrypt (extract message from image)
  3. Exit
Enter choice: 1

--- ENCRYPT MODE ---
Available images in '../image' folder:
  1. input.png
  2. photo.jpg
  0. Enter custom path
Select image: 1

Enter your message (press Enter twice to finish):
> Hello, this is a secret message!
> 
Enter encryption key: mykey123
```

### Decrypt Mode (Extract Message)

1. Select option `2` (Decrypt)
2. Choose source folder:
   - `1` = `encrypted/` folder (where encrypted images are saved)
   - `2` = `image/` folder (original images)
   - `3` = Custom path
3. Select image from chosen folder
4. Enter encryption key (must match the key used for encryption)
5. Decoded message is displayed in terminal

**Example:**
```
Enter choice: 2

--- DECRYPT MODE ---
Select image to decrypt:
  1. From '../encrypted' folder
  2. From '../image' folder
  3. Enter custom path
Enter choice: 1

Available images in '../encrypted' folder:
  1. encrypted_input.png
Select image: 1

Enter encryption key: mykey123

📩 DECODED MESSAGE:
   "Hello, this is a secret message!"
```

## Folder Structure

- `image/` - Place your input images here (auto-created if missing)
- `encrypted/` - Encrypted images are saved here automatically
- `build/` - Build directory (created by CMake)

## Technical Details

- **Image Format**: Supports PNG, JPG, JPEG, BMP (RGB, 3 channels)
- **Embedding**: Uses 4-byte length header + encrypted message
- **Mask Algorithm**: 8×8 pixel blocks analyzed for local median vs global median and standard deviation
- **Threading**: 2 main threads (encryption + analysis), 4 worker threads for image analysis
- **Memory**: Efficient histogram-based median calculation for large images

## Limitations

- XOR encryption is **not cryptographically secure** (for demonstration only)
- Requires sufficient low-contrast regions in image for message capacity
- Image dimensions must match between encoding and decoding (mask is recomputed)
- Works best with images that have smooth, low-contrast areas

## License

This project is for educational/demonstration purposes.
# finalfinalstenog
