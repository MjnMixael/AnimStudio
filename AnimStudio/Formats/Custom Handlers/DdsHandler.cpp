#include "DdsHandler.h"
#include "compressonator.h"
#include <QDebug>
#include <QImage>

// If Compressonator needs updates then we'll have to rebuild the debug and release libs.
// Get the Compressonator SDK source. Last time we built with cmp_compressonatorlib.sln in build_sdk
// we built the release_MD and debug_MD versions. Copy the .lib and the compressonator.h into
// dependencies and call it a day. Compiling for other platforms was not handled.

bool DdsHandler::read(QImage* image)
{
    CMP_MipSet mipSetIn = {};

    // Load the DDS texture from the specified file
    if (CMP_LoadTexture(m_device.toStdString().c_str(), &mipSetIn) != CMP_OK) {
        qWarning() << "Failed to load texture:" << m_device;
        return false;
    }

    // Prepare a destination MipSet for the decompressed data
    // We will decompress to a standard RGBA 8-bit format
    CMP_MipSet mipSetOut = {};
    if (CMP_CreateMipSet(&mipSetOut, mipSetIn.m_nWidth, mipSetIn.m_nHeight, mipSetIn.m_nDepth, CF_8bit, mipSetIn.m_TextureType) != CMP_OK) {
        qWarning() << "Failed to create destination MipSet.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }

    mipSetOut.m_format = CMP_FORMAT_RGBA_8888;


    // Set up compression options
    CMP_CompressOptions options = {};
    options.dwSize = sizeof(CMP_CompressOptions);
    options.SourceFormat = mipSetIn.m_format;
    options.DestFormat = mipSetOut.m_format;

    CMP_ERROR result = CMP_ConvertMipTexture(&mipSetIn, &mipSetOut, &options, nullptr);

    // Decompress
    if (result != CMP_OK) {
        qWarning() << "Failed to convert/decompress texture.";
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    // Get the top-level mipmap of the decompressed data
    CMP_MipLevel* mipLevel = nullptr;
    CMP_GetMipLevel(&mipLevel, &mipSetOut, 0, 0);

    if (!mipLevel || !mipLevel->m_pbData) {
        qWarning() << "Failed to retrieve decompressed mip level data.";
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    // Create a QImage from the raw RGBA data
    QImage img(static_cast<const uchar*>(mipLevel->m_pbData),
        mipLevel->m_nWidth,
        mipLevel->m_nHeight,
        QImage::Format_RGBA8888);

    *image = img.copy();

    // Clean up the MipSet structures to avoid memory leaks
    CMP_FreeMipSet(&mipSetIn);
    CMP_FreeMipSet(&mipSetOut);

    return true;
}

bool DdsHandler::write(const QImage& image)
{
    QImage sourceImage = image.convertToFormat(QImage::Format_RGBA8888);

    const int width = sourceImage.width();
    const int height = sourceImage.height();
    const int depth = 1;

    // 1. Create source MipSet
    CMP_MipSet mipSetIn = {};
    if (CMP_CreateMipSet(&mipSetIn, width, height, depth, CF_8bit, TT_2D) != CMP_OK) {
        qWarning() << "Failed to create source MipSet for writing.";
        return false;
    }

    mipSetIn.m_format = CMP_FORMAT_RGBA_8888;
    mipSetIn.m_nMipLevels = 1;

    // Copy base level image data
    CMP_MipLevel* mipLevelIn = nullptr;
    CMP_GetMipLevel(&mipLevelIn, &mipSetIn, 0, 0);
    if (!mipLevelIn) {
        qWarning() << "Failed to get source MipLevel.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }
    memcpy(mipLevelIn->m_pbData, sourceImage.bits(), sourceImage.sizeInBytes());

    // 2. Generate mipmaps
    if (CMP_GenerateMIPLevels(&mipSetIn, false) != CMP_OK) {
        qWarning() << "Failed to generate mipmaps.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }

    // 3. Create destination MipSet
    CMP_MipSet mipSetOut = {};
    if (CMP_CreateMipSet(&mipSetOut, width, height, depth, CF_Compressed, TT_2D) != CMP_OK) {
        qWarning() << "Failed to create destination MipSet.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }

    switch (m_compressionFormat) {
    case CompressionFormat::BC1:
        mipSetOut.m_format = CMP_FORMAT_BC1;
        break;
    case CompressionFormat::BC3:
        mipSetOut.m_format = CMP_FORMAT_BC3;
        break;
    case CompressionFormat::BC7:
        mipSetOut.m_format = CMP_FORMAT_BC7;
        break;
    }

    mipSetOut.m_nMipLevels = mipSetIn.m_nMipLevels;

    // 4. Set compression options
    CMP_CompressOptions options = {};
    options.dwSize = sizeof(CMP_CompressOptions);
    options.SourceFormat = mipSetIn.m_format;
    options.DestFormat = mipSetOut.m_format;
    options.fquality = 0.8f;
    options.dwnumThreads = 0;

    // 5. Compress full mip chain
    CMP_ERROR result = CMP_ConvertMipTexture(&mipSetIn, &mipSetOut, &options, nullptr);
    if (result != CMP_OK) {
        qWarning() << "Failed to compress texture. Error:" << result;
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    // 6. Save DDS
    if (CMP_SaveTexture(m_device.toStdString().c_str(), &mipSetOut) != CMP_OK) {
        qWarning() << "Failed to save DDS file:" << m_device;
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    CMP_FreeMipSet(&mipSetIn);
    CMP_FreeMipSet(&mipSetOut);
    return true;
}