// Decoders for the texture formats used by the game:
//   * JNG (JPEG colour + zlib or JPEG alpha) stored with a .dds/.jng extension
//   * DDS A8R8G8B8, A4R4G4B4, DXT5
//   * JPEG / PNG (via stb_image)
#pragma once
#include <vector>
#include <cstdint>

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba; // w*h*4, straight alpha
};

bool decodeImage(const uint8_t* data, size_t size, Image& out);
