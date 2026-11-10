"""Check Windows/Linux install rule generation without executing those apps."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--cxx", required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH",
                   "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH",
                   "CMAKE_INSTALL_MODE", "DESTDIR"} or key.startswith(
                       ("LVRS_", "QT_", "QML", "DYLD_", "LD_")):
            env.pop(key, None)
    with tempfile.TemporaryDirectory(prefix="native-deployment-platforms-", dir=args.build) as temp:
        for platform in ("Linux", "Windows"):
            for bad_sign in (False, True):
                target = Path(temp) / platform / ("invalid/build" if bad_sign else "valid/build")
                command = [args.cmake, "-S", str(source / "tests/native_deployment_platforms"),
                           "-B", str(target), "-G", "Ninja", f"-DCMAKE_SYSTEM_NAME={platform}",
                           f"-DCMAKE_CXX_COMPILER={args.cxx}", "-DCMAKE_CXX_COMPILER_WORKS=TRUE",
                           "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY", "-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON",
                           f"-DLVUI_AOT_MODULE={source / 'cmake/LVUIAot.cmake'}"]
                if bad_sign:
                    command.append("-DBAD_SIGN=ON")
                result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
                if bad_sign:
                    if result.returncode == 0 or "only available for macOS" not in result.stdout:
                        raise AssertionError(f"{platform} accepted macOS signing: {result.stdout}")
                else:
                    if result.returncode:
                        raise AssertionError(f"{platform} configuration failed: {result.stdout}")
                    script = (target / "cmake_install.cmake").read_text()
                    if (f"set(LVUI_DEPLOY_PLATFORM [==[{platform}]==])" not in script or
                            "LVUIInstallApplication.cmake" not in script):
                        raise AssertionError(f"{platform} install script was not generated")
    print("Windows and Linux install rules configured; binaries were not built or run")


if __name__ == "__main__":
    main()
