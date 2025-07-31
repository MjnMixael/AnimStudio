#include "DdsHandler.h"
#include "compressonator.h"
#include <QDebug>
#include <QImage>

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
    // Ensure the source image is in a format Compressonator can read (RGBA8888)
    QImage sourceImage = image.convertToFormat(QImage::Format_RGBA8888);

    const int width = sourceImage.width();
    const int height = sourceImage.height();
    const int depth = 1;

    // 1. Create a source MipSet from the QImage data
    CMP_MipSet mipSetIn = {};
    if (CMP_CreateMipSet(&mipSetIn, sourceImage.width(), sourceImage.height(), 1, CF_8bit, TT_2D) != CMP_OK) {
        qWarning() << "Failed to create source MipSet for writing.";
        return false;
    }

    // Copy the QImage data into the source MipSet
    CMP_MipLevel* mipLevelIn = nullptr;
    CMP_GetMipLevel(&mipLevelIn, &mipSetIn, 0, 0);
    if (!mipLevelIn) {
        qWarning() << "Failed to get source MipLevel for writing.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }
    memcpy(mipLevelIn->m_pbData, sourceImage.bits(), sourceImage.sizeInBytes());
    mipSetIn.m_format = CMP_FORMAT_RGBA_8888;

    // 2. Create a destination MipSet for the compressed data
    CMP_MipSet mipSetOut = {};
    if (CMP_CreateMipSet(&mipSetOut, sourceImage.width(), sourceImage.height(), 1, CF_Compressed, TT_2D) != CMP_OK) {
        qWarning() << "Failed to create destination MipSet for writing.";
        CMP_FreeMipSet(&mipSetIn);
        return false;
    }
    mipSetOut.m_format = CMP_FORMAT_BC7; // Set the destination to BC7

    // 3. Set up compression options for BC7
    CMP_CompressOptions options = {};
    options.dwSize = sizeof(CMP_CompressOptions);
    options.SourceFormat = mipSetIn.m_format;
    options.DestFormat = mipSetOut.m_format;
    options.fquality = 0.8f; // Quality setting, 0.0 (fastest) to 1.0 (best)
    options.dwnumThreads = 0; // 0 = auto, let the library decide

    // 4. Compress the texture
    CMP_ERROR result = CMP_ConvertMipTexture(&mipSetIn, &mipSetOut, &options, nullptr);
    if (result != CMP_OK) {
        qWarning() << "Failed to compress texture to BC7. Error:" << result;
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    // 5. Save the compressed MipSet to a DDS file
    if (CMP_SaveTexture(m_device.toStdString().c_str(), &mipSetOut) != CMP_OK) {
        qWarning() << "Failed to save DDS file:" << m_device;
        CMP_FreeMipSet(&mipSetIn);
        CMP_FreeMipSet(&mipSetOut);
        return false;
    }

    // Clean up
    CMP_FreeMipSet(&mipSetIn);
    CMP_FreeMipSet(&mipSetOut);

    return true;
}