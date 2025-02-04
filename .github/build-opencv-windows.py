import os
import subprocess
import tarfile
import shutil

# Constants
REPO_URL = "https://github.com/opencv/opencv/archive/refs/tags/4.11.0.tar.gz"
TARBALL_NAME = "opencv-4.11.0.tar.gz"
EXTRACTED_DIR_NAME = "opencv-4.11.0"

def download_opencv_source_code(target_dir):
    # Ensure the target directory exists
    os.makedirs(os.path.dirname(target_dir), exist_ok=True)

    print(f"Downloading repository tarball from {REPO_URL}...")
    download_opencv_cmd = ["curl", "-L", "-o", f"./{TARBALL_NAME}", REPO_URL]
    subprocess.run(download_opencv_cmd, check=True)

    print(f"Extracting {TARBALL_NAME}...")
    with tarfile.open(TARBALL_NAME, "r:gz") as tar:
        tar.extractall(path=os.path.dirname(target_dir))

    os.rename(os.path.join(os.path.dirname(target_dir), EXTRACTED_DIR_NAME), target_dir)
    print(f"Repository extracted to {target_dir}")

    # Clean up the tarball
    os.remove(TARBALL_NAME)
    print(f"Removed tarball {TARBALL_NAME}")

def cmake_build():
    cmake_configure_cmd = [
        "cmake",
        "-G", "Visual Studio 17 2022",
        "-T", "host=x64",
        # f"-DCMAKE_OSX_ARCHITECTURES='x86_64;arm64'",
        "-DCMAKE_CXX_FLAGS=/MT",
        "-DCMAKE_CXX_FLAGS_DEBUG=/MTd",
        "-DCMAKE_CXX_FLAGS_RELEASE=/MT",
        "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MT",
        "-DCMAKE_CXX_FLAGS_MINSIZEREL=/MT",
        f"-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={opencv_out_dir_path}",
        f"-DBUILD_CUDA_STUBS=OFF",
        f"-DBUILD_DOCS=OFF",
        f"-DBUILD_EXAMPLES=OFF",
        f"-DBUILD_FAT_JAVA_LIB=OFF",
        f"-DBUILD_ITT=OFF",
        f"-DBUILD_JASPER=OFF",
        f"-DBUILD_JAVA=OFF",
        f"-DBUILD_JPEG=OFF",
        f"-DBUILD_OPENEXR=OFF",
        f"-DBUILD_OPENJPEG=OFF",
        f"-DBUILD_PERF_TESTS=OFF",
        f"-DBUILD_PNG=OFF",
        f"-DBUILD_PROTOBUF=OFF",
        f"-DBUILD_SHARED_LIBS=OFF",
        f"-DBUILD_TBB=OFF",
        f"-DBUILD_IPP_IW=OFF",
        f"-DWITH_IPP=OFF",
        f"-DBUILD_TESTS=OFF",
        f"-DBUILD_TIFF=OFF",
        f"-DBUILD_WEBP=OFF",
        f"-DBUILD_ZLIB=OFF",
        f"-DBUILD_opencv_apps=OFF",
        f"-DBUILD_opencv_dnn=OFF",
        f"-DBUILD_opencv_java_bindings_generator=OFF",
        f"-DBUILD_opencv_js_bindings_generator=OFF",
        f"-DBUILD_opencv_objc_bindings_generator=OFF",
        f"-DBUILD_opencv_python_bindings_generator=OFF",
        f"-DBUILD_opencv_python_tests=OFF",
        f"-DBUILD_opencv_ts=OFF",
        f"-DBUILD_opencv_world=OFF",
        f"-DCPU_BASELINE=''",
        f"-DCPU_DISPATCH=''",
        f"-DENABLE_LIBJPEG_TURBO_SIMD=OFF",
        f"-DOPENCL_FOUND=OFF",
        f"-DOPENCV_DNN_CUDA=OFF",
        f"-DOPENCV_DNN_OPENCL=OFF",
        f"-DOPENCV_DNN_OPENVINO=OFF",
        f"-DOPENCV_DNN_PERF_CAFFE=OFF",
        f"-DOPENCV_DNN_TFLITE=OFF",
        f"-DOPENCV_TEST_DNN_TFLITE=OFF",
        f"-DPNG_ARM_NEON=OFF",
        f"-DWITH_ADE=OFF",
        f"-DWITH_AVFOUNDATION=OFF",
        f"-DWITH_AVIF=OFF",
        f"-DWITH_CANN=OFF",
        f"-DWITH_EIGEN=OFF",
        f"-DWITH_CLP=OFF",
        f"-DWITH_CUDA=OFF",
        f"-DWITH_FFMPEG=OFF",
        f"-DWITH_FLATBUFFERS=OFF",
        f"-DWITH_FREETYPE=OFF",
        f"-DWITH_GDAL=OFF",
        f"-DWITH_GDCM=OFF",
        f"-DWITH_CAROTENE=OFF",
        f"-DWITH_GPHOTO2=OFF",
        f"-DWITH_GSTREAMER=OFF",
        f"-DWITH_HALIDE=OFF",
        f"-DWITH_IMGCODEC_GIF=OFF",
        f"-DWITH_IMGCODEC_HDR=OFF",
        f"-DWITH_IMGCODEC_PFM=OFF",
        f"-DWITH_IMGCODEC_PXM=OFF",
        f"-DWITH_IMGCODEC_SUNRASTER=OFF",
        f"-DWITH_ITT=OFF",
        f"-DWITH_JASPER=OFF",
        f"-DWITH_JPEG=OFF",
        f"-DWITH_JPEGXL=OFF",
        f"-DWITH_ONNX=OFF",
        f"-DWITH_OPENCL=OFF",
        f"-DWITH_OPENCLAMDBLAS=OFF",
        f"-DWITH_OPENCLAMDFFT=OFF",
        f"-DWITH_OPENCL_SVM=OFF",
        f"-DWITH_OPENEXR=OFF",
        f"-DWITH_OPENGL=OFF",
        f"-DWITH_OPENJPEG=OFF",
        f"-DWITH_OPENMP=OFF",
        f"-DWITH_OPENNI=OFF",
        f"-DWITH_OPENNI2=OFF",
        f"-DWITH_OPENVINO=OFF",
        f"-DWITH_OPENVX=OFF",
        f"-DWITH_PLAIDML=OFF",
        f"-DWITH_PNG=OFF",
        f"-DWITH_PROTOBUF=OFF",
        f"-DWITH_QT=OFF",
        f"-DWITH_QUIRC=OFF",
        f"-DWITH_SPNG=OFF",
        f"-DWITH_TBB=OFF",
        f"-DWITH_TIFF=OFF",
        f"-DWITH_TIMVX=OFF",
        f"-DWITH_VTK=OFF",
        f"-DWITH_VULKAN=OFF",
        f"-DWITH_WEBNN=OFF",
        f"-DWITH_WEBP=OFF",
        f"-DWITH_XIMEA=OFF",
        f"-DWITH_ZLIB=OFF",
        f"-DWITH_ZLIB_NG=OFF",
        f"-DWITH_LAPACK=OFF",
        f"-DWITH_OBSENSOR=OFF",
        f"-Dold-jpeg=OFF",
        f"..",
    ]

    # Run cmake configure
    subprocess.run(cmake_configure_cmd, check=True)
    build_cmd = ["cmake", "--build", ".", "--target", "install", "--config", "Release"]
    subprocess.run(build_cmd, check=True)

# Build OpenCV
parent_dir_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
print(f"Parent directory path is {parent_dir_path}")

third_party_dir_path = os.path.join(parent_dir_path, "3rdparty")
print(f"Third party directory path is {third_party_dir_path}")

opencv_src_dir_path = os.path.join(third_party_dir_path, "opencv-4.11.0")
print(f"OpenCV source directory path is {opencv_src_dir_path}")

binary_dir_path = os.path.join(parent_dir_path, "binary")
print(f"Binary directory path is {binary_dir_path}")

opencv_out_dir_path = os.path.join(binary_dir_path, "opencv-4.11.0-macos")
print(f"OpenCV output directory path is {opencv_out_dir_path}")

opencv_tmp_dir_path = os.path.join(opencv_src_dir_path, ".build")
print(f"OpenCV temporary directory path is {opencv_tmp_dir_path}")

# Ensure the output directory exists
if os.path.exists(opencv_out_dir_path):
    print(f"Output directory {opencv_out_dir_path} already contains files. Keep it.")
else:
    os.makedirs(opencv_out_dir_path)
    download_opencv_source_code(opencv_src_dir_path)

    # Have a temporary directory for building OpenCV
    if os.path.exists(opencv_tmp_dir_path):
        print(f"Temporary directory {opencv_tmp_dir_path} already exists. Removing it.")
        shutil.rmtree(opencv_tmp_dir_path)
    os.makedirs(opencv_tmp_dir_path)
    # cd to the temporary directory
    os.chdir(opencv_tmp_dir_path)

    cmake_build()

    # Remove the temporary directory and downloaded source
    # shutil.rmtree(third_party_dir_path)
