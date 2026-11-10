from pathlib import Path
import errno
import re
import unittest
from unittest.mock import patch

import native_install
import package_workspace as workspace


class PackageWorkspaceTest(unittest.TestCase):
    def test_cleanup_retries_only_transient_nonempty_directories(self):
        class Temporary:
            def __init__(self, failures):
                self.failures = list(failures)
                self.calls = 0
            def cleanup(self):
                self.calls += 1
                if self.failures:
                    raise OSError(self.failures.pop(0), "cleanup failed")

        transient = Temporary([errno.ENOTEMPTY, errno.ENOTEMPTY])
        workspace.cleanup_owned_temporary(transient)
        self.assertEqual(transient.calls, 3)
        denied = Temporary([errno.EACCES])
        with self.assertRaises(OSError) as caught:
            workspace.cleanup_owned_temporary(denied)
        self.assertEqual(caught.exception.errno, errno.EACCES)
        self.assertEqual(denied.calls, 1)
        persistent = Temporary([errno.ENOTEMPTY] * 5)
        with self.assertRaises(OSError) as caught:
            workspace.cleanup_owned_temporary(persistent)
        self.assertEqual(caught.exception.errno, errno.ENOTEMPTY)
        self.assertEqual(persistent.calls, 4)

    def test_relocated_consumer_retries_its_own_temporary_build_cleanup(self):
        class Temporary:
            name = '/project/build/native-consumer-build-owned'
            def __init__(self): self.calls = 0
            def cleanup(self):
                self.calls += 1
                if self.calls == 1: raise OSError(errno.ENOTEMPTY, 'concurrent metadata')
        temporary = Temporary()
        with patch.object(native_install.tempfile, 'TemporaryDirectory', return_value=temporary) as factory:
            with native_install.native_build_directory(Path('/project/build')) as result:
                self.assertEqual(result, temporary.name)
            factory.assert_called_once_with(prefix='native-consumer-build-', dir=Path('/project/build'))
        self.assertEqual(temporary.calls, 2)

    def test_redirects_both_manifests_and_rejects_ambiguous_scripts(self):
        build, owner = Path("/project/build"), Path("/project/resources/.t/abcd")
        text = str(build / "install_local_manifest.txt") + "\n" + str(build / "${CMAKE_INSTALL_MANIFEST}")
        self.assertNotIn(str(build), workspace.redirect_manifest(text, build, owner))
        with self.assertRaises(ValueError):
            workspace.redirect_manifest(text + "\n" + text, build, owner)

    def test_absolute_or_escaping_install_paths_are_rejected(self):
        for value in ("/elsewhere", "../escape", "C:\\outside", "C:relative", "\\outside", "lib\\..\\outside"):
            with self.assertRaises(ValueError):
                workspace.validate_install_dirs("CMAKE_INSTALL_LIBDIR:PATH=" + value)
        workspace.validate_install_dirs("CMAKE_INSTALL_LIBDIR:PATH=lib")

    def test_native_install_uses_safe_relative_directories(self):
        config = ('CMAKE_INSTALL_LIBDIR:PATH=lib64\n'
                  'CMAKE_INSTALL_INCLUDEDIR:PATH=include/sdk\n'
                  'CMAKE_INSTALL_BINDIR:PATH=tools\n'
                  'CMAKE_INSTALL_DATADIR:PATH=share\n')
        self.assertEqual(native_install.install_paths(config),
                         {'lib': Path('lib64'), 'include': Path('include/sdk'), 'bin': Path('tools')})
        self.assertEqual(native_install.install_paths(''),
                         {'lib': Path('lib'), 'include': Path('include'), 'bin': Path('bin')})
        for value in ('../outside', '/outside', 'C:\\outside'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                native_install.install_paths('CMAKE_INSTALL_LIBDIR:PATH=' + value)
        with self.assertRaises(ValueError):
            native_install.install_paths('CMAKE_INSTALL_LIBDIR:PATH=lib\nCMAKE_INSTALL_DATADIR:PATH=../outside')

    def test_image_io_consumer_uses_explicit_sdk_dependencies_only(self):
        cache = {
            'iiGUI_DIR': '/sdk/gui', 'iiFileProviderNative_DIR': '/sdk/files',
            'iiNetwork_DIR': '/sdk/network', 'iiVisualCodec_DIR': '/sdk/codec',
            'iiJson_DIR': '/sdk/json', 'WebP_DIR': '/sdk/webp',
            'OPENSSL_CRYPTO_LIBRARY': '/sdk/libcrypto.a',
            'CURL_LIBRARY_DEBUG': 'CURL_LIBRARY_DEBUG-NOTFOUND',
            'Qt6_DIR': '/legacy/qt', 'CMAKE_PREFIX_PATH': '/unrelated',
        }
        options = native_install.image_io_consumer_options(cache)
        self.assertIn('-DiiJson_DIR=/sdk/json', options)
        self.assertIn('-DWebP_DIR=/sdk/webp', options)
        self.assertIn('-DOPENSSL_CRYPTO_LIBRARY=/sdk/libcrypto.a', options)
        self.assertFalse(any('Qt6_DIR' in option or 'CMAKE_PREFIX_PATH' in option or 'NOTFOUND' in option
                             for option in options))
        del cache['iiNetwork_DIR']
        with self.assertRaisesRegex(ValueError, 'iiNetwork_DIR'):
            native_install.image_io_consumer_options(cache)

    def test_image_io_install_must_match_enabled_configuration(self):
        self.assertTrue(native_install.image_io_consumer_required({'LVUI_BUILD_IMAGE_IO': 'ON'}, True))
        self.assertFalse(native_install.image_io_consumer_required({'LVUI_BUILD_IMAGE_IO': 'OFF'}, False))
        with self.assertRaisesRegex(RuntimeError, 'ImageIO'):
            native_install.image_io_consumer_required({'LVUI_BUILD_IMAGE_IO': 'ON'}, False)
        with self.assertRaisesRegex(RuntimeError, 'ImageIO'):
            native_install.image_io_consumer_required({'LVUI_BUILD_IMAGE_IO': 'OFF'}, True)

    def test_install_heavy_checks_are_serialized_and_have_sufficient_timeout(self):
        cmake = (Path(__file__).resolve().parents[1] / 'CMakeLists.txt').read_text()
        for name in ('NativeInstalled', 'PackageBoundary'):
            match = re.search(r'set_tests_properties\(LVUI\.' + name + r'\s+PROPERTIES\s+([^)]*)\)', cmake)
            self.assertIsNotNone(match, name)
            self.assertRegex(match.group(1), r'RESOURCE_LOCK\s+lvui_install_snapshot')
            timeout = re.search(r'TIMEOUT\s+(\d+)', match.group(1))
            self.assertIsNotNone(timeout, name)
            self.assertGreaterEqual(int(timeout.group(1)), 600)


if __name__ == "__main__":
    unittest.main()
