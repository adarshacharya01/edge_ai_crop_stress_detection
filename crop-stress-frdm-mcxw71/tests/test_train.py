import csv
import tempfile
import unittest
from pathlib import Path

from tools.train import export_header, load_data
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler


class TrainingTests(unittest.TestCase):
    def test_load_and_export(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "readings.csv"
            with path.open("w", newline="") as file:
                writer = csv.writer(file)
                writer.writerow(["soil_pct", "temp_c", "rh_pct", "lux", "stress"])
                for i in range(10):
                    writer.writerow([70 + i, 24, 60, 400, 0])
                    writer.writerow([10 + i, 33, 30, 1100, 1])
            X, y = load_data(path)
            self.assertEqual(X.shape, (20, 4))
            scaler = StandardScaler().fit(X)
            model = LogisticRegression().fit(scaler.transform(X), y)
            output = Path(directory) / "model_params.h"
            export_header(output, scaler, model)
            self.assertIn("MODEL_W[4]", output.read_text())

    def test_rejects_missing_columns(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.csv"
            path.write_text("soil_pct,stress\n10,1\n")
            with self.assertRaisesRegex(ValueError, "CSV columns"):
                load_data(path)


if __name__ == "__main__":
    unittest.main()
