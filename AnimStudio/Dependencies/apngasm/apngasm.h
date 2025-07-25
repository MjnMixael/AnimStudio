/**
 * @file apngasm.h
 * @brief apngasm core. All apngasm functionality can is accessable from here.
 * @author Max Stepin, Naoki Iwakawa (GSSZ)
 */

#ifndef _APNGASM_H_
#define _APNGASM_H_

#include <vector>
#include <string>
#include <functional> 
#include <png.h>
#include <zlib.h> // z_stream
#include "apngframe.h"

/**
 * @namespace apngasm
 * @brief The apngasm namespace contains all functionality of apngasm.
 */
namespace apngasm {

    typedef struct { unsigned char *p; unsigned int size; int x, y, w, h, valid, filters; } OP;

    /**
     * @struct CHUNK
     * @brief A chunk of image data.
     */
    struct CHUNK { unsigned int size; unsigned char * p; };

    /**
     * @class APNGAsm
     * @brief The APNGAsm core class is all you need to assemble and disassemble Animated PNG.
     */
    class APNGAsm {
    public:
        /**
         * @brief Construct an empty APNGAsm object.
         */
		APNGAsm(void);
		
        /**
         * @brief Construct APNGAsm object from an existing vector of apngasm frames.
         * @param a std::vector of APNGFrame objects.
         */
		APNGAsm(const std::vector<APNGFrame> &frames);

        /**
         * @brief APNGAsm destructor. !reset() is called here and will destroy the memory for all frames!
         */
		~APNGAsm(void);

        /** Register a 0->1 progress callback (you�ll call this from your exporter) */
        void setProgressCallback(std::function<void(float)> cb);

        /**
         * @brief Adds a frame from a PNG file or frames from a APNG file to the frame vector.
         * @param filePath The relative or absolute path to an image file.
         * @param delayNum The delay numerator for this frame (defaults to DEFAULT_FRAME_NUMERATOR).
         * @param delayDen The delay denominator for this frame (defaults to DEFAULT_FRAME_DENMINATOR).
         * @return The [new] number of frames/the number of this frame on the frame vector.
         */
		size_t addFrame(const std::string &filePath, unsigned delayNum = DEFAULT_FRAME_NUMERATOR, unsigned delayDen = DEFAULT_FRAME_DENOMINATOR);

        /**
		 * @brief Adds an APNGFrame object to the frame vector.
         * @param frame the APNGFrame object to be added
         * @return The [new] number of frames/the number of this frame on the frame vector.
         */
		size_t addFrame(const APNGFrame &frame);

        /**
         * @brief Adds an APNGFrame object to the vector.
         * @param pixels The RGB pixel data.
         * @param width The width of the pixel data.
         * @param height The height of the pixel data.
         * @param trns_color An array of transparency data.
         * @param delayNum The delay numerator for this frame (defaults to DEFAULT_FRAME_NUMERATOR).
         * @param delayDen The delay denominator for this frame (defaults to DEFAULT_FRAME_DENMINATOR).
         * @return The [new] number of frames/the number of this frame on the frame vector.
         */
        size_t addFrame(rgb *pixels, unsigned int width, unsigned int height, rgb *trns_color = NULL, unsigned delayNum = DEFAULT_FRAME_NUMERATOR, unsigned delayDen = DEFAULT_FRAME_DENOMINATOR);

        /**
         * @brief Adds an APNGFrame object to the vector.
         * @param pixels The RGBA pixel data.
         * @param width The width of the pixel data.
         * @param height The height of the pixel data.
         * @param delayNum The delay numerator for this frame (defaults to DEFAULT_FRAME_NUMERATOR).
         * @param delayDen The delay denominator for this frame (defaults to DEFAULT_FRAME_DENMINATOR).
         * @return The [new] number of frames/the number of this frame on the frame vector.
         */
        size_t addFrame(rgba *pixels, unsigned int width, unsigned int height, unsigned delayNum = DEFAULT_FRAME_NUMERATOR, unsigned delayDen = DEFAULT_FRAME_DENOMINATOR);
		
        /**
		 * @brief Adds an APNGFrame object to the frame vector.
         * @param frame An APNGFrame object.
         * @return The [new] number of frames/the number of this frame on the frame vector.
         */
		APNGAsm& operator << (const APNGFrame &frame);

        /**
         * @brief Assembles and outputs an APNG file.
         * @param outputPath The output file path.
         * @return Returns true if assemble completed succesfully.
         */
		bool assemble(const std::string &outputPath);

        /**
         * @brief Set loop count of animation.
         * @param loops Loop count of animation. If the argument is 0 a loop count is infinity.
         */
        void setLoops(unsigned int loops=0);

        /**
         * @brief Set flag of skip first frame.
         * @param skipFirst Flag of skip first frame.
         */
        void setSkipFirst(bool skipFirst);

        /**
         * @brief Returns the frame vector.
         * @return Returns the frame vector.
         */
		const std::vector<APNGFrame>& getFrames() const;

        /**
         * @brief Returns the loop count.
         * @return Returns the loop count.
         */
        unsigned int getLoops() const;

        /**
         * @brief Returns the flag of skip first frame.
         * @return Returns the flag of skip first frame.
         */
        bool isSkipFirst() const;

        /**
         * @brief Returns the number of frames.
         * @return Returns the number of frames.
         */
		size_t frameCount();

        /**
         * @brief Destroy all frames in memory/dispose of the frame vector. Leaves the apngasm object in a clean state.
         * @return Retruns number of frames disposed of.
         */
		size_t reset();

        /**
         * @brief Returns the version of APNGAsm.
         * @return Version string.
         */
		const char* version(void) const;

    private:
    //apng frame vector
    std::vector<APNGFrame> _frames;

    // Animation loop count.
    unsigned int _loops;

    // Flag of skip first frame.
    bool _skipFirst;

    // Progress callback
    std::function<void(float)> _progressCallback;

    unsigned char findCommonType(void);
    int upconvertToCommonType(unsigned char coltype);
    void dirtyTransparencyOptimization(unsigned char coltype);
    unsigned char downconvertOptimizations(unsigned char coltype, bool keep_palette, bool keep_coltype);
    void duplicateFramesOptimization(unsigned char coltype, unsigned int first);

    bool save(const std::string &outputPath, unsigned char coltype, unsigned first, unsigned loops);

    void process_rect(unsigned char * row, int rowbytes, int bpp, int stride, int h, unsigned char * rows);
    void deflate_rect_fin(unsigned char * zbuf, unsigned int * zsize, int bpp, int stride, unsigned char * rows, int zbuf_size, int n);
    void deflate_rect_op(unsigned char *pdata, int x, int y, int w, int h, int bpp, int stride, int zbuf_size, int n);
    void get_rect(unsigned int w, unsigned int h, unsigned char *pimage1, unsigned char *pimage2, unsigned char *ptemp, unsigned char coltype, unsigned int bpp, unsigned int stride, int zbuf_size, unsigned int has_tcolor, unsigned int tcolor, int n);

    void write_chunk(FILE * f, const char * name, unsigned char * data, unsigned int length);
    void write_IDATs(FILE * f, int frame, unsigned char * data, unsigned int length, unsigned int idat_size);

    OP              _op[6];
    z_stream        _op_zstream1;
    z_stream        _op_zstream2;
    unsigned char * _op_zbuf1;
    unsigned char * _op_zbuf2;
    unsigned char * _row_buf;
    unsigned char * _sub_row;
    unsigned char * _up_row;
    unsigned char * _avg_row;
    unsigned char * _paeth_row;
    unsigned int    _next_seq_num;

    unsigned int    _width;
    unsigned int    _height;
    unsigned int    _size;
    rgb             _palette[256];
    unsigned char   _trns[256];
    unsigned int    _palsize;
    unsigned int    _trnssize;
	};	// class APNGAsm
	
}	// namespace apngasm

#endif  // _APNGASM_H_
