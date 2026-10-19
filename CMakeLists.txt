# AnimStudio - animation creation & conversion for FreeSpace Open formats.
#
# Cross-platform (Windows + Linux) CMake build, replacing the former Qt VS
# Tools .vcxproj. Vendored libpng/zlib/libimagequant/apngasm/apngdisassembler
# and a subset of AMD Compressonator are compiled directly into the exe.

cmake_minimum_required(VERSION 3.21)

project(AnimStudio
    VERSION 1.3.0
    DESCRIPTION "Animation creation and conversion tool for FreeSpace Open formats"
    LANGUAGES C CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

qt_standard_project_setup()

# qt_standard_project_setup() turns on AUTOMOC/AUTOUIC but not AUTORCC; this
# app has a real .qrc of UI images, so compile resources too.
set(CMAKE_AUTORCC ON)

# Vendored C/C++ dependencies, compiled straight into the executable. This is
# the exact subset the old .vcxproj compiled - NOT a glob, because the
# Compressonator tree also contains D3D/Vulkan/GL sources with unguarded
# windows.h that are not part of this build.
set(ANIMSTUDIO_VENDORED
    "AnimStudio/Dependencies/apngasm/apngasm.cpp"
    "AnimStudio/Dependencies/apngasm/apngframe.cpp"
    "AnimStudio/Dependencies/apngdisassembler/apng_dis.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_libs/cmp_math/cmp_math_common.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_libs/cmp_math/cpu_extensions.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_encode/hpc/ccpu_hpc.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_encode/hpc/cmp_hpc.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_encode/hpc/compute_cpu_hpc.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc1/bc1.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc2/bc2.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc3/bc3.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc4/bc4.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc5/bc5.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc6/bc6h.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bc7/bc7.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/ccmp_sdk/bcn.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/cimage/dds/dds.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/cimage/dds/dds_dx10.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/cimage/dds/dds_file.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/cimage/dds/dds_helpers.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/atiformats.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/cmp_fileio.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/codec_common.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/cpu_timing.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/format_conversion.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/misc.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/pluginmanager.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/tc_plugininternal.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/texture_utils.cpp"
    "AnimStudio/Dependencies/compressonator/applications/_plugins/common/utilfuncs.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/apc/apc_decode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/apc/apc_encode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/apc/codec_apc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/atc/codec_atc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/atc/codec_atc_rgb.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/atc/codec_atc_rgba_explicit.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/atc/codec_atc_rgba_interpolated.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/codec_ati1n.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/codec_ati2n.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/codec_ati2n_dxt5.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/codec_ati_tc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/compressonatori_tc.c"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/ati/compressonatorxcodec.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/basis/codec_basis.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/bc6h_decode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/bc6h_definitions.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/bc6h_encode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/bc6h_library.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/bc6h_utils.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc6h/codec_bc6h.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/3dquant_vpc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_decode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_definitions.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_encode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_library.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_partitions.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/bc7_utils.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/codec_bc7.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/reconstruct.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/bc7/shake.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/block/codec_block.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/block/codec_block_4x4.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/block/codec_block_8x8.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_block.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r16.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r16f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r32.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r32f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r8.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_r8s.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg16.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg16f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg32.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg32f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg8.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rg8s.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgb888.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgb888s.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgb9995ef.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba1010102.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba16.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba16f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba2101010.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba32.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba32f.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba8888.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/buffer/codecbuffer_rgba8888s.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/common/codec.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/cmp_compress.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/compressonator.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxtc/codec_dxtc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxtc/codec_dxtc_alpha.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxtc/codec_dxtc_rgba.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxtc/dxtc_v11_compress.c"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxtc/dxtc_v11_compress_asm.c"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt1.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt3.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_rbxg.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_rgxb.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_rxbg.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_swizzled.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_xgbr.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_xgxr.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/dxt/codec_dxt5_xrbg.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc2.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc2_rgb.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc2_rgba.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc2_rgba1.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc_rgb.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc_rgba_explicit.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/codec_etc_rgba_interpolated.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/etcpack/etcdec.cxx"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/etcpack/etcimage.cxx"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/etc/etcpack/etcpack.cxx"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/gt/codec_gt.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/gt/gt_decode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_compressonatorlib/gt/gt_encode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc1_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc2_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc3_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc4_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc5_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc6_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/shaders/bc7_encode_kernel.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/source/core_simd_avx.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/source/core_simd_avx512.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_core/source/core_simd_sse.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_framework/common/cmp_boxfilter.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_framework/common/cmp_mips.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_framework/common/half/half.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_framework/common/hdr_encode.cpp"
    "AnimStudio/Dependencies/compressonator/cmp_framework/compute_base.cpp"
    "AnimStudio/Dependencies/libimagequant/blur.c"
    "AnimStudio/Dependencies/libimagequant/kmeans.c"
    "AnimStudio/Dependencies/libimagequant/libimagequant.c"
    "AnimStudio/Dependencies/libimagequant/mediancut.c"
    "AnimStudio/Dependencies/libimagequant/mempool.c"
    "AnimStudio/Dependencies/libimagequant/nearest.c"
    "AnimStudio/Dependencies/libimagequant/pam.c"
    "AnimStudio/Dependencies/libimagequant/remap.c"
    "AnimStudio/Dependencies/libpng/png.c"
    "AnimStudio/Dependencies/libpng/pngerror.c"
    "AnimStudio/Dependencies/libpng/pngget.c"
    "AnimStudio/Dependencies/libpng/pngmem.c"
    "AnimStudio/Dependencies/libpng/pngpread.c"
    "AnimStudio/Dependencies/libpng/pngread.c"
    "AnimStudio/Dependencies/libpng/pngrio.c"
    "AnimStudio/Dependencies/libpng/pngrtran.c"
    "AnimStudio/Dependencies/libpng/pngrutil.c"
    "AnimStudio/Dependencies/libpng/pngset.c"
    "AnimStudio/Dependencies/libpng/pngtrans.c"
    "AnimStudio/Dependencies/libpng/pngwio.c"
    "AnimStudio/Dependencies/libpng/pngwrite.c"
    "AnimStudio/Dependencies/libpng/pngwtran.c"
    "AnimStudio/Dependencies/libpng/pngwutil.c"
    "AnimStudio/Dependencies/zlib/adler32.c"
    "AnimStudio/Dependencies/zlib/compress.c"
    "AnimStudio/Dependencies/zlib/crc32.c"
    "AnimStudio/Dependencies/zlib/deflate.c"
    "AnimStudio/Dependencies/zlib/gzclose.c"
    "AnimStudio/Dependencies/zlib/gzlib.c"
    "AnimStudio/Dependencies/zlib/gzread.c"
    "AnimStudio/Dependencies/zlib/gzwrite.c"
    "AnimStudio/Dependencies/zlib/infback.c"
    "AnimStudio/Dependencies/zlib/inffast.c"
    "AnimStudio/Dependencies/zlib/inflate.c"
    "AnimStudio/Dependencies/zlib/inftrees.c"
    "AnimStudio/Dependencies/zlib/trees.c"
    "AnimStudio/Dependencies/zlib/uncompr.c"
    "AnimStudio/Dependencies/zlib/zutil.c"
)

# The vendored C/C++ is plain - keep Qt autogen (moc/uic/rcc) off it.
set_source_files_properties(${ANIMSTUDIO_VENDORED} PROPERTIES SKIP_AUTOGEN ON)

qt_add_executable(AnimStudio WIN32
    "AnimStudio/Animation/AnimationController.cpp"
    "AnimStudio/Animation/AnimationData.cpp"
    "AnimStudio/Animation/BuiltInPalettes.cpp"
    "AnimStudio/Animation/Palette.cpp"
    "AnimStudio/Formats/Custom Handlers/DdsHandler.cpp"
    "AnimStudio/Formats/Custom Handlers/PcxHandler.cpp"
    "AnimStudio/Formats/Custom Handlers/TgaHandler.cpp"
    "AnimStudio/Formats/Export/AniExporter.cpp"
    "AnimStudio/Formats/Export/ApngExporter.cpp"
    "AnimStudio/Formats/Export/EffExporter.cpp"
    "AnimStudio/Formats/Export/RawExporter.cpp"
    "AnimStudio/Formats/ImageFormats.cpp"
    "AnimStudio/Formats/ImageLoader.cpp"
    "AnimStudio/Formats/ImageWriter.cpp"
    "AnimStudio/Formats/Import/AniImporter.cpp"
    "AnimStudio/Formats/Import/ApngImporter.cpp"
    "AnimStudio/Formats/Import/EffImporter.cpp"
    "AnimStudio/Formats/Import/RawImporter.cpp"
    "AnimStudio/Animation/Quantizer.cpp"
    "AnimStudio/Widgets/SpinnerWidget.cpp"
    "AnimStudio/Windows/ExportAnimation.cpp"
    "AnimStudio/Windows/ReduceColors.cpp"
    "AnimStudio/Windows/AnimStudio.cpp"
    "AnimStudio/main.cpp"
    # Q_OBJECT headers (AUTOMOC)
    "AnimStudio/Windows/AnimStudio.h"
    "AnimStudio/Widgets/SpinnerWidget.h"
    "AnimStudio/Windows/ExportAnimation.h"
    "AnimStudio/Windows/ReduceColors.h"
    "AnimStudio/Animation/AnimationController.h"
    # Designer forms (AUTOUIC) and resources (AUTORCC)
    "AnimStudio/Forms/AnimStudio.ui"
    "AnimStudio/Forms/ExportAnimation.ui"
    "AnimStudio/Forms/ReduceColors.ui"
    "AnimStudio/Forms/AnimStudio.qrc"
    ${ANIMSTUDIO_VENDORED}
)

target_compile_features(AnimStudio PRIVATE cxx_std_17)

# The .ui files live in Forms/, but the Q_OBJECT headers that #include their
# generated ui_*.h live in Windows/ - tell AUTOUIC where to find the forms.
set_target_properties(AnimStudio PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/AnimStudio/Forms")

# Make F5 in the generated VS solution launch the app, not the non-runnable
# ALL_BUILD utility target (ignored by non-VS generators).
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT AnimStudio)

# Match the old project include paths; AnimStudio/ lets app sources and the
# resource compiler resolve their own headers by bare name.
target_include_directories(AnimStudio PRIVATE
    AnimStudio
    AnimStudio/Dependencies/libpng
    AnimStudio/Dependencies/libimagequant
    AnimStudio/Dependencies/zlib
    AnimStudio/Dependencies/apngdisassembler
    AnimStudio/Dependencies/apngasm
    AnimStudio/Dependencies/compressonator/cmp_compressonatorlib
)

target_compile_definitions(AnimStudio PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_definitions(AnimStudio PRIVATE UNICODE _UNICODE)
endif()

target_link_libraries(AnimStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

# Windows resource (app icon). Windows-only input.
if(WIN32)
    target_sources(AnimStudio PRIVATE AnimStudio/app_icon.rc)
endif()

# Developer convenience: stage the Qt runtime next to the exe so the app runs
# from the build tree (F5 in VS). CI deploys/packages separately.
if(WIN32)
    get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET AnimStudio POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:AnimStudio>"
            COMMENT "Deploying Qt runtime with windeployqt"
            VERBATIM)
    endif()
endif()
