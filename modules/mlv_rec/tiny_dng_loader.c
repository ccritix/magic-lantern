//
// TinyDNGLoader, single header only DNG loader.
//

/*
The MIT License (MIT)

Copyright (c) 2016 Syoyo Fujita.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef TINY_DNG_LOADER_H_
#define TINY_DNG_LOADER_H_

// @note {
// https://www.adobe.com/content/dam/Adobe/en/products/photoshop/pdfs/dng_spec_1.4.0.0.pdf
// }

#include <string>
#include <vector>

namespace tinydng {

typedef enum {
  LIGHTSOURCE_UNKNOWN = 0,
  LIGHTSOURCE_DAYLIGHT = 1,
  LIGHTSOURCE_FLUORESCENT = 2,
  LIGHTSOURCE_TUNGSTEN = 3,
  LIGHTSOURCE_FLASH = 4,
  LIGHTSOURCE_FINE_WEATHER = 9,
  LIGHTSOURCE_CLOUDY_WEATHER = 10,
  LIGHTSOURCE_SHADE = 11,
  LIGHTSOURCE_DAYLIGHT_FLUORESCENT = 12,
  LIGHTSOURCE_DAY_WHITE_FLUORESCENT = 13,
  LIGHTSOURCE_COOL_WHITE_FLUORESCENT = 14,
  LIGHTSOURCE_WHITE_FLUORESCENT = 15,
  LIGHTSOURCE_STANDARD_LIGHT_A = 17,
  LIGHTSOURCE_STANDARD_LIGHT_B = 18,
  LIGHTSOURCE_STANDARD_LIGHT_C = 19,
  LIGHTSOURCE_D55 = 20,
  LIGHTSOURCE_D65 = 21,
  LIGHTSOURCE_D75 = 22,
  LIGHTSOURCE_D50 = 23,
  LIGHTSOURCE_ISO_STUDIO_TUNGSTEN = 24,
  LIGHTSOURCE_OTHER_LIGHT_SOURCE = 255
} LightSource;

typedef enum {
  COMPRESSION_NONE = 1,
  COMPRESSION_OLD_JPEG = 6,   // JPEG or lossless JPEG
  COMPRESSION_NEW_JPEG = 7,   // Usually lossles JPEG, may be JPEG
  COMPRESSION_LOSSY = 34892,  // Lossy JPEGG
  COMPRESSION_NEF = 34713     // NIKON RAW
} Compression;

struct DNGImage {
  int black_level[4];  // for each spp(up to 4)
  int white_level[4];  // for each spp(up to 4)
  int version;         // DNG version

  int samples_per_pixel;

  int bits_per_sample_original;  // BitsPerSample in stored file.
  int bits_per_sample;  // Bits per sample after reading(decoding) DNG image.

  char cfa_plane_color[4];  // 0:red, 1:green, 2:blue, 3:cyan, 4:magenta,
                            // 5:yellow, 6:white
  int cfa_pattern[2][2];    // @fixme { Support non 2x2 CFA pattern. }
  int cfa_pattern_dim;
  int cfa_layout;
  int active_area[4];  // top, left, bottom, right
  bool has_active_area;
  unsigned char pad_has_active_area[3];

  int tile_width;
  int tile_length;
  unsigned int tile_offset;
  unsigned int tile_byte_count;  // (compressed) size

  double analog_balance[3];
  int pad0;
  bool has_analog_balance;
  unsigned char pad1[7];

  int pad2;
  double as_shot_neutral[3];
  int pad3;
  bool has_as_shot_neutral;
  unsigned char pad4[7];

  int pad5;
  double color_matrix1[3][3];
  double color_matrix2[3][3];

  double forward_matrix1[3][3];
  double forward_matrix2[3][3];

  double camera_calibration1[3][3];
  double camera_calibration2[3][3];

  LightSource calibration_illuminant1;
  LightSource calibration_illuminant2;

  int width;
  int height;
  int compression;
  unsigned int offset;
  int orientation;
  int strip_byte_count;
  int jpeg_byte_count;
  int planar_configuration;  // 1: chunky, 2: planar

  // CR2 specific
  unsigned short cr2_slices[3];
  unsigned short pad_c;

  std::vector<unsigned char>
      data;  // Decoded pixel data(len = spp * width * height * bps / 8)
};

// When DNG contains multiple images(e.g. full-res image + thumnail image),
// The function creates `DNGImage` data strucure for each images.
// Returns true upon success.
// Returns false upon failure and store error message into `err`.
bool LoadDNG(std::vector<DNGImage>* images,  // [out] DNG images.
             std::string* err,               // [out] error message.
             const char* filename);

}  // namespace tinydng

#ifdef TINY_DNG_LOADER_IMPLEMENTATION

#include <stdint.h>  // for lj92
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef TINY_DNG_LOADER_PROFILING
// Requires C++11 feature
#include <chrono>
#endif

#ifdef TINY_DNG_LOADER_DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

namespace tinydng {

// Very simple count leading zero implementation.
static int clz32(unsigned int x) {
  int n;
  if (x == 0) return 32;
  for (n = 0; ((x & 0x80000000) == 0); n++, x <<= 1)
    ;
  return n;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4334)
#pragma warning(disable : 4244)
#endif

// STB image to decode jpeg image.
// Assume STB_IMAGE_IMPLEMENTATION is defined elsewhere
#include "stb_image.h"

namespace {
// Begin liblj92, Lossless JPEG decode/encoder ------------------------------

/*
lj92.c
(c) Andrew Baldwin 2014

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

enum LJ92_ERRORS {
  LJ92_ERROR_NONE = 0,
  LJ92_ERROR_CORRUPT = -1,
  LJ92_ERROR_NO_MEMORY = -2,
  LJ92_ERROR_BAD_HANDLE = -3,
  LJ92_ERROR_TOO_WIDE = -4
};

typedef struct _ljp* lj92;

/* Parse a lossless JPEG (1992) structure returning
 * - a handle that can be used to decode the data
 * - width/height/bitdepth of the data
 * Returns status code.
 * If status == LJ92_ERROR_NONE, handle must be closed with lj92_close
 */
static int lj92_open(lj92* lj,                    // Return handle here
                     uint8_t* data, int datalen,  // The encoded data
                     int* width, int* height,
                     int* bitdepth);  // Width, height and bitdepth

/* Release a decoder object */
static void lj92_close(lj92 lj);

/*
 * Decode previously opened lossless JPEG (1992) into a 2D tile of memory
 * Starting at target, write writeLength 16bit values, then skip 16bit
 * skipLength value before writing again
 * If linearize is not NULL, use table at linearize to convert data values from
 * output value to target value
 * Data is only correct if LJ92_ERROR_NONE is returned
 */
static int lj92_decode(
    lj92 lj, uint16_t* target, int writeLength,
    int skipLength,  // The image is written to target as a tile
    uint16_t* linearize,
    int linearizeLength);  // If not null, linearize the data using this table

/*
 * Encode a grayscale image supplied as 16bit values within the given bitdepth
 * Read from tile in the image
 * Apply delinearization if given
 * Return the encoded lossless JPEG stream
 */
static int lj92_encode(uint16_t* image, int width, int height, int bitdepth,
                       int readLength, int skipLength, uint16_t* delinearize,
                       int delinearizeLength, uint8_t** encoded,
                       int* encodedLength);

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

//#define SLOW_HUFF
//#define LJ92_DEBUG

#define LJ92_MAX_COMPONENTS (16)

typedef struct _ljp {
  u8* data;
  u8* dataend;
  int datalen;
  int scanstart;
  int ix;
  int x;           // Width
  int y;           // Height
  int bits;        // Bit depth
  int components;  // Components(Nf)
  int writelen;    // Write rows this long
  int skiplen;     // Skip this many values after each row
  u16* linearize;  // Linearization table
  int linlen;
  int sssshist[16];

// Huffman table - only one supported, and probably needed
#ifdef SLOW_HUFF
  // NOTE: Huffman table for each components is not supported for SLOW_HUFF code
  // path.
  int* maxcode;
  int* mincode;
  int* valptr;
  u8* huffval;
  int* huffsize;
  int* huffcode;
#else
  // Huffman table for each components
  u16* hufflut[LJ92_MAX_COMPONENTS];
  int huffbits[LJ92_MAX_COMPONENTS];
  int num_huff_idx;
#endif
  // Parse state
  int cnt;
  u32 b;
  u16* image;
  u16* rowcache;
  u16* outrow[2];
} ljp;

static int find(ljp* self) {
  int ix = self->ix;
  u8* data = self->data;
  while (data[ix] != 0xFF && ix < (self->datalen - 1)) {
    ix += 1;
  }
  ix += 2;
  if (ix >= self->datalen) {
    // DPRINTF("idx = %d, datalen = %\d\n", ix, self->datalen);
    return -1;
  }
  self->ix = ix;
  // DPRINTF("ix = %d, data = %d\n", ix, data[ix - 1]);
  return data[ix - 1];
}

#define BEH(ptr) ((((int)(*&ptr)) << 8) | (*(&ptr + 1)))

static int parseHuff(ljp* self) {
  int ret = LJ92_ERROR_CORRUPT;
  u8* huffhead =
      &self->data
           [self->ix];  // xstruct.unpack('>HB16B',self.data[self.ix:self.ix+19])
  u8* bits = &huffhead[2];
  bits[0] = 0;  // Because table starts from 1
  int hufflen = BEH(huffhead[0]);
  if ((self->ix + hufflen) >= self->datalen) return ret;
#ifdef SLOW_HUFF
  u8* huffval = calloc(hufflen - 19, sizeof(u8));
  if (huffval == NULL) return LJ92_ERROR_NO_MEMORY;
  self->huffval = huffval;
  for (int hix = 0; hix < (hufflen - 19); hix++) {
    huffval[hix] = self->data[self->ix + 19 + hix];
#ifdef LJ92_DEBUG
    DPRINTF("huffval[%d]=%d\n", hix, huffval[hix]);
#endif
  }
  self->ix += hufflen;
  // Generate huffman table
  int k = 0;
  int i = 1;
  int j = 1;
  int huffsize_needed = 1;
  // First calculate how long huffsize needs to be
  while (i <= 16) {
    while (j <= bits[i]) {
      huffsize_needed++;
      k = k + 1;
      j = j + 1;
    }
    i = i + 1;
    j = 1;
  }
  // Now allocate and do it
  int* huffsize = calloc(huffsize_needed, sizeof(int));
  if (huffsize == NULL) return LJ92_ERROR_NO_MEMORY;
  self->huffsize = huffsize;
  k = 0;
  i = 1;
  j = 1;
  // First calculate how long huffsize needs to be
  int hsix = 0;
  while (i <= 16) {
    while (j <= bits[i]) {
      huffsize[hsix++] = i;
      k = k + 1;
      j = j + 1;
    }
    i = i + 1;
    j = 1;
  }
  huffsize[hsix++] = 0;

  // Calculate the size of huffcode array
  int huffcode_needed = 0;
  k = 0;
  int code = 0;
  int si = huffsize[0];
  while (1) {
    while (huffsize[k] == si) {
      huffcode_needed++;
      code = code + 1;
      k = k + 1;
    }
    if (huffsize[k] == 0) break;
    while (huffsize[k] != si) {
      code = code << 1;
      si = si + 1;
    }
  }
  // Now fill it
  int* huffcode = calloc(huffcode_needed, sizeof(int));
  if (huffcode == NULL) return LJ92_ERROR_NO_MEMORY;
  self->huffcode = huffcode;
  int hcix = 0;
  k = 0;
  code = 0;
  si = huffsize[0];
  while (1) {
    while (huffsize[k] == si) {
      huffcode[hcix++] = code;
      code = code + 1;
      k = k + 1;
    }
    if (huffsize[k] == 0) break;
    while (huffsize[k] != si) {
      code = code << 1;
      si = si + 1;
    }
  }

  i = 0;
  j = 0;

  int* maxcode = calloc(17, sizeof(int));
  if (maxcode == NULL) return LJ92_ERROR_NO_MEMORY;
  self->maxcode = maxcode;
  int* mincode = calloc(17, sizeof(int));
  if (mincode == NULL) return LJ92_ERROR_NO_MEMORY;
  self->mincode = mincode;
  int* valptr = calloc(17, sizeof(int));
  if (valptr == NULL) return LJ92_ERROR_NO_MEMORY;
  self->valptr = valptr;

  while (1) {
    while (1) {
      i++;
      if (i > 16) break;
      if (bits[i] != 0) break;
      maxcode[i] = -1;
    }
    if (i > 16) break;
    valptr[i] = j;
    mincode[i] = huffcode[j];
    j = j + bits[i] - 1;
    maxcode[i] = huffcode[j];
    j++;
  }
  free(huffsize);
  self->huffsize = NULL;
  free(huffcode);
  self->huffcode = NULL;
  ret = LJ92_ERROR_NONE;
#else
  /* Calculate huffman direct lut */
  // How many bits in the table - find highest entry
  u8* huffvals = &self->data[self->ix + 19];
  int maxbits = 16;
  while (maxbits > 0) {
    if (bits[maxbits]) break;
    maxbits--;
  }
  self->huffbits[self->num_huff_idx] = maxbits;
  /* Now fill the lut */
  u16* hufflut = (u16*)malloc((1 << maxbits) * sizeof(u16));
  // DPRINTF("maxbits = %d\n", maxbits);
  if (hufflut == NULL) return LJ92_ERROR_NO_MEMORY;
  self->hufflut[self->num_huff_idx] = hufflut;
  int i = 0;
  int hv = 0;
  int rv = 0;
  int vl = 0;  // i
  int hcode;
  int bitsused = 1;
#ifdef LJ92_DEBUG
  DPRINTF("%04x:%x:%d:%x\n", i, huffvals[hv], bitsused,
          1 << (maxbits - bitsused));
#endif
  while (i < 1 << maxbits) {
    if (bitsused > maxbits) {
      break;  // Done. Should never get here!
    }
    if (vl >= bits[bitsused]) {
      bitsused++;
      vl = 0;
      continue;
    }
    if (rv == 1 << (maxbits - bitsused)) {
      rv = 0;
      vl++;
      hv++;
#ifdef LJ92_DEBUG
      DPRINTF("%04x:%x:%d:%x\n", i, huffvals[hv], bitsused,
              1 << (maxbits - bitsused));
#endif
      continue;
    }
    hcode = huffvals[hv];
    hufflut[i] = hcode << 8 | bitsused;
    // DPRINTF("%d %d %d\n",i,bitsused,hcode);
    i++;
    rv++;
  }
  ret = LJ92_ERROR_NONE;
#endif
  self->num_huff_idx++;

  return ret;
}

static int parseSof3(ljp* self) {
  if (self->ix + 6 >= self->datalen) return LJ92_ERROR_CORRUPT;
  self->y = BEH(self->data[self->ix + 3]);
  self->x = BEH(self->data[self->ix + 5]);
  self->bits = self->data[self->ix + 2];
  self->components = self->data[self->ix + 7];
  self->ix += BEH(self->data[self->ix]);

  assert(self->components >= 1);
  assert(self->components < 6);

  return LJ92_ERROR_NONE;
}

static int parseBlock(ljp* self, int marker) {
  self->ix += BEH(self->data[self->ix]);
  if (self->ix >= self->datalen) {
    DPRINTF("parseBlock: ix %d, datalen %d\n", self->ix, self->datalen);
    return LJ92_ERROR_CORRUPT;
  }
  return LJ92_ERROR_NONE;
}

#ifdef SLOW_HUFF
static int nextbit(ljp* self) {
  u32 b = self->b;
  if (self->cnt == 0) {
    u8* data = &self->data[self->ix];
    u32 next = *data++;
    b = next;
    if (next == 0xff) {
      data++;
      self->ix++;
    }
    self->ix++;
    self->cnt = 8;
  }
  int bit = b >> 7;
  self->cnt--;
  self->b = (b << 1) & 0xFF;
  return bit;
}

static int decode(ljp* self) {
  int i = 1;
  int code = nextbit(self);
  while (code > self->maxcode[i]) {
    i++;
    code = (code << 1) + nextbit(self);
  }
  int j = self->valptr[i];
  j = j + code - self->mincode[i];
  int value = self->huffval[j];
  return value;
}

static int receive(ljp* self, int ssss) {
  int i = 0;
  int v = 0;
  while (i != ssss) {
    i++;
    v = (v << 1) + nextbit(self);
  }
  return v;
}

static int extend(ljp* self, int v, int t) {
  int vt = 1 << (t - 1);
  if (v < vt) {
    vt = (-1 << t) + 1;
    v = v + vt;
  }
  return v;
}
#endif

inline static int nextdiff(ljp* self, int component_idx, int Px) {
#ifdef SLOW_HUFF
  int t = decode(self);
  int diff = receive(self, t);
  diff = extend(self, diff, t);
// DPRINTF("%d %d %d %x\n",Px+diff,Px,diff,t);//,index,usedbits);
#else
  assert(component_idx <= self->num_huff_idx);
  u32 b = self->b;
  int cnt = self->cnt;
  int huffbits = self->huffbits[component_idx];
  int ix = self->ix;
  int next;
  while (cnt < huffbits) {
    next = *(u16*)&self->data[ix];
    int one = next & 0xFF;
    int two = next >> 8;
    b = (b << 16) | (one << 8) | two;
    cnt += 16;
    ix += 2;
    if (one == 0xFF) {
      // DPRINTF("%x %x %x %x %d\n",one,two,b,b>>8,cnt);
      b >>= 8;
      cnt -= 8;
    } else if (two == 0xFF)
      ix++;
  }
  int index = b >> (cnt - huffbits);
  // DPRINTF("component_idx = %d / %d, index = %d\n", component_idx,
  // self->components, index);

  u16 ssssused = self->hufflut[component_idx][index];
  int usedbits = ssssused & 0xFF;
  int t = ssssused >> 8;
  self->sssshist[t]++;
  cnt -= usedbits;
  int keepbitsmask = (1 << cnt) - 1;
  b &= keepbitsmask;
  while (cnt < t) {
    next = *(u16*)&self->data[ix];
    int one = next & 0xFF;
    int two = next >> 8;
    b = (b << 16) | (one << 8) | two;
    cnt += 16;
    ix += 2;
    if (one == 0xFF) {
      b >>= 8;
      cnt -= 8;
    } else if (two == 0xFF)
      ix++;
  }
  cnt -= t;
  int diff = b >> cnt;
  int vt = 1 << (t - 1);
  if (diff < vt) {
    vt = (-1 << t) + 1;
    diff += vt;
  }
  keepbitsmask = (1 << cnt) - 1;
  self->b = b & keepbitsmask;
  self->cnt = cnt;
  self->ix = ix;
// DPRINTF("%d %d\n",t,diff);
// DPRINTF("%d %d %d %x %x %d\n",Px+diff,Px,diff,t,index,usedbits);
#ifdef LJ92_DEBUG
#endif
#endif
  return diff;
}

static int parsePred6(ljp* self) {
  DPRINTF("parsePred6\n");
  int ret = LJ92_ERROR_CORRUPT;
  self->ix = self->scanstart;
  // int compcount = self->data[self->ix+2];
  self->ix += BEH(self->data[self->ix]);
  self->cnt = 0;
  self->b = 0;
  int write = self->writelen;
  // Now need to decode huffman coded values
  int c = 0;
  int pixels = self->y * self->x;
  u16* out = self->image;
  u16* temprow;
  u16* thisrow = self->outrow[0];
  u16* lastrow = self->outrow[1];

  // First pixel predicted from base value
  int diff;
  int Px;
  int col = 0;
  int row = 0;
  int left = 0;
  int linear;

  assert(self->num_huff_idx <= self->components);
  // First pixel
  diff = nextdiff(self, self->num_huff_idx - 1,
                  0);  // FIXME(syoyo): Is using (self->num_huff_idx-1) correct?
  Px = 1 << (self->bits - 1);
  left = Px + diff;
  if (self->linearize)
    linear = self->linearize[left];
  else
    linear = left;
  thisrow[col++] = left;
  out[c++] = linear;
  if (self->ix >= self->datalen) {
    DPRINTF("ix = %d, datalen = %d\n", self->ix, self->datalen);
    return ret;
  }
  --write;
  int rowcount = self->x - 1;
  while (rowcount--) {
    diff = nextdiff(self, self->num_huff_idx - 1, 0);
    Px = left;
    left = Px + diff;
    if (self->linearize)
      linear = self->linearize[left];
    else
      linear = left;
    thisrow[col++] = left;
    out[c++] = linear;
    // DPRINTF("%d %d %d %d
    // %x\n",col-1,diff,left,thisrow[col-1],&thisrow[col-1]);
    if (self->ix >= self->datalen) {
      DPRINTF("a: self->ix = %d, datalen = %d\n", self->ix, self->datalen);
      return ret;
    }
    if (--write == 0) {
      out += self->skiplen;
      write = self->writelen;
    }
  }
  temprow = lastrow;
  lastrow = thisrow;
  thisrow = temprow;
  row++;
  // DPRINTF("%x %x\n",thisrow,lastrow);
  while (c < pixels) {
    col = 0;
    diff = nextdiff(self, self->num_huff_idx - 1, 0);
    Px = lastrow[col];  // Use value above for first pixel in row
    left = Px + diff;
    if (self->linearize) {
      if (left > self->linlen) return LJ92_ERROR_CORRUPT;
      linear = self->linearize[left];
    } else
      linear = left;
    thisrow[col++] = left;
    // DPRINTF("%d %d %d %d\n",col,diff,left,lastrow[col]);
    out[c++] = linear;
    if (self->ix >= self->datalen) break;
    rowcount = self->x - 1;
    if (--write == 0) {
      out += self->skiplen;
      write = self->writelen;
    }
    while (rowcount--) {
      diff = nextdiff(self, self->num_huff_idx - 1, 0);
      Px = lastrow[col] + ((left - lastrow[col - 1]) >> 1);
      left = Px + diff;
      // DPRINTF("%d %d %d %d %d
      // %x\n",col,diff,left,lastrow[col],lastrow[col-1],&lastrow[col]);
      if (self->linearize) {
        if (left > self->linlen) return LJ92_ERROR_CORRUPT;
        linear = self->linearize[left];
      } else
        linear = left;
      thisrow[col++] = left;
      out[c++] = linear;
      if (--write == 0) {
        out += self->skiplen;
        write = self->writelen;
      }
    }
    temprow = lastrow;
    lastrow = thisrow;
    thisrow = temprow;
    if (self->ix >= self->datalen) break;
  }
  if (c >= pixels) ret = LJ92_ERROR_NONE;
  return ret;
}

static int parseScan(ljp* self) {
  int ret = LJ92_ERROR_CORRUPT;
  memset(self->sssshist, 0, sizeof(self->sssshist));
  self->ix = self->scanstart;
  int compcount = self->data[self->ix + 2];
  int pred = self->data[self->ix + 3 + 2 * compcount];
  if (pred < 0 || pred > 7) return ret;
  if (pred == 6) return parsePred6(self);  // Fast path
                                           // DPRINTF("pref = %d\n", pred);
  self->ix += BEH(self->data[self->ix]);
  self->cnt = 0;
  self->b = 0;
  // int write = self->writelen;
  // Now need to decode huffman coded values
  // int c = 0;
  // int pixels = self->y * self->x * self->components;
  u16* out = self->image;
  u16* thisrow = self->outrow[0];
  u16* lastrow = self->outrow[1];

  // First pixel predicted from base value
  int diff;
  int Px = 0;
  // int col = 0;
  // int row = 0;
  int left = 0;
  // DPRINTF("w = %d, h = %d, components = %d, skiplen = %d\n", self->x,
  // self->y,
  //       self->components, self->skiplen);
  for (int row = 0; row < self->y; row++) {
    // DPRINTF("row = %d / %d\n", row, self->y);
    for (int col = 0; col < self->x; col++) {
      int colx = col * self->components;
      for (int c = 0; c < self->components; c++) {
        // DPRINTF("c = %d, col = %d, row = %d\n", c, col, row);
        if ((col == 0) && (row == 0)) {
          Px = 1 << (self->bits - 1);
        } else if (row == 0) {
          // Px = left;
          assert(col > 0);
          Px = thisrow[(col - 1) * self->components + c];
        } else if (col == 0) {
          Px = lastrow[c];  // Use value above for first pixel in row
        } else {
          int prev_colx = (col - 1) * self->components;
          // DPRINTF("pred = %d\n", pred);
          switch (pred) {
            case 0:
              Px = 0;
              break;  // No prediction... should not be used
            case 1:
              Px = thisrow[prev_colx + c];
              break;
            case 2:
              Px = lastrow[colx + c];
              break;
            case 3:
              Px = lastrow[prev_colx + c];
              break;
            case 4:
              Px = left + lastrow[colx + c] - lastrow[prev_colx + c];
              break;
            case 5:
              Px = left + ((lastrow[colx + c] - lastrow[prev_colx + c]) >> 1);
              break;
            case 6:
              Px = lastrow[colx + c] + ((left - lastrow[prev_colx + c]) >> 1);
              break;
            case 7:
              Px = (left + lastrow[colx + c]) >> 1;
              break;
          }
        }

        int huff_idx = c;
        if (c >= self->num_huff_idx) {
          // It looks huffman tables are shared for all components.
          // Currently we assume # of huffman tables is 1.
          assert(self->num_huff_idx == 1);
          huff_idx = 0;  // Look up the first huffman table.
        }

        diff = nextdiff(self, huff_idx, Px);
        left = Px + diff;
        // DPRINTF("c[%d] Px = %d, diff = %d, left = %d\n", c, Px, diff, left);
        assert(left >= 0);
        assert(left < (1 << self->bits));
        // DPRINTF("pix = %d\n", left);
        // DPRINTF("%d %d %d\n",c,diff,left);
        int linear;
        if (self->linearize) {
          if (left > self->linlen) return LJ92_ERROR_CORRUPT;
          linear = self->linearize[left];
        } else {
          linear = left;
        }

        // DPRINTF("linear = %d\n", linear);
        thisrow[colx + c] = left;
        out[colx + c] = linear;  // HACK
      }                          // c
    }                            // col

    u16* temprow = lastrow;
    lastrow = thisrow;
    thisrow = temprow;

    out += self->x * self->components + self->skiplen;
    // out += self->skiplen;
    // DPRINTF("out = %p, %p, diff = %lld\n", out, self->image, out -
    // self->image);

  }  // row

  ret = LJ92_ERROR_NONE;

  // if (++col == self->x) {
  //	col = 0;
  //	row++;
  //}
  // if (--write == 0) {
  //	out += self->skiplen;
  //	write = self->writelen;
  //}
  // if (self->ix >= self->datalen + 2) break;

  // if (c >= pixels) ret = LJ92_ERROR_NONE;
  /*for (int h=0;h<17;h++) {
      DPRINTF("ssss:%d=%d
  (%f)\n",h,self->sssshist[h],(float)self->sssshist[h]/(float)(pixels));
  }*/
  return ret;
}

static int parseImage(ljp* self) {
  // DPRINTF("parseImage\n");
  int ret = LJ92_ERROR_NONE;
  while (1) {
    int nextMarker = find(self);
    DPRINTF("marker = 0x%08x\n", nextMarker);
    if (nextMarker == 0xc4)
      ret = parseHuff(self);
    else if (nextMarker == 0xc3)
      ret = parseSof3(self);
    else if (nextMarker == 0xfe)  // Comment
      ret = parseBlock(self, nextMarker);
    else if (nextMarker == 0xd9)  // End of image
      break;
    else if (nextMarker == 0xda) {
      self->scanstart = self->ix;
      ret = LJ92_ERROR_NONE;
      break;
    } else if (nextMarker == -1) {
      ret = LJ92_ERROR_CORRUPT;
      break;
    } else
      ret = parseBlock(self, nextMarker);
    if (ret != LJ92_ERROR_NONE) break;
  }
  return ret;
}

static int findSoI(ljp* self) {
  int ret = LJ92_ERROR_CORRUPT;
  if (find(self) == 0xd8) {
    ret = parseImage(self);
  } else {
    DPRINTF("findSoI: corrupt\n");
  }
  return ret;
}

static void free_memory(ljp* self) {
#ifdef SLOW_HUFF
  free(self->maxcode);
  self->maxcode = NULL;
  free(self->mincode);
  self->mincode = NULL;
  free(self->valptr);
  self->valptr = NULL;
  free(self->huffval);
  self->huffval = NULL;
  free(self->huffsize);
  self->huffsize = NULL;
  free(self->huffcode);
  self->huffcode = NULL;
#else
  for (int i = 0; i < self->num_huff_idx; i++) {
    free(self->hufflut[i]);
    self->hufflut[i] = NULL;
  }
#endif
  free(self->rowcache);
  self->rowcache = NULL;
}

int lj92_open(lj92* lj, const uint8_t* data, int datalen, int* width,
              int* height, int* bitdepth) {
  ljp* self = (ljp*)calloc(sizeof(ljp), 1);
  if (self == NULL) return LJ92_ERROR_NO_MEMORY;

  self->data = (u8*)data;
  self->dataend = self->data + datalen;
  self->datalen = datalen;
  self->num_huff_idx = 0;

  int ret = findSoI(self);

  if (ret == LJ92_ERROR_NONE) {
    u16* rowcache = (u16*)calloc(self->x * self->components * 2, sizeof(u16));
    if (rowcache == NULL)
      ret = LJ92_ERROR_NO_MEMORY;
    else {
      self->rowcache = rowcache;
      self->outrow[0] = rowcache;
      self->outrow[1] = &rowcache[self->x];
    }
  }

  if (ret != LJ92_ERROR_NONE) {  // Failed, clean up
    *lj = NULL;
    free_memory(self);
    free(self);
  } else {
    *width = self->x;
    *height = self->y;
    *bitdepth = self->bits;
    *lj = self;
  }
  return ret;
}

int lj92_decode(lj92 lj, uint16_t* target, int writeLength, int skipLength,
                uint16_t* linearize, int linearizeLength) {
  int ret = LJ92_ERROR_NONE;
  ljp* self = lj;
  if (self == NULL) return LJ92_ERROR_BAD_HANDLE;
  self->image = target;
  self->writelen = writeLength;
  self->skiplen = skipLength;
  self->linearize = linearize;
  self->linlen = linearizeLength;
  ret = parseScan(self);
  return ret;
}

void lj92_close(lj92 lj) {
  ljp* self = lj;
  if (self != NULL) free_memory(self);
  free(self);
}

/* Encoder implementation */

typedef struct _lje {
  uint16_t* image;
  int width;
  int height;
  int bitdepth;
  int components;
  int readLength;
  int skipLength;
  uint16_t* delinearize;
  int delinearizeLength;
  uint8_t* encoded;
  int encodedWritten;
  int encodedLength;
  int hist[17];  // SSSS frequency histogram
  int bits[17];
  int huffval[17];
  u16 huffenc[17];
  u16 huffbits[17];
  int huffsym[17];
} lje;

int frequencyScan(lje* self) {
  // Scan through the tile using the standard type 6 prediction
  // Need to cache the previous 2 row in target coordinates because of tiling
  uint16_t* pixel = self->image;
  int pixcount = self->width * self->height;
  int scan = self->readLength;
  uint16_t* rowcache = (uint16_t*)calloc(1, self->width * self->components * 4);
  uint16_t* rows[2];
  rows[0] = rowcache;
  rows[1] = &rowcache[self->width];

  int col = 0;
  int row = 0;
  int Px = 0;
  int32_t diff = 0;
  int maxval = (1 << self->bitdepth);
  while (pixcount--) {
    uint16_t p = *pixel;
    if (self->delinearize) {
      if (p >= self->delinearizeLength) {
        free(rowcache);
        return LJ92_ERROR_TOO_WIDE;
      }
      p = self->delinearize[p];
    }
    if (p >= maxval) {
      free(rowcache);
      return LJ92_ERROR_TOO_WIDE;
    }
    rows[1][col] = p;

    if ((row == 0) && (col == 0))
      Px = 1 << (self->bitdepth - 1);
    else if (row == 0)
      Px = rows[1][col - 1];
    else if (col == 0)
      Px = rows[0][col];
    else
      Px = rows[0][col] + ((rows[1][col - 1] - rows[0][col - 1]) >> 1);
    diff = rows[1][col] - Px;
    // int ssss = 32 - __builtin_clz(abs(diff));
    int ssss = 32 - clz32(abs(diff));
    if (diff == 0) ssss = 0;
    self->hist[ssss]++;
    // DPRINTF("%d %d %d %d %d %d\n",col,row,p,Px,diff,ssss);
    pixel++;
    scan--;
    col++;
    if (scan == 0) {
      pixel += self->skipLength;
      scan = self->readLength;
    }
    if (col == self->width) {
      uint16_t* tmprow = rows[1];
      rows[1] = rows[0];
      rows[0] = tmprow;
      col = 0;
      row++;
    }
  }
#ifdef LJ92_DEBUG
  int sort[17];
  for (int h = 0; h < 17; h++) {
    sort[h] = h;
    DPRINTF("%d:%d\n", h, self->hist[h]);
  }
#endif
  free(rowcache);
  return LJ92_ERROR_NONE;
}

void createEncodeTable(lje* self) {
  float freq[18];
  int codesize[18];
  int others[18];

  // Calculate frequencies
  float totalpixels = self->width * self->height;
  for (int i = 0; i < 17; i++) {
    freq[i] = (float)(self->hist[i]) / totalpixels;
#ifdef LJ92_DEBUG
    DPRINTF("%d:%f\n", i, freq[i]);
#endif
    codesize[i] = 0;
    others[i] = -1;
  }
  codesize[17] = 0;
  others[17] = -1;
  freq[17] = 1.0f;

  float v1f, v2f;
  int v1, v2;

  while (1) {
    v1f = 3.0f;
    v1 = -1;
    for (int i = 0; i < 18; i++) {
      if ((freq[i] <= v1f) && (freq[i] > 0.0f)) {
        v1f = freq[i];
        v1 = i;
      }
    }
#ifdef LJ92_DEBUG
    DPRINTF("v1:%d,%f\n", v1, v1f);
#endif
    v2f = 3.0f;
    v2 = -1;
    for (int i = 0; i < 18; i++) {
      if (i == v1) continue;
      if ((freq[i] < v2f) && (freq[i] > 0.0f)) {
        v2f = freq[i];
        v2 = i;
      }
    }
    if (v2 == -1) break;  // Done

    freq[v1] += freq[v2];
    freq[v2] = 0.0f;

    while (1) {
      codesize[v1]++;
      if (others[v1] == -1) break;
      v1 = others[v1];
    }
    others[v1] = v2;
    while (1) {
      codesize[v2]++;
      if (others[v2] == -1) break;
      v2 = others[v2];
    }
  }
  int* bits = self->bits;
  memset(bits, 0, sizeof(self->bits));
  for (int i = 0; i < 18; i++) {
    if (codesize[i] != 0) {
      bits[codesize[i]]++;
    }
  }
#ifdef LJ92_DEBUG
  for (int i = 0; i < 17; i++) {
    DPRINTF("bits:%d,%d,%d\n", i, bits[i], codesize[i]);
  }
#endif
  int* huffval = self->huffval;
  int i = 1;
  int k = 0;
  int j;
  memset(huffval, 0, sizeof(self->huffval));
  while (i <= 32) {
    j = 0;
    while (j < 17) {
      if (codesize[j] == i) {
        huffval[k++] = j;
      }
      j++;
    }
    i++;
  }
#ifdef LJ92_DEBUG
  for (i = 0; i < 17; i++) {
    DPRINTF("i=%d,huffval[i]=%x\n", i, huffval[i]);
  }
#endif
  int maxbits = 16;
  while (maxbits > 0) {
    if (bits[maxbits]) break;
    maxbits--;
  }
  u16* huffenc = self->huffenc;
  u16* huffbits = self->huffbits;
  int* huffsym = self->huffsym;
  memset(huffenc, 0, sizeof(self->huffenc));
  memset(huffbits, 0, sizeof(self->huffbits));
  memset(self->huffsym, 0, sizeof(self->huffsym));
  i = 0;
  int hv = 0;
  int rv = 0;
  int vl = 0;  // i
  // int hcode;
  int bitsused = 1;
  int sym = 0;
  // DPRINTF("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
  while (i < 1 << maxbits) {
    if (bitsused > maxbits) {
      break;  // Done. Should never get here!
    }
    if (vl >= bits[bitsused]) {
      bitsused++;
      vl = 0;
      continue;
    }
    if (rv == 1 << (maxbits - bitsused)) {
      rv = 0;
      vl++;
      hv++;
      // DPRINTF("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
      continue;
    }
    huffbits[sym] = bitsused;
    huffenc[sym++] = i >> (maxbits - bitsused);
    // DPRINTF("%d %d %d\n",i,bitsused,hcode);
    i += (1 << (maxbits - bitsused));
    rv = 1 << (maxbits - bitsused);
  }
  for (i = 0; i < 17; i++) {
    if (huffbits[i] > 0) {
      huffsym[huffval[i]] = i;
    }
#ifdef LJ92_DEBUG
    DPRINTF("huffval[%d]=%d,huffenc[%d]=%x,bits=%d\n", i, huffval[i], i,
            huffenc[i], huffbits[i]);
#endif
    if (huffbits[i] > 0) {
      huffsym[huffval[i]] = i;
    }
  }
#ifdef LJ92_DEBUG
  for (i = 0; i < 17; i++) {
    DPRINTF("huffsym[%d]=%d\n", i, huffsym[i]);
  }
#endif
}

void writeHeader(lje* self) {
  int w = self->encodedWritten;
  uint8_t* e = self->encoded;
  e[w++] = 0xff;
  e[w++] = 0xd8;  // SOI
  e[w++] = 0xff;
  e[w++] = 0xc3;  // SOF3
  // Write SOF
  e[w++] = 0x0;
  e[w++] = 11;  // Lf, frame header length
  e[w++] = self->bitdepth;
  e[w++] = self->height >> 8;
  e[w++] = self->height & 0xFF;
  e[w++] = self->width >> 8;
  e[w++] = self->width & 0xFF;
  e[w++] = 1;     // Components
  e[w++] = 0;     // Component ID
  e[w++] = 0x11;  // Component X/Y
  e[w++] = 0;     // Unused (Quantisation)
  e[w++] = 0xff;
  e[w++] = 0xc4;  // HUFF
  // Write HUFF
  int count = 0;
  for (int i = 0; i < 17; i++) {
    count += self->bits[i];
  }
  e[w++] = 0x0;
  e[w++] = 17 + 2 + count;  // Lf, frame header length
  e[w++] = 0;               // Table ID
  for (int i = 1; i < 17; i++) {
    e[w++] = self->bits[i];
  }
  for (int i = 0; i < count; i++) {
    e[w++] = self->huffval[i];
  }
  e[w++] = 0xff;
  e[w++] = 0xda;  // SCAN
  // Write SCAN
  e[w++] = 0x0;
  e[w++] = 8;  // Ls, scan header length
  e[w++] = 1;  // Components
  e[w++] = 0;  //
  e[w++] = 0;  //
  e[w++] = 6;  // Predictor
  e[w++] = 0;  //
  e[w++] = 0;  //
  self->encodedWritten = w;
}

void writePost(lje* self) {
  int w = self->encodedWritten;
  uint8_t* e = self->encoded;
  e[w++] = 0xff;
  e[w++] = 0xd9;  // EOI
  self->encodedWritten = w;
}

void writeBody(lje* self) {
  // Scan through the tile using the standard type 6 prediction
  // Need to cache the previous 2 row in target coordinates because of tiling
  uint16_t* pixel = self->image;
  int pixcount = self->width * self->height;
  int scan = self->readLength;
  uint16_t* rowcache = (uint16_t*)calloc(1, self->width * self->components * 4);
  uint16_t* rows[2];
  rows[0] = rowcache;
  rows[1] = &rowcache[self->width];

  int col = 0;
  int row = 0;
  int Px = 0;
  int32_t diff = 0;
  int bitcount = 0;
  uint8_t* out = self->encoded;
  int w = self->encodedWritten;
  uint8_t next = 0;
  uint8_t nextbits = 8;
  while (pixcount--) {
    uint16_t p = *pixel;
    if (self->delinearize) p = self->delinearize[p];
    rows[1][col] = p;

    if ((row == 0) && (col == 0))
      Px = 1 << (self->bitdepth - 1);
    else if (row == 0)
      Px = rows[1][col - 1];
    else if (col == 0)
      Px = rows[0][col];
    else
      Px = rows[0][col] + ((rows[1][col - 1] - rows[0][col - 1]) >> 1);
    diff = rows[1][col] - Px;
    // int ssss = 32 - __builtin_clz(abs(diff));
    int ssss = 32 - clz32(abs(diff));
    if (diff == 0) ssss = 0;
    // DPRINTF("%d %d %d %d %d\n",col,row,Px,diff,ssss);

    // Write the huffman code for the ssss value
    int huffcode = self->huffsym[ssss];
    int huffenc = self->huffenc[huffcode];
    int huffbits = self->huffbits[huffcode];
    bitcount += huffbits + ssss;

    int vt = ssss > 0 ? (1 << (ssss - 1)) : 0;
// DPRINTF("%d %d %d %d\n",rows[1][col],Px,diff,Px+diff);
#ifdef LJ92_DEBUG
#endif
    if (diff < vt) diff += (1 << (ssss)) - 1;

    // Write the ssss
    while (huffbits > 0) {
      int usebits = huffbits > nextbits ? nextbits : huffbits;
      // Add top usebits from huffval to next usebits of nextbits
      int tophuff = huffenc >> (huffbits - usebits);
      next |= (tophuff << (nextbits - usebits));
      nextbits -= usebits;
      huffbits -= usebits;
      huffenc &= (1 << huffbits) - 1;
      if (nextbits == 0) {
        out[w++] = next;
        if (next == 0xff) out[w++] = 0x0;
        next = 0;
        nextbits = 8;
      }
    }
    // Write the rest of the bits for the value

    while (ssss > 0) {
      int usebits = ssss > nextbits ? nextbits : ssss;
      // Add top usebits from huffval to next usebits of nextbits
      int tophuff = diff >> (ssss - usebits);
      next |= (tophuff << (nextbits - usebits));
      nextbits -= usebits;
      ssss -= usebits;
      diff &= (1 << ssss) - 1;
      if (nextbits == 0) {
        out[w++] = next;
        if (next == 0xff) out[w++] = 0x0;
        next = 0;
        nextbits = 8;
      }
    }

    // DPRINTF("%d %d\n",diff,ssss);
    pixel++;
    scan--;
    col++;
    if (scan == 0) {
      pixel += self->skipLength;
      scan = self->readLength;
    }
    if (col == self->width) {
      uint16_t* tmprow = rows[1];
      rows[1] = rows[0];
      rows[0] = tmprow;
      col = 0;
      row++;
    }
  }
  // Flush the final bits
  if (nextbits < 8) {
    out[w++] = next;
    if (next == 0xff) out[w++] = 0x0;
  }
#ifdef LJ92_DEBUG
  int sort[17];
  for (int h = 0; h < 17; h++) {
    sort[h] = h;
    DPRINTF("%d:%d\n", h, self->hist[h]);
  }
  DPRINTF("Total bytes: %d\n", bitcount >> 3);
#endif
  free(rowcache);
  self->encodedWritten = w;
}

#if 0
/* Encoder
 * Read tile from an image and encode in one shot
 * Return the encoded data
 */
int lj92_encode(uint16_t* image, int width, int height, int bitdepth,
                int readLength, int skipLength, uint16_t* delinearize,
                int delinearizeLength, uint8_t** encoded, int* encodedLength) {
  int ret = LJ92_ERROR_NONE;

  lje* self = (lje*)calloc(sizeof(lje), 1);
  if (self == NULL) return LJ92_ERROR_NO_MEMORY;
  self->image = image;
  self->width = width;
  self->height = height;
  self->bitdepth = bitdepth;
  self->readLength = readLength;
  self->skipLength = skipLength;
  self->delinearize = delinearize;
  self->delinearizeLength = delinearizeLength;
  self->encodedLength = width * height * 3 + 200;
  self->encoded = (uint8_t*)malloc(self->encodedLength);
  if (self->encoded == NULL) {
    free(self);
    return LJ92_ERROR_NO_MEMORY;
  }
  // Scan through data to gather frequencies of ssss prefixes
  ret = frequencyScan(self);
  if (ret != LJ92_ERROR_NONE) {
    free(self->encoded);
    free(self);
    return ret;
  }
  // Create encoded table based on frequencies
  createEncodeTable(self);
  // Write JPEG head and scan header
  writeHeader(self);
  // Scan through and do the compression
  writeBody(self);
  // Finish
  writePost(self);
#ifdef LJ92_DEBUG
  DPRINTF("written:%d\n", self->encodedWritten);
#endif
  self->encoded = (uint8_t*)realloc(self->encoded, self->encodedWritten);
  self->encodedLength = self->encodedWritten;
  *encoded = self->encoded;
  *encodedLength = self->encodedLength;

  free(self);

  return ret;
}
#endif

// End liblj92 ---------------------------------------------------------
}  // namespace
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef enum {
  TAG_IMAGE_WIDTH = 256,
  TAG_IMAGE_HEIGHT = 257,
  TAG_BITS_PER_SAMPLE = 258,
  TAG_COMPRESSION = 259,
  TAG_STRIP_OFFSET = 273,
  TAG_ORIENTATION = 274,
  TAG_SAMPLES_PER_PIXEL = 277,
  TAG_STRIP_BYTE_COUNTS = 279,
  TAG_PLANAR_CONFIGURATION = 284,
  TAG_SUB_IFDS = 330,
  TAG_TILE_WIDTH = 322,
  TAG_TILE_LENGTH = 323,
  TAG_TILE_OFFSETS = 324,
  TAG_TILE_BYTE_COUNTS = 325,
  TAG_JPEG_IF_OFFSET = 513,
  TAG_JPEG_IF_BYTE_COUNT = 514,
  TAG_CFA_PATTERN_DIM = 33421,
  TAG_CFA_PATTERN = 33422,
  TAG_CFA_PLANE_COLOR = 50710,
  TAG_CFA_LAYOUT = 50711,
  TAG_BLACK_LEVEL = 50714,
  TAG_WHITE_LEVEL = 50717,
  TAG_COLOR_MATRIX1 = 50721,
  TAG_COLOR_MATRIX2 = 50722,
  TAG_CAMERA_CALIBRATION1 = 50723,
  TAG_CAMERA_CALIBRATION2 = 50724,
  TAG_DNG_VERSION = 50706,
  TAG_ANALOG_BALANCE = 50727,
  TAG_AS_SHOT_NEUTRAL = 50728,
  TAG_CALIBRATION_ILLUMINANT1 = 50778,
  TAG_CALIBRATION_ILLUMINANT2 = 50779,
  TAG_ACTIVE_AREA = 50829,
  TAG_FORWARD_MATRIX1 = 50964,
  TAG_FORWARD_MATRIX2 = 50965,

  // CR2 extension
  // http://lclevy.free.fr/cr2/
  TAG_CR2_META0 = 50648,
  TAG_CR2_META1 = 50656,
  TAG_CR2_SLICES = 50752,
  TAG_CR2_META2 = 50885,

  TAG_INVALID = 65535
} TiffTag;

static void swap2(unsigned short* val) {
  unsigned short tmp = *val;
  unsigned char* dst = reinterpret_cast<unsigned char*>(val);
  unsigned char* src = reinterpret_cast<unsigned char*>(&tmp);

  dst[0] = src[1];
  dst[1] = src[0];
}

static void swap4(unsigned int* val) {
  unsigned int tmp = *val;
  unsigned char* dst = reinterpret_cast<unsigned char*>(val);
  unsigned char* src = reinterpret_cast<unsigned char*>(&tmp);

  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
}

static unsigned short Read2(FILE* fp, bool swap) {
  unsigned short val;
  size_t ret = fread(&val, 1, 2, fp);
  assert(ret == 2);
  if (swap) {
    swap2(&val);
  }
  return val;
}

static unsigned int Read4(FILE* fp, bool swap) {
  unsigned int val;
  size_t ret = fread(&val, 1, 4, fp);
  assert(ret == 4);
  if (swap) {
    swap4(&val);
  }
  return val;
}

static unsigned int ReadUInt(int type, FILE* fp, bool swap) {
  // @todo {8, 9, 10, 11, 12}
  if (type == 3) {
    unsigned short val;
    size_t ret = fread(&val, 1, 2, fp);
    assert(ret == 2);
    if (swap) {
      swap2(&val);
    }
    return static_cast<unsigned int>(val);
  } else if (type == 5) {
    unsigned int val0;
    size_t ret = fread(&val0, 1, 4, fp);
    assert(ret == 4);
    if (swap) {
      swap4(&val0);
    }

    unsigned int val1;
    ret = fread(&val1, 1, 4, fp);
    assert(ret == 4);
    if (swap) {
      swap4(&val1);
    }

    return static_cast<unsigned int>(val0 / val1);

  } else {  // guess 4.
    assert(type == 4);
    unsigned int val;
    size_t ret = fread(&val, 1, 4, fp);
    assert(ret == 4);
    if (swap) {
      swap4(&val);
    }
    return static_cast<unsigned int>(val);
  }
}

static double ReadReal(int type, FILE* fp, bool swap) {
  assert(type == 5 || type == 10);  // @todo { Support more types. }

  if (type == 5) {
    unsigned int num = static_cast<unsigned int>(Read4(fp, swap));
    unsigned int denom = static_cast<unsigned int>(Read4(fp, swap));

    return static_cast<double>(num) / static_cast<double>(denom);
  } else if (type == 10) {
    int num = static_cast<int>(Read4(fp, swap));
    int denom = static_cast<int>(Read4(fp, swap));

    return static_cast<double>(num) / static_cast<double>(denom);
  } else {
    assert(0);
    return 0.0;
  }
}

static void GetTIFFTag(unsigned short* tag, unsigned short* type,
                       unsigned int* len, unsigned int* saved_offt, FILE* fp,
                       bool swap_endian) {
  size_t ret;
  ret = fread(tag, 1, 2, fp);
  assert(ret == 2);
  ret = fread(type, 1, 2, fp);
  assert(ret == 2);
  ret = fread(len, 1, 4, fp);
  assert(ret == 4);

  if (swap_endian) {
    swap2(tag);
    swap2(type);
    swap4(len);
  }

  (*saved_offt) = static_cast<unsigned int>(ftell(fp)) + 4;

  size_t typesize_table[] = {1, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4};

  if ((*len) * (typesize_table[(*type) < 14 ? (*type) : 0]) > 4) {
    unsigned int base = 0;  // fixme
    unsigned int offt;
    ret = fread(&offt, 1, 4, fp);
    assert(ret == 4);
    if (swap_endian) {
      swap4(&offt);
    }
    fseek(fp, offt + base, SEEK_SET);
  }
}

static void InitializeDNGImage(tinydng::DNGImage* image) {
  image->version = 0;

  image->color_matrix1[0][0] = 1.0;
  image->color_matrix1[0][1] = 0.0;
  image->color_matrix1[0][2] = 0.0;
  image->color_matrix1[1][0] = 0.0;
  image->color_matrix1[1][1] = 1.0;
  image->color_matrix1[1][2] = 0.0;
  image->color_matrix1[2][0] = 0.0;
  image->color_matrix1[2][1] = 0.0;
  image->color_matrix1[2][2] = 1.0;

  image->color_matrix2[0][0] = 1.0;
  image->color_matrix2[0][1] = 0.0;
  image->color_matrix2[0][2] = 0.0;
  image->color_matrix2[1][0] = 0.0;
  image->color_matrix2[1][1] = 1.0;
  image->color_matrix2[1][2] = 0.0;
  image->color_matrix2[2][0] = 0.0;
  image->color_matrix2[2][1] = 0.0;
  image->color_matrix2[2][2] = 1.0;

  image->forward_matrix1[0][0] = 1.0;
  image->forward_matrix1[0][1] = 0.0;
  image->forward_matrix1[0][2] = 0.0;
  image->forward_matrix1[1][0] = 0.0;
  image->forward_matrix1[1][1] = 1.0;
  image->forward_matrix1[1][2] = 0.0;
  image->forward_matrix1[2][0] = 0.0;
  image->forward_matrix1[2][1] = 0.0;
  image->forward_matrix1[2][2] = 1.0;

  image->forward_matrix2[0][0] = 1.0;
  image->forward_matrix2[0][1] = 0.0;
  image->forward_matrix2[0][2] = 0.0;
  image->forward_matrix2[1][0] = 0.0;
  image->forward_matrix2[1][1] = 1.0;
  image->forward_matrix2[1][2] = 0.0;
  image->forward_matrix2[2][0] = 0.0;
  image->forward_matrix2[2][1] = 0.0;
  image->forward_matrix2[2][2] = 1.0;

  image->camera_calibration1[0][0] = 1.0;
  image->camera_calibration1[0][1] = 0.0;
  image->camera_calibration1[0][2] = 0.0;
  image->camera_calibration1[1][0] = 0.0;
  image->camera_calibration1[1][1] = 1.0;
  image->camera_calibration1[1][2] = 0.0;
  image->camera_calibration1[2][0] = 0.0;
  image->camera_calibration1[2][1] = 0.0;
  image->camera_calibration1[2][2] = 1.0;

  image->camera_calibration2[0][0] = 1.0;
  image->camera_calibration2[0][1] = 0.0;
  image->camera_calibration2[0][2] = 0.0;
  image->camera_calibration2[1][0] = 0.0;
  image->camera_calibration2[1][1] = 1.0;
  image->camera_calibration2[1][2] = 0.0;
  image->camera_calibration2[2][0] = 0.0;
  image->camera_calibration2[2][1] = 0.0;
  image->camera_calibration2[2][2] = 1.0;

  image->calibration_illuminant1 = LIGHTSOURCE_UNKNOWN;
  image->calibration_illuminant2 = LIGHTSOURCE_UNKNOWN;

  image->white_level[0] = -1;  // White level will be set after parsing TAG.
                               // The spec says: The default value for this
                               // tag is (2 ** BitsPerSample)
                               // -1 for unsigned integer images, and 1.0 for
                               // floating point images.

  image->white_level[1] = -1;
  image->white_level[2] = -1;
  image->white_level[3] = -1;
  image->black_level[0] = 0;
  image->black_level[1] = 0;
  image->black_level[2] = 0;
  image->black_level[3] = 0;

  image->bits_per_sample = 0;

  image->has_active_area = false;
  image->active_area[0] = -1;
  image->active_area[1] = -1;
  image->active_area[2] = -1;
  image->active_area[3] = -1;

  image->cfa_plane_color[0] = 0;
  image->cfa_plane_color[1] = 1;
  image->cfa_plane_color[2] = 2;
  image->cfa_plane_color[3] = 0;  // optional?

  image->cfa_pattern_dim = 2;

  // The spec says default is None, thus fill with -1(=invalid).
  image->cfa_pattern[0][0] = -1;
  image->cfa_pattern[0][1] = -1;
  image->cfa_pattern[1][0] = -1;
  image->cfa_pattern[1][1] = -1;

  image->cfa_layout = 1;

  image->offset = 0;

  image->tile_width = -1;
  image->tile_length = -1;
  image->tile_offset = 0;

  image->planar_configuration = 1;  // chunky

  image->has_analog_balance = false;
  image->has_as_shot_neutral = false;

  image->jpeg_byte_count = -1;
  image->strip_byte_count = -1;

  image->samples_per_pixel = 1;
  image->bits_per_sample_original = 1;

  image->compression = COMPRESSION_NONE;

  // CR2 specific
  image->cr2_slices[0] = 0;
  image->cr2_slices[1] = 0;
  image->cr2_slices[2] = 0;
}

// Check if JPEG data is lossless JPEG or not(baseline JPEG)
static bool IsLosslessJPEG(const uint8_t* header_addr, int data_len, int* width,
                           int* height, int* bits, int* components) {
  DPRINTF("islossless jpeg\n");
  int lj_width = 0;
  int lj_height = 0;
  int lj_bits = 0;
  lj92 ljp;
  int ret =
      lj92_open(&ljp, header_addr, data_len, &lj_width, &lj_height, &lj_bits);
  if (ret == LJ92_ERROR_NONE) {
    // DPRINTF("w = %d, h = %d, bits = %d, components = %d\n", lj_width,
    // lj_height, lj_bits, ljp->components);

    if ((lj_width == 0) || (lj_height == 0) || (lj_bits == 0) ||
        (lj_bits == 8)) {
      // Looks like baseline JPEG
      lj92_close(ljp);
      return false;
    }

    if (components) (*components) = ljp->components;

    lj92_close(ljp);

    if (width) (*width) = lj_width;
    if (height) (*height) = lj_height;
    if (bits) (*bits) = lj_bits;
  }
  return (ret == LJ92_ERROR_NONE) ? true : false;
}

// Decompress LosslesJPEG adta.
// Need to pass whole DNG file content to `src` and `src_length` to support
// decoding LJPEG in tiled format.
// (i.e, src = the beginning of the file, src_len = file size)
static bool DecompressLosslessJPEG(unsigned short* dst_data, int dst_width,
                                   const unsigned char* src,
                                   const size_t src_length, FILE* fp,
                                   const DNGImage& image_info,
                                   bool swap_endian) {
  // @todo { Remove FILE dependency. }
  //
  unsigned int tiff_h = 0, tiff_w = 0;
  int offset = 0;

#ifdef TINY_DNG_LOADER_PROFILING
  auto start_t = std::chrono::system_clock::now();
#endif

  if ((image_info.tile_width > 0) && (image_info.tile_length > 0)) {
    // Assume Lossless JPEG data is stored in tiled format.
    assert(image_info.tile_width > 0);
    assert(image_info.tile_length > 0);

    // <-       image width(skip len)           ->
    // +-----------------------------------------+
    // |                                         |
    // |              <- tile w  ->              |
    // |              +-----------+              |
    // |              |           | \            |
    // |              |           | |            |
    // |              |           | | tile h     |
    // |              |           | |            |
    // |              |           | /            |
    // |              +-----------+              |
    // |                                         |
    // |                                         |
    // +-----------------------------------------+

    // DPRINTF("tile = %d, %d\n", image_info.tile_width,
    // image_info.tile_length);

    // Currently we only support tile data for tile.length == tiff.height.
    // assert(image_info.tile_length == image_info.height);

    size_t column_step = 0;
    while (tiff_h < static_cast<unsigned int>(image_info.height)) {
      // Read offset to JPEG data location.
      offset = static_cast<int>(Read4(fp, swap_endian));
      DPRINTF("offt = %d\n", offset);

      int lj_width = 0;
      int lj_height = 0;
      int lj_bits = 0;
      lj92 ljp;

      size_t input_len = src_length - static_cast<size_t>(offset);

      // @fixme { Parse LJPEG header first and set exact compressed LJPEG data
      // length to `data_len` arg. }
      int ret = lj92_open(&ljp, reinterpret_cast<const uint8_t*>(&src[offset]),
                          /* data_len */ static_cast<int>(input_len), &lj_width,
                          &lj_height, &lj_bits);
      DPRINTF("ret = %d\n", ret);
      assert(ret == LJ92_ERROR_NONE);

      // DPRINTF("lj %d, %d, %d\n", lj_width, lj_height, lj_bits);
      // DPRINTF("ljp x %d, y %d, c %d\n", ljp->x, ljp->y, ljp->components);
      // DPRINTF("tile width = %d\n", image_info.tile_width);
      // DPRINTF("tile height = %d\n", image_info.tile_length);
      // DPRINTF("col = %d, tiff_w = %d / %d\n", column_step, tiff_w,
      // image_info.width);

      assert((lj_width * ljp->components * lj_height) ==
             image_info.tile_width * image_info.tile_length);

      // int write_length = image_info.tile_width;
      // int skip_length = dst_width - image_info.tile_width;
      // DPRINTF("write_len = %d, skip_len = %d\n", write_length, skip_length);

      // size_t dst_offset =
      //    column_step * static_cast<size_t>(image_info.tile_width) +
      //    static_cast<unsigned int>(dst_width) * tiff_h;

      // Decode into temporary buffer.

      std::vector<uint16_t> tmpbuf;
      tmpbuf.resize(
          static_cast<size_t>(lj_width * lj_height * ljp->components));

      ret = lj92_decode(ljp, tmpbuf.data(), image_info.tile_width, 0, NULL, 0);
      assert(ret == LJ92_ERROR_NONE);
      (void)ret;
      // ret = lj92_decode(ljp, dst_data + dst_offset, write_length,
      // skip_length,
      //                  NULL, 0);

      // Copy to dest buffer.
      // NOTE: For some DNG file, tiled image may exceed the extent of target
      // image resolution.
      for (unsigned int y = 0;
           y < static_cast<unsigned int>(image_info.tile_length); y++) {
        unsigned int y_offset = y + tiff_h;
        if (y_offset >= static_cast<unsigned int>(image_info.height)) {
          continue;
        }

        size_t dst_offset =
            tiff_w + static_cast<unsigned int>(dst_width) * y_offset;

        size_t x_len = static_cast<size_t>(image_info.tile_width);
        if ((tiff_w + static_cast<unsigned int>(image_info.tile_width)) >=
            static_cast<unsigned int>(dst_width)) {
          x_len = static_cast<size_t>(dst_width) - tiff_w;
        }
        for (size_t x = 0; x < x_len; x++) {
          dst_data[dst_offset + x] =
              tmpbuf[y * static_cast<size_t>(image_info.tile_width) + x];
        }
      }

      assert(ret == LJ92_ERROR_NONE);

      lj92_close(ljp);

      tiff_w += static_cast<unsigned int>(image_info.tile_width);
      column_step++;
      // DPRINTF("col = %d, tiff_w = %d / %d\n", column_step, tiff_w,
      // image_info.width);
      if (tiff_w >= static_cast<unsigned int>(image_info.width)) {
        // tiff_h += static_cast<unsigned int>(image_info.tile_length);
        tiff_h += static_cast<unsigned int>(image_info.tile_length);
        // DPRINTF("tiff_h = %d\n", tiff_h);
        tiff_w = 0;
        column_step = 0;
      }
    }
  } else {
    // Assume LJPEG data is not stored in tiled format.

    // Read offset to JPEG data location.
    // offset = static_cast<int>(Read4(fp, swap_endian));
    // DPRINTF("offt = %d\n", offset);

    assert(image_info.offset > 0);
    offset = static_cast<int>(image_info.offset);

    int lj_width = 0;
    int lj_height = 0;
    int lj_bits = 0;
    lj92 ljp;

    size_t input_len = src_length - static_cast<size_t>(offset);

    // @fixme { Parse LJPEG header first and set exact compressed LJPEG data
    // length to `data_len` arg. }
    int ret = lj92_open(&ljp, reinterpret_cast<const uint8_t*>(&src[offset]),
                        /* data_len */ static_cast<int>(input_len), &lj_width,
                        &lj_height, &lj_bits);
    // DPRINTF("ret = %d\n", ret);
    assert(ret == LJ92_ERROR_NONE);

    // DPRINTF("lj %d, %d, %d\n", lj_width, lj_height, lj_bits);

    int write_length = image_info.width;
    int skip_length = 0;

    ret = lj92_decode(ljp, dst_data, write_length, skip_length, NULL, 0);
    // DPRINTF("ret = %d\n", ret);

    assert(ret == LJ92_ERROR_NONE);

    lj92_close(ljp);
  }

#ifdef TINY_DNG_LOADER_PROFILING
  auto end_t = std::chrono::system_clock::now();
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t);

  std::cout << "DecompressLosslessJPEG : " << ms.count() << " [ms]"
            << std::endl;
#endif

  return true;
}

// Parse TIFF IFD.
// Returns true upon success, false if failed to parse.
static bool ParseTIFFIFD(std::vector<tinydng::DNGImage>* images, FILE* fp,
                         bool swap_endian) {
  tinydng::DNGImage image;
  InitializeDNGImage(&image);

  // DPRINTF("id = %d\n", idx);
  unsigned short num_entries = 0;
  size_t ret = fread(&num_entries, 1, 2, fp);
  // DPRINTF("ret = %ld\n", ret);
  if (swap_endian) {
    swap2(&num_entries);
  }

  assert(ret == 2);
  if (num_entries == 0) {
    assert(0);
    return false;  // @fixme
  }

  // TIFFInfo info;
  // InitializeTIFFInfo(&info);

  // DPRINTF("----------\n");

  while (num_entries--) {
    unsigned short tag, type;
    unsigned int len;
    unsigned int saved_offt;
    GetTIFFTag(&tag, &type, &len, &saved_offt, fp, swap_endian);
    // DPRINTF("tag = %d\n", tag);

    switch (tag) {
      case 2:
      case TAG_IMAGE_WIDTH:
      case 61441:  // ImageWidth
        image.width = static_cast<int>(ReadUInt(type, fp, swap_endian));
        // DPRINTF("[%d] width = %d\n", idx, info.width);
        break;

      case 3:
      case TAG_IMAGE_HEIGHT:
      case 61442:  // ImageHeight
        image.height = static_cast<int>(ReadUInt(type, fp, swap_endian));
        // DPRINTF("height = %d\n", image.height);
        break;

      case TAG_BITS_PER_SAMPLE:
      case 61443:  // BitsPerSample
        // image->samples = len & 7;
        image.bits_per_sample_original =
            static_cast<int>(ReadUInt(type, fp, swap_endian));
        break;

      case TAG_SAMPLES_PER_PIXEL:
        image.samples_per_pixel = static_cast<int>(Read2(fp, swap_endian));
        assert(image.samples_per_pixel <= 4);
        // DPRINTF("spp = %d\n", image.samples_per_pixel);
        break;

      case TAG_COMPRESSION:
        image.compression = static_cast<int>(ReadUInt(type, fp, swap_endian));
        // DPRINTF("tag-compression = %d\n", image.compression);
        break;

      case TAG_STRIP_OFFSET:
      case TAG_JPEG_IF_OFFSET:
        image.offset = Read4(fp, swap_endian);
        // DPRINTF("strip_offset = %d\n", image.offset);
        break;

      case TAG_JPEG_IF_BYTE_COUNT:
        image.jpeg_byte_count = static_cast<int>(Read4(fp, swap_endian));
        break;

      case TAG_ORIENTATION:
        image.orientation = Read2(fp, swap_endian);
        break;

      case TAG_STRIP_BYTE_COUNTS:
        image.strip_byte_count = static_cast<int>(Read4(fp, swap_endian));
        // DPRINTF("strip_byte_count = %d\n", image->strip_byte_count);
        break;

      case TAG_PLANAR_CONFIGURATION:
        image.planar_configuration = Read2(fp, swap_endian);
        break;

      case TAG_SUB_IFDS:

      {
        // DPRINTF("sub_ifds = %d\n", len);
        for (size_t k = 0; k < len; k++) {
          unsigned int i = static_cast<unsigned int>(ftell(fp));
          unsigned int offt = Read4(fp, swap_endian);
          unsigned int base = 0;  // @fixme
          fseek(fp, offt + base, SEEK_SET);

          ParseTIFFIFD(images, fp, swap_endian);  // recursive call
          fseek(fp, i + 4, SEEK_SET);             // rewind
        }
        // DPRINTF("sub_ifds DONE\n");
      }

      break;

      case TAG_TILE_WIDTH:
        image.tile_width = static_cast<int>(ReadUInt(type, fp, swap_endian));
        // DPRINTF("tile_width = %d\n", image.tile_width);
        break;

      case TAG_TILE_LENGTH:
        image.tile_length = static_cast<int>(ReadUInt(type, fp, swap_endian));
        // DPRINTF("tile_length = %d\n", image.tile_length);
        break;

      case TAG_TILE_OFFSETS:
        image.tile_offset = len > 1 ? static_cast<unsigned int>(ftell(fp))
                                    : Read4(fp, swap_endian);
        // DPRINTF("tile_offt = %d\n", image->tile_offset);
        break;

      case TAG_TILE_BYTE_COUNTS:
        image.tile_byte_count = len > 1 ? static_cast<unsigned int>(ftell(fp))
                                        : Read4(fp, swap_endian);
        break;

      case TAG_CFA_PATTERN_DIM:
        image.cfa_pattern_dim = Read2(fp, swap_endian);
        break;

      case TAG_CFA_PATTERN: {
        char buf[16];
        size_t readLen = len;
        if (readLen > 16) readLen = 16;
        // Assume 2x2 CFAPattern.
        assert(readLen == 4);
        size_t n = fread(buf, 1, readLen, fp);
        assert(n == readLen);
        (void)n;
        image.cfa_pattern[0][0] = buf[0];
        image.cfa_pattern[0][1] = buf[1];
        image.cfa_pattern[1][0] = buf[2];
        image.cfa_pattern[1][1] = buf[3];
      } break;

      case TAG_DNG_VERSION: {
        char data[4];
        data[0] = static_cast<char>(fgetc(fp));
        data[1] = static_cast<char>(fgetc(fp));
        data[2] = static_cast<char>(fgetc(fp));
        data[3] = static_cast<char>(fgetc(fp));

        image.version =
            (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
      } break;

      case TAG_CFA_PLANE_COLOR: {
        char buf[4];
        size_t readLen = len;
        if (readLen > 4) readLen = 4;
        size_t n = fread(buf, 1, readLen, fp);
        assert(n == readLen);
        (void)n;
        for (size_t i = 0; i < readLen; i++) {
          image.cfa_plane_color[i] = buf[i];
        }
      } break;

      case TAG_CFA_LAYOUT: {
        int layout = Read2(fp, swap_endian);
        image.cfa_layout = layout;
      } break;

      case TAG_ACTIVE_AREA:
        image.has_active_area = true;
        image.active_area[0] =
            static_cast<int>(ReadUInt(type, fp, swap_endian));
        image.active_area[1] =
            static_cast<int>(ReadUInt(type, fp, swap_endian));
        image.active_area[2] =
            static_cast<int>(ReadUInt(type, fp, swap_endian));
        image.active_area[3] =
            static_cast<int>(ReadUInt(type, fp, swap_endian));
        break;

      case TAG_BLACK_LEVEL: {
        // Assume TAG_SAMPLES_PER_PIXEL is read before
        // FIXME(syoyo): scan TAG_SAMPLES_PER_PIXEL in IFD table in advance.
        for (int s = 0; s < image.samples_per_pixel; s++) {
          image.black_level[s] =
              static_cast<int>(ReadUInt(type, fp, swap_endian));
        }
      } break;

      case TAG_WHITE_LEVEL: {
        // Assume TAG_SAMPLES_PER_PIXEL is read before
        // FIXME(syoyo): scan TAG_SAMPLES_PER_PIXEL in IFD table in advance.
        for (int s = 0; s < image.samples_per_pixel; s++) {
          image.white_level[s] =
              static_cast<int>(ReadUInt(type, fp, swap_endian));
        }
      } break;

      case TAG_ANALOG_BALANCE:
        // Assume RGB
        image.analog_balance[0] = ReadReal(type, fp, swap_endian);
        image.analog_balance[1] = ReadReal(type, fp, swap_endian);
        image.analog_balance[2] = ReadReal(type, fp, swap_endian);
        image.has_analog_balance = true;
        break;

      case TAG_AS_SHOT_NEUTRAL:
        // Assume RGB
        // DPRINTF("ty = %d\n", type);
        image.as_shot_neutral[0] = ReadReal(type, fp, swap_endian);
        image.as_shot_neutral[1] = ReadReal(type, fp, swap_endian);
        image.as_shot_neutral[2] = ReadReal(type, fp, swap_endian);
        image.has_as_shot_neutral = true;
        break;

      case TAG_CALIBRATION_ILLUMINANT1:
        image.calibration_illuminant1 =
            static_cast<LightSource>(Read2(fp, swap_endian));
        break;

      case TAG_CALIBRATION_ILLUMINANT2:
        image.calibration_illuminant2 =
            static_cast<LightSource>(Read2(fp, swap_endian));
        break;

      case TAG_COLOR_MATRIX1: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.color_matrix1[c][k] = val;
          }
        }
      } break;

      case TAG_COLOR_MATRIX2: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.color_matrix2[c][k] = val;
          }
        }
      } break;

      case TAG_FORWARD_MATRIX1: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.forward_matrix1[c][k] = val;
          }
        }
      } break;

      case TAG_FORWARD_MATRIX2: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.forward_matrix2[c][k] = val;
          }
        }
      } break;

      case TAG_CAMERA_CALIBRATION1: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.camera_calibration1[c][k] = val;
          }
        }
      } break;

      case TAG_CAMERA_CALIBRATION2: {
        for (int c = 0; c < 3; c++) {
          for (int k = 0; k < 3; k++) {
            double val = ReadReal(type, fp, swap_endian);
            image.camera_calibration2[c][k] = val;
          }
        }
      } break;

      case TAG_CR2_SLICES: {
        // Assume 3 ushorts;
        image.cr2_slices[0] = Read2(fp, swap_endian);
        image.cr2_slices[1] = Read2(fp, swap_endian);
        image.cr2_slices[2] = Read2(fp, swap_endian);
        // DPRINTF("cr2_slices = %d, %d, %d\n",
        //  image.cr2_slices[0],
        //  image.cr2_slices[1],
        //  image.cr2_slices[2]);
      } break;

      default:
        // DPRINTF("unknown or unsupported tag = %d\n", tag);
        break;
    }

    fseek(fp, saved_offt, SEEK_SET);
  }

  // Add to images.
  images->push_back(image);

  // DPRINTF("DONE ---------\n");

  return true;
}

static bool ParseDNG(std::vector<tinydng::DNGImage>* images, FILE* fp,
                     bool swap_endian) {
  int offt;
  size_t ret = fread(&offt, 1, 4, fp);
  assert(ret == 4);

  if (swap_endian) {
    swap4(reinterpret_cast<unsigned int*>(&offt));
  }

  assert(images);

  while (offt) {
    fseek(fp, offt, SEEK_SET);

    // DPRINTF("Parse TIFF IFD\n");
    if (!ParseTIFFIFD(images, fp, swap_endian)) {
      break;
    }
    // Get next IFD offset(0 = end of file).
    ret = fread(&offt, 1, 4, fp);
    // DPRINTF("Next IFD offset = %d\n", offt);
    assert(ret == 4);
  }

  for (size_t i = 0; i < images->size(); i++) {
    tinydng::DNGImage* image = &((*images)[i]);
    assert(image->samples_per_pixel <= 4);
    for (int s = 0; s < image->samples_per_pixel; s++) {
      if (image->white_level[s] == -1) {
        // Set white level with (2 ** BitsPerSample) according to the DNG spec.
        assert(image->bits_per_sample_original > 0);
        assert(image->bits_per_sample_original < 32);
        image->white_level[s] = (1 << image->bits_per_sample_original);
      }
    }
  }

  return true;
}

static bool IsBigEndian() {
  uint32_t i = 0x01020304;
  char c[4];
  memcpy(c, &i, 4);
  return (c[0] == 1);
}

bool LoadDNG(std::vector<DNGImage>* images, std::string* err,
             const char* filename) {
  std::stringstream ss;

  assert(images);

#ifdef _MSC_VER
  FILE *fp;
  fopen_s(&fp, filename, "rb");
#else
  FILE* fp = fopen(filename, "rb");
#endif
  if (!fp) {
    ss << "File not found or cannot open file " << filename << std::endl;
    if (err) {
      (*err) = ss.str();
    }
    return false;
  }

  unsigned short magic;
  char header[32];

  size_t ret;
  ret = fread(&magic, 1, 2, fp);
  assert(ret == 2);

  int seek_ret = fseek(fp, 0, SEEK_SET);
  assert(seek_ret == 0);

  ret = fread(header, 1, 32, fp);
  assert(ret == 32);
  seek_ret = fseek(fp, 0, SEEK_END);
  assert(seek_ret == 0);

  size_t file_size = static_cast<size_t>(ftell(fp));

  // Buffer to hold whole DNG file data.
  std::vector<unsigned char> whole_data;
  {
    whole_data.resize(file_size);
    fseek(fp, 0, SEEK_SET);
    size_t read_len = fread(whole_data.data(), 1, file_size, fp);
    assert(read_len == file_size);

    fseek(fp, 0, SEEK_SET);
  }

  bool is_dng_big_endian = false;

  if (magic == 0x4949) {
    // might be TIFF(DNG).
  } else if (magic == 0x4d4d) {
    // might be TIFF(DNG, bigendian).
    is_dng_big_endian = true;
  } else {
    ss << "Seems the file is not a DNG format." << std::endl;
    if (err) {
      (*err) = ss.str();
    }

    fclose(fp);
    return false;
  }

  // skip magic header
  fseek(fp, 4, SEEK_SET);

  const bool swap_endian = (is_dng_big_endian && (!IsBigEndian()));
  ret = ParseDNG(images, fp, swap_endian);

  if (!ret) {
    fclose(fp);
    return false;
  }

  for (size_t i = 0; i < images->size(); i++) {
    tinydng::DNGImage* image = &((*images)[i]);
    // DPRINTF("[%lu] compression = %d\n", i, image->compression);

    const size_t data_offset =
        (image->offset > 0) ? image->offset : image->tile_offset;
    assert(data_offset > 0);

    // std::cout << "offt =\n" << image->offset << std::endl;
    // std::cout << "tile_offt = \n" << image->tile_offset << std::endl;
    // std::cout << "data_offset = " << data_offset << std::endl;

    if (image->compression == COMPRESSION_NONE) {  // no compression

      if (image->jpeg_byte_count > 0) {
        // Looks like CR2 IFD#1(thumbnail jpeg image)
        // Currently skip parsing jpeg data.
        // TODO(syoyo): Decode jpeg data.
        image->width = 0;
        image->height = 0;
      } else {
        image->bits_per_sample = image->bits_per_sample_original;
        assert(((image->width * image->height * image->bits_per_sample) % 8) ==
               0);
        const size_t len =
            static_cast<size_t>((image->samples_per_pixel * image->width *
                                 image->height * image->bits_per_sample) /
                                8);
        assert(len > 0);
        image->data.resize(len);
        fseek(fp, static_cast<long>(data_offset), SEEK_SET);

        ret = fread(&(image->data.at(0)), 1, len, fp);
        assert(ret == len);
      }
    } else if (image->compression ==
               COMPRESSION_OLD_JPEG) {  // old jpeg compression

      // std::cout << "IFD " << i << std::endl;

      // First check if JPEG is lossless JPEG
      // TODO(syoyo): Compure conservative data_len.
      assert(file_size > data_offset);
      size_t data_len = file_size - data_offset;
      int lj_width = -1, lj_height = -1, lj_bits = -1, lj_components = -1;
      if (IsLosslessJPEG(&whole_data.at(data_offset),
                         static_cast<int>(data_len), &lj_width, &lj_height,
                         &lj_bits, &lj_components)) {
        // std::cout << "IFD " << i << " is LJPEG" << std::endl;

        assert(lj_width > 0);
        assert(lj_height > 0);
        assert(lj_bits > 0);
        assert(lj_components > 0);

        // Assume not in tiled format.
        assert(image->tile_width == -1);
        assert(image->tile_length == -1);

        image->height = lj_height;

        // Is Canon CR2?
        const bool is_cr2 = (image->cr2_slices[0] != 0) ? true : false;

        if (is_cr2) {
          // For CR2 RAW, slices[0] * slices[1] + slices[2] = image width
          image->width = image->cr2_slices[0] * image->cr2_slices[1] +
                         image->cr2_slices[2];
        } else {
          image->width = lj_width;
        }

        image->bits_per_sample_original = lj_bits;

        // lj92 decodes data into 16bits, so modify bps.
        image->bits_per_sample = 16;

        assert(((image->width * image->height * image->bits_per_sample) % 8) ==
               0);
        const size_t len =
            static_cast<size_t>((image->samples_per_pixel * image->width *
                                 image->height * image->bits_per_sample) /
                                8);
        // std::cout << "spp = " << image->samples_per_pixel;
        // std::cout << ", w = " << image->width << ", h = " << image->height <<
        // ", bps = " << image->bits_per_sample << std::endl;
        assert(len > 0);
        image->data.resize(len);

        assert(file_size > data_offset);

        std::vector<unsigned short> buf;
        buf.resize(static_cast<size_t>(image->width * image->height *
                                       image->samples_per_pixel));

        bool ok = DecompressLosslessJPEG(&buf.at(0), image->width,
                                         &(whole_data.at(0)), file_size, fp,
                                         (*image), swap_endian);
        if (!ok) {
          if (err) {
            ss << "Failed to decompress LJPEG." << std::endl;
            (*err) = ss.str();
          }
          return false;
        }

        if (is_cr2) {
          // CR2 stores image in tiled format(image slices. left to right).
          // Convert it to scanline format.
          int nslices = image->cr2_slices[0];
          int slice_width = image->cr2_slices[1];
          int slice_remainder_width = image->cr2_slices[2];
          size_t src_offset = 0;

          unsigned short* dst_ptr =
              reinterpret_cast<unsigned short*>(image->data.data());

          for (int slice = 0; slice < nslices; slice++) {
            int x_offset = slice * slice_width;
            for (int y = 0; y < image->height; y++) {
              size_t dst_offset =
                  static_cast<size_t>(y * image->width + x_offset);
              memcpy(&dst_ptr[dst_offset], &buf[src_offset],
                     sizeof(unsigned short) * static_cast<size_t>(slice_width));
              src_offset += static_cast<size_t>(slice_width);
            }
          }

          // remainder(the last slice).
          {
            int x_offset = nslices * slice_width;
            for (int y = 0; y < image->height; y++) {
              size_t dst_offset =
                  static_cast<size_t>(y * image->width + x_offset);
              // std::cout << "y = " << y << ", dst = " << dst_offset << ", src
              // = " << src_offset << ", len = " << buf.size() << std::endl;
              memcpy(&dst_ptr[dst_offset], &buf[src_offset],
                     sizeof(unsigned short) *
                         static_cast<size_t>(slice_remainder_width));
              src_offset += static_cast<size_t>(slice_remainder_width);
            }
          }

        } else {
          memcpy(image->data.data(), static_cast<void*>(&(buf.at(0))), len);
        }

      } else {
        // Baseline 8bit JPEG

        image->bits_per_sample = 8;

        size_t jpeg_len = static_cast<size_t>(image->jpeg_byte_count);
        if (image->jpeg_byte_count == -1) {
          // No jpeg datalen. Set to the size of file - offset.
          assert(file_size > data_offset);
          jpeg_len = file_size - data_offset;
        }
        assert(jpeg_len > 0);

        // Assume RGB jpeg
        int w = 0, h = 0, components = 0;
        unsigned char* decoded_image = stbi_load_from_memory(
            &whole_data.at(data_offset), static_cast<int>(jpeg_len), &w, &h,
            &components, /* desired_channels */ 3);
        assert(decoded_image);

        // Currently we just discard JPEG image(since JPEG image would be just a
        // thumbnail or LDR image of RAW).
        // TODO(syoyo): Do not discard JPEG image.
        free(decoded_image);

        // std::cout << "w = " << w << std::endl;
        // std::cout << "h = " << w << std::endl;
        // std::cout << "c = " << components << std::endl;

        assert(w > 0);
        assert(h > 0);

        image->width = w;
        image->height = h;
      }

    } else if (image->compression ==
               COMPRESSION_NEW_JPEG) {  //  new JPEG(baseline DCT JPEG or
                                        //  lossless JPEG)

      // lj92 decodes data into 16bits, so modify bps.
      image->bits_per_sample = 16;

      // std::cout << "w = " << image->width << ", h = " << image->height <<
      // std::endl;

      assert(((image->width * image->height * image->bits_per_sample) % 8) ==
             0);
      const size_t len =
          static_cast<size_t>((image->samples_per_pixel * image->width *
                               image->height * image->bits_per_sample) /
                              8);
      assert(len > 0);
      image->data.resize(len);

      assert(file_size > data_offset);

      fseek(fp, static_cast<long>(data_offset), SEEK_SET);

      bool ok = DecompressLosslessJPEG(
          reinterpret_cast<unsigned short*>(&(image->data.at(0))), image->width,
          &(whole_data.at(0)), file_size, fp, (*image), swap_endian);
      if (!ok) {
        if (err) {
          ss << "Failed to decompress LJPEG." << std::endl;
          (*err) = ss.str();
        }
        return false;
      }

    } else if (image->compression == 8) {  // ZIP
      if (err) {
        ss << "ZIP compression is not supported." << std::endl;
        (*err) = ss.str();
      }
    } else if (image->compression == 34892) {  // lossy JPEG
      if (err) {
        ss << "lossy JPEG compression is not supported." << std::endl;
        (*err) = ss.str();
      }
    } else if (image->compression == 34713) {  // NEF lossless?
      if (err) {
        ss << "Seems a NEF RAW. This compression is not supported."
           << std::endl;
        (*err) = ss.str();
      }
    } else {
      if (err) {
        ss << "IFD [" << i << "] "
           << " Unsupported compression type : " << image->compression
           << std::endl;
        (*err) = ss.str();
      }
      return false;
    }

#if 0
    if (info->has_active_area) {
      info->active_area[0] = image->active_area[0];
      info->active_area[1] = image->active_area[1];
      info->active_area[2] = image->active_area[2];
      info->active_area[3] = image->active_area[3];
    } else {
      info->active_area[0] = 0;
      info->active_area[1] = 0;
      info->active_area[2] = image->width;
      info->active_area[3] = image->height;
    }

    info->cfa_plane_color[0] = image->cfa_plane_color[0];
    info->cfa_plane_color[1] = image->cfa_plane_color[1];
    info->cfa_plane_color[2] = image->cfa_plane_color[2];
    info->cfa_plane_color[3] = image->cfa_plane_color[3];
    info->cfa_pattern[0][0] = image->cfa_pattern[0][0];
    info->cfa_pattern[0][1] = image->cfa_pattern[0][1];
    info->cfa_pattern[1][0] = image->cfa_pattern[1][0];
    info->cfa_pattern[1][1] = image->cfa_pattern[1][1];
    info->cfa_layout = image->cfa_layout;

    info->has_as_shot_neutral = image->has_as_shot_neutral;
    info->as_shot_neutral[0] = image->as_shot_neutral[0];
    info->as_shot_neutral[1] = image->as_shot_neutral[1];
    info->as_shot_neutral[2] = image->as_shot_neutral[2];

    info->has_analog_balance = image->has_analog_balance;
    info->analog_balance[0] = image->analog_balance[0];
    info->analog_balance[1] = image->analog_balance[1];
    info->analog_balance[2] = image->analog_balance[2];

    info->tile_width = image->tile_width;
    info->tile_length = image->tile_length;
    info->tile_offset = image->tile_offset;
#endif
  }

  fclose(fp);

  return ret ? true : false;
}

}  // namespace tinydng

#endif

#endif  // TINY_DNG_LOADER_H_
