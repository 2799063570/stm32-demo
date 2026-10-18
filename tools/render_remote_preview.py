"""Render actual OLED snapshots from remote C tests; these are test scenes, not board photos."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CASES = [('HOME / ZERO menu', 'menu'), ('Joystick input', 'input'),
         ('Joint feedback', 'joints'), ('Request accepted, planning', 'planning'),
         ('Actual pose verified', 'done'), ('Feedback expired', 'offline')]


def render():
    canvas = Image.new('RGB', (856, 804), '#111925')
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    draw.text((24, 14), 'STM32 <-> ROS1 | Serial remote', fill='#f1f5ff', font=font)
    draw.text((24, 46), 'Actual framebuffer, synthetic test scenes. Hardware untested.', fill='#a9b9cc', font=font)
    for index, (title, name) in enumerate(CASES):
        data = (ROOT / 'tests/build' / f'remote_{name}_oled.bin').read_bytes()
        assert len(data) == 1024
        screen = Image.new('RGB', (128, 64), '#020609')
        for y in range(64):
            for x in range(128):
                if data[y // 8 * 128 + x] & (1 << (y % 8)):
                    screen.putpixel((x, y), (176, 233, 255))
        x, y = 24 + index % 2 * 416, 92 + index // 2 * 236
        draw.text((x, y), title, fill='#e2ebfa', font=font)
        canvas.paste(screen.resize((384, 192), Image.Resampling.NEAREST), (x, y + 30))
    output = ROOT / 'docs/串口遥控显示预览.png'
    canvas.save(output)
    print(output)


if __name__ == '__main__':
    render()
