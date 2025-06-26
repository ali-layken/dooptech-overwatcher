import cv2
import numpy as np

def match_hero(base_img, hero_templates):
    gray_base = cv2.cvtColor(base_img, cv2.COLOR_BGR2GRAY)
    results = []

    for name, template in hero_templates.items():
        gray_template = cv2.cvtColor(template, cv2.COLOR_BGR2GRAY)
        res = cv2.matchTemplate(gray_base, gray_template, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, _ = cv2.minMaxLoc(res)
        results.append((name, max_val))

    results.sort(key=lambda x: x[1], reverse=True)
    return results[0]  # best match

if __name__ == "__main__":
    base = cv2.imread("./samples/RoadhogSample.png")
    templates = {
        "ana": cv2.imread("./scaled_templates/ana.png"),
        "roadhog": cv2.imread("./scaled_templates/roadhog.png"),
        "soldier": cv2.imread("./scaled_templates/soldier.png"),
        "zarya": cv2.imread("./scaled_templates/zarya.png"),

    }

    hero, score = match_hero(base, templates)
    print(f"Detected: {hero} (score: {score:.2f})")
