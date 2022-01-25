
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define ASIZE(x) ((sizeof (x)) / (sizeof *(x)))

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display *, GLXFBConfig, GLXContext, Bool, const int *);

static bool isExtensionSupported(const char *extList, const char *extension) {
	const char *start;
	const char *where, *terminator;

	where = strchr(extension, ' ');
	if (where || *extension == '\0') {
		return false;
	}

	for (start=extList;;) {
		where = strstr(start, extension);

		if (!where) {
		 	break;
		}

		terminator = where + strlen(extension);

		if ( where == start || *(where - 1) == ' ' ) {
			if ( *terminator == ' ' || *terminator == '\0' ) {
				return true;
			}
		}	

		start = terminator;
	}

	return false;
}


int main(int argc, char **argv)
{
	Display *display = XOpenDisplay(NULL);
	if (!display) {
		fprintf(stderr, "could not open display\n");
		exit(EXIT_FAILURE);
	}	
	
	Screen *screen = DefaultScreenOfDisplay(display);
	int screenId = DefaultScreen(display);

	GLint majorGLX, minorGLX = 0;
	glXQueryVersion(display, &majorGLX, &minorGLX);
	if (majorGLX <= 1 && minorGLX < 2) {
		fprintf(stderr, "GLX 1.2 or greater is required\n");
		XCloseDisplay(display);
		exit(EXIT_FAILURE);
	}
	else {
		printf("GLX client version: %s\n"
		       "GLX client vendor: %s\n"
		       "GLX client extensions: %s\n"
		       "GLX server version: %s\n"
		       "GLX server vendor: %s\n"
		       "GLX server extensions: %s\n",
		       glXGetClientString(display, GLX_VERSION),
		       glXGetClientString(display, GLX_VENDOR),
		       glXGetClientString(display, GLX_EXTENSIONS),
		       glXQueryServerString(display, screenId, GLX_VERSION),
		       glXQueryServerString(display, screenId, GLX_VENDOR),
		       glXQueryServerString(display, screenId, GLX_EXTENSIONS));
	}

	GLint glxAttribs[] = {
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
		None
	};

	int fbcount;
	GLXFBConfig *fbc = glXChooseFBConfig(display, screenId, glxAttribs, &fbcount);
	if (!fbc) {
		fprintf(stderr, "failed to retrieve framebuffer\n");
		XCloseDisplay(display);
		exit(EXIT_FAILURE);
	}

	printf("Found %d matching framebuffers.\nGetting best XVisualInfo\n");
	int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
	for (int i = 0; i < fbcount; i++) {
		XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[i]);
		if (vi != 0) {
			int samp_buf, samples;
			glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
			glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLES, &samples);
			printf("Matching fbconfig %d, SAMPLE_BUFFERS = %d, SAMPLES = %d\n",
			       i, samp_buf, samples);
			if (best_fbc < 0 || (samp_buf && samples > best_num_samp)) {
				best_fbc = i;
				best_num_samp = samples;
			}
			if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp) {
				worst_fbc = i;
				worst_num_samp = samples;
			}
		}
		XFree(vi);
	}

	printf("Best visual info index: %d\n", best_fbc);
	GLXFBConfig best_fb_config = fbc[best_fbc];
	XFree(fbc);

	XVisualInfo *visual = glXGetVisualFromFBConfig(display, best_fb_config);

	if (!visual) {
		fprintf(stderr, "Could not create correct visual window.\n");
		XCloseDisplay(display);
		exit(EXIT_FAILURE);
	}

	if (screenId != visual->screen) {
		fprintf(stderr, "screenId (%d) does not match visual->screen (%d)\n",
			screenId, visual->screen);
	}
	
	Window root = RootWindowOfScreen(screen);
	XSetWindowAttributes window_attribs;
	window_attribs.border_pixel = BlackPixel(display, screenId);
	window_attribs.background_pixel = WhitePixel(display, screenId);
	window_attribs.override_redirect = True;
	window_attribs.colormap = XCreateColormap(display, root, visual->visual, AllocNone);
	window_attribs.event_mask = ExposureMask;
	
	Window window = XCreateWindow(display, root,
				      0, 0, 800, 800, 0,
				      visual->depth, InputOutput, visual->visual,
				      CWBackPixel | CWColormap | CWBorderPixel | CWEventMask,
				      &window_attribs);

	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
	const char *glx_exts = glXQueryExtensionsString(display, screenId);
	printf("Late extensions:\n%s\n\n", glx_exts);
	if (!glXCreateContextAttribsARB) {
		printf("glXCreateContextAttribsARB() not found\n");
	}

	int context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 2,
		GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
	};

	GLXContext context = 0;
	if (!isExtensionSupported(glx_exts, "GLX_ARB_create_context")) {
		context = glXCreateNewContext(display, best_fb_config, GLX_RGBA_TYPE, 0, True);
	}
	else {
		context = glXCreateContextAttribsARB(display, best_fb_config, 0, true, context_attribs);
	}

	XSync(display, False);

	if (!glXIsDirect(display, context)) {
		printf("Indirect GLX rendering context obtained\n");
	}
	else {
		printf("Direct GLX rendering context obtained\n");
	}
	glXMakeCurrent(display, window, context);

	printf("GL Vendor: %s\n"
	       "GL Renderer: %s\n"
	       "GL Version: %s\n"
	       "GL Shading Language: %s\n",
	       glGetString(GL_VENDOR), glGetString(GL_RENDERER),
	       glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	XStoreName(display, window, "qsketch");
	XSelectInput(display, window, KeyPressMask | KeyReleaseMask | KeymapStateMask);
	XClearWindow(display, window);
	XMapRaised(display, window);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	
	XEvent ev = {0};
	KeySym keysym = 0;
	char str[32] = {0};
	int length = 0;
	bool running = true;

	while (running) {
		XNextEvent(display, &ev);
		switch(ev.type) {
		case KeymapNotify:
			XRefreshKeyboardMapping(&ev.xmapping);
		case KeyPress:
			length = XLookupString(&ev.xkey, str, ASIZE(str), &keysym, NULL);
			if (keysym == XK_Escape) {
				running = false;
			}
			break;
		}

		glClear(GL_COLOR_BUFFER_BIT);
		glXSwapBuffers(display, window);
	}

	glXDestroyContext(display, context);
	
	XFree(visual);
	XFreeColormap(display, window_attribs.colormap);
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	exit(EXIT_SUCCESS);
}
