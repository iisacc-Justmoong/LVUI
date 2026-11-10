from pathlib import Path
import unittest

from workspace_support import checked_workspace, provider


class WorkspaceValidationTest(unittest.TestCase):
    def test_accepts_runner_owned_workspace(self):
        self.assertEqual(checked_workspace(Path.cwd()), Path.cwd().resolve())

    def test_rejects_parent_and_nested_directory(self):
        with self.assertRaises(ValueError):
            checked_workspace(Path.cwd().parent)
        child = Path.cwd() / "nested"
        provider.mkdir(child)
        with self.assertRaises(ValueError):
            checked_workspace(child)

    def test_rejects_symlink(self):
        link = Path.cwd() / "link"
        try:
            link.symlink_to(Path.cwd(), target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Cannot create a symbolic link: {error}")
        with self.assertRaises(ValueError):
            checked_workspace(link)


if __name__ == "__main__":
    unittest.main()
