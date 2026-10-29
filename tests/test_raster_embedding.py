import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("embed_raster", ROOT / "tools/embed_raster.py")
embedding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedding)


class RasterEmbeddingTests(unittest.TestCase):
    def test_design_asset_is_valid_and_deterministic(self):
        data = (ROOT / "resources/images/color-picker-hue-ring.png").read_bytes()
        first = embedding.render(data)
        self.assertEqual(first, embedding.render(data))
        self.assertIn(f"array<std::uint8_t, {len(data)}>", first)

    def test_invalid_or_corrupted_image_is_rejected(self):
        data = (ROOT / "resources/images/color-picker-hue-ring.png").read_bytes()
        for invalid in [b"not a png", data[:-1], data[:30] + bytes([data[30] ^ 1]) + data[31:]]:
            with self.subTest(size=len(invalid)), self.assertRaises(ValueError):
                embedding.render(invalid)


if __name__ == "__main__":
    unittest.main()
