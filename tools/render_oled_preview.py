"""把真实OLED绘图代码导出的页显存渲染为预览；数值为测试夹具，不是实测。"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CASES = [
    ('UART + DMA + IDLE', 'oled_uart.bin'),
    ('MPU6050 / I2C DMA', 'oled_mpu.bin'),
    ('CAN interrupt counters', 'oled_can.bin'),
    ('Capture + encoder', 'oled_capture.bin'),
    ('PID status / signed PWM', 'oled_motor.bin'),
    ('PID history / target dots', 'oled_trend.bin'),
    ('Fault forces status page', 'oled_fault.bin'),
]

def font(size):
    path = Path('C:/Windows/Fonts/segoeui.ttf')
    return ImageFont.truetype(str(path), size) if path.exists() else ImageFont.load_default()

def render():
    canvas = Image.new('RGB', (920, 914), '#10151e')
    draw = ImageDraw.Draw(canvas)
    draw.text((28, 18), 'OLED UI preview | 128 x 64', font=font(28), fill='#f3f6ff')
    draw.text((28, 60), 'Simulated values, rendered from the project framebuffer. Hardware untested.', font=font(17), fill='#aebdd2')
    for index, (title, name) in enumerate(CASES):
        data = (ROOT / 'tests/build' / name).read_bytes()
        assert len(data) == 1024
        screen = Image.new('RGB', (128, 64), '#02070a')
        for y in range(64):
            for x in range(128):
                if data[y // 8 * 128 + x] & (1 << (y % 8)):
                    screen.putpixel((x, y), (173, 228, 255))
        screen = screen.resize((384, 192), Image.Resampling.NEAREST)
        x = 28 + index % 2 * 448
        y = 106 + index // 2 * 196
        # Compact cards keep the seven screens on a single exported sheet.
        screen = screen.resize((320, 160), Image.Resampling.NEAREST)
        draw.text((x, y), title, font=font(18), fill='#f0f3fa')
        canvas.paste(screen, (x, y + 28))
    output = ROOT / 'docs/OLED显示预览.png'
    canvas.save(output)
    print(output)

if __name__ == '__main__':
    render()
