import os
from PIL import Image

def find_largest_pngs(folder_path):
    largest_width = -1
    largest_height = -1
    largest_width_file = None
    largest_height_file = None

    for filename in os.listdir(folder_path):
        if filename.lower().endswith('.png'):
            full_path = os.path.join(folder_path, filename)
            try:
                with Image.open(full_path) as img:
                    width, height = img.size
                    if width > largest_width:
                        largest_width = width
                        largest_width_file = full_path
                    if height > largest_height:
                        largest_height = height
                        largest_height_file = full_path
            except Exception as e:
                print(f"Failed to open {filename}: {e}")

    return largest_width_file, largest_width, largest_height_file, largest_height

if __name__ == "__main__":
    folder = os.path.abspath(os.path.join(os.path.dirname(__file__), "../data/hero_images"))
    width_file, width, height_file, height = find_largest_pngs(folder)

    if width_file:
        print(f"Largest width .png: {width_file} ({width}px)")
    else:
        print("No valid .png files found for width.")

    if height_file:
        print(f"Largest height .png: {height_file} ({height}px)")
    else:
        print("No valid .png files found for height.")
