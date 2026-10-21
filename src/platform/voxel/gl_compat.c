// OpenGL ES 2.0 emulation of the legacy fixed-function subset used by the
// voxel renderer. See gl_compat.h.

#if defined(__ANDROID__)

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include "gl_compat.h"

// Inside this file the unprefixed names must be the real GLES2 functions.
#undef glBegin
#undef glEnd
#undef glVertex2f
#undef glVertex3f
#undef glTexCoord2f
#undef glColor3f
#undef glColor4f
#undef glMatrixMode
#undef glLoadIdentity
#undef glOrtho
#undef glEnable
#undef glDisable
#undef glAlphaFunc
#undef glBindTexture
#undef glTexImage2D
#undef glDeleteTextures
#undef glBlendFunc
#undef glDepthFunc
#undef glViewport
#undef glClear
#undef glFlush

#define LOG_TAG "GLCompat"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef struct
{
    float x, y, z;
    float u, v;
    float r, g, b, a;
} Vertex;

typedef struct
{
    Vertex *data;
    int count;
    int capacity;
} VertexList;

static VertexList sImmediate;   // vertices between glBegin and glEnd
static VertexList sBatch;       // triangles waiting to be drawn

static GLenum sPrimitive = GL_TRIANGLES;
static float sCurU, sCurV;
static float sCurR = 1.0f, sCurG = 1.0f, sCurB = 1.0f, sCurA = 1.0f;

static GLenum sMatrixMode = GL_MODELVIEW;
static float sProjection[16];
static float sModelView[16];
static bool sMatricesInit = false;

static bool sTextureEnabled = false;
static bool sAlphaTestEnabled = false;
static float sAlphaRef = 0.5f;

static GLuint sProgram = 0;
static GLint sLocMvp = -1;
static GLint sLocTex = -1;
static GLint sLocUseTex = -1;
static GLint sLocAlphaTest = -1;
static GLint sLocAlphaRef = -1;
static GLint sLocPos = -1;
static GLint sLocUV = -1;
static GLint sLocColor = -1;
static bool sProgramFailed = false;

// ---------------------------------------------------------------------------
//  Small helpers
// ---------------------------------------------------------------------------

static void ListPush(VertexList *list, Vertex v)
{
    if (list->count == list->capacity)
    {
        int newCap = list->capacity ? list->capacity * 2 : 4096;
        Vertex *p = realloc(list->data, newCap * sizeof(Vertex));
        if (p == NULL)
            return;
        list->data = p;
        list->capacity = newCap;
    }
    list->data[list->count++] = v;
}

static void MatIdentity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// out = a * b (column-major)
static void MatMul(float *out, const float *a, const float *b)
{
    float tmp[16];
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += a[k * 4 + row] * b[col * 4 + k];
            tmp[col * 4 + row] = sum;
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

static void InitMatrices(void)
{
    if (!sMatricesInit)
    {
        MatIdentity(sProjection);
        MatIdentity(sModelView);
        sMatricesInit = true;
    }
}

static float *CurrentMatrix(void)
{
    InitMatrices();
    return sMatrixMode == GL_PROJECTION ? sProjection : sModelView;
}

// ---------------------------------------------------------------------------
//  Shader program
// ---------------------------------------------------------------------------

static const char *kVertexSrc =
    "uniform mat4 uMvp;\n"
    "attribute vec3 aPos;\n"
    "attribute vec2 aUV;\n"
    "attribute vec4 aColor;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    vColor = aColor;\n"
    "    gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "}\n";

static const char *kFragmentSrc =
    "precision mediump float;\n"
    "uniform sampler2D uTex;\n"
    "uniform float uUseTex;\n"
    "uniform float uAlphaTest;\n"
    "uniform float uAlphaRef;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "    vec4 c = vColor;\n"
    "    if (uUseTex > 0.5)\n"
    "        c *= texture2D(uTex, vUV);\n"
    "    if (uAlphaTest > 0.5 && c.a <= uAlphaRef)\n"
    "        discard;\n"
    "    gl_FragColor = c;\n"
    "}\n";

static GLuint CompileShader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool EnsureProgram(void)
{
    if (sProgram != 0)
        return true;
    if (sProgramFailed)
        return false;

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    if (vs == 0 || fs == 0)
    {
        sProgramFailed = true;
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        LOGE("program link failed: %s", log);
        glDeleteProgram(program);
        sProgramFailed = true;
        return false;
    }

    sProgram = program;
    sLocMvp = glGetUniformLocation(program, "uMvp");
    sLocTex = glGetUniformLocation(program, "uTex");
    sLocUseTex = glGetUniformLocation(program, "uUseTex");
    sLocAlphaTest = glGetUniformLocation(program, "uAlphaTest");
    sLocAlphaRef = glGetUniformLocation(program, "uAlphaRef");
    sLocPos = glGetAttribLocation(program, "aPos");
    sLocUV = glGetAttribLocation(program, "aUV");
    sLocColor = glGetAttribLocation(program, "aColor");
    return true;
}

// ---------------------------------------------------------------------------
//  Batch flush
// ---------------------------------------------------------------------------

void glcFlushBatch(void)
{
    if (sBatch.count == 0)
        return;

    if (EnsureProgram())
    {
        float mvp[16];
        InitMatrices();
        MatMul(mvp, sProjection, sModelView);

        glUseProgram(sProgram);
        glUniformMatrix4fv(sLocMvp, 1, GL_FALSE, mvp);
        glUniform1i(sLocTex, 0);
        glUniform1f(sLocUseTex, sTextureEnabled ? 1.0f : 0.0f);
        glUniform1f(sLocAlphaTest, sAlphaTestEnabled ? 1.0f : 0.0f);
        glUniform1f(sLocAlphaRef, sAlphaRef);

        const GLsizei stride = sizeof(Vertex);
        const Vertex *base = sBatch.data;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(sLocPos);
        glEnableVertexAttribArray(sLocUV);
        glEnableVertexAttribArray(sLocColor);
        glVertexAttribPointer(sLocPos, 3, GL_FLOAT, GL_FALSE, stride, &base->x);
        glVertexAttribPointer(sLocUV, 2, GL_FLOAT, GL_FALSE, stride, &base->u);
        glVertexAttribPointer(sLocColor, 4, GL_FLOAT, GL_FALSE, stride, &base->r);
        glDrawArrays(GL_TRIANGLES, 0, sBatch.count);
        glDisableVertexAttribArray(sLocPos);
        glDisableVertexAttribArray(sLocUV);
        glDisableVertexAttribArray(sLocColor);
    }
    sBatch.count = 0;
}

void glcFlush(void)
{
    glcFlushBatch();
    glFlush();
}

// ---------------------------------------------------------------------------
//  Immediate mode
// ---------------------------------------------------------------------------

void glcBegin(GLenum mode)
{
    sPrimitive = mode;
    sImmediate.count = 0;
}

static void PushImmediate(float x, float y, float z)
{
    Vertex v = { x, y, z, sCurU, sCurV, sCurR, sCurG, sCurB, sCurA };
    ListPush(&sImmediate, v);
}

void glcVertex2f(GLfloat x, GLfloat y) { PushImmediate(x, y, 0.0f); }
void glcVertex3f(GLfloat x, GLfloat y, GLfloat z) { PushImmediate(x, y, z); }

void glcTexCoord2f(GLfloat u, GLfloat v)
{
    sCurU = u;
    sCurV = v;
}

void glcColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    sCurR = r; sCurG = g; sCurB = b; sCurA = 1.0f;
}

void glcColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    sCurR = r; sCurG = g; sCurB = b; sCurA = a;
}

void glcEnd(void)
{
    const Vertex *v = sImmediate.data;
    int n = sImmediate.count;

    if (sPrimitive == GL_QUADS)
    {
        for (int i = 0; i + 3 < n; i += 4)
        {
            ListPush(&sBatch, v[i + 0]);
            ListPush(&sBatch, v[i + 1]);
            ListPush(&sBatch, v[i + 2]);
            ListPush(&sBatch, v[i + 0]);
            ListPush(&sBatch, v[i + 2]);
            ListPush(&sBatch, v[i + 3]);
        }
    }
    else if (sPrimitive == GL_TRIANGLES)
    {
        for (int i = 0; i + 2 < n; i += 3)
        {
            ListPush(&sBatch, v[i + 0]);
            ListPush(&sBatch, v[i + 1]);
            ListPush(&sBatch, v[i + 2]);
        }
    }
    sImmediate.count = 0;

    // Keep memory bounded when a frame emits a very large amount of geometry.
    if (sBatch.count > 120000)
        glcFlushBatch();
}

// ---------------------------------------------------------------------------
//  Matrix stack
// ---------------------------------------------------------------------------

void glcMatrixMode(GLenum mode)
{
    sMatrixMode = mode;
}

void glcLoadIdentity(void)
{
    glcFlushBatch();
    MatIdentity(CurrentMatrix());
}

void glcOrtho(double l, double r, double b, double t, double n, double f)
{
    glcFlushBatch();
    float m[16];
    MatIdentity(m);
    m[0]  = (float)(2.0 / (r - l));
    m[5]  = (float)(2.0 / (t - b));
    m[10] = (float)(-2.0 / (f - n));
    m[12] = (float)(-(r + l) / (r - l));
    m[13] = (float)(-(t + b) / (t - b));
    m[14] = (float)(-(f + n) / (f - n));
    float *cur = CurrentMatrix();
    MatMul(cur, cur, m);
}

void gluPerspective(double fovyDeg, double aspect, double zNear, double zFar)
{
    glcFlushBatch();
    double fovy = fovyDeg * 3.14159265358979323846 / 180.0;
    double f = 1.0 / tan(fovy / 2.0);
    float m[16];
    memset(m, 0, sizeof(m));
    m[0]  = (float)(f / aspect);
    m[5]  = (float)f;
    m[10] = (float)((zFar + zNear) / (zNear - zFar));
    m[11] = -1.0f;
    m[14] = (float)((2.0 * zFar * zNear) / (zNear - zFar));
    float *cur = CurrentMatrix();
    MatMul(cur, cur, m);
}

void gluLookAt(double eyeX, double eyeY, double eyeZ,
               double centerX, double centerY, double centerZ,
               double upX, double upY, double upZ)
{
    glcFlushBatch();

    double fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
    double fl = sqrt(fx * fx + fy * fy + fz * fz);
    if (fl == 0.0)
        return;
    fx /= fl; fy /= fl; fz /= fl;

    // s = f x up
    double sx = fy * upZ - fz * upY;
    double sy = fz * upX - fx * upZ;
    double sz = fx * upY - fy * upX;
    double sl = sqrt(sx * sx + sy * sy + sz * sz);
    if (sl == 0.0)
        return;
    sx /= sl; sy /= sl; sz /= sl;

    // u = s x f
    double ux = sy * fz - sz * fy;
    double uy = sz * fx - sx * fz;
    double uz = sx * fy - sy * fx;

    float m[16];
    MatIdentity(m);
    m[0] = (float)sx;  m[4] = (float)sy;  m[8]  = (float)sz;
    m[1] = (float)ux;  m[5] = (float)uy;  m[9]  = (float)uz;
    m[2] = (float)-fx; m[6] = (float)-fy; m[10] = (float)-fz;

    float t[16];
    MatIdentity(t);
    t[12] = (float)-eyeX;
    t[13] = (float)-eyeY;
    t[14] = (float)-eyeZ;

    float *cur = CurrentMatrix();
    MatMul(cur, cur, m);
    MatMul(cur, cur, t);
}

// ---------------------------------------------------------------------------
//  State wrappers: flush pending geometry first, then change state.
// ---------------------------------------------------------------------------

void glcEnable(GLenum cap)
{
    glcFlushBatch();
    if (cap == GL_TEXTURE_2D)
        sTextureEnabled = true;
    else if (cap == GL_ALPHA_TEST)
        sAlphaTestEnabled = true;
    else
        glEnable(cap);
}

void glcDisable(GLenum cap)
{
    glcFlushBatch();
    if (cap == GL_TEXTURE_2D)
        sTextureEnabled = false;
    else if (cap == GL_ALPHA_TEST)
        sAlphaTestEnabled = false;
    else
        glDisable(cap);
}

void glcAlphaFunc(GLenum func, GLfloat ref)
{
    (void)func; // only GL_GREATER is used by the voxel code
    glcFlushBatch();
    sAlphaRef = ref;
}

void glcBindTexture(GLenum target, GLuint texture)
{
    glcFlushBatch();
    glBindTexture(target, texture);
}

void glcTexImage2D(GLenum target, GLint level, GLint internalformat,
                   GLsizei width, GLsizei height, GLint border,
                   GLenum format, GLenum type, const void *pixels)
{
    glcFlushBatch();
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glcDeleteTextures(GLsizei n, const GLuint *textures)
{
    glcFlushBatch();
    glDeleteTextures(n, textures);
}

void glcBlendFunc(GLenum sfactor, GLenum dfactor)
{
    glcFlushBatch();
    glBlendFunc(sfactor, dfactor);
}

void glcDepthFunc(GLenum func)
{
    glcFlushBatch();
    glDepthFunc(func);
}

void glcViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    glcFlushBatch();
    glViewport(x, y, w, h);
}

void glcClear(GLbitfield mask)
{
    glcFlushBatch();
    glClear(mask);
}

#endif // __ANDROID__
