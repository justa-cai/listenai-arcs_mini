import math
import struct
import os
import argparse

def main():
    parser = argparse.ArgumentParser(description="Generate a sine wave audio clip as a C header file.")
    parser.add_argument('--frequency', type=float, default=900.0,
                        help='Frequency of the sine wave in Hz (default: 900.0)')
    parser.add_argument('--output', type=str, default='../src/audio_clip.h',
                        help='Output header file path (default: ../src/audio_clip.h)')
    parser.add_argument('--duration', type=float, default=3.0,
                        help='Duration of the audio in seconds (default: 3.0)')
    parser.add_argument('--sample_rate', type=int, default=16000,
                        help='Sample rate in Hz (default: 16000)')
    parser.add_argument('--amplitude', type=int, default=32000,
                        help='Amplitude of the wave (default: 32000)')
    
    args = parser.parse_args()

    sample_rate = args.sample_rate
    duration = args.duration
    frequency = args.frequency
    amplitude = args.amplitude
    output_path = args.output

    num_samples = int(sample_rate * duration)
    num_bytes = num_samples * 2 # 16-bit
    
    print(f"Generating {output_path}...")
    print(f"Format: {sample_rate}Hz, 16bit, Mono, {duration}s, {frequency}Hz Sine Wave")

    # Ensure output directory exists
    output_dir = os.path.dirname(output_path)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    header_content = [
        "/*",
        " * Generated Audio Clip",
        f" * Format: {sample_rate}Hz, 16-bit, Mono, {duration}s, {frequency}Hz Sine Wave",
        " */",
        "#include <stdint.h>",
        "",
        f"#define AUDIO_CLIP_FREQ ({frequency}f)",
        f"#define AUDIO_CLIP_SAMPLES ({num_samples})",
        f"#define AUDIO_CLIP_BYTES ({num_bytes})",
        "",
        "__attribute__((aligned(4))) static const uint8_t g_audio_clip[] = {"
    ]

    data_bytes = bytearray()
    for i in range(num_samples):
        t = i / sample_rate
        sample = int(amplitude * math.sin(2 * math.pi * frequency * t))
        # Little Endian 16-bit
        data_bytes.extend(struct.pack('<h', sample))

    # 转换为 C 数组格式
    lines = []
    current_line = "    "
    for i, byte in enumerate(data_bytes):
        current_line += f"0x{byte:02X}, "
        if (i + 1) % 16 == 0:
            lines.append(current_line.strip())
            current_line = "    "
    if current_line.strip():
        lines.append(current_line.strip())

    header_content.extend(lines)
    header_content.append("};")
    header_content.append("")

    with open(output_path, "w") as f:
        f.write("\n".join(header_content))

    print("Done.")

if __name__ == '__main__':
    main()
