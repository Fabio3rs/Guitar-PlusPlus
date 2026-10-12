#!/usr/bin/env python3
"""Exercise real Studio mouse/keyboard input, exports and CLI captures under Xvfb."""
import re
import pathlib
import struct
import subprocess
import sys
import tempfile
import time
import zlib

exe = pathlib.Path(sys.argv[1]).resolve()
artifacts = pathlib.Path(sys.argv[2]).resolve()
artifacts.mkdir(parents=True, exist_ok=True)


def tga(path, width, height, atlas=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    header = bytearray(18)
    header[2] = 2
    struct.pack_into('<HH', header, 12, width, height)
    header[16:18] = bytes((32, 8))
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            ink = not atlas or (2 <= x % 12 < 9 and 3 <= y % 16 < 13)
            pixels.extend((255, 255, 255, 255 if ink else 0))
    path.write_bytes(header + pixels)


def png_pixels(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset = 8
    compressed = bytearray()
    dimensions = None
    while offset < len(data):
        length = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset + 4:offset + 8]
        content = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from('>I', data, offset + 8 + length)[0]
        assert zlib.crc32(kind + content) & 0xffffffff == crc
        if kind == b'IHDR':
            dimensions = struct.unpack_from('>II', content)
        elif kind == b'IDAT':
            compressed.extend(content)
        offset += length + 12
    assert dimensions == (1280, 800), dimensions
    raw = zlib.decompress(compressed)
    stride = 1280 * 4
    previous = bytearray(stride)
    image = bytearray()
    def paeth(a, b, c):
        p = a + b - c
        da, db, dc = abs(p - a), abs(p - b), abs(p - c)
        return a if da <= db and da <= dc else b if db <= dc else c
    for y in range(800):
        start = y * (stride + 1)
        method = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            up = previous[x]
            corner = previous[x - 4] if x >= 4 else 0
            predictor = (0 if method == 0 else left if method == 1 else up if method == 2
                         else (left + up) // 2 if method == 3 else paeth(left, up, corner))
            row[x] = (row[x] + predictor) & 255
        image.extend(row)
        previous = row
    return bytes(image)


def preview_pixels(image):
    # Exclude inspector and filenames: this region is the CFonts-rendered text.
    return b''.join(image[(y * 1280 + 458) * 4:(y * 1280 + 1268) * 4]
                    for y in range(570, 690))


with tempfile.TemporaryDirectory(prefix='gui-', dir=artifacts) as temporary:
    root = pathlib.Path(temporary)
    assets = root / 'assets with spaces'
    output = root / 'output'
    output.mkdir()
    tga(assets / 'data/sprites/FONT.tga', 384, 128, True)
    tga(assets / 'data/sprites/interfacebg.tga', 2, 2)
    mapping = ''.join(chr(i) for i in range(32, 127))
    mapping = '\n'.join(mapping[i:i + 32] for i in range(0, len(mapping), 32))
    escaped = mapping.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
    theme = assets / 'data/themes/gppdefaulttheme/Theme.lua'
    theme.parent.mkdir(parents=True)
    theme.write_text('setCallBackToEvent("applyTheme", function()\n'
                     f'assert(addTextureToFont("default","data/sprites","FONT.tga","{escaped}",32,8))\n'
                     'assert(setFontMetrics("default",0.2,0.8,0.2))\nend)\n')
    source_before = theme.read_bytes()
    base = [str(exe), '--asset-root', str(assets), '--output-dir', str(output)]
    subprocess.run(base + ['--panel', 'fonts', '--frames', '5', '--ui-capture', 'before.png'], check=True, timeout=20)
    log = open(output / 'ui.log', 'w')
    process = subprocess.Popen(base, stdout=log, stderr=subprocess.STDOUT)

    def xdo(*args):
        return subprocess.check_output(['xdotool', *map(str, args)], text=True, timeout=5).strip()

    def click(x, y):
        xdo('mousemove', '--window', window, x, y)
        xdo('click', 1)
        time.sleep(.12)

    def wait_file(path):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if path.exists():
                return
            if process.poll() is not None:
                raise AssertionError('Studio exited: ' + (output / 'ui.log').read_text())
            time.sleep(.05)
        raise AssertionError('Export missing: ' + str(path))

    try:
        deadline = time.monotonic() + 10
        window = None
        while time.monotonic() < deadline and process.poll() is None:
            found = subprocess.run(['xdotool', 'search', '--all', '--onlyvisible', '--pid', str(process.pid),
                                    '--name', r'^Guitar\+\+ GUI Studio$'],
                                   text=True, capture_output=True)
            if found.returncode == 0:
                window = found.stdout.strip().splitlines()[-1]
                break
            time.sleep(.1)
        assert window, (output / 'ui.log').read_text()
        xdo('windowfocus', '--sync', window)
        time.sleep(.3)
        # Select the name control and reorder it without breaking its binding.
        click(35, 238)
        click(35, 302)
        # Drag that control in the actual CMenu preview.
        click(900, 375)
        xdo('mousedown', 1)
        time.sleep(.12)
        xdo('mousemove', '--window', window, 925, 385)
        time.sleep(.15)
        xdo('mouseup', 1)
        click(370, 95)
        exported_menu = output / 'Gui.lua'
        wait_file(exported_menu)
        time.sleep(.2)
        xdo('key', 'Return')  # Dismiss the export confirmation.
        time.sleep(.12)
        definition = exported_menu.read_text()
        assert definition.index('id = "name"') < definition.index('id = "title"')
        position = re.search(r'id = "name".*?x = ([^,]+)', definition, re.S)
        assert position and float(position.group(1)) > 0, definition
        # Test the modal's real text input and cancellation path.
        click(467, 70)
        click(935, 385)
        xdo('type', '--clearmodifiers', 'Z')
        xdo('key', 'Escape')
        xdo('key', 'Escape')
        time.sleep(.15)
        click(680, 70)  # Reopen the preview.
        click(467, 70)  # Back to edit mode.
        # Font Studio: edit only g's offset through the numeric editor.
        click(160, 42)
        time.sleep(.2)
        # Character shortcuts use the produced symbol, including Shift.
        # Editing the numeric field must not select its typed digits instead.
        for key, offset in [('shift+a', '0.125'), ('shift+1', '0.25'), ('7', '0.375')]:
            click(1100, 390)
            xdo('key', key)
            time.sleep(.12)
            xdo('mousemove', '--window', window, 120, 443)
            xdo('keydown', 'ctrl')
            xdo('click', 1)
            xdo('keyup', 'ctrl')
            xdo('key', 'ctrl+a')
            xdo('type', '--clearmodifiers', offset)
            xdo('key', 'Return')
            time.sleep(.12)
        click(1100, 390)
        xdo('key', 'g')
        time.sleep(.12)
        # Escape cancels selection: this must leave g selected.
        xdo('key', 'Escape')
        xdo('key', 'a')
        time.sleep(.12)
        xdo('mousemove', '--window', window, 120, 443)
        xdo('keydown', 'ctrl')
        xdo('click', 1)
        xdo('keyup', 'ctrl')
        xdo('key', 'ctrl+a')
        xdo('type', '--clearmodifiers', '--', '-0.25')
        xdo('key', 'Return')
        time.sleep(.15)
        click(418, 95)
        exported_font = output / 'Theme.lua'
        wait_file(exported_font)
        time.sleep(.2)
        xdo('key', 'Return')  # Dismiss the export confirmation.
        time.sleep(.12)
        font_text = exported_font.read_text()
        assert 'setFontCharMetrics("default", "g", 1, -0.25)' in font_text
        for char, offset in [('A', '0.125'), ('!', '0.25'), ('7', '0.375')]:
            assert f'setFontCharMetrics("default", "{char}", 1, {offset})' in font_text
        assert 'setFontCharMetrics("default", "a"' not in font_text
        assert 'setFontCharMetrics("default", "0"' not in font_text
        click(1230, 18)
        assert process.wait(timeout=5) == 0
        assert theme.read_bytes() == source_before, 'Source atlas theme was overwritten'
        assert not (assets / 'data/saves').exists(), 'Studio initialized game settings'
        # Reopen both exports through the public CLI and check real PNGs.
        subprocess.run(base + ['--menus', str(exported_menu), '--font', str(exported_font),
                               '--panel', 'fonts', '--frames', '5', '--ui-capture', 'after.png'],
                       check=True, timeout=20)
        before = preview_pixels(png_pixels(output / 'before.png'))
        after = preview_pixels(png_pixels(output / 'after.png'))
        assert sum(min(before[i:i + 3]) > 190 for i in range(0, len(before), 4)) > 100
        assert before != after, 'Glyph offset did not change the actual rendered preview' 
        for name in ('before.png', 'after.png', 'Gui.lua', 'Theme.lua', 'ui.log'):
            (artifacts / name).write_bytes((output / name).read_bytes())
        # Explicit invalid inputs fail, rather than silently loading another file.
        failed = subprocess.run(base + ['--font', str(root / 'missing.lua'), '--frames', '1'],
                                capture_output=True, timeout=20)
        assert failed.returncode != 0
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        log.close()
        for name in ('Gui.lua', 'Theme.lua', 'ui.log'):
            if (output / name).exists():
                (artifacts / name).write_bytes((output / name).read_bytes())
print('GUI interaction, export round trips and PNG captures passed')
