import tabulate
import pandas
import sys


def load_dataframe(path):
    """Loads from @path pandas dataframe and computes averages and medians in each metric

    :param path: path to csv file delimited by ;
    :return: averages and medians of each column
    """
    df = pandas.read_csv(path, sep=';')
    avgs = df.mean(numeric_only=True)
    meds = df.median(numeric_only=True)
    return avgs, meds


if __name__ == "__main__":
    profiles = sys.argv[1:]
    if len(profiles) == 0:
        printf(f"usage: compare_profiles.py [target.csv baseline1.csv ... baselinen.csv]")

    averages, medians = [], []
    columns = set()
    for profile in profiles:
        avg, med = load_dataframe(profile)
        columns.update(list(avg.keys()))
        columns.update(list(avg.keys()))
        averages.append(avg)
        medians.append(med)

    headers = ['metric'] + ['target (avg)', 'target (med)']
    if len(profiles) == 2:
        headers += ['baseline (avg)', 'baseline (med)']
    elif len(profiles) > 2:
        for i in range(1, len(profiles)+1):
            headers += [f"base{i} (avg)", f"base{i} (med)"]

    data = []
    for col in columns:
        row = [col]
        for i in range(0, len(profiles)):
            row += [averages[i][col]]
            row += [medians[i][col]]
        data.append(row)

    print(tabulate.tabulate(data, headers=headers, floatfmt=".3f"))

