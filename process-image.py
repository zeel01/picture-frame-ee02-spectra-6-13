from PIL import Image, ImageOps, ImageEnhance
import os
import sys

# Display configuration
# Note: Buffer is 1600x1200 to match firmware expectations
# Image is rotated 90° CCW for portrait-mounted display
FRAME_WIDTH = 1200
FRAME_HEIGHT = 1600
BUFFER_SIZE = 960000  # (1600 * 1200) / 2 bytes

# The Spectra 6 Color Palette (RGB)
PALETTE_RGB = [
    (0, 0, 0),       # Black
    (255, 255, 255), # White
    (255, 255, 0),   # Yellow
    (255, 0, 0),     # Red
    (0, 0, 255),     # Blue
    (41, 204, 20)    # Green
]

# Map palette index to hardware 4-bit codes
HARDWARE_MAP = {
    0: 0x0F,  # Black
    1: 0x00,  # White
    2: 0x0B,  # Yellow
    3: 0x06,  # Red
    4: 0x0D,  # Blue
    5: 0x02   # Green
}

# Image to display - change this path to your desired image
DEFAULT_IMAGE_PATH = "image.jpg"
DEFAULT_OUTPUT_DIR = "output"

# Image rotation configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR = os.path.join(SCRIPT_DIR, "images")
STATE_FILE = os.path.join(SCRIPT_DIR, ".eink_rotation_state.json")
DEVICE_CONFIG_FILENAME = "device_config.json"
GLOBAL_DEVICE_CONFIG_PATH = os.path.join(SCRIPT_DIR, DEVICE_CONFIG_FILENAME)
SUPPORTED_EXTENSIONS = {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.heic', '.webp'}
DEFAULT_DEVICE_ID = "default"
GLOBAL_SCHEDULE_TARGET = "global"
SCHEDULE_KEYS = (
    'refresh_interval_minutes',
    'active_start_hour',
    'active_end_hour',
    'timezone_offset_minutes',
)

# Image enhancement settings
DEFAULT_CONTRAST = 1.2
DEFAULT_BRIGHTNESS = 1.0
DEFAULT_SATURATION = 1.2

def create_palette_image():
    """Create a palette image for PIL quantization."""
    palette_img = Image.new('P', (1, 1))
    palette_data = []
    for r, g, b in PALETTE_RGB:
        palette_data.extend([r, g, b])
    # Pad to 256 colors (PIL requirement)
    palette_data.extend([0, 0, 0] * (256 - len(PALETTE_RGB)))
    palette_img.putpalette(palette_data)
    return palette_img

def process_image_to_packed(image_path, contrast=DEFAULT_CONTRAST,
                            brightness=DEFAULT_BRIGHTNESS,
                            saturation=DEFAULT_SATURATION):
    """
    Process an image file to packed 4bpp binary data for the Spectra 6 display.

    Args:
        image_path: Path to the source image
        contrast: Contrast enhancement factor (1.0 = original)
        brightness: Brightness enhancement factor (1.0 = original)
        saturation: Saturation enhancement factor (1.0 = original)

    Returns:
        bytes: Packed binary data (960,000 bytes)
    """

    # Open and prepare image
    img = Image.open(image_path)
    img = ImageOps.exif_transpose(img)  # Handle EXIF orientation (camera rotation)
    img = img.convert("RGB")

    # For portrait-mounted display: fit to portrait dimensions first,
    # then rotate to match the 1600x1200 buffer layout expected by firmware
    img = ImageOps.fit(img, (FRAME_WIDTH, FRAME_HEIGHT),  # 1200x1600 portrait
                       method=Image.Resampling.LANCZOS,
                       centering=(0.5, 0.0))

    # Rotate 270° (90° clockwise) to convert portrait image to landscape buffer
    # and match the physical display orientation with board attached at bottom
    # img = img.rotate(270, expand=True)

    # Apply enhancements
    if brightness != 1.0:
        img = ImageEnhance.Brightness(img).enhance(brightness)
    if contrast != 1.0:
        img = ImageEnhance.Contrast(img).enhance(contrast)
    if saturation != 1.0:
        img = ImageEnhance.Color(img).enhance(saturation)

    # Quantize to 6-color palette with Floyd-Steinberg dithering
    palette_img = create_palette_image()
    dithered = img.quantize(
        colors=len(PALETTE_RGB),
        palette=palette_img,
        dither=Image.Dither.FLOYDSTEINBERG
    )

    # Pack bits (2 pixels per byte)
    pixels = list(dithered.get_flattened_data())
    packed_data = bytearray()

    for i in range(0, len(pixels), 2):
        p1_idx = pixels[i]
        p2_idx = pixels[i+1] if i+1 < len(pixels) else 0

        val1 = HARDWARE_MAP.get(p1_idx, 0x01)
        val2 = HARDWARE_MAP.get(p2_idx, 0x01)

        byte_val = (val1 << 4) | val2
        packed_data.append(byte_val)

    return bytes(packed_data)

def process_image(image_path, count=0, output_dir="."):
    packed_data = process_image_to_packed(image_path)
    output_path = os.path.join(output_dir, f"{count:03d}.bin")
    with open(output_path, "wb") as f:
        f.write(packed_data)

    # with open("src/image.h", "w") as f:
    #     f.write("#ifndef _IMAGE_H_\n#define _IMAGE_H_\n\n")
    #     f.write(f"const unsigned char gImage_13inch3[] PROGMEM = {{\n")
    #     for i in range(0, len(packed_data), 16):
    #         line = ", ".join(f"0x{byte:02X}" for byte in packed_data[i:i+16])
    #         f.write(f"    {line},\n")
    #     f.write("};\n")
    #     f.write("\n#endif // _IMAGE_H_\n")

    print(f"Processed image saved as {count:03d}.bin ({len(packed_data)} bytes)")

def main():
    # Get image path from command line argument or use default
    image_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IMAGE_PATH
    output_dir = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUTPUT_DIR

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if os.path.isdir(image_path):
        images = [f for f in os.listdir(image_path) if os.path.splitext(f)[1].lower() in SUPPORTED_EXTENSIONS]
        if not images:
            print(f"No supported image files found in directory: {image_path}")
            return
        
        print(f"Processing {len(images)} images from directory: {image_path}")
        count = 0
        for img_file in images:
            img_path = os.path.join(image_path, img_file)
            print(f"Processing {img_path}...")
            process_image(img_path, count, output_dir)
            count += 1
    else:
        if not os.path.isfile(image_path):
            print(f"Image file not found: {image_path}")
            return
        print(f"Processing image: {image_path}")
        process_image(image_path, 0, output_dir)

if __name__ == "__main__":
    main()