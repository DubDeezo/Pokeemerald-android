#ifndef GUARD_GL_COMPAT_H
#define GUARD_GL_COMPAT_H

// Graphics API portability layer for the voxel renderer.
//
// On desktop Linux the voxel code uses the legacy fixed-function OpenGL 2.1
// API directly (glBegin/glEnd, matrix stack, GL_ALPHA_TEST, GLU). OpenGL ES 2.0
// (Android) has none of that, so on Android this header redirects those calls
// to a small emulation layer (gl_compat.c) that batches the geometry into
// triangles and draws it with a single shader program.
//
// Only the subset of the legacy API that the voxel code actually uses is
// emulated: GL_QUADS / GL_TRIANGLES immediate mode, the projection/modelview
// matrix stack (identity, ortho, perspective, lookAt), the texture on/off
// switch, and the alpha test.

#if defined(__ANDROID__)

#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tokens that exist in desktop GL but not in GLES2.
#ifndef GL_QUADS
#define GL_QUADS      0x0007
#endif
#ifndef GL_MODELVIEW
#define GL_MODELVIEW  0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

// Immediate mode.
void glcBegin(GLenum mode);
void glcEnd(void);
void glcVertex2f(GLfloat x, GLfloat y);
void glcVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glcTexCoord2f(GLfloat u, GLfloat v);
void glcColor3f(GLfloat r, GLfloat g, GLfloat b);
void glcColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

// Matrix stack.
void glcMatrixMode(GLenum mode);
void glcLoadIdentity(void);
void glcOrtho(double l, double r, double b, double t, double n, double f);
void gluPerspective(double fovyDeg, double aspect, double zNear, double zFar);
void gluLookAt(double eyeX, double eyeY, double eyeZ,
               double centerX, double centerY, double centerZ,
               double upX, double upY, double upZ);

// State that must flush the pending batch before it changes.
void glcEnable(GLenum cap);
void glcDisable(GLenum cap);
void glcAlphaFunc(GLenum func, GLfloat ref);
void glcBindTexture(GLenum target, GLuint texture);
void glcTexImage2D(GLenum target, GLint level, GLint internalformat,
                   GLsizei width, GLsizei height, GLint border,
                   GLenum format, GLenum type, const void *pixels);
void glcDeleteTextures(GLsizei n, const GLuint *textures);
void glcBlendFunc(GLenum sfactor, GLenum dfactor);
void glcDepthFunc(GLenum func);
void glcViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void glcClear(GLbitfield mask);

// Draw anything still batched. Call before swapping buffers.
void glcFlushBatch(void);
void glcFlush(void);

#define glBegin        glcBegin
#define glEnd          glcEnd
#define glVertex2f     glcVertex2f
#define glVertex3f     glcVertex3f
#define glTexCoord2f   glcTexCoord2f
#define glColor3f      glcColor3f
#define glColor4f      glcColor4f
#define glMatrixMode   glcMatrixMode
#define glLoadIdentity glcLoadIdentity
#define glOrtho        glcOrtho
#define glEnable       glcEnable
#define glDisable      glcDisable
#define glAlphaFunc    glcAlphaFunc
#define glBindTexture  glcBindTexture
#define glTexImage2D   glcTexImage2D
#define glDeleteTextures glcDeleteTextures
#define glBlendFunc    glcBlendFunc
#define glDepthFunc    glcDepthFunc
#define glViewport     glcViewport
#define glClear        glcClear
#define glFlush        glcFlush

#ifdef __cplusplus
}
#endif

#else // desktop OpenGL

#include <GL/gl.h>
#include <GL/glu.h>

static inline void glcFlushBatch(void) {}

#endif

#endif // GUARD_GL_COMPAT_H
