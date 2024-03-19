import argparse
import os
import requests

from multiprocessing import Pool, cpu_count
from pathlib import Path


def download(parameters):
    file_path, url = parameters
    try:
        with open(file_path, 'wb') as temp_file, requests.get(url, stream=True) as r:
            r.raise_for_status()
            for chunk in r.iter_content(chunk_size=16392):
                temp_file.write(chunk)
        print("finish downloading {}".format(file_path))
    except Exception as e:
        print("error trying {}: {}".format(file_path, e))


if __name__ == "__main__":
    available_cities = [
        "Albuquerque",
        "Austin",
        "Berkeley",
        "Boulder",
        "CambridgeMa",
        "Chicago",
        "Corvallis",
        "Dallas",
        "Davis",
        "Denver",
        "Eugene",
        "FortCollins",
        "Huntsville",
        "Madison",
        "Miami",
        "NewOrleans",
        "NewYork",
        "Orlando",
        "PaloAlto",
        "Philadelphia",
        "Portland",
        "PortlandME",
        "Sacramento",
        "SanFrancisco",
        "SanJose",
        "SantaBarbara",
        "SantaCruz",
        "Seattle",
        "Stockton",
        "Tucson",
        "WashingtonDC"
    ]
    parser = argparse.ArgumentParser(description="Pull data for backend")
    parser.add_argument("--dir", required=True, help="OSM directory")
    parser.add_argument("--cities", nargs="+", help="List of cities to pull cities' maps", required=True,
                        choices=available_cities + ["all"])
    args = parser.parse_args()
    directory = Path(args.dir)
    os.makedirs(directory, exist_ok=True)
    chosen = args.cities if "all" not in args.cities else available_cities
    arguments = [
        (directory / ("{}.osm.pbf".format(city)), "https://download.bbbike.org/osm/bbbike/{0}/{0}.osm.pbf".format(city))
        for city in chosen]
    arguments = list(filter(lambda x: not os.path.exists(x[0]), arguments))
    if not arguments:
        exit(0)
    pool = Pool(min(cpu_count(), len(arguments)))
    results = pool.map(download, arguments)
    pool.close()
    pool.join()
