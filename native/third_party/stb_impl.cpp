// The vendored stb implementations (native/third_party/stb), compiled once: PNG decoding and raw-deflate inflating (stb_image),
// PNG writing (stb_image_write).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
