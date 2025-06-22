#ifndef APNG_DIS_H
#define APNG_DIS_H

#include <vector>

// Simple structure to hold PNG chunks.
struct Image
{
    typedef unsigned char* ROW;
    unsigned int w, h, bpp, delay_num, delay_den;
    unsigned char* p;
    ROW* rows;
    Image() : w(0), h(0), bpp(0), delay_num(1), delay_den(10), p(0), rows(0) {}
    ~Image() {}
    void init(unsigned int w1, unsigned int h1, unsigned int bpp1)
    {
        w = w1; h = h1; bpp = bpp1;
        int rowbytes = w * bpp;
        rows = new ROW[h];
        rows[0] = p = new unsigned char[h * rowbytes];
        for (unsigned int j = 1; j < h; j++)
            rows[j] = rows[j - 1] + rowbytes;
    }
    void free() { delete[] rows; delete[] p; }
};

// Load APNG file and fill img vector with frames.
extern int load_apng(wchar_t* szIn, std::vector<Image>& img);

#endif // APNG_DIS_H
