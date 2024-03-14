import argparse
import csv
import os
import requests
import tempfile
import zipfile
import io
from datetime import datetime
from multiprocessing import Pool, cpu_count
from pathlib import Path

use_ram = False


def download(parameters):
    directory, file_name, extension, url = parameters
    try:
        os.makedirs(directory, exist_ok=True)
        with (open(directory/file_name, 'wb') if extension == 'osm'
                else tempfile.SpooledTemporaryFile() if use_ram
                else tempfile.TemporaryFile() as temp_file):
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                for chunk in r.iter_content(chunk_size=16392):
                    temp_file.write(chunk)
            print("finish downloading {}".format(file_name))
            if extension == 'zip':
                with zipfile.ZipFile(temp_file) as z:
                    z.extractall(directory / file_name)
            elif extension == 'csv':
                temp_file.seek(0)
                date_format = "%m/%d/%Y %I:%M:%S %p"
                with io.TextIOWrapper(temp_file) as in_file, open(directory / file_name, "w", newline='') as out_file:
                    reader = csv.reader(in_file)
                    header = next(reader)
                    data = sorted(reader, key=lambda x: datetime.strptime(x[2], date_format))
                    writer = csv.writer(out_file)
                    writer.writerow(header)
                    writer.writerows(data)
            print("finish extracting {}".format(file_name))
    except Exception as e:
        print("error trying {}: {}".format(file_name, e))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pull data for analytic")
    parser.add_argument("--dir", required=True, help="Data directory")
    parser.add_argument("--years", nargs="*", type=int, help="List of years to pull csv data",
                        choices=range(2013, 2024))
    parser.add_argument("--use-ram", action=argparse.BooleanOptionalAction, default=False)
    args = parser.parse_args()
    use_ram = args.use_ram
    data_dir = Path(args.dir)

    maps = {2013: "6h2x-drp2", 2014: "66as-63gf", 2015: "9arg-bn2i", 2016: "bk5j-9eu2", 2017: "jeij-fq8w",
            2018: "vbsw-zws8", 2019: "h4cq-z3dy", 2020: "r2u4-wwk3", 2021: "9kgb-ykyt", 2022: "npd7-ywjz",
            2023: "e55j-2ewb"}
    # # urls = ["https://data.cityofchicago.org/api/views/{}/rows.csv?accessType=DOWNLOAD&api_foundry=true".format(x)]
    arguments = [(data_dir / "osm", "Chicago.osm.pbf", "osm",
                  "https://download.bbbike.org/osm/bbbike/Chicago/Chicago.osm.pbf"),
                 (data_dir, "community_area", "zip",
                  "https://data.cityofchicago.org/api/geospatial/cauq-8yn6?method=export&format=Shapefile"),
                 (data_dir, "census_tract", "zip",
                  "https://data.cityofchicago.org/api/geospatial/5jrd-6zik?method=export&format=Shapefile")]
    for year in args.years:
        arguments.append((data_dir / "chicago_taxi_csv", str(year) + ".csv", "csv",
                          "https://data.cityofchicago.org/api/views/{}/rows.csv?accessType=DOWNLOAD&api_foundry=true"
                          .format(maps[int(year)])))
    arguments = list(filter(lambda x: not os.path.exists(x[0] / x[1]), arguments))
    if not arguments:
        exit(0)
    pool = Pool(min(cpu_count(), len(arguments)))
    results = pool.map(download, arguments)
    pool.close()
    pool.join()
