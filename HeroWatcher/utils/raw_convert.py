from PIL import Image
import sys

# Fill these in from your logs
WIDTH = 1920  # example
HEIGHT = 1080  # example

with open("../crop_1751746813.raw", "rb") as f:
    data = f.read()

img = Image.frombytes("RGBA", (WIDTH, HEIGHT), data)
img.save("out.png")
