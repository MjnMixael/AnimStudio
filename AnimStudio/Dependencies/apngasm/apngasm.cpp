#include "apngasm.h"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <png.h>
#include <zlib.h>

#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

#define id_IHDR 0x52444849
#define id_acTL 0x4C546361
#define id_fcTL 0x4C546366
#define id_IDAT 0x54414449
#define id_fdAT 0x54416466
#define id_IEND 0x444E4549

namespace {

  typedef struct { unsigned int num; unsigned char r, g, b, a; } COLORS;

  int compareColors(const void *arg1, const void *arg2)
  {
    if ( ((COLORS*)arg1)->a != ((COLORS*)arg2)->a )
      return (int)(((COLORS*)arg1)->a) - (int)(((COLORS*)arg2)->a);

    if ( ((COLORS*)arg1)->num != ((COLORS*)arg2)->num )
      return (int)(((COLORS*)arg2)->num) - (int)(((COLORS*)arg1)->num);

    if ( ((COLORS*)arg1)->r != ((COLORS*)arg2)->r )
      return (int)(((COLORS*)arg1)->r) - (int)(((COLORS*)arg2)->r);

    if ( ((COLORS*)arg1)->g != ((COLORS*)arg2)->g )
      return (int)(((COLORS*)arg1)->g) - (int)(((COLORS*)arg2)->g);

    return (int)(((COLORS*)arg1)->b) - (int)(((COLORS*)arg2)->b);
  }

} // unnamed namespace

namespace apngasm {

  //Construct APNGAsm object
  APNGAsm::APNGAsm(void)
    : _loops(0)
    , _skipFirst(false)
  {
    // nop
  }

  //Construct APNGAsm object
  APNGAsm::APNGAsm(const std::vector<APNGFrame> &frames)
    : _loops(0)
    , _skipFirst(false)
  {
    _frames.insert(_frames.end(), frames.begin(), frames.end());
  }

  //Returns the version of APNGAsm
  const char* APNGAsm::version(void) const
  {
    return "3.1.0*";
  }

  // Returns the vector of frames.
  const std::vector<APNGFrame>& APNGAsm::getFrames() const
  {
    return _frames;
  }

  // Returns the loop count of animation.
  unsigned int APNGAsm::getLoops() const
  {
    return _loops;
  }

  // Returns the flag of skip first frame.
  bool APNGAsm::isSkipFirst() const
  {
    return _skipFirst;
  }

  size_t APNGAsm::frameCount()
  {
    return _frames.size();
  }

  size_t APNGAsm::reset()
  {
    if (_frames.empty())
      return 0;

    for (size_t n = 0; n < _frames.size(); ++n)
    {
      delete[] _frames[n]._pixels;
      delete[] _frames[n]._rows;
    }
    _frames.clear();

    return _frames.size();
  }

  APNGAsm::~APNGAsm()
  {
    reset();
  }

  void APNGAsm::setProgressCallback(std::function<void(float)> cb) {
      _progressCallback = std::move(cb);
  }

  //Adds an APNGFrame object to the frame vector
  //Returns the frame number in the frame vector
  size_t APNGAsm::addFrame(const APNGFrame &frame)
  {
    _frames.push_back(frame);
    return _frames.size();
  }

  //Adds an APNGFrame object to the frame vector
  //Returns the frame number in the frame vector
  size_t APNGAsm::addFrame(rgb *pixels, unsigned int width, unsigned int height, rgb *trns_color, unsigned delayNum, unsigned delayDen)
  {
    return addFrame(APNGFrame(pixels, width, height, trns_color, delayNum, delayDen));
  }

  //Adds an APNGFrame object to the frame vector
  //Returns the frame number in the frame vector
  size_t APNGAsm::addFrame(rgba *pixels, unsigned int width, unsigned int height, unsigned delayNum, unsigned delayDen)
  {
    return addFrame(APNGFrame(pixels, width, height, delayNum, delayDen));
  }

  //Adds an APNGFrame object to the frame vector
  //Returns a frame vector with the added frames
  APNGAsm& APNGAsm::operator << (const APNGFrame &frame)
  {
    addFrame(frame);
    return *this;
  }

  // Set loop count of animation.
  // If argument is 0, loop count is infinity.
  void APNGAsm::setLoops(unsigned int loops)
  {
    _loops = loops;
  }

  // Set flag of skip first frame.
  void APNGAsm::setSkipFirst(bool skipFirst)
  {
    _skipFirst = skipFirst;
  }

  //Assembles and outputs an APNG file
  //Returns the assembled file object
  //If no output path is specified only the file object is returned
  bool APNGAsm::assemble(const std::string &outputPath)
  {
    if (_frames.empty())
      return false;

    if (_progressCallback) _progressCallback(0.5f);

    _width  = _frames[0]._width;
    _height = _frames[0]._height;
    _size   = _width * _height;

    for (size_t n = 1; n < _frames.size(); ++n)
      if (_width != _frames[n]._width || _height != _frames[n]._height)
        return false;

    unsigned char coltype = findCommonType();

    if (upconvertToCommonType(coltype))
      return false;

    dirtyTransparencyOptimization(coltype);

    coltype = downconvertOptimizations(coltype, false, false);

    duplicateFramesOptimization(coltype, (_skipFirst ? 1 : 0));

    if( !save(outputPath, coltype, (_skipFirst ? 1 : 0), _loops) )
      return false;

    return true;
  }

  unsigned char APNGAsm::findCommonType(void)
  {
    unsigned char coltype = _frames[0]._colorType;

    for (size_t n = 1; n < _frames.size(); ++n)
    {
      if (_frames[0]._paletteSize != _frames[n]._paletteSize || memcmp(_frames[0]._palette, _frames[n]._palette, _frames[0]._paletteSize*3) != 0)
        coltype = 6;
      else
      if (_frames[0]._transparencySize != _frames[n]._transparencySize || memcmp(_frames[0]._transparency, _frames[n]._transparency, _frames[0]._transparencySize) != 0)
        coltype = 6;
      else
      if (_frames[n]._colorType != 3)
      {
        if (coltype != 3)
          coltype |= _frames[n]._colorType;
        else
          coltype = 6;
      }
      else
        if (coltype != 3)
          coltype = 6;

      // Emit progress mapped into [0.05, 0.1]
      if (_progressCallback) {
          float frac = float(n + 1) / float(_frames.size());
          _progressCallback(0.05f + frac * (0.10f - 0.05f));
      }
    }
    return coltype;
  }

  int APNGAsm::upconvertToCommonType(unsigned char coltype)
  {
    unsigned char  * sp;
    unsigned char  * dp;
    unsigned char    r, g, b;
    unsigned int     j;

    for (size_t n = 0; n < _frames.size(); ++n)
    {
      if (coltype == 6 && _frames[n]._colorType != 6)
      {
        unsigned char * dst = new unsigned char[_size * 4];

        if (_frames[n]._colorType == 0)
        {
          sp = _frames[n]._pixels;
          dp = dst;
          if (_frames[n]._transparencySize == 0)
          for (j=0; j<_size; j++)
          {
            *dp++ = *sp;
            *dp++ = *sp;
            *dp++ = *sp++;
            *dp++ = 255;
          }
          else
          for (j=0; j<_size; j++)
          {
            g = *sp++;
            *dp++ = g;
            *dp++ = g;
            *dp++ = g;
            *dp++ = (_frames[n]._transparency[1] == g) ? 0 : 255;
          }
        }
        else
        if (_frames[n]._colorType == 2)
        {
          sp = _frames[n]._pixels;
          dp = dst;
          if (_frames[n]._transparencySize == 0)
          for (j=0; j<_size; j++)
          {
            *dp++ = *sp++;
            *dp++ = *sp++;
            *dp++ = *sp++;
            *dp++ = 255;
          }
          else
          for (j=0; j<_size; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            *dp++ = r;
            *dp++ = g;
            *dp++ = b;
            *dp++ = (_frames[n]._transparency[1] == r && _frames[n]._transparency[3] == g && _frames[n]._transparency[5] == b) ? 0 : 255;
          }
        }
        else
        if (_frames[n]._colorType == 3)
        {
          sp = _frames[n]._pixels;
          dp = dst;
          for (j=0; j<_size; j++, sp++)
          {
            *dp++ = _frames[n]._palette[*sp].r;
            *dp++ = _frames[n]._palette[*sp].g;
            *dp++ = _frames[n]._palette[*sp].b;
            *dp++ = _frames[n]._transparency[*sp];
          }
        }
        else
        if (_frames[n]._colorType == 4)
        {
          sp = _frames[n]._pixels;
          dp = dst;
          for (j=0; j<_size; j++)
          {
            *dp++ = *sp;
            *dp++ = *sp;
            *dp++ = *sp++;
            *dp++ = *sp++;
          }
        }
        delete[] _frames[n]._pixels;
        _frames[n]._pixels = dst;
        _frames[n]._colorType = coltype;

        for (j=0; j<_frames[n]._height; ++j, dst += _frames[n]._width * 4)
          _frames[n]._rows[j] = dst;
      }
      else
      if (coltype == 4 && _frames[n]._colorType == 0)
      {
        unsigned char * dst = new unsigned char[_size * 2];

        sp = _frames[n]._pixels;
        dp = dst;
        for (j=0; j<_size; j++)
        {
          *dp++ = *sp++;
          *dp++ = 255;
        }
        delete[] _frames[n]._pixels;
        _frames[n]._pixels = dst;
        _frames[n]._colorType = coltype;

        for (j=0; j<_frames[n]._height; ++j, dst += _frames[n]._width * 2)
          _frames[n]._rows[j] = dst;
      }
      else
      if (coltype == 2 && _frames[n]._colorType == 0)
      {
        unsigned char * dst = new unsigned char[_size * 3];

        sp = _frames[n]._pixels;
        dp = dst;
        for (j=0; j<_size; j++)
        {
          *dp++ = *sp;
          *dp++ = *sp;
          *dp++ = *sp++;
        }
        delete[] _frames[n]._pixels;
        _frames[n]._pixels = dst;
        _frames[n]._colorType = coltype;

        for (j=0; j<_frames[n]._height; ++j, dst += _frames[n]._width * 3)
          _frames[n]._rows[j] = dst;
      }

      // Emit progress mapped into [0.1, 0.15]
      if (_progressCallback) {
          float frac = float(n + 1) / float(_frames.size());
          _progressCallback(0.10f + frac * (0.15f - 0.10f));
      }
    }
    return 0;
  }

  void APNGAsm::dirtyTransparencyOptimization(unsigned char coltype)
  {
    if (coltype == 6)
    {
      for (size_t n = 0; n < _frames.size(); ++n)
      {
        unsigned char * sp = _frames[n]._pixels;
        for (unsigned int j=0; j<_size; j++, sp+=4)
          if (sp[3] == 0)
             sp[0] = sp[1] = sp[2] = 0;

        // Emit progress mapped into [0.15, 0.2]
        if (_progressCallback) {
            float frac = float(n + 1) / float(_frames.size());
            _progressCallback(0.15f + frac * (0.20f - 0.15f));
        }
      }
    }
    else
    if (coltype == 4)
    {
      for (size_t n = 0; n < _frames.size(); ++n)
      {
        unsigned char * sp = _frames[n]._pixels;
        for (unsigned int j=0; j<_size; j++, sp+=2)
          if (sp[1] == 0)
            sp[0] = 0;

        // Emit progress mapped into [0.15, 0.2]
        if (_progressCallback) {
            float frac = float(n + 1) / float(_frames.size());
            _progressCallback(0.15f + frac * (0.20f - 0.15f));
        }
      }
    }
  }

  unsigned char APNGAsm::downconvertOptimizations(unsigned char coltype, bool keep_palette, bool keep_coltype)
  {
    unsigned int     has_tcolor = 0;
    unsigned int     colors = 0;
    unsigned char  * sp;
    unsigned char  * dp;
    unsigned char    r, g, b, a;
    unsigned int     i, j, k;
    unsigned char    cube[4096];
    unsigned char    gray[256];
    COLORS           col[256];

    memset(&cube, 0, sizeof(cube));
    memset(&gray, 0, sizeof(gray));

    for (i=0; i<256; i++)
    {
      col[i].num = 0;
      col[i].r = col[i].g = col[i].b = i;
      col[i].a = _trns[i] = 255;
    }
    _palsize = 0;
    _trnssize = 0;

    if (coltype == 6 && !keep_coltype)
    {
      int transparent = 255;
      int simple_trans = 1;
      int grayscale = 1;

      for (size_t n = 0; n < _frames.size(); ++n)
      {
        sp = _frames[n]._pixels;
        for (j=0; j<_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;
          a = *sp++;
          transparent &= a;

          if (a != 0)
          {
            if (a != 255)
              simple_trans = 0;
            else
              if (((r | g | b) & 15) == 0)
                cube[(r<<4) + g + (b>>4)] = 1;

            if (r != g || g != b)
              grayscale = 0;
            else
              gray[r] = 1;
          }

          if (colors <= 256)
          {
            int found = 0;
            for (k=0; k<colors; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
            {
              found = 1;
              col[k].num++;
              break;
            }
            if (found == 0)
            {
              if (colors < 256)
              {
                col[colors].num++;
                col[colors].r = r;
                col[colors].g = g;
                col[colors].b = b;
                col[colors].a = a;
                if (a == 0) has_tcolor = 1;
              }
              colors++;
            }
          }
        }
      }

      if (grayscale && simple_trans && colors<=256) /* 6 -> 0 */
      {
        coltype = 0;

        for (i=0; i<256; i++)
        if (gray[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = i;
          _trnssize = 2;
          break;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++, sp+=4)
          {
            if (sp[3] == 0)
              *dp++ = _trns[1];
            else
              *dp++ = sp[0];
          }
        }
      }
      else
      if (colors<=256)   /* 6 -> 3 */
      {
        coltype = 3;

        if (has_tcolor==0 && colors<256)
          col[colors++].a = 0;

        qsort(&col[0], colors, sizeof(COLORS), compareColors);

        _palsize = colors;
        for (i=0; i<colors; i++)
        {
          _palette[i].r = col[i].r;
          _palette[i].g = col[i].g;
          _palette[i].b = col[i].b;
          _trns[i]      = col[i].a;
          if (_trns[i] != 255) _trnssize = i+1;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            a = *sp++;
            for (k=0; k<colors; k++)
              if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
                break;
            *dp++ = k;
          }
        }
      }
      else
      if (grayscale)     /* 6 -> 4 */
      {
        coltype = 4;
        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            r = *sp++;
            g = *sp++;
            *dp++ = *sp++;
            *dp++ = *sp++;
          }
        }
      }
      else
      if (simple_trans)  /* 6 -> 2 */
      {
        for (i=0; i<4096; i++)
        if (cube[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = (i>>4)&0xF0;
          _trns[2] = 0;
          _trns[3] = i&0xF0;
          _trns[4] = 0;
          _trns[5] = (i<<4)&0xF0;
          _trnssize = 6;
          break;
        }
        if (transparent == 255)
        {
          coltype = 2;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = dp = _frames[n]._pixels;
            for (j=0; j<_size; j++)
            {
              *dp++ = *sp++;
              *dp++ = *sp++;
              *dp++ = *sp++;
              sp++;
            }
          }
        }
        else
        if (_trnssize != 0)
        {
          coltype = 2;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = dp = _frames[n]._pixels;
            for (j=0; j<_size; j++)
            {
              r = *sp++;
              g = *sp++;
              b = *sp++;
              a = *sp++;
              if (a == 0)
              {
                *dp++ = _trns[1];
                *dp++ = _trns[3];
                *dp++ = _trns[5];
              }
              else
              {
                *dp++ = r;
                *dp++ = g;
                *dp++ = b;
              }
            }
          }
        }
      }
    }
    else
    if (coltype == 2)
    {
      int grayscale = 1;

      for (size_t n = 0; n < _frames.size(); ++n)
      {
        sp = _frames[n]._pixels;
        for (j=0; j<_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;

          if (_frames[n]._transparencySize == 0)
            if (((r | g | b) & 15) == 0)
              cube[(r<<4) + g + (b>>4)] = 1;

          if (r != g || g != b)
            grayscale = 0;
          else
            gray[r] = 1;

          if (colors <= 256)
          {
            int found = 0;
            for (k=0; k<colors; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b)
            {
              found = 1;
              col[k].num++;
              break;
            }
            if (found == 0)
            {
              if (colors < 256)
              {
                col[colors].num++;
                col[colors].r = r;
                col[colors].g = g;
                col[colors].b = b;
                if (_frames[n]._transparencySize == 6 && _frames[n]._transparency[1] == r && _frames[n]._transparency[3] == g && _frames[n]._transparency[5] == b)
                {
                  col[colors].a = 0;
                  has_tcolor = 1;
                }
              }
              colors++;
            }
          }
        }
      }

      if (grayscale && colors<=256 && !keep_coltype) /* 2 -> 0 */
      {
        for (i=0; i<256; i++)
        if (gray[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = i;
          _trnssize = 2;
          break;
        }
        if (_frames[0]._transparencySize == 0)
        {
          coltype = 0;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = dp = _frames[n]._pixels;
            for (j=0; j<_size; j++, sp+=3)
              *dp++ = *sp;
          }
        }
        else
        if (_trnssize != 0)
        {
          coltype = 0;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = dp = _frames[n]._pixels;
            for (j=0; j<_size; j++)
            {
              r = *sp++;
              g = *sp++;
              b = *sp++;
              if (_frames[n]._transparency[1] == r && _frames[n]._transparency[3] == g && _frames[n]._transparency[5] == b)
                *dp++ = _trns[1];
              else
                *dp++ = g;
            }
          }
        }
      }
      else
      if (colors<=256 && !keep_coltype)   /* 2 -> 3 */
      {
        coltype = 3;

        if (has_tcolor==0 && colors<256)
          col[colors++].a = 0;

        qsort(&col[0], colors, sizeof(COLORS), compareColors);

        _palsize = colors;
        for (i=0; i<colors; i++)
        {
          _palette[i].r = col[i].r;
          _palette[i].g = col[i].g;
          _palette[i].b = col[i].b;
          _trns[i]      = col[i].a;
          if (_trns[i] != 255) _trnssize = i+1;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;

            for (k=0; k<colors; k++)
              if (col[k].r == r && col[k].g == g && col[k].b == b)
                break;
            *dp++ = k;
          }
        }
      }
      else /* 2 -> 2 */
      {
        if (_frames[0]._transparencySize != 0)
        {
          memcpy(_trns, _frames[0]._transparency, _frames[0]._transparencySize);
          _trnssize = _frames[0]._transparencySize;
        }
        else
        for (i=0; i<4096; i++)
        if (cube[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = (i>>4)&0xF0;
          _trns[2] = 0;
          _trns[3] = i&0xF0;
          _trns[4] = 0;
          _trns[5] = (i<<4)&0xF0;
          _trnssize = 6;
          break;
        }
      }
    }
    else
    if (coltype == 4 && !keep_coltype)
    {
      int simple_trans = 1;

      for (size_t n = 0; n < _frames.size(); ++n)
      {
        sp = _frames[n]._pixels;
        for (j=0; j<_size; j++)
        {
          g = *sp++;
          a = *sp++;

          if (a != 0)
          {
            if (a != 255)
              simple_trans = 0;
            else
              gray[g] = 1;
          }

          if (colors <= 256)
          {
            int found = 0;
            for (k=0; k<colors; k++)
            if (col[k].g == g && col[k].a == a)
            {
              found = 1;
              col[k].num++;
              break;
            }
            if (found == 0)
            {
              if (colors < 256)
              {
                col[colors].num++;
                col[colors].r = g;
                col[colors].g = g;
                col[colors].b = g;
                col[colors].a = a;
                if (a == 0) has_tcolor = 1;
              }
              colors++;
            }
          }
        }
      }

      if (simple_trans && colors<=256)   /* 4 -> 0 */
      {
        coltype = 0;

        for (i=0; i<256; i++)
        if (gray[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = i;
          _trnssize = 2;
          break;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            g = *sp++;
            if (*sp++ == 0)
              *dp++ = _trns[1];
            else
              *dp++ = g;
          }
        }
      }
      else
      if (colors<=256)   /* 4 -> 3 */
      {
        coltype = 3;

        if (has_tcolor==0 && colors<256)
          col[colors++].a = 0;

        qsort(&col[0], colors, sizeof(COLORS), compareColors);

        _palsize = colors;
        for (i=0; i<colors; i++)
        {
          _palette[i].r = col[i].r;
          _palette[i].g = col[i].g;
          _palette[i].b = col[i].b;
          _trns[i]      = col[i].a;
          if (_trns[i] != 255) _trnssize = i+1;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = dp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            g = *sp++;
            a = *sp++;
            for (k=0; k<colors; k++)
              if (col[k].g == g && col[k].a == a)
                break;
            *dp++ = k;
          }
        }
      }
    }
    else
    if (coltype == 3)
    {
      int simple_trans = 1;
      int grayscale = 1;

      for (int c=0; c<_frames[0]._paletteSize; c++)
      {
        col[c].r = _frames[0]._palette[c].r;
        col[c].g = _frames[0]._palette[c].g;
        col[c].b = _frames[0]._palette[c].b;
      }

      for (int c=0; c<_frames[0]._transparencySize; c++)
        col[c].a = _frames[0]._transparency[c];

      for (size_t n = 0; n < _frames.size(); ++n)
      {
        sp = _frames[n]._pixels;
        for (j=0; j<_size; j++)
          col[*sp++].num++;
      }

      for (i=0; i<256; i++)
      if (col[i].num != 0)
      {
        if (col[i].a != 0)
        {
          if (col[i].a != 255)
            simple_trans = 0;
          else
          if (col[i].r != col[i].g || col[i].g != col[i].b)
            grayscale = 0;
          else
            gray[col[i].g] = 1;
        }
        else
          has_tcolor = 1;
      }

      if (grayscale && simple_trans && !keep_coltype) /* 3 -> 0 */
      {
        for (i=0; i<256; i++)
        if (gray[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = i;
          _trnssize = 2;
          break;
        }
        if (!has_tcolor)
        {
          coltype = 0;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = _frames[n]._pixels;
            for (j=0; j<_size; j++, sp++)
              *sp = _frames[n]._palette[*sp].g;
          }
        }
        else
        if (_trnssize != 0)
        {
          coltype = 0;
          for (size_t n = 0; n < _frames.size(); ++n)
          {
            sp = _frames[n]._pixels;
            for (j=0; j<_size; j++, sp++)
            {
              if (col[*sp].a == 0)
                *sp = _trns[1];
              else
                *sp = _frames[n]._palette[*sp].g;
            }
          }
        }
      }
      else
      if (!keep_palette)                 /* 3 -> 3 */
      {
        for (i=0; i<256; i++)
        if (col[i].num == 0)
        {
          col[i].a = 255;
          if (!has_tcolor)
          {
            col[i].a = 0;
            has_tcolor = 1;
          }
        }

        qsort(&col[0], 256, sizeof(COLORS), compareColors);

        for (i=0; i<256; i++)
        {
          _palette[i].r = col[i].r;
          _palette[i].g = col[i].g;
          _palette[i].b = col[i].b;
          _trns[i]      = col[i].a;
          if (col[i].num != 0)
            _palsize = i+1;
          if (_trns[i] != 255)
            _trnssize = i+1;
        }

        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
          {
            r = _frames[n]._palette[*sp].r;
            g = _frames[n]._palette[*sp].g;
            b = _frames[n]._palette[*sp].b;
            a = _frames[n]._transparency[*sp];

            for (k=0; k<_palsize; k++)
              if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
                break;
            *sp++ = k;
          }
        }
      }
      else
      {
        _palsize = _frames[0]._paletteSize;
        _trnssize = _frames[0]._transparencySize;
        for (i=0; i<_palsize; i++)
        {
          _palette[i].r = col[i].r;
          _palette[i].g = col[i].g;
          _palette[i].b = col[i].b;
        }
        for (i=0; i<_trnssize; i++)
          _trns[i] = col[i].a;
      }
    }
    else
    if (coltype == 0)  /* 0 -> 0 */
    {
      if (_frames[0]._transparencySize != 0)
      {
        memcpy(_trns, _frames[0]._transparency, _frames[0]._transparencySize);
        _trnssize = _frames[0]._transparencySize;
      }
      else
      {
        for (size_t n = 0; n < _frames.size(); ++n)
        {
          sp = _frames[n]._pixels;
          for (j=0; j<_size; j++)
            gray[*sp++] = 1;
        }
        for (i=0; i<256; i++)
        if (gray[i] == 0)
        {
          _trns[0] = 0;
          _trns[1] = i;
          _trnssize = 2;
          break;
        }
      }
    }

    for (size_t n = 0; n < _frames.size(); ++n)
    {
      _frames[n]._colorType = coltype;
      _frames[n]._paletteSize = _palsize;
      _frames[n]._transparencySize = _trnssize;

      memcpy(_frames[n]._palette, _palette, 256 * 3);
      memcpy(_frames[n]._transparency, _trns, 256);

      if (coltype == 0 || coltype == 3)
        for (j=0; j<_frames[n]._height; ++j)
          _frames[n]._rows[j] = _frames[n]._pixels + j * _frames[n]._width;
      else
      if (coltype == 2)
        for (j=0; j<_frames[n]._height; ++j)
          _frames[n]._rows[j] = _frames[n]._pixels + j * _frames[n]._width * 3;
      else
      if (coltype == 4)
        for (j=0; j<_frames[n]._height; ++j)
          _frames[n]._rows[j] = _frames[n]._pixels + j * _frames[n]._width * 2;

      // Emit progress mapped into [0.2, 0.25]
      if (_progressCallback) {
          float frac = float(n + 1) / float(_frames.size());
          _progressCallback(0.20f + frac * (0.25f - 0.20f));
      }
    }

    return coltype;
  }

  void APNGAsm::duplicateFramesOptimization(unsigned char coltype, unsigned int first)
  {
    unsigned int bpp = 1;
    if (coltype == 2)
      bpp = 3;
    else
    if (coltype == 4)
      bpp = 2;
    else
    if (coltype == 6)
      bpp = 4;

    size_t n = first;

    while (++n < _frames.size())
    {
      if (memcmp(_frames[n-1]._pixels, _frames[n]._pixels, _size * bpp) != 0)
        continue;

      n--;
      delete[] _frames[n]._pixels;
      delete[] _frames[n]._rows;
      unsigned int num = _frames[n]._delayNum;
      unsigned int den = _frames[n]._delayDen;
      _frames.erase(_frames.begin()+n);

      if (_frames[n]._delayDen == den)
        _frames[n]._delayNum += num;
      else
      {
        _frames[n]._delayNum = num = num*_frames[n]._delayDen + den*_frames[n]._delayNum;
        _frames[n]._delayDen = den = den*_frames[n]._delayDen;
        while (num && den)
        {
          if (num > den)
            num = num % den;
          else
            den = den % num;
        }
        num += den;
        _frames[n]._delayNum /= num;
        _frames[n]._delayDen /= num;
      }

      // Emit progress mapped into [0.25, 0.3]
      if (_progressCallback) {
          float frac = float(n + 1) / float(_frames.size());
          _progressCallback(0.25f + frac * (0.30f - 0.25f));
      }
    }
  }

  bool APNGAsm::save(const std::string &outputPath, unsigned char coltype, unsigned int first, unsigned int loops)
  {
    unsigned int    j, k;
    unsigned int    has_tcolor = 0;
    unsigned int    tcolor = 0;
    unsigned int    i, rowbytes, imagesize;
    unsigned int    idat_size, zbuf_size, zsize;
    unsigned char * zbuf;
    FILE*           f;
    unsigned char   png_sign[8] = {137,  80,  78,  71,  13,  10,  26,  10};
    unsigned char   png_Software[27] = { 83, 111, 102, 116, 119, 97, 114, 101, '\0',
                                         65,  80,  78,  71,  32, 65, 115, 115, 101,
                                        109,  98, 108, 101, 114, 32,  51,  46,  48};

    unsigned int bpp = 1;
    if (coltype == 2)
      bpp = 3;
    else
    if (coltype == 4)
      bpp = 2;
    else
    if (coltype == 6)
      bpp = 4;

    if (coltype == 0)
    {
      if (_trnssize)
      {
        has_tcolor = 1;
        tcolor = _trns[1];
      }
    }
    else
    if (coltype == 2)
    {
      if (_trnssize)
      {
        has_tcolor = 1;
        tcolor = (((_trns[5]<<8)+_trns[3])<<8)+_trns[1];
      }
    }
    else
    if (coltype == 3)
    {
      for (i=0; i<_trnssize; i++)
      if (_trns[i] == 0)
      {
        has_tcolor = 1;
        tcolor = i;
        break;
      }
    }
    else
    {
      has_tcolor = 1;
      tcolor = 0;
    }

    rowbytes  = _width * bpp;
    imagesize = rowbytes * _height;

    unsigned char * temp  = new unsigned char[imagesize];
    unsigned char * over1 = new unsigned char[imagesize];
    unsigned char * over2 = new unsigned char[imagesize];
    unsigned char * over3 = new unsigned char[imagesize];
    unsigned char * prev  = new unsigned char[imagesize];
    unsigned char * rows  = new unsigned char[(rowbytes + 1) * _height];

    if ((f = fopen(outputPath.c_str(), "wb")) != 0)
    {
      unsigned char buf_IHDR[13];
      unsigned char buf_acTL[8];
      unsigned char buf_fcTL[26];

      png_save_uint_32(buf_IHDR, _width);
      png_save_uint_32(buf_IHDR + 4, _height);
      buf_IHDR[8] = 8;
      buf_IHDR[9] = coltype;
      buf_IHDR[10] = 0;
      buf_IHDR[11] = 0;
      buf_IHDR[12] = 0;

      png_save_uint_32(buf_acTL, static_cast<png_uint_32>(_frames.size() - first));
      png_save_uint_32(buf_acTL + 4, loops);

      fwrite(png_sign, 1, 8, f);

      write_chunk(f, "IHDR", buf_IHDR, 13);

      if (_frames.size() > 1)
        write_chunk(f, "acTL", buf_acTL, 8);
      else
        first = 0;

      if (_palsize > 0)
        write_chunk(f, "PLTE", (unsigned char *)(&_palette), _palsize*3);

      if (_trnssize > 0)
        write_chunk(f, "tRNS", _trns, _trnssize);

      _op_zstream1.data_type = Z_BINARY;
      _op_zstream1.zalloc = Z_NULL;
      _op_zstream1.zfree = Z_NULL;
      _op_zstream1.opaque = Z_NULL;
      deflateInit2(&_op_zstream1, Z_BEST_SPEED+1, 8, 15, 8, Z_DEFAULT_STRATEGY);

      _op_zstream2.data_type = Z_BINARY;
      _op_zstream2.zalloc = Z_NULL;
      _op_zstream2.zfree = Z_NULL;
      _op_zstream2.opaque = Z_NULL;
      deflateInit2(&_op_zstream2, Z_BEST_SPEED+1, 8, 15, 8, Z_FILTERED);

      idat_size = (rowbytes + 1) * _height;
      zbuf_size = idat_size + ((idat_size + 7) >> 3) + ((idat_size + 63) >> 6) + 11;

      zbuf = new unsigned char[zbuf_size];
      _op_zbuf1 = new unsigned char[zbuf_size];
      _op_zbuf2 = new unsigned char[zbuf_size];
      _row_buf = new unsigned char[rowbytes + 1];
      _sub_row = new unsigned char[rowbytes + 1];
      _up_row = new unsigned char[rowbytes + 1];
      _avg_row = new unsigned char[rowbytes + 1];
      _paeth_row = new unsigned char[rowbytes + 1];

      _row_buf[0] = 0;
      _sub_row[0] = 1;
      _up_row[0] = 2;
      _avg_row[0] = 3;
      _paeth_row[0] = 4;

      unsigned int x0 = 0;
      unsigned int y0 = 0;
      unsigned int w0 = _width;
      unsigned int h0 = _height;
      unsigned char bop = 0;
      unsigned char dop = 0;
      _next_seq_num = 0;

      for (j=0; j<6; j++)
        _op[j].valid = 0;
      deflate_rect_op(_frames[0]._pixels, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);
      deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, 0);

      if (first)
      {
        write_IDATs(f, 0, zbuf, zsize, idat_size);

        for (j=0; j<6; j++)
          _op[j].valid = 0;
        deflate_rect_op(_frames[1]._pixels, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);
        deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, 0);
      }

      for (size_t n = first; n < _frames.size()-1; ++n)
      {
        unsigned int op_min;
        int          op_best;

        for (j=0; j<6; j++)
          _op[j].valid = 0;

        /* dispose = none */
        get_rect(_width, _height, _frames[n]._pixels, _frames[n+1]._pixels, over1, coltype, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 0);

        /* dispose = background */
        if (has_tcolor)
        {
          memcpy(temp, _frames[n]._pixels, imagesize);
          if (coltype == 2)
            for (j=0; j<h0; j++)
              for (k=0; k<w0; k++)
                memcpy(temp + ((j+y0)*_width + (k+x0))*3, &tcolor, 3);
          else
            for (j=0; j<h0; j++)
              memset(temp + ((j+y0)*_width + x0)*bpp, tcolor, w0*bpp);

          get_rect(_width, _height, temp, _frames[n+1]._pixels, over2, coltype, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 1);
        }

        /* dispose = previous */
        if (n > first)
          get_rect(_width, _height, prev, _frames[n+1]._pixels, over3, coltype, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 2);

        op_min = _op[0].size;
        op_best = 0;
        for (j=1; j<6; j++)
        if (_op[j].valid)
        {
          if (_op[j].size < op_min)
          {
            op_min = _op[j].size;
            op_best = j;
          }
        }

        dop = op_best >> 1;

        png_save_uint_32(buf_fcTL, _next_seq_num++);
        png_save_uint_32(buf_fcTL + 4, w0);
        png_save_uint_32(buf_fcTL + 8, h0);
        png_save_uint_32(buf_fcTL + 12, x0);
        png_save_uint_32(buf_fcTL + 16, y0);
        png_save_uint_16(buf_fcTL + 20, _frames[n]._delayNum);
        png_save_uint_16(buf_fcTL + 22, _frames[n]._delayDen);
        buf_fcTL[24] = dop;
        buf_fcTL[25] = bop;
        write_chunk(f, "fcTL", buf_fcTL, 26);

        write_IDATs(f, static_cast<int>(n), zbuf, zsize, idat_size);

        /* process apng dispose - begin */
        if (dop != 2)
          memcpy(prev, _frames[n]._pixels, imagesize);

        if (dop == 1)
        {
          if (coltype == 2)
            for (j=0; j<h0; j++)
              for (k=0; k<w0; k++)
                memcpy(prev + ((j+y0)*_width + (k+x0))*3, &tcolor, 3);
          else
            for (j=0; j<h0; j++)
              memset(prev + ((j+y0)*_width + x0)*bpp, tcolor, w0*bpp);
        }
        /* process apng dispose - end */

        x0 = _op[op_best].x;
        y0 = _op[op_best].y;
        w0 = _op[op_best].w;
        h0 = _op[op_best].h;
        bop = op_best & 1;

        deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, op_best);

        // Emit progress mapped into [0.3, 0.99]
        if (_progressCallback) {
            float frac = float(n + 1) / float(_frames.size());
            _progressCallback(0.30f + frac * (0.99f - 0.30f));
        }
      }

      if (_frames.size() > 1)
      {
        png_save_uint_32(buf_fcTL, _next_seq_num++);
        png_save_uint_32(buf_fcTL + 4, w0);
        png_save_uint_32(buf_fcTL + 8, h0);
        png_save_uint_32(buf_fcTL + 12, x0);
        png_save_uint_32(buf_fcTL + 16, y0);
        png_save_uint_16(buf_fcTL + 20, _frames[_frames.size()-1]._delayNum);
        png_save_uint_16(buf_fcTL + 22, _frames[_frames.size()-1]._delayDen);
        buf_fcTL[24] = 0;
        buf_fcTL[25] = bop;
        write_chunk(f, "fcTL", buf_fcTL, 26);
      }

      write_IDATs(f, static_cast<int>(_frames.size()-1), zbuf, zsize, idat_size);

      write_chunk(f, "tEXt", png_Software, 27);
      write_chunk(f, "IEND", 0, 0);
      fclose(f);

      delete[] zbuf;
      delete[] _op_zbuf1;
      delete[] _op_zbuf2;
      delete[] _row_buf;
      delete[] _sub_row;
      delete[] _up_row;
      delete[] _avg_row;
      delete[] _paeth_row;

      deflateEnd(&_op_zstream1);
      deflateEnd(&_op_zstream2);
    }
    else
      return false;

    delete[] temp;
    delete[] over1;
    delete[] over2;
    delete[] over3;
    delete[] prev;
    delete[] rows;

    return true;
  }

  void APNGAsm::process_rect(unsigned char * row, int rowbytes, int bpp, int stride, int h, unsigned char * rows)
  {
    int i, j, v;
    int a, b, c, pa, pb, pc, p;
    unsigned char * prev = NULL;
    unsigned char * dp  = rows;
    unsigned char * out;

    for (j=0; j<h; j++)
    {
      unsigned int    sum = 0;
      unsigned char * best_row = _row_buf;
      unsigned int    mins = ((unsigned int)(-1)) >> 1;

      out = _row_buf+1;
      for (i=0; i<rowbytes; i++)
      {
        v = out[i] = row[i];
        sum += (v < 128) ? v : 256 - v;
      }
      mins = sum;

      sum = 0;
      out = _sub_row+1;
      for (i=0; i<bpp; i++)
      {
        v = out[i] = row[i];
        sum += (v < 128) ? v : 256 - v;
      }
      for (i=bpp; i<rowbytes; i++)
      {
        v = out[i] = row[i] - row[i-bpp];
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        mins = sum;
        best_row = _sub_row;
      }

      if (prev)
      {
        sum = 0;
        out = _up_row+1;
        for (i=0; i<rowbytes; i++)
        {
          v = out[i] = row[i] - prev[i];
          sum += (v < 128) ? v : 256 - v;
          if (sum > mins) break;
        }
        if (sum < mins)
        {
          mins = sum;
          best_row = _up_row;
        }

        sum = 0;
        out = _avg_row+1;
        for (i=0; i<bpp; i++)
        {
          v = out[i] = row[i] - prev[i]/2;
          sum += (v < 128) ? v : 256 - v;
        }
        for (i=bpp; i<rowbytes; i++)
        {
          v = out[i] = row[i] - (prev[i] + row[i-bpp])/2;
          sum += (v < 128) ? v : 256 - v;
          if (sum > mins) break;
        }
        if (sum < mins)
        {
          mins = sum;
          best_row = _avg_row;
        }

        sum = 0;
        out = _paeth_row+1;
        for (i=0; i<bpp; i++)
        {
          v = out[i] = row[i] - prev[i];
          sum += (v < 128) ? v : 256 - v;
        }
        for (i=bpp; i<rowbytes; i++)
        {
          a = row[i-bpp];
          b = prev[i];
          c = prev[i-bpp];
          p = b - c;
          pc = a - c;
          pa = abs(p);
          pb = abs(pc);
          pc = abs(p + pc);
          p = (pa <= pb && pa <=pc) ? a : (pb <= pc) ? b : c;
          v = out[i] = row[i] - p;
          sum += (v < 128) ? v : 256 - v;
          if (sum > mins) break;
        }
        if (sum < mins)
        {
          best_row = _paeth_row;
        }
      }

      if (rows == NULL)
      {
        // deflate_rect_op()
        _op_zstream1.next_in = _row_buf;
        _op_zstream1.avail_in = rowbytes + 1;
        deflate(&_op_zstream1, Z_NO_FLUSH);

        _op_zstream2.next_in = best_row;
        _op_zstream2.avail_in = rowbytes + 1;
        deflate(&_op_zstream2, Z_NO_FLUSH);
      }
      else
      {
        // deflate_rect_fin()
        memcpy(dp, best_row, rowbytes+1);
        dp += rowbytes+1;
      }

      prev = row;
      row += stride;
    }
  }

  void APNGAsm::deflate_rect_fin(unsigned char * zbuf, unsigned int * zsize, int bpp, int stride, unsigned char * rows, int zbuf_size, int n)
  {
    unsigned char * row  = _op[n].p + _op[n].y*stride + _op[n].x*bpp;
    int rowbytes = _op[n].w*bpp;

    z_stream _fin_zstream;
    _fin_zstream.data_type = Z_BINARY;
    _fin_zstream.zalloc = Z_NULL;
    _fin_zstream.zfree = Z_NULL;
    _fin_zstream.opaque = Z_NULL;

    if (_op[n].filters == 0)
    {
      deflateInit2(&_fin_zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_DEFAULT_STRATEGY);
      unsigned char * dp  = rows;
      for (int j=0; j<_op[n].h; j++)
      {
        *dp++ = 0;
        memcpy(dp, row, rowbytes);
        dp += rowbytes;
        row += stride;
      }
    }
    else
    {
      deflateInit2(&_fin_zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_FILTERED);
      process_rect(row, rowbytes, bpp, stride, _op[n].h, rows);
    }

    _fin_zstream.next_out = zbuf;
    _fin_zstream.avail_out = zbuf_size;
    _fin_zstream.next_in = rows;
    _fin_zstream.avail_in = _op[n].h*(rowbytes + 1);
    deflate(&_fin_zstream, Z_FINISH);
    *zsize = _fin_zstream.total_out;
    deflateEnd(&_fin_zstream);
  }

  void APNGAsm::deflate_rect_op(unsigned char *pdata, int x, int y, int w, int h, int bpp, int stride, int zbuf_size, int n)
  {
    unsigned char * row  = pdata + y*stride + x*bpp;
    int rowbytes = w * bpp;

    _op_zstream1.data_type = Z_BINARY;
    _op_zstream1.next_out = _op_zbuf1;
    _op_zstream1.avail_out = zbuf_size;

    _op_zstream2.data_type = Z_BINARY;
    _op_zstream2.next_out = _op_zbuf2;
    _op_zstream2.avail_out = zbuf_size;

    process_rect(row, rowbytes, bpp, stride, h, NULL);

    deflate(&_op_zstream1, Z_FINISH);
    deflate(&_op_zstream2, Z_FINISH);
    _op[n].p = pdata;
    if (_op_zstream1.total_out < _op_zstream2.total_out)
    {
      _op[n].size = _op_zstream1.total_out;
      _op[n].filters = 0;
    }
    else
    {
      _op[n].size = _op_zstream2.total_out;
      _op[n].filters = 1;
    }
    _op[n].x = x;
    _op[n].y = y;
    _op[n].w = w;
    _op[n].h = h;
    _op[n].valid = 1;
    deflateReset(&_op_zstream1);
    deflateReset(&_op_zstream2);
  }

  void APNGAsm::get_rect(unsigned int w, unsigned int h, unsigned char *pimage1, unsigned char *pimage2, unsigned char *ptemp, unsigned char coltype, unsigned int bpp, unsigned int stride, int zbuf_size, unsigned int has_tcolor, unsigned int tcolor, int n)
  {
    unsigned int   i, j, x0, y0, w0, h0;
    unsigned int   x_min = w-1;
    unsigned int   y_min = h-1;
    unsigned int   x_max = 0;
    unsigned int   y_max = 0;
    unsigned int   diffnum = 0;
    unsigned int   over_is_possible = 1;

    if (!has_tcolor)
      over_is_possible = 0;

    if (bpp == 1)
    {
      unsigned char *pa = pimage1;
      unsigned char *pb = pimage2;
      unsigned char *pc = ptemp;

      for (j=0; j<h; j++)
      for (i=0; i<w; i++)
      {
        unsigned char c = *pb++;
        if (*pa++ != c)
        {
          diffnum++;
          if (coltype == 0 && has_tcolor && c == tcolor) over_is_possible = 0;
          if (coltype == 3 && _trns[c] != 0xFF) over_is_possible = 0;
          if (i<x_min) x_min = i;
          if (i>x_max) x_max = i;
          if (j<y_min) y_min = j;
          if (j>y_max) y_max = j;
        }
        else
          c = tcolor;

        *pc++ = c;
      }
    }
    else
    if (bpp == 2)
    {
      unsigned short *pa = (unsigned short *)pimage1;
      unsigned short *pb = (unsigned short *)pimage2;
      unsigned short *pc = (unsigned short *)ptemp;

      for (j=0; j<h; j++)
      for (i=0; i<w; i++)
      {
        unsigned int c1 = *pa++;
        unsigned int c2 = *pb++;
        if ((c1 != c2) && ((c1>>8) || (c2>>8)))
        {
          diffnum++;
          if ((c2 >> 8) != 0xFF) over_is_possible = 0;
          if (i<x_min) x_min = i;
          if (i>x_max) x_max = i;
          if (j<y_min) y_min = j;
          if (j>y_max) y_max = j;
        }
        else
          c2 = 0;

        *pc++ = c2;
      }
    }
    else
    if (bpp == 3)
    {
      unsigned char *pa = pimage1;
      unsigned char *pb = pimage2;
      unsigned char *pc = ptemp;

      for (j=0; j<h; j++)
      for (i=0; i<w; i++)
      {
        unsigned int c1 = (pa[2]<<16) + (pa[1]<<8) + pa[0];
        unsigned int c2 = (pb[2]<<16) + (pb[1]<<8) + pb[0];
        if (c1 != c2)
        {
          diffnum++;
          if (has_tcolor && c2 == tcolor) over_is_possible = 0;
          if (i<x_min) x_min = i;
          if (i>x_max) x_max = i;
          if (j<y_min) y_min = j;
          if (j>y_max) y_max = j;
        }
        else
          c2 = tcolor;

        memcpy(pc, &c2, 3);
        pa += 3;
        pb += 3;
        pc += 3;
      }
    }
    else
    if (bpp == 4)
    {
      unsigned int *pa = (unsigned int *)pimage1;
      unsigned int *pb = (unsigned int *)pimage2;
      unsigned int *pc = (unsigned int *)ptemp;

      for (j=0; j<h; j++)
      for (i=0; i<w; i++)
      {
        unsigned int c1 = *pa++;
        unsigned int c2 = *pb++;
        if ((c1 != c2) && ((c1>>24) || (c2>>24)))
        {
          diffnum++;
          if ((c2 >> 24) != 0xFF) over_is_possible = 0;
          if (i<x_min) x_min = i;
          if (i>x_max) x_max = i;
          if (j<y_min) y_min = j;
          if (j>y_max) y_max = j;
        }
        else
          c2 = 0;

        *pc++ = c2;
      }
    }

    if (diffnum == 0)
    {
      x0 = y0 = 0;
      w0 = h0 = 1;
    }
    else
    {
      x0 = x_min;
      y0 = y_min;
      w0 = x_max-x_min+1;
      h0 = y_max-y_min+1;
    }

    deflate_rect_op(pimage2, x0, y0, w0, h0, bpp, stride, zbuf_size, n*2);

    if (over_is_possible)
      deflate_rect_op(ptemp, x0, y0, w0, h0, bpp, stride, zbuf_size, n*2+1);
  }

  void APNGAsm::write_chunk(FILE * f, const char * name, unsigned char * data, unsigned int length)
  {
    unsigned char buf[4];
    unsigned int crc = crc32(0, Z_NULL, 0);

    png_save_uint_32(buf, length);
    fwrite(buf, 1, 4, f);
    fwrite(name, 1, 4, f);
    crc = crc32(crc, (const Bytef *)name, 4);

    if (memcmp(name, "fdAT", 4) == 0)
    {
      png_save_uint_32(buf, _next_seq_num++);
      fwrite(buf, 1, 4, f);
      crc = crc32(crc, buf, 4);
      length -= 4;
    }

    if (data != NULL && length > 0)
    {
      fwrite(data, 1, length, f);
      crc = crc32(crc, data, length);
    }

    png_save_uint_32(buf, crc);
    fwrite(buf, 1, 4, f);
  }

  void APNGAsm::write_IDATs(FILE * f, int n, unsigned char * data, unsigned int length, unsigned int idat_size)
  {
    unsigned int z_cmf = data[0];
    if ((z_cmf & 0x0f) == 8 && (z_cmf & 0xf0) <= 0x70)
    {
      if (length >= 2)
      {
        unsigned int z_cinfo = z_cmf >> 4;
        unsigned int half_z_window_size = 1 << (z_cinfo + 7);
        while (idat_size <= half_z_window_size && half_z_window_size >= 256)
        {
          z_cinfo--;
          half_z_window_size >>= 1;
        }
        z_cmf = (z_cmf & 0x0f) | (z_cinfo << 4);
        if (data[0] != (unsigned char)z_cmf)
        {
          data[0] = (unsigned char)z_cmf;
          data[1] &= 0xe0;
          data[1] += (unsigned char)(0x1f - ((z_cmf << 8) + data[1]) % 0x1f);
        }
      }
    }

    while (length > 0)
    {
      unsigned int ds = length;
      if (ds > 32768)
        ds = 32768;

      if (n == 0)
        write_chunk(f, "IDAT", data, ds);
      else
        write_chunk(f, "fdAT", data, ds+4);

      data += ds;
      length -= ds;
    }
  }

} // namespace apngasm
