# 🌐 Installation of Cuda on Windows 11 Systems

## 📦 Installation

For optimal functionality, we recommend the following versions:

| Software | Version | Download Link | Purpose |
|----------|---------|---------------|---------|
| Cuda Toolkit | 12.5 | [Download](https://developer.download.nvidia.com/compute/cuda/12.5.0/local_installers/cuda_12.5.0_555.85_windows.exe) | Enables installation of OpenCV with CUDA libraries |
| cuDNN (Optional) | 9.3 | [Download](https://developer.download.nvidia.com/compute/cudnn/9.3.0/local_installers/cudnn_9.3.0_windows.exe) | Accelerates deep learning operations (check compatibility) |
| CMake | 3.30.2 | [Download](https://github.com/Kitware/CMake/releases/download/v3.30.2/cmake-3.30.2-windows-x86_64.msi) | Necessary for OpenCV installation |

### OpenCV with CUDA

To enable CUDA support in OpenCV, you'll need to compile OpenCV from source. Follow these steps:

1. **Download Required Files**
    - OpenCV 4.10.0: [Download](https://codeload.github.com/opencv/opencv/zip/refs/tags/4.10.0)
    - OpenCV Contrib 4.10.0: [Download](https://codeload.github.com/opencv/opencv_contrib/zip/refs/tags/4.10.0)

2. **Prepare Directories**
    - Extract both archives to `C:\Program Files\OpenCV`
    - Create `build` and `install` directories inside this location

3. **Configure with CMake**
    - Open CMake GUI
    - Set source code path to the extracted `opencv-4.10.0` directory
    - Set build path to `C:\Program Files\OpenCV\build`
    - Check "Grouped" ☑
    - Click "Configure"
        - Specify Visual Studio as the generator
        - Select x64 as the platform

4. **Set CMake Parameters**
   Set the following parameters in CMake:

   | Parameter | Value | Notes |
      |-----------|-------|-------|
   | `WITH_CUDA` | ON | |
   | `OPENCV_DNN_CUDA` | ON | |
   | `BUILD_opencv_dnn` | ON | |
   | `ENABLE_FAST_MATH` | ON | |
   | `BUILD_opencv_world` | ON | Optional, for single library file |
   | `OPENCV_EXTRA_MODULES_PATH` | Path to `opencv_contrib-4.10.0\modules` | |
   | `CUDA_ARCH_BIN` | Your GPU architecture (e.g., 8.9 for RTX 4090) | See note below |
   | `CUDA_FAST_MATH` | ON | Optional, for performance improvement |
   | `CMAKE_INSTALL_PREFIX` | `C:\Program Files\OpenCV\install` | |
   | `CMAKE_CONFIGURATION_TYPES` | Release | |

   > **Note:** To find the correct value for `CUDA_ARCH_BIN`, refer to the [NVIDIA CUDA GPUs](https://developer.nvidia.com/cuda-gpus) list. Find your GPU model and use the "Compute Capability" value.

   > ⚠️ **Note:** In case some of the parameters are missing, reconfigure using the Configure Button after after seeting WITH_CUDA to on

    - Click "Configure" again after setting these parameters
    - Click "Generate" to create the project files

5. **Build and Install**
    - Open Windows Command Prompt as Administrator
    - Run the following command:
```
"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Program Files\OpenCV\build" --target INSTALL --config Release
``` 
> ⚠️ **Note:** Building OpenCV with CUDA support can be a complex process. If you encounter issues, refer to the official OpenCV documentation or seek help in the OpenCV forums.
