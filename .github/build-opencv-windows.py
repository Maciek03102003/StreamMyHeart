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
        "-DBUILD_WITH_STATIC_CRT=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={opencv_out_dir_path}",
        "-DBUILD_CUDA_STUBS=OFF",
        "-DBUILD_DOCS=OFF",
        "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_FAT_JAVA_LIB=OFF",
        "-DBUILD_ITT=OFF",
        "-DBUILD_JASPER=OFF",
        "-DBUILD_JAVA=OFF",
        "-DBUILD_JPEG=OFF",
        "-DBUILD_OPENEXR=OFF",
        "-DBUILD_OPENJPEG=OFF",
        "-DBUILD_PERF_TESTS=OFF",
        "-DBUILD_PNG=OFF",
        "-DBUILD_PROTOBUF=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_TBB=OFF",
        "-DBUILD_IPP_IW=OFF",
        "-DWITH_IPP=OFF",
        "-DBUILD_TESTS=OFF",
        "-DBUILD_TIFF=OFF",
        "-DBUILD_WEBP=OFF",
        "-DBUILD_ZLIB=OFF",
        "-DBUILD_opencv_apps=OFF",
        "-DBUILD_opencv_dnn=ON",
        "-DBUILD_opencv_java_bindings_generator=OFF",
        "-DBUILD_opencv_js_bindings_generator=OFF",
        "-DBUILD_opencv_objc_bindings_generator=OFF",
        "-DBUILD_opencv_python_bindings_generator=OFF",
        "-DBUILD_opencv_python_tests=OFF",
        "-DBUILD_opencv_ts=OFF",
        "-DBUILD_opencv_world=OFF",
        "-DCPU_BASELINE=''",
        "-DCPU_DISPATCH=''",
        "-DENABLE_LIBJPEG_TURBO_SIMD=OFF",
        "-DOPENCL_FOUND=OFF",
        "-DOPENCV_DNN_CUDA=OFF",
        "-DOPENCV_DNN_OPENCL=ON",
        "-DOPENCV_DNN_OPENVINO=OFF",
        "-DOPENCV_DNN_PERF_CAFFE=OFF",
        "-DOPENCV_DNN_TFLITE=ON",
        "-DOPENCV_TEST_DNN_TFLITE=OFF",
        "-DPNG_ARM_NEON=OFF",
        "-DWITH_ADE=OFF",
        "-DWITH_AVFOUNDATION=OFF",
        "-DWITH_AVIF=OFF",
        "-DWITH_CANN=OFF",
        "-DWITH_EIGEN=OFF",
        "-DWITH_CLP=OFF",
        "-DWITH_CUDA=OFF",
        "-DWITH_FFMPEG=OFF",
        "-DWITH_FLATBUFFERS=OFF",
        "-DWITH_FREETYPE=OFF",
        "-DWITH_GDAL=OFF",
        "-DWITH_GDCM=OFF",
        "-DWITH_CAROTENE=OFF",
        "-DWITH_GPHOTO2=OFF",
        "-DWITH_GSTREAMER=OFF",
        "-DWITH_HALIDE=OFF",
        "-DWITH_IMGCODEC_GIF=OFF",
        "-DWITH_IMGCODEC_HDR=OFF",
        "-DWITH_IMGCODEC_PFM=OFF",
        "-DWITH_IMGCODEC_PXM=OFF",
        "-DWITH_IMGCODEC_SUNRASTER=OFF",
        "-DWITH_ITT=OFF",
        "-DWITH_JASPER=OFF",
        "-DWITH_JPEG=OFF",
        "-DWITH_JPEGXL=OFF",
        "-DWITH_ONNX=OFF",
        "-DWITH_OPENCL=OFF",
        "-DWITH_OPENCLAMDBLAS=OFF",
        "-DWITH_OPENCLAMDFFT=OFF",
        "-DWITH_OPENCL_SVM=OFF",
        "-DWITH_OPENEXR=OFF",
        "-DWITH_OPENGL=OFF",
        "-DWITH_OPENJPEG=OFF",
        "-DWITH_OPENMP=OFF",
        "-DWITH_OPENNI=OFF",
        "-DWITH_OPENNI2=OFF",
        "-DWITH_OPENVINO=OFF",
        "-DWITH_OPENVX=OFF",
        "-DWITH_PLAIDML=OFF",
        "-DWITH_PNG=OFF",
        "-DWITH_PROTOBUF=OFF",
        "-DWITH_QT=OFF",
        "-DWITH_QUIRC=OFF",
        "-DWITH_SPNG=OFF",
        "-DWITH_TBB=OFF",
        "-DWITH_TIFF=OFF",
        "-DWITH_TIMVX=OFF",
        "-DWITH_VTK=OFF",
        "-DWITH_VULKAN=OFF",
        "-DWITH_WEBNN=OFF",
        "-DWITH_WEBP=OFF",
        "-DWITH_XIMEA=OFF",
        "-DWITH_ZLIB=OFF",
        "-DWITH_ZLIB_NG=OFF",
        "-DWITH_LAPACK=OFF",
        "-DWITH_OBSENSOR=OFF",
        "-DWITH_MSMF=ON",
        "-DWITH_MSMF_DXVA=ON",
        "-DWITH_DIRECTML=ON",
        "-DWITH_DIRECTX=ON",
        "-DWITH_DSHOW=ON",
        "-Dold-jpeg=OFF",
        "-DUSE_WIN32_FILEIO=ON",
        "-DOPENCV_ENABLE_ATOMIC_LONG_LONG=ON",
        "-DOPENCV_MSVC_PARALLEL=ON",
        "..",
    ]

    # Run cmake configure
    subprocess.run(cmake_configure_cmd, check=True)
    build_cmd = ["cmake", "--build", ".", "--target", "install", "--config", "Release"]
    subprocess.run(build_cmd, check=True)
    
    # Verify MD using command line: dumpbin /headers opencv_core.lib | findstr "Runtime"
    verify_cmd = ["dumpbin", "/headers", "opencv_core.lib", "|", "findstr", "Runtime"]
    subprocess.run(verify_cmd, check=True)

# Build OpenCV
parent_dir_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
print(f"Parent directory path is {parent_dir_path}")

third_party_dir_path = os.path.join(parent_dir_path, "3rdparty")
print(f"Third party directory path is {third_party_dir_path}")

opencv_src_dir_path = os.path.join(third_party_dir_path, "opencv-4.11.0")
print(f"OpenCV source directory path is {opencv_src_dir_path}")

binary_dir_path = os.path.join(parent_dir_path, "binary")
print(f"Binary directory path is {binary_dir_path}")

opencv_out_dir_path = os.path.join(binary_dir_path, "opencv-4.11.0-windows")
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
    shutil.rmtree(opencv_src_dir_path)