/**
* Fixed point math, inline-in-header part.
* Adapted from WAHas fixed point vector library by ripping parts not needed out and
* changing a bunch of matrix constructors
*/

#ifndef __FIXEDMATH_H__
#define __FIXEDMATH_H__

// For sqrt
#include <math.h>

// Scalars
#include <stdint.h>

// "Signed shift" warnings. Should your compiler actually not
// compile signed shifts as arithmetic, then well, change this.
#define FLOAT_FIXED(val)  (int32_t)((val)*4096.0)
#define INT_FIXED(val) ((val) << 12)

#define FIXED_FLOAT(val) ((float)(val) / 4096.0)
#define FIXED_INT(val) ((val) >> 12)
#define FIXED_INT_ROUND(val) (((val) + 0x800) >> 12)

// TODO FIXME these are all not very good
static inline int64_t imul64(int64_t a, int64_t b) { return (a*b) >> 12; }
static inline int32_t imul(int32_t a, int32_t b) { return (int32_t)imul64(a, b); }

static inline int64_t idiv64(int64_t num, int64_t den) { return (num << 12) / den; }
static inline int32_t idiv(int32_t num, int32_t den) { return (int32_t)idiv64(num, den); }

static inline int32_t isqrt(int32_t val) { return (int32_t)sqrt(((double)val)*4096.0); } // TODO how good is sqrt on Cortex-M4F?

static inline int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t iabs(int32_t a) { return a < 0 ? -a : a; }

// Note that trig functions use an input range of 0 -> 1
int32_t isin(int a);
static inline int32_t icos(int a) { return isin(a + 1024); }
static inline int32_t itan(int a) { return idiv(isin(a), icos(a)); }

// Vectors
typedef struct { int32_t x, y, z; } ivec3_t;
typedef struct { int32_t x, y, z, w; } ivec4_t;

static inline ivec3_t ivec3(int32_t x, int32_t y, int32_t z) { return (ivec3_t) { x, y, z }; }
static inline ivec4_t ivec4(int32_t x, int32_t y, int32_t z, int32_t w) { return (ivec4_t) { x, y, z, w }; }

static inline ivec3_t ivec3add(ivec3_t a, ivec3_t b) { return ivec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline ivec4_t ivec4add(ivec4_t a, ivec4_t b) { return ivec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }

static inline ivec3_t ivec3add3(ivec3_t a, ivec3_t b, ivec3_t c) { return ivec3(a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z); }
static inline ivec4_t ivec4add3(ivec4_t a, ivec4_t b, ivec4_t c) { return ivec4(a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z, a.w + b.w + c.w); }

static inline ivec3_t ivec3sub(ivec3_t a, ivec3_t b) { return ivec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline ivec4_t ivec4sub(ivec4_t a, ivec4_t b) { return ivec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

static inline ivec3_t ivec3mul(ivec3_t v, int32_t s) { return ivec3(imul(v.x, s), imul(v.y, s), imul(v.z, s)); }
static inline ivec4_t ivec4mul(ivec4_t v, int32_t s) { return ivec4(imul(v.x, s), imul(v.y, s), imul(v.z, s), imul(v.w, s)); }

static inline ivec3_t ivec3div(ivec3_t v, int32_t s) { return ivec3(idiv(v.x, s), idiv(v.y, s), idiv(v.z, s)); }
static inline ivec4_t ivec4div(ivec4_t v, int32_t s) { return ivec4(idiv(v.x, s), idiv(v.y, s), idiv(v.z, s), idiv(v.w, s)); }

static inline int32_t ivec3dot(ivec3_t a, ivec3_t b) { return imul(a.x, b.x) + imul(a.y, b.y) + imul(a.z, b.z); }
static inline int32_t ivec4dot(ivec4_t a, ivec4_t b) { return imul(a.x, b.x) + imul(a.y, b.y) + imul(a.z, b.z) + imul(a.w, b.w); }

static inline int32_t ivec3abs(ivec3_t v) { return isqrt(ivec3dot(v, v)); }
static inline int32_t ivec4abs(ivec4_t v) { return isqrt(ivec4dot(v, v)); }

static inline ivec3_t ivec3norm(ivec3_t v) {
    int32_t abs = ivec3abs(v);
    if (abs == 0) {
        return ivec3(0, 0, 0);
    }
    else {
        return ivec3div(v, abs);
    }
}

static inline ivec4_t ivec4norm(ivec4_t v) {
    int32_t abs = ivec4abs(v);
    if (abs == 0) {
        return ivec4(0, 0, 0, 0);
    }
    else {
        return ivec4div(v, abs);
    }
}

// Matrices
typedef struct { int32_t m[9]; } imat3x3_t;
typedef struct { int32_t m[16]; } imat4x4_t;

static inline imat3x3_t imat3x3(
    int32_t a11, int32_t a12, int32_t a13,
    int32_t a21, int32_t a22, int32_t a23,
    int32_t a31, int32_t a32, int32_t a33
) {
    return (imat3x3_t) { a11, a21, a31, a12, a22, a32, a13, a23, a33 };
}

static inline imat4x4_t imat4x4(
    int32_t a11, int32_t a12, int32_t a13, int32_t a14,
    int32_t a21, int32_t a22, int32_t a23, int32_t a24,
    int32_t a31, int32_t a32, int32_t a33, int32_t a34,
    int32_t a41, int32_t a42, int32_t a43, int32_t a44
) {
    return (imat4x4_t) { a11, a21, a31, a41, a12, a22, a32, a42, a13, a23, a33, a43, a14, a24, a34, a44 };
}

static inline imat4x4_t imat4x4affine3x3(imat3x3_t m) {
    return imat4x4(
        m.m[0], m.m[1], m.m[2], 0,
        m.m[3], m.m[4], m.m[5], 0,
        m.m[6], m.m[7], m.m[8], 0,
        0, 0, 0, INT_FIXED(1));
}

static inline imat3x3_t imat3x3rotatex(int a) {
    return imat3x3(
        INT_FIXED(1), 0, 0,
        0, icos(a), -isin(a),
        0, isin(a), icos(a)
    );
}

static inline imat3x3_t imat3x3rotatey(int a) {
    return imat3x3(
        icos(a), 0, isin(a),
        0, INT_FIXED(1), 0,
        -isin(a), 0, icos(a)
    );
}

static inline imat3x3_t imat3x3rotatez(int a) {
    return imat3x3(
        icos(a), -isin(a), 0,
        isin(a), icos(a), 0,
        0, 0, INT_FIXED(1)
    );
}

static inline imat4x4_t imat4x4rotatex(int a) { 
    return imat4x4affine3x3(imat3x3rotatex(a)); 
}

static inline imat4x4_t imat4x4rotatey(int a) { 
    return imat4x4affine3x3(imat3x3rotatey(a)); 
}

static inline imat4x4_t imat4x4rotatez(int a) { 
    return imat4x4affine3x3(imat3x3rotatez(a)); 
}

static inline imat4x4_t imat4x4translate(ivec3_t v) {
    return imat4x4(
        INT_FIXED(1), 0, 0, v.x,
        0, INT_FIXED(1), 0, v.y,
        0, 0, INT_FIXED(1), v.z,
        0, 0, 0, INT_FIXED(1)
    );
}

static inline imat4x4_t imat4x4scale(int s) {
    return imat4x4(
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, INT_FIXED(1)
    );
}

static inline imat4x4_t imat4x4perspective(int32_t fov, int32_t aspect, int32_t znear, int32_t zfar) {
    int32_t norm_term = FLOAT_FIXED(0.00277777777); // 1 / (180 * 2). Mind: Trig function angles are 0 -> 1
    int32_t f = idiv(INT_FIXED(1), itan(imul(fov, norm_term)));

    return imat4x4(
        idiv(f, aspect), 0, 0, 0,
        0, f, 0, 0,
        0, 0, idiv(znear + zfar, znear - zfar), idiv(imul(INT_FIXED(2), imul(znear, zfar)), znear - zfar),
        0, 0, INT_FIXED(-1), 0
        );
}

static inline ivec3_t imat3x3transform(imat3x3_t m, ivec3_t v) {
    return ivec3(
        imul(v.x, m.m[0]) + imul(v.y, m.m[3]) + imul(v.z, m.m[6]),
        imul(v.x, m.m[1]) + imul(v.y, m.m[4]) + imul(v.z, m.m[7]),
        imul(v.x, m.m[2]) + imul(v.y, m.m[5]) + imul(v.z, m.m[8])
    );
}

static inline ivec4_t imat4x4transform(imat4x4_t m, ivec4_t v) {
    return ivec4(
        imul(v.x, m.m[0]) + imul(v.y, m.m[4]) + imul(v.z, m.m[8]) + imul(v.w, m.m[12]),
        imul(v.x, m.m[1]) + imul(v.y, m.m[5]) + imul(v.z, m.m[9]) + imul(v.w, m.m[13]),
        imul(v.x, m.m[2]) + imul(v.y, m.m[6]) + imul(v.z, m.m[10]) + imul(v.w, m.m[14]),
        imul(v.x, m.m[3]) + imul(v.y, m.m[7]) + imul(v.z, m.m[11]) + imul(v.w, m.m[15])
    );
}

static inline imat3x3_t imat3x3mul(imat3x3_t a, imat3x3_t b) {
    return imat3x3(
        imul(a.m[0], b.m[0]) + imul(a.m[3], b.m[1]) + imul(a.m[6], b.m[2]),
        imul(a.m[0], b.m[3]) + imul(a.m[3], b.m[4]) + imul(a.m[6], b.m[5]),
        imul(a.m[0], b.m[6]) + imul(a.m[3], b.m[7]) + imul(a.m[6], b.m[8]),

        imul(a.m[1], b.m[0]) + imul(a.m[4], b.m[1]) + imul(a.m[7], b.m[2]),
        imul(a.m[1], b.m[3]) + imul(a.m[4], b.m[4]) + imul(a.m[7], b.m[5]),
        imul(a.m[1], b.m[6]) + imul(a.m[4], b.m[7]) + imul(a.m[7], b.m[8]),

        imul(a.m[2], b.m[0]) + imul(a.m[5], b.m[1]) + imul(a.m[8], b.m[2]),
        imul(a.m[2], b.m[3]) + imul(a.m[5], b.m[4]) + imul(a.m[8], b.m[5]),
        imul(a.m[2], b.m[6]) + imul(a.m[5], b.m[7]) + imul(a.m[8], b.m[8])
    );
}

static inline imat4x4_t imat4x4mul(imat4x4_t a, imat4x4_t b) {
    imat4x4_t res;

    for (int i = 0; i < 16; i++) {
        int row = i & 3, column = i & 12;
        int32_t val = 0;

        for (int j = 0;j < 4; j++) {
            val += imul(a.m[row + j * 4], b.m[column + j]);
        }

        res.m[i] = val;
    }
    return res;
}

static inline imat4x4_t imat4x4affineinverse(imat4x4_t m) {
    imat4x4_t res;
    int32_t det=imul(imul(m.m[0],m.m[5]),m.m[10])-imul(imul(m.m[0],m.m[6]),m.m[9])+
                imul(imul(m.m[1],m.m[6]),m.m[8])-imul(imul(m.m[1],m.m[4]),m.m[10])+
                imul(imul(m.m[2],m.m[4]),m.m[9])-imul(imul(m.m[2],m.m[5]),m.m[8]);
    // singular if det==0

    res.m[0]=idiv((imul(m.m[5],m.m[10])-imul(m.m[6],m.m[9])),det);
    res.m[4]=-idiv((imul(m.m[4],m.m[10])-imul(m.m[6],m.m[8])),det);
    res.m[8]=idiv((imul(m.m[4],m.m[9])-imul(m.m[5],m.m[8])),det);

    res.m[1]=-idiv((imul(m.m[1],m.m[10])-imul(m.m[2],m.m[9])),det);
    res.m[5]=idiv((imul(m.m[0],m.m[10])-imul(m.m[2],m.m[8])),det);
    res.m[9]=-idiv((imul(m.m[0],m.m[9])-imul(m.m[1],m.m[8])),det);

    res.m[2]=idiv((imul(m.m[1],m.m[6])-imul(m.m[2],m.m[5])),det);
    res.m[6]=-idiv((imul(m.m[0],m.m[6])-imul(m.m[2],m.m[4])),det);
    res.m[10]=idiv((imul(m.m[0],m.m[5])-imul(m.m[1],m.m[4])),det);

    res.m[3]=0;
    res.m[7]=0;
    res.m[11]=0;

    res.m[12]=-(imul(m.m[12],res.m[0])+imul(m.m[13],res.m[4])+imul(m.m[14],res.m[8]));
    res.m[13]=-(imul(m.m[12],res.m[1])+imul(m.m[13],res.m[5])+imul(m.m[14],res.m[9]));
    res.m[14]=-(imul(m.m[12],res.m[2])+imul(m.m[13],res.m[6])+imul(m.m[14],res.m[10]));
    res.m[15]=INT_FIXED(1);

    return res;
}

static inline imat4x4_t imat4x4inverse(imat4x4_t m) {
    imat4x4_t res;

    int32_t a0=imul(m.m[0],m.m[5])-imul(m.m[1],m.m[4]);
    int32_t a1=imul(m.m[0],m.m[6])-imul(m.m[2],m.m[4]);
    int32_t a2=imul(m.m[0],m.m[7])-imul(m.m[3],m.m[4]);
    int32_t a3=imul(m.m[1],m.m[6])-imul(m.m[2],m.m[5]);
    int32_t a4=imul(m.m[1],m.m[7])-imul(m.m[3],m.m[5]);
    int32_t a5=imul(m.m[2],m.m[7])-imul(m.m[3],m.m[6]);
    int32_t b0=imul(m.m[8],m.m[13])-imul(m.m[9],m.m[12]);
    int32_t b1=imul(m.m[8],m.m[14])-imul(m.m[10],m.m[12]);
    int32_t b2=imul(m.m[8],m.m[15])-imul(m.m[11],m.m[12]);
    int32_t b3=imul(m.m[9],m.m[14])-imul(m.m[10],m.m[13]);
    int32_t b4=imul(m.m[9],m.m[15])-imul(m.m[11],m.m[13]);
    int32_t b5=imul(m.m[10],m.m[15])-imul(m.m[11],m.m[14]);
    int32_t det=imul(a0,b5)-imul(a1,b4)+imul(a2,b3)+imul(a3,b2)-imul(a4,b1)+imul(a5,b0);
    // singular if det==0

    res.m[0]=idiv((imul(m.m[5],b5)-imul(m.m[6],b4)+imul(m.m[7],b3)),det);
    res.m[4]=-idiv((imul(m.m[4],b5)-imul(m.m[6],b2)+imul(m.m[7],b1)),det);
    res.m[8]=idiv((imul(m.m[4],b4)-imul(m.m[5],b2)+imul(m.m[7],b0)),det);
    res.m[12]=-idiv((imul(m.m[4],b3)-imul(m.m[5],b1)+imul(m.m[6],b0)),det);

    res.m[1]=-idiv((imul(m.m[1],b5)-imul(m.m[2],b4)+imul(m.m[3],b3)),det);
    res.m[5]=idiv((imul(m.m[0],b5)-imul(m.m[2],b2)+imul(m.m[3],b1)),det);
    res.m[9]=-idiv((imul(m.m[0],b4)-imul(m.m[1],b2)+imul(m.m[3],b0)),det);
    res.m[13]=idiv((imul(m.m[0],b3)-imul(m.m[1],b1)+imul(m.m[2],b0)),det);

    res.m[2]=idiv((imul(m.m[13],a5)-imul(m.m[14],a4)+imul(m.m[15],a3)),det);
    res.m[6]=-idiv((imul(m.m[12],a5)-imul(m.m[14],a2)+imul(m.m[15],a1)),det);
    res.m[10]=idiv((imul(m.m[12],a4)-imul(m.m[13],a2)+imul(m.m[15],a0)),det);
    res.m[14]=-idiv((imul(m.m[12],a3)-imul(m.m[13],a1)+imul(m.m[14],a0)),det);

    res.m[3]=-idiv((imul(m.m[9],a5)-imul(m.m[10],a4)+imul(m.m[11],a3)),det);
    res.m[7]=idiv((imul(m.m[8],a5)-imul(m.m[10],a2)+imul(m.m[11],a1)),det);
    res.m[11]=-idiv((imul(m.m[8],a4)-imul(m.m[9],a2)+imul(m.m[11],a0)),det);
    res.m[15]=idiv((imul(m.m[8],a3)-imul(m.m[9],a1)+imul(m.m[10],a0)),det);

    return res;
}

static inline ivec3_t ivec3cross(ivec3_t a, ivec3_t b) {
    return ivec3(
        imul(a.y, b.z) - imul(a.z, b.y),
        imul(a.z, b.x) - imul(a.x, b.z),
        imul(a.x, b.y) - imul(a.y, b.x)
    );
}

static inline imat4x4_t imat4x4lookat(ivec3_t eye, ivec3_t lookat, ivec3_t up) {
    ivec3_t forward = ivec3norm(ivec3sub(lookat, eye));
    ivec3_t sideways = ivec3norm(ivec3cross(forward, up));
    ivec3_t realup = ivec3norm(ivec3cross(sideways, forward));

    /*return imat4x4(
        right.x, realup.x, forward.x, ivec3dot(eye, right),
        right.y, realup.y, forward.y, ivec3dot(eye, realup),
        right.z, realup.z, forward.z, ivec3dot(eye, forward),
        0,       0,        0,         INT_FIXED(1)
    );*/

    /*return imat4x4(
        right.x, right.y, right.z, -ivec3dot(eye, right),
        realup.x, realup.y, realup.z, -ivec3dot(eye, realup),
        -forward.x, -forward.y, -forward.z, -ivec3dot(eye, forward),
        0, 0, 0, INT_FIXED(1)
    );*/

    return imat4x4(
        sideways.x, sideways.y, sideways.z, -ivec3dot(eye, sideways),
        realup.x, realup.y, realup.z, -ivec3dot(eye, realup),
        -forward.x, -forward.y, -forward.z, ivec3dot(eye, forward),
        0, 0, 0, INT_FIXED(1)
    );
}
#endif
