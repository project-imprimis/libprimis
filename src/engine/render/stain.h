#ifndef STAIN_H_
#define STAIN_H_

extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

enum
{
    StainBuffer_Opaque = 0,
    StainBuffer_Transparent,
    StainBuffer_Mapmodel,
    StainBuffer_Number,
};

/**
 * @brief Clears stains array
 *
 * Loops through the stains[] global variable array and runs clearstains for each entry.
 */
extern void clearstains();
extern bool renderstains(int sbuf, bool gbuf, int layer = 0);

/**
 * @brief Cleans up each stain in the stains global.
 */
extern void cleanupstains();
#endif
