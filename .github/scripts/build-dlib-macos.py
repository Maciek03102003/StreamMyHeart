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

binary_dir_path = os.path.join(parent_dir_path, "binary")
print(f"Binary directory path is {binary_dir_path}")

dlib_out_dir_path = os.path.join(binary_dir_path, "dlib-19.24.6-macos")
print(f"dlib output directory path is {dlib_out_dir_path}")

if os.path.exists(dlib_build_path):
    print(f"Output directory {dlib_build_path} already contains files. Keep it.")
else:
    if not os.path.exists(dlib_src_dir_path):
        download_and_extract_tarball(REPO_URL, TARBALL_NAME, EXTRACTED_DIR_NAME, dlib_src_dir_path)
    
    os.makedirs(dlib_build_path)
    os.chdir(dlib_build_path)
    
    cmake_configure_cmd = [
        "cmake", 
        "-G", "Unix Makefiles",
        "-DCMAKE_OSX_ARCHITECTURES='x86_64;arm64'",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SHARED_LIBS=OFF", 
        # f"-DCMAKE_INSTALL_PREFIX={dlib_out_dir_path}",
        "-DLIB_USE_MKL_FFT=ON",
        "-DLIB_USE_MKL_SEQUENTIAL=OFF",
        "-DLIB_USE_MKL_WITH_TBB=ON",
        "-DLIB_USE_CUDA=ON",
        "-DLIB_USE_BLAS=ON",
        "-DLIB_USE_LAPACK=ON",
        "-DLIB_JPEG_SUPPORT=ON",
        "-DLIB_PNG_SUPPORT=ON",
        "-DLIB_WEBP_SUPPORT=ON",
        "-DLIB_ENABLE_ASSERTS=OFF",
        "-DLIB_ENABLE_STACK_TRACE=OFF",
        "-DLIB_USE_CUDA_COMPUTE_CAPABILI=50",
        ".."
    ]
    
    # Used to disable errors: -DCMAKE_CXX_FLAGS="-Wno-newline-eof -Wno-comma -Wno-error"
    subprocess.run(cmake_configure_cmd, check=True)
    subprocess.run(["cmake", "--build", ".", "--config", "Release"], check=True)    
    # subprocess.run(["cmake", "--build", ".", "--target", "install", "--config", "Release"], check=True)    