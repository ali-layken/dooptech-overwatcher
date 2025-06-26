import cv2
import os

TARGET_WIDTH = 120
TARGET_HEIGHT = 60

input_dir = "./base"
output_dir = "./scaled_templates"
os.makedirs(output_dir, exist_ok=True)

for fname in os.listdir(input_dir):
    if not fname.endswith(".png"):
        continue
    img = cv2.imread(os.path.join(input_dir, fname))
    if img is None:
        print(f"Could not read {fname}")
        continue

    resized = cv2.resize(img, (TARGET_WIDTH, TARGET_HEIGHT), interpolation=cv2.INTER_AREA)
    cv2.imwrite(os.path.join(output_dir, fname), resized)
    print(f"Resized {fname}")