"""渲染测试调用真实GameView/OLED代码生成的显存；不是实板照片。"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CASES = [('Start', 'title'), ('Dodge obstacles', 'play'), ('Pause', 'pause'), ('Retry', 'over')]


def render():
    canvas = Image.new('RGB', (856, 568), '#111925')
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    draw.text((24, 14), 'STM32 + joystick | Plane Dodge', fill='#f1f5ff', font=font)
    draw.text((24, 46), 'Real game framebuffer, test scene. Hardware untested.', fill='#a9b9cc', font=font)
    for index, (title, name) in enumerate(CASES):
        data = (ROOT / 'tests/build' / f'game_{name}.bin').read_bytes()
        assert len(data) == 1024
        screen = Image.new('RGB', (128, 64), '#020609')
        for y in range(64):
            for x in range(128):
                if data[y // 8 * 128 + x] & (1 << (y % 8)):
                    screen.putpixel((x, y), (176, 233, 255))
        x, y = 24 + index % 2 * 416, 92 + index // 2 * 236
        draw.text((x, y), title, fill='#e2ebfa', font=font)
        canvas.paste(screen.resize((384, 192), Image.Resampling.NEAREST), (x, y + 30))
    output = ROOT / 'docs/飞机避障游戏预览.png'
    canvas.save(output)
    print(output)


if __name__ == '__main__':
    render()
