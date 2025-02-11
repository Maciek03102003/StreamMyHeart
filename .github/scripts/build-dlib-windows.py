from utils import download_and_extract_tarball

import os
import subprocess

REPO_URL = "https://github.com/davisking/dlib/archive/refs/tags/v19.24.6.tar.gz"
TARBALL_NAME = "dlib-19.24.6.tar.gz"
EXTRACTED_DIR_NAME = "dlib-19.24.6"

parent_dir_path = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
print(f"Parent directory path is {parent_dir_path}")

third_party_dir_path = os.path.join(parent_dir_path, "3rdparty")
print(f"Third party directory path is {third_party_dir_path}")

dlib_src_dir_path = os.path.join(third_party_dir_path, EXTRACTED_DIR_NAME)
print(f"dlib source directory path is {dlib_src_dir_path}")

dlib_build_path = os.path.join(dlib_src_dir_path, "build")
print(f"dlib build directory path is {dlib_build_path}")

if os.path.exists(dlib_build_path):
    print(f"Output directory {dlib_build_path} already contains files. Keep it.")
else:
    if not os.path.exists(dlib_src_dir_path):
        download_and_extract_tarball(
            REPO_URL, TARBALL_NAME, EXTRACTED_DIR_NAME, dlib_src_dir_path
        )

    os.makedirs(dlib_build_path)
    os.chdir(dlib_build_path)

    subprocess.run(
        [
            "cmake",
            "-G", "Visual Studio 17 2022",
            "-T", "host=x64",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DDLIB_USE_CUDA=OFF",
            "-DCMAKE_INSTALL_PREFIX=../install",
            "-DBUILD_SHARED_LIBS=OFF",
            "-DDLIB_FORCE_MSVC_STATIC_RUNTIME=OFF",
            "-DCMAKE_CXX_FLAGS=/MD",
            "-DCMAKE_CXX_FLAGS_DEBUG=/MDd",
            "-DCMAKE_CXX_FLAGS_RELEASE=/MD",
            "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MD",
            "-DCMAKE_CXX_FLAGS_MINSIZEREL=/MD",
            "..",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", ".", "--target", "install", "--config", "Release"],
        check=True,
    )
    # print where the library is installed
    print(f"Dlib is installed to: {os.getcwd()}")
    result = subprocess.run(
        ["cmd", "/c", "dir", "/s", "/b", "dlib19*.lib"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=dlib_build_path,
    )
    
    # verify runtime using command 
    # verify_cmd = ["dumpbin", "/headers", "dlib*.lib", "|", "findstr", "Runtime"]
    # subprocess.run(verify_cmd, check=True)