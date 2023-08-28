import tabulate
import pandas
import sys
import numpy

def load_dataframe(path):
    """Loads from @path pandas dataframe and computes averages and medians in each metric

    :param path: path to csv file delimited by ;
    :return: averages and medians of each column
    """
    def transform(cell):
        if cell == 'TO' or cell == 'ERR':
            return numpy.nan
        else:
            try:
                return float(cell)
            except ValueError:
                return cell
    df = pandas.read_csv(path, sep=';')
    timeouts = {
        col: df[col].value_counts()['TO'] for col in df.columns if col.endswith('runtime')
    }
    df = df.applymap(transform).drop(columns=['name'])
    avgs = df.mean(numeric_only=True, skipna=True)
    meds = df.median(numeric_only=True, skipna=True)
    return avgs, meds, timeouts


if __name__ == "__main__":
    profiles = sys.argv[1:]
    if len(profiles) == 0:
        printf(f"usage: compare_profiles.py [target.csv baseline1.csv ... baselinen.csv]")

    averages, medians, timeouts = [], [], []
    columns, to_columns = set(), set()
    for profile in profiles:
        avg, med, tos = load_dataframe(profile)
        columns.update(list(avg.keys()))
        columns.update(list(avg.keys()))
        averages.append(avg)
        medians.append(med)
        timeouts.append(tos)
        print(tos)
        to_columns.update(list(tos.keys()))

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
    data = sorted(data, key=lambda x: x[0])

    print(tabulate.tabulate(data, headers=headers, floatfmt=".3f"))

    print()

    # Timeout summary
    headers = ['metric'] + ['target (timeouts)']
    if len(profiles) == 2:
        headers += ['baseline (timeouts)']
    elif len(profiles) > 2:
        for i in range(1, len(profiles)+1):
            headers += [f"base{i} (timeouts)"]

    data = []
    for col in to_columns:
        row = [col]
        for i in range(0, len(profiles)):
            row += [int(timeouts[i][col])]
        data.append(row)
    data = sorted(data, key=lambda x: x[0])

    print(tabulate.tabulate(data, headers=headers, floatfmt=".3f"))
