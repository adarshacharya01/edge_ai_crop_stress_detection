"""Train on measured, labelled readings and export a small C inference model."""
import argparse
import csv
from pathlib import Path

import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import balanced_accuracy_score
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler


def load_data(path):
    features, labels = [], []
    with path.open(newline="") as file:
        reader = csv.DictReader(file)
        required = {"soil_pct", "temp_c", "rh_pct", "lux", "stress"}
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise ValueError(f"CSV columns must include: {', '.join(sorted(required))}")
        for row in reader:
            soil = float(row["soil_pct"])
            temp = float(row["temp_c"])
            rh = float(row["rh_pct"])
            lux = float(row["lux"])
            label = int(row["stress"])
            if label not in (0, 1) or not (0 <= soil <= 100) or not (0 <= rh <= 100) or lux < 0:
                raise ValueError("Invalid label, soil percentage, humidity, or light value")
            features.append([soil, temp, rh, np.log1p(lux)])
            labels.append(label)
    X = np.asarray(features, dtype=float)
    y = np.asarray(labels, dtype=int)
    if X.size == 0 or not np.isfinite(X).all() or min(np.bincount(y, minlength=2)) < 8:
        raise ValueError("Collect at least 8 finite readings for each class")
    return X, y


def export_header(output, scaler, model):
    def values(items):
        return ", ".join(f"{float(item):.9e}f" for item in items)

    content = (
        "#ifndef CROP_MODEL_PARAMS_H\n#define CROP_MODEL_PARAMS_H\n"
        f"static const float MODEL_MEAN[4] = {{{values(scaler.mean_)}}};\n"
        f"static const float MODEL_SCALE[4] = {{{values(scaler.scale_)}}};\n"
        f"static const float MODEL_W[4] = {{{values(model.coef_[0])}}};\n"
        f"static const float MODEL_B = {float(model.intercept_[0]):.9e}f;\n"
        "#endif\n"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Labelled CSV readings")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[1] / "Model" / "model_params.h")
    args = parser.parse_args()
    X, y = load_data(args.input)
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, stratify=y, random_state=42
    )
    scaler = StandardScaler().fit(X_train)
    model = LogisticRegression(class_weight="balanced", max_iter=1000)
    model.fit(scaler.transform(X_train), y_train)
    print("Held-out balanced accuracy:",
          balanced_accuracy_score(y_test, model.predict(scaler.transform(X_test))))
    scaler.fit(X)
    model.fit(scaler.transform(X), y)
    export_header(args.output, scaler, model)
    print("Wrote", args.output)


if __name__ == "__main__":
    main()
