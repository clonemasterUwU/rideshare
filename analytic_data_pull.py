import os, sys, requests, zipfile
from multiprocessing import Pool, cpu_count


def download(file_name_and_url):
    file_name, url = file_name_and_url
    if file_name.endswith('.zip'):
        folder_name = file_name[:-4]
        if os.path.exists(folder_name):
            print("{} already exists".format(folder_name))
            return
        os.makedirs(folder_name)
        try:
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                with open(file_name, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=16392):
                        f.write(chunk)
            print("finish downloading {}".format(file_name))
            with zipfile.ZipFile(file_name) as z:
                z.extractall(folder_name)
            os.remove(file_name)
            print("finish extracting {}".format(file_name))
        except Exception as e:
            print("error downloading {}: {}".format(file_name, e))
    else:
        if os.path.exists(file_name):
            print("{} already exists".format(file_name))
            return
        try:
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                with open(file_name, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=16392):
                        f.write(chunk)
            print("finish downloading {}".format(file_name))
        except Exception as e:
            print("error downloading {}: {}".format(file_name, e))


if __name__ == "__main__":
    data_dir_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
    if not os.path.exists(data_dir_path):
        os.makedirs(data_dir_path)
    maps = {2013: "6h2x-drp2", 2014: "66as-63gf", 2015: "9arg-bn2i", 2016: "bk5j-9eu2", 2017: "jeij-fq8w",
            2018: "vbsw-zws8", 2019: "h4cq-z3dy", 2020: "r2u4-wwk3", 2021: "9kgb-ykyt", 2022: "npd7-ywjz",
            2023: "e55j-2ewb"}
    # urls = ["https://data.cityofchicago.org/api/views/{}/rows.csv?accessType=DOWNLOAD&api_foundry=true".format(x)]
    arg = [(os.path.join(data_dir_path, "Chicago.osm.pbf"),
            "https://download.bbbike.org/osm/bbbike/Chicago/Chicago.osm.pbf"),
           (os.path.join(data_dir_path, "community_area.zip"),
            "https://data.cityofchicago.org/api/geospatial/cauq-8yn6?method=export&format=Shapefile"),
           (os.path.join(data_dir_path, "census_tract_2010.zip"),
            "https://data.cityofchicago.org/api/geospatial/5jrd-6zik?method=export&format=Shapefile")]
    for year in sys.argv[1:]:
        try:
            if int(year) in maps:
                arg.append((os.path.join(data_dir_path, "{}.csv".format(year)),
                            "https://data.cityofchicago.org/api/views/{}/rows.csv?accessType=DOWNLOAD&api_foundry=true"
                            .format(maps[int(year)])))
            else:
                print("year {} is not in data list".format(year))
        except ValueError:
            print("int argument, not {}".format(year))
    pool = Pool(min(cpu_count(), len(arg)))
    results = pool.map(download, arg)
    pool.close()
    pool.join()
