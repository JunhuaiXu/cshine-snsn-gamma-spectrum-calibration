#!/usr/bin/env python3
"""Find crystal subsets closest to a target high-energy multiplicity count."""

import argparse

import ROOT


CRYSTAL_COUNT = 15
MASK_COUNT = 1 << CRYSTAL_COUNT
TARGET_N1 = 6731
TARGET_N2 = 19


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_root")
    parser.add_argument(
        "--object-name", default="high_core_mask_frequency"
    )
    parser.add_argument("--limit", type=int, default=10)
    return parser.parse_args()


def selected_cores(mask):
    return ",".join(
        str(core)
        for core in range(CRYSTAL_COUNT)
        if mask & (1 << core)
    )


def population_count(mask):
    return bin(mask).count("1")


def main():
    arguments = parse_arguments()
    root_file = ROOT.TFile.Open(arguments.input_root, "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % arguments.input_root)
    histogram = root_file.Get(arguments.object_name)
    if not histogram:
        raise KeyError("Missing %s" % arguments.object_name)

    nonzero_masks = []
    for event_mask in range(MASK_COUNT):
        frequency = int(round(histogram.GetBinContent(event_mask + 1)))
        if frequency:
            nonzero_masks.append((event_mask, frequency))
    root_file.Close()

    results = []
    for selection in range(1, MASK_COUNT):
        count_one = 0
        count_two = 0
        for event_mask, frequency in nonzero_masks:
            multiplicity = population_count(event_mask & selection)
            if multiplicity == 1:
                count_one += frequency
            elif multiplicity == 2:
                count_two += frequency
        results.append((selection, count_one, count_two))

    exact_n2 = sorted(
        (item for item in results if item[2] == TARGET_N2),
        key=lambda item: (abs(item[1] - TARGET_N1), population_count(item[0])),
    )
    overall = sorted(
        results,
        key=lambda item: (
            abs(item[2] - TARGET_N2),
            abs(item[1] - TARGET_N1),
            population_count(item[0]),
        ),
    )

    print("target %d,%d" % (TARGET_N1, TARGET_N2))
    print("closest_with_N2_equal_19")
    for selection, count_one, count_two in exact_n2[: arguments.limit]:
        print("%s %d,%d" % (selected_cores(selection), count_one, count_two))
    print("closest_lexicographic_N2_then_N1")
    for selection, count_one, count_two in overall[: arguments.limit]:
        print("%s %d,%d" % (selected_cores(selection), count_one, count_two))


if __name__ == "__main__":
    main()
