## Generate embedded font data

First use [monobit](https://github.com/robhagemans/monobit) to convert a font (monospace, 8x16) to png.

```bash
./convert.py font.bdf to font.png
```

Then run this script.

```python
from PIL import Image 

im = Image.open('font.png')

char_size = (8, 16)
chars_skipped = 32
chars_line = 32

assert char_size[0] <= 8
assert im.size[0] == chars_line * (char_size[0] + 1) - 1
assert im.size[1] >= int((128 - chars_skipped + chars_line - 1) / chars_line) * (char_size[1] + 1) - 1

encoded = []

for y in range(char_size[1]):
    for k in range(128 - chars_skipped):
        offset = (int(k % chars_line) * (char_size[0] + 1), int(k / chars_line) * (char_size[1] + 1))
        out = 0

        for x in range(char_size[0]):
            avg = sum(im.getpixel((x + offset[0], y + offset[1])))
            out = (0x80 if avg > 0 else 0) | (out >> 1)

        encoded.append(f'0x{out:02x}, ')

print(''.join(encoded))
```