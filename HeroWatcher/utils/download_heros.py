import json
import os
import urllib.request

path = "../data/hero_images"

# Load the JSON file
with open("heros.json", "r", encoding="utf-8") as f:
    heros = json.load(f)

# Make a directory to store the images
os.makedirs(path, exist_ok=True)

# Download each image
for name, url in heros.items():
    safe_name = name.replace(":", "").replace("/", "_")  # handle special characters
    out_path = os.path.join(path, f"{safe_name}.png")
    print(f"Downloading {name}...")

    try:
        urllib.request.urlretrieve(url, out_path)
    except Exception as e:
        print(f"Failed to download {name}: {e}")
