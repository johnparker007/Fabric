import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class FabricPublicContractTests(unittest.TestCase):
    def test_public_header_is_neutral_and_compiles_as_c(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("cc is unavailable")
        header = (ROOT / "include/fabric/fabric.h").read_text()
        for forbidden in ("amber/", "JPM", "System6", "Oasis"):
            self.assertNotIn(forbidden, header)
        source = '#include "fabric/fabric.h"\nint main(void) { return FABRIC_OK; }\n'
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "contract.c"
            path.write_text(source)
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"),
                 str(path), "-o", str(path.with_suffix(""))], check=True
            )

    def test_transport_details_are_not_public(self):
        header = (ROOT / "include/fabric/fabric.h").read_text().lower()
        for forbidden in ("loadlibrary", "dlopen", "process_handle", "grpc", "json"):
            self.assertNotIn(forbidden, header)


if __name__ == "__main__":
    unittest.main()
