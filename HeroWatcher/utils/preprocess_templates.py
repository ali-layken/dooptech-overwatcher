# preprocess_heroes.py
# Run this to generate preprocessed binary images and Hu‐moments JSON for all heroes.

import os
import cv2
import json

def preprocess_heroes(input_dir, output_img_dir, output_json_path):
    if not os.path.exists(output_img_dir):
        os.makedirs(output_img_dir)

    hero_hu = {}
    for fname in os.listdir(input_dir):
        if not fname.lower().endswith('.png'):
            continue

        hero_name = os.path.splitext(fname)[0]
        img_path = os.path.join(input_dir, fname)
        img = cv2.imread(img_path)
        if img is None:
            print(f"Failed to read {img_path}")
            continue

        # 1. Grayscale + Otsu threshold to binary
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        _, bw = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY | cv2.THRESH_OTSU)

        # 2. Morphological closing to fill gaps
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        cleaned = cv2.morphologyEx(bw, cv2.MORPH_CLOSE, kernel)

        # 3. Find largest contour
        contours, _ = cv2.findContours(cleaned, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            print(f"No contours found for {hero_name}")
            continue
        largest = max(contours, key=cv2.contourArea)

        # 4. Compute Hu Moments
        moments = cv2.moments(largest)
        hu = cv2.HuMoments(moments).flatten().tolist()
        hero_hu[hero_name] = hu

        # 5. Save the cleaned binary image
        out_img_path = os.path.join(output_img_dir, fname)
        cv2.imwrite(out_img_path, cleaned)

    # 6. Dump all Hu moments to JSON
    with open(output_json_path, 'w', encoding='utf-8') as f:
        json.dump(hero_hu, f, indent=2)
    print(f"Preprocessed {len(hero_hu)} heroes, output saved to:")
    print(f"  Images: {output_img_dir}")
    print(f"  JSON:   {output_json_path}")


if __name__ == '__main__':
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../data'))
    input_dir = os.path.join(base_dir, 'hero_images')
    output_img_dir = os.path.join(base_dir, 'preprocessed_images')
    output_json_path = os.path.join(base_dir, 'hero_hu_moments.json')

    preprocess_heroes(input_dir, output_img_dir, output_json_path)
