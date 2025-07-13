#!/usr/bin/env python3
"""
Fetch the Overwatch Wiki Weapon page and extract each weapon’s ammo counts.
"""

import requests
from bs4 import BeautifulSoup

def extract_ammo_counts():
    URL = "https://overwatch.fandom.com/wiki/Weapon"
    resp = requests.get(URL)
    resp.raise_for_status()

    soup = BeautifulSoup(resp.text, "html.parser")

    # 1) Grab all wikitable tables
    tables = soup.find_all("table", class_="wikitable")
    table = None

    # 2) Find the one whose header row includes both "Weapon" and "Ammo"
    for t in tables:
        first_row = t.find("tr")
        headers = [cell.get_text(strip=True) for cell in first_row.find_all(["th","td"])]
        if "Weapon" in headers and "Ammo" in headers:
            table = t
            break

    if table is None:
        raise RuntimeError("Could not find the weapons table on the page")

    # 3) Locate header cells (prefer <thead>)
    thead = table.find("thead")
    if thead:
        header_cells = thead.find("tr").find_all(["th","td"])
    else:
        header_cells = table.find("tr").find_all(["th","td"])

    cols = [cell.get_text(strip=True) for cell in header_cells]
    weapon_idx = cols.index("Weapon")
    ammo_idx   = cols.index("Ammo")

    # 4) Iterate body rows and pull Weapon → Ammo
    ammo_counts = {}
    for row in table.find_all("tr")[1:]:
        cells = row.find_all("td")
        if len(cells) <= max(weapon_idx, ammo_idx):
            continue
        name = cells[weapon_idx].get_text(strip=True)
        ammo = cells[ammo_idx].get_text(strip=True)
        ammo_counts[name] = ammo

    return ammo_counts

if __name__ == "__main__":
    for w, a in extract_ammo_counts().items():
        print(f"{w:30s} → {a}")
