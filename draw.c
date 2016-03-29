/*
 *  echinus wm written by Alexander Polakov <polachok@gmail.com>
 *  this file contains code related to drawing
 */

#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include "pegasus.h"
#include "config.h"

enum { Normal, Selected };
enum { AlignLeft, AlignCenter, AlignRight };

typedef struct {
	unsigned int x, y, w, h;
	GC gc;
} DC;

DC dc;

static int drawbutton(Drawable d, Button btn, unsigned long col[ColLast], int x, int y) {
	if (btn.action == NULL)
		return 0;

	XSetForeground(dpy, dc.gc, col[ColBG]);
	XFillRectangle(dpy, d, dc.gc, x, 0, dc.h, dc.h);
	XSetForeground(dpy, dc.gc, btn.pressed ? col[ColFG] : col[ColButton]);
	XSetBackground(dpy, dc.gc, col[ColBG]);
	XCopyPlane(dpy, btn.pm, d, dc.gc, 0, 0, button[Iconify].pw,
	           button[Iconify].ph, x, y + button[Iconify].py, 1);

	return dc.h;
}

static int drawelement(char which, int x, int position, Client *c) {
	int w;
	unsigned int j;
	unsigned long *color = c == sel ? style.color.sel : style.color.norm;

	switch (which) {
		case '|':
			XSetForeground(dpy, dc.gc, color[ColBorder]);
			XDrawLine(dpy, c->drawable, dc.gc, dc.x + dc.h / 4, 0,
			          dc.x + dc.h / 4, dc.h);
			w = dc.h / 2;
			break;
		case 'I':
			button[Iconify].x = dc.x;
			w = drawbutton(c->drawable, button[Iconify], color,
			               dc.x, dc.h / 2 - button[Iconify].ph / 2);
			break;
		case 'M':
			button[Maximize].x = dc.x;
			w = drawbutton(c->drawable, button[Maximize], color,
			               dc.x, dc.h / 2 - button[Maximize].ph / 2);
			break;
		case 'C':
			button[Close].x = dc.x;
			w = drawbutton(c->drawable, button[Close], color, dc.x,
			               dc.h / 2 - button[Maximize].ph / 2);
			break;
		default:
			w = 0;
			break;
	}

	return w;
}

static int elementw(char which, Client *c) {
	int w;
	unsigned int j;

	switch (which) {
		case 'I':
		case 'M':
		case 'C':
			return dc.h;
		case '|':
			return dc.h / 2;
	}

	return 0;
}

void drawclient(Client *c) {
	size_t i;

	if (!isvisible(c, NULL))
		return;

	if (!c->title)
		return;

	dc.x = dc.y = 0;
	dc.w = c->w;
	dc.h = style.titleheight;
	XSetForeground(dpy, dc.gc, c == sel ? style.color.sel[ColBG] : style.color.norm[ColBG]);
	XSetLineAttributes(dpy, dc.gc, style.border, LineSolid, CapNotLast, JoinMiter);
	XFillRectangle(dpy, c->drawable, dc.gc, dc.x, dc.y, dc.w, dc.h);

	/* Left */
	for (i = 0; i < strlen(style.titlelayout); i++) {
		if (i == 0)
			dc.x += 18;

		if (style.titlelayout[i] == ' ' || style.titlelayout[i] == '-')
			break;

		dc.x += drawelement(style.titlelayout[i], dc.x, AlignLeft, c);
	}
	if (i == strlen(style.titlelayout) || dc.x >= dc.w)
		goto end;

	/* Center */
	dc.x = dc.w / 2;
	for (i++; i < strlen(style.titlelayout); i++) {
		if (style.titlelayout[i] == ' ' || style.titlelayout[i] == '-')
			break;

		dc.x -= elementw(style.titlelayout[i], c) / 2;
		dc.x += drawelement(style.titlelayout[i], 0, AlignCenter, c);
	}
	if (i == strlen(style.titlelayout) || dc.x >= dc.w)
		goto end;

	/* Right */
	dc.x = dc.w;
	for (i = strlen(style.titlelayout); i-- ; ) {
		if (style.titlelayout[i] == ' ' || style.titlelayout[i] == '-')
			break;

		dc.x -= elementw(style.titlelayout[i], c);
		drawelement(style.titlelayout[i], 0, AlignRight, c);
	}

	end:
	if (style.outline) {
		XSetForeground(dpy, dc.gc,
		    c == sel ? style.color.sel[ColBorder] : style.color.norm[ColBorder]);
		XDrawLine(dpy, c->drawable, dc.gc, 0, dc.h - 1, dc.w, dc.h - 1);
	}

	XCopyArea(dpy, c->drawable, c->title, dc.gc, 0, 0, c->w, dc.h, 0, 0);
}

static unsigned long getcolor(const char *colstr) {
	XColor color;

	if (!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), colstr, &color, &color))
		eprint("error, cannot allocate color '%s'\n", colstr);

	return color.pixel;
}

static int initpixmap(const char *file, Button *b) {
	b->pm = XCreatePixmap(dpy, root, style.titleheight, style.titleheight, 1);

	if (BitmapSuccess == XReadBitmapFile(dpy, root, file, &b->pw, &b->ph,
	    &b->pm, &b->px, &b->py)) {
		if (b->px == -1 || b->py == -1)
			b->px = b->py = 0;

		return 0;
	} else
		return 1;
}

static void initbuttons() {
	button[Iconify].action = iconify;
	button[Maximize].action = togglemax;
	button[Close].action = killclient;
	button[Iconify].x = button[Close].x = button[Maximize].x = -1;
	XSetForeground(dpy, dc.gc, style.color.norm[ColButton]);
	XSetBackground(dpy, dc.gc, style.color.norm[ColBG]);

	if (initpixmap(getresource("button.iconify.pixmap", ICONPIXMAP),
	    &button[Iconify]))
		button[Iconify].action = NULL;
	if (initpixmap(getresource("button.maximize.pixmap", MAXPIXMAP),
	    &button[Maximize]))
		button[Maximize].action = NULL;
	if (initpixmap(getresource("button.close.pixmap", CLOSEPIXMAP),
	    &button[Close]))
		button[Close].action = NULL;
}

void initstyle() {
	style.color.norm[ColBorder] = getcolor(getresource("normal.border", NORMBORDERCOLOR));
	style.color.norm[ColBG] = getcolor(getresource("normal.bg", NORMBGCOLOR));
	style.color.norm[ColFG] = getcolor(getresource("normal.fg", NORMFGCOLOR));
	style.color.norm[ColButton] = getcolor(getresource("normal.button", NORMBUTTONCOLOR));

	style.color.sel[ColBorder] = getcolor(getresource("selected.border", SELBORDERCOLOR));
	style.color.sel[ColBG] = getcolor(getresource("selected.bg", SELBGCOLOR));
	style.color.sel[ColFG] = getcolor(getresource("selected.fg", SELFGCOLOR));
	style.color.sel[ColButton] = getcolor(getresource("selected.button", SELBUTTONCOLOR));

	style.border = atoi(getresource("border", STR(BORDERPX)));
	style.outline = atoi(getresource("outline", "0"));
	strncpy(style.titlelayout, getresource("titlelayout", "IMC"),
	    LENGTH(style.titlelayout));
	style.titlelayout[LENGTH(style.titlelayout) - 1] = '\0';
	style.titleheight = atoi(getresource("title", STR(TITLEHEIGHT)));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	initbuttons();
}

void deinitstyle() {
	/* XXX: more to do */
	XFreeGC(dpy, dc.gc);
}
