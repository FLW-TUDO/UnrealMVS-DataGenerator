# Unreal Synthetic Data Generator

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-brightgreen.svg)

## Overview

UE5 plugin for generating synthetic training data for AI/ML:
- 360° Equirectangular Images
- Depth Maps
- Fisheye Images (multiple projection types)
- ROS bag export (`.mcap`) with camera poses — ⚠️ *experimental, not fully tested*

Uses six cameras to produce seamless 360° captures with precise depth data.

> ⚠️ **Note:** This plugin has been developed and tested exclusively for Windows systems.
> 
- **UnrealMVS:** [FLW-TUDO/UnrealMVS](https://github.com/FLW-TUDO/UnrealMVS) — the Neural network for depth estimation to confirm the usability of the here generated data.
- **Datasets (HuggingFace):** [flw-tu-dortmund/UnrealMVS](https://huggingface.co/datasets/flw-tu-dortmund/UnrealMVS) — the datasets used in this project.

## Installation

### Prerequisites

Enable OpenEXR support in OpenCV:

1. Open Windows "Environment Variables" (Start → search "environment variables")
2. Add new **System Variable**:
   - Name: `OPENCV_IO_ENABLE_OPENEXR`
   - Value: `true`
3. Restart your IDE/Editor

### Option A: Project Plugin (Recommended)

1. Create or open a C++ UE5 project
2. Create a `Plugins` folder in your project directory
3. Clone this repository **with submodules** into `Plugins/`:
   ```
   cd YourProject/Plugins
   git clone --recursive https://github.com/FLW-TUDO/UnrealMVS-DataGenerator.git UnrealMVS-DataGenerator
   ```
   > If you already cloned without `--recursive`, run:
   > ```
   > git submodule update --init --recursive
   > ```
4. Open the project in UE5 - the plugin builds automatically
5. Verify in Edit → Plugins → search "OmniRGBDCamera"

### Option B: Build Plugin Standalone

Build the plugin without opening the Editor:

```batch
"<UE5_INSTALL_PATH>\Engine\Build\BatchFiles\RunUAT.bat" ^
    BuildPlugin ^
    -Plugin="PATH\TO\OmniRGBDCamera\OmniRGBDCamera.uplugin" ^
    -Package="OUTPUT\PATH" ^
    -TargetPlatforms=Win64
```

> `<UE5_INSTALL_PATH>` is typically `C:\Program Files\Epic Games\UE_5.X`

## Output Examples

| Equirectangular                                 | Depth Map                   |
| ----------------------------------------------- | --------------------------- |
| ![Equirectangular](/images/Equirectangular.png) | ![Depth](/images/Depth.png) |

| Fisheye Projections                         |
| ------------------------------------------- |
| ![Fisheye](/images/Fisheye_Projections.png) |

> Sample images use Unreal Engine demo scenes for illustration only.

## Citation

This work has been accepted for **ETFA 2026**. If you use UnrealMVS in your research, please cite it.

> **Note:** The DOI will be added here once it is available.

```bibtex
K. P. Rösner, J. Rutinowski, H. Surmann and A. Kirchheim, "UnrealMVS: A Generator for Synthetic Multi-Fisheye Omnidirectional Depth Data," 2026 IEEE 31st International Conference on Emerging Technologies and Factory Automation (ETFA), Vasteras, Sweden, 2026, pp. 01-08, doi: 10.1109/ETFA68377.2026.11725285. keywords: {Modeling;Cameras;Printing;Pipelines;Training;Rendering (computer graphics);Image sensors;Calibration;Conferences;Urban areas;Synthetic Datasets;Equirectangular Images;Depth Images;Multi-View Stereo;Fisheye Cameras},
```

---

## Acknowledgements

This project was created in cooperation between **TU Dortmund University** and
**Westphalian University of Applied Sciences (WHS)**.

<p align="center">
  <img src="docs/assets/tu.png" height="120" alt="TU Dortmund University">
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/assets/wh.png" height="120" alt="Westphalian University of Applied Sciences">
</p>
