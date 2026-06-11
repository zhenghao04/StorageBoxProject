from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
PNG_PATH = ASSETS / "app_icon.png"
ICO_PATH = ASSETS / "app_icon.ico"


def rounded_gradient(size: int, radius: int) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    draw_mask = ImageDraw.Draw(mask)
    draw_mask.rounded_rectangle((size * 0.09, size * 0.09, size * 0.91, size * 0.91), radius=radius, fill=255)

    top = (15, 23, 42)
    mid = (29, 78, 216)
    bottom = (15, 118, 110)
    gradient = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = gradient.load()
    for y in range(size):
        t = y / (size - 1)
        if t < 0.58:
            local = t / 0.58
            color = tuple(round(top[i] * (1 - local) + mid[i] * local) for i in range(3))
        else:
            local = (t - 0.58) / 0.42
            color = tuple(round(mid[i] * (1 - local) + bottom[i] * local) for i in range(3))
        for x in range(size):
            pixels[x, y] = (*color, 255)

    image.alpha_composite(Image.composite(gradient, image, mask))
    return image


def draw_icon(size: int) -> Image.Image:
    scale = size / 512
    image = rounded_gradient(size, round(108 * scale))
    draw = ImageDraw.Draw(image, "RGBA")

    def box(values):
        return tuple(round(v * scale) for v in values)

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow, "RGBA")
    shadow_draw.rounded_rectangle(box((126, 220, 438, 442)), radius=round(56 * scale), fill=(2, 6, 23, 100))
    shadow = shadow.filter(ImageFilter.GaussianBlur(round(16 * scale)))
    image.alpha_composite(shadow)

    lid = [box((118, 185)), box((382, 132)), box((431, 162)), box((447, 220)), box((154, 279)), box((91, 231))]
    draw.polygon(lid, fill=(249, 115, 22, 255))
    draw.polygon([box((118, 185)), box((382, 132)), box((431, 162)), box((405, 178)), box((174, 225)), box((91, 231))], fill=(251, 191, 36, 255))
    draw.line([box((118, 185)), box((382, 132)), box((431, 162)), box((447, 220)), box((154, 279)), box((91, 231)), box((118, 185))], fill=(255, 237, 213, 150), width=max(2, round(8 * scale)))

    body_rect = box((74, 225, 438, 437))
    body_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(body_mask).rounded_rectangle(body_rect, radius=round(48 * scale), fill=255)
    body_gradient = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    body_draw = ImageDraw.Draw(body_gradient, "RGBA")
    for y in range(body_rect[1], body_rect[3]):
        t = (y - body_rect[1]) / max(1, body_rect[3] - body_rect[1])
        color = (
            round(103 * (1 - t) + 8 * t),
            round(232 * (1 - t) + 145 * t),
            round(249 * (1 - t) + 178 * t),
            255,
        )
        body_draw.line([(body_rect[0], y), (body_rect[2], y)], fill=color)
    image.alpha_composite(Image.composite(body_gradient, Image.new("RGBA", (size, size), (0, 0, 0, 0)), body_mask))

    draw.rounded_rectangle(box((78, 226, 435, 435)), radius=round(48 * scale), outline=(236, 254, 255, 120), width=max(2, round(10 * scale)))
    draw.rounded_rectangle(box((87, 236, 425, 285)), radius=round(36 * scale), fill=(207, 250, 254, 68))

    colors = [
        (255, 255, 255, 255),
        (209, 250, 229, 255),
        (254, 243, 199, 255),
        (224, 242, 254, 255),
        (255, 255, 255, 255),
        (204, 251, 241, 255),
        (219, 234, 254, 255),
        (240, 253, 250, 255),
        (254, 215, 170, 255),
    ]
    start_x, start_y = 158, 293
    step = 64
    row_step = 50
    tile = 42
    for row in range(3):
        for col in range(3):
            x = start_x + col * step
            y = start_y + row * row_step
            draw.rounded_rectangle(
                box((x, y, x + tile, y + tile)),
                radius=round(12 * scale),
                fill=colors[row * 3 + col],
            )

    return image


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    base = draw_icon(512)
    base.save(PNG_PATH)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    base.save(ICO_PATH, format="ICO", sizes=[(s, s) for s in sizes])
    print(PNG_PATH)
    print(ICO_PATH)


if __name__ == "__main__":
    main()
