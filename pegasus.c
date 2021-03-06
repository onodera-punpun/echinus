/*
 *  echinus wm written by Alexander Polakov <polachok at gmail dot com>
 *  pegasus wm forked by Camille Scholtz <onodera at openmailbox dot org>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <signal.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include "pegasus.h"

#define BUTTONMASK            (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)       (mask & ~(numlockmask | LockMask))
#define MOUSEMASK             (BUTTONMASK | PointerMotionMask)
#define CLIENTMASK            (PropertyChangeMask | StructureNotifyMask | FocusChangeMask)
#define CLIENTNOPROPAGATEMASK (BUTTONMASK | ButtonMotionMask)
#define FRAMEMASK             (MOUSEMASK | SubstructureRedirectMask | SubstructureNotifyMask | EnterWindowMask | LeaveWindowMask)

enum { StrutsOn, StrutsOff, StrutsHide };
enum { CurNormal, CurResize, CurMove, CurLast };
enum { Clk2Focus, SloppyFloat, AllSloppy, SloppyRaise };

void applyatoms(Client *c);
void applyrules(Client *c);
void arrange(Monitor *m);
void attach(Client *c, Bool attachaside);
void attachstack(Client *c);
void ban(Client *c);
void buttonpress(XEvent *e);
void bstack(Monitor *m);
void checkotherwm(void);
void cleanup(void);
void compileregs(void);
void configure(Client *c);
void configurenotify(XEvent *e);
void configurerequest(XEvent *e);
void destroynotify(XEvent *e);
void detach(Client *c);
void detachstack(Client *c);
void *emallocz(unsigned int size);
void enternotify(XEvent *e);
void eprint(const char *errstr, ...);
void expose(XEvent *e);
void iconify(const char *arg);
void incnmaster(const char *arg);
void focus(Client *c);
Client *getclient(Window w, Client *list, int part);
const char *getresource(const char *resource, const char *defval);
long getstate(Window w);
Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
void getpointer(int *x, int *y);
Monitor *getmon(int x, int y);
Monitor *curmon();
Monitor *clientmon(Client *c);
int idxoftag(const char *tag);
Bool isvisible(Client *c, Monitor *m);
void initmons(XEvent *e);
void keypress(XEvent *e);
void killclient(const char *arg);
void leavenotify(XEvent *e);
void focusin(XEvent *e);
void manage(Window w, XWindowAttributes *wa);
void mappingnotify(XEvent *e);
void monocle(Monitor *m);
void maprequest(XEvent *e);
void mousemove(Client *c);
void mouseresize(Client *c);
void moveresizekb(const char *arg);
Client *nexttiled(Client *c, Monitor *m);
Client *prevtiled(Client *c, Monitor *m);
void place(Client *c);
void propertynotify(XEvent *e);
void reparentnotify(XEvent *e);
void quit(const char *arg);
void restart(const char *arg);
void resize(Client *c, int x, int y, int w, int h, Bool sizehints);
void restack(Monitor *m);
void run(void);
void save(Client *c);
void scan(void);
void setclientstate(Client *c, long state);
void setlayout(const char *arg);
void setmax(Client *c);
void setmwfact(const char *arg);
void setunmax(Client *c);
void setup(char *);
void spawn(const char *arg);
void tag(const char *arg);
void fibonacci(Monitor *m, int s);
void dwindle(Monitor *m);
void spiral(Monitor *m);
void togglestruts(const char *arg);
void togglefloatingwin(const char *arg);
void togglefloatingtag(const char *arg);
void togglemax(const char *arg);
void togglefill(const char *arg);
void toggletag(const char *arg);
void toggleview(const char *arg);
void togglemon(const char *arg);
void focusview(const char *arg);
void unban(Client *c);
void unmanage(Client *c);
void updategeom(Monitor *m);
void updatestruts(Monitor *m);
void unmapnotify(XEvent *e);
void updatesizehints(Client *c);
void updateframe(Client *c);
void updatetitle(Client *c);
void view(const char *arg);
void viewprevtag(const char *arg);
void viewlefttag(const char *arg);
void viewrighttag(const char *arg);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dsply, XErrorEvent *ee);
int xerrorstart(Display *dsply, XErrorEvent *ee);
int (*xerrorxlib) (Display *, XErrorEvent *);
void zoom(const char *arg);

char **cargv;
Display *dpy;
int screen;
Window root;
XrmDatabase xrdb;
Bool otherwm;
Bool running = True;
Bool selscreen = True;
Monitor *mons;
Client *clients;
Client *sel;
Client *stack;
Cursor cursor[CurLast];
Style style;
Button button[LastBtn];
View *views;
Key **keys;
Rule **rules;
char **tags;
unsigned int ntags;
unsigned int nkeys;
unsigned int nrules;
unsigned int modkey;
unsigned int numlockmask;

#include "config.h"

struct {
	Bool attachaside;
	Bool dectiled;
	const char *deftilinglayout;
	Bool hidebastards;
	int focus;
	int gap;
	int margin;
	int snap;
	char rootrightclick[255];
	char rootscrolldown[255];
	char rootscrollup[255];
} options;

Layout layouts[] = {
	{  NULL,    'i',  OVERLAP },
	{  dwindle, 'd',  MWFACT | NMASTER | ZOOM },
	{  spiral,  's',  MWFACT | NMASTER | ZOOM },
	{  bstack,  'b',  MWFACT | ZOOM },
	{  monocle, 'm',  0 },
	{  NULL,    'f',  OVERLAP },
	{  NULL,    '\0', 0 },
};

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress]      = buttonpress,
	[ButtonRelease]    = buttonpress,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify]  = configurenotify,
	[DestroyNotify]    = destroynotify,
	[EnterNotify]      = enternotify,
	[LeaveNotify]      = leavenotify,
	[FocusIn]          = focusin,
	[Expose]           = expose,
	[KeyPress]         = keypress,
	[MappingNotify]    = mappingnotify,
	[MapRequest]       = maprequest,
	[PropertyNotify]   = propertynotify,
	[ReparentNotify]   = reparentnotify,
	[UnmapNotify]      = unmapnotify,
	[ClientMessage]    = clientmessage,
};

void applyatoms(Client *c) {
	unsigned int *t;
	unsigned long n;
	int i;

	/* Restore tag number from atom */
	t = (unsigned int*)getatom(c->win, atom[WindowDesk], &n);
	if (n != 0) {
		if (*t >= ntags)
			return;

		for (i = 0; i < ntags; i++)
			c->tags[i] = (i == *t) ? 1 : 0;
	}
}

void applyrules(Client *c) {
	static char buf[512];
	unsigned int i, j;
	regmatch_t tmp;
	Bool matched = False;
	XClassHint ch = { 0 };

	/* Rule matching */
	XGetClassHint(dpy, c->win, &ch);
	snprintf(buf, sizeof(buf), "%s:%s:%s", ch.res_class ? ch.res_class : "",
	ch.res_name ? ch.res_name : "", c->name);
	buf[LENGTH(buf)-1] = 0;

	for (i = 0; i < nrules; i++) {
		if (rules[i]->propregex && !regexec(rules[i]->propregex, buf, 1, &tmp, 0)) {
			c->isfloating = rules[i]->isfloating;
			c->title = rules[i]->hastitle;
			for (j = 0; rules[i]->tagregex && j < ntags; j++) {
				if (!regexec(rules[i]->tagregex, tags[j], 1, &tmp, 0)) {
					matched = True;
					c->tags[j] = True;
				}
			}
		}
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (!matched)
		memcpy(c->tags, curseltags, ntags * sizeof(curseltags[0]));
}

void arrangefloats(Monitor *m) {
	Client *c;
	Monitor *om;
	int dx, dy;

	for (c = stack; c; c = c->snext) {
		if (isvisible(c, m) && !c->isbastard
		    && (c->isfloating || MFEATURES(m, OVERLAP))
		    && !c->ismax) {
			DPRINTF("%d %d\n", c->rx, c->ry);
			if (!(om = getmon(c->rx + c->rw/2,
			c->ry + c->rh/2)))
				continue;
			dx = om->sx + om->sw - c->rx;
			dy = om->sy + om->sh - c->ry;
			if (dx > m->sw) 
				dx = m->sw;
			if (dy > m->sh) 
				dy = m->sh;

			resize(c, m->sx + m->sw - dx, m->sy + m->sh - dy, c->rw, c->rh, True);
			save(c);
		}
	}
}

void arrangemon(Monitor *m) {
	Client *c;

	if (views[m->curtag].layout->arrange)
	    views[m->curtag].layout->arrange(m);

	arrangefloats(m);
	restack(m);

	for (c = stack; c; c = c->snext) {
		if ((clientmon(c) == m) && (!c->isbastard
		    || (c->isbastard && views[m->curtag].barpos == StrutsOn))) {
			unban(c);
		} else if ((clientmon(c) == NULL) ||
		           (c->isbastard && views[m->curtag].barpos == StrutsHide)) {
			ban(c);
		}
	}

}

void arrange(Monitor *m) {
	if (!m) {
		for (m = mons; m; m = m->next)
			arrangemon(m);
	} else
		arrangemon(m);
}

void attach(Client *c, Bool attachaside) {
	if (attachaside) {
		if (clients) {
			Client * lastClient = clients;
			while (lastClient->next)
				lastClient = lastClient->next;
			c->prev = lastClient;
			lastClient->next = c;
		} else
			clients = c;
	} else {
		if (clients)
			clients->prev = c;
		c->next = clients;
		clients = c;
	}
}

void attachstack(Client *c) {
	c->snext = stack;
	stack = c;
}

void ban(Client *c) {
	if (c->isbanned)
		return;

	c->ignoreunmap++;
	setclientstate(c, IconicState);
	XSelectInput(dpy, c->win, CLIENTMASK & ~(StructureNotifyMask | EnterWindowMask));
	XSelectInput(dpy, c->frame, NoEventMask);
	XUnmapWindow(dpy, c->frame);
	XUnmapWindow(dpy, c->win);
	XSelectInput(dpy, c->win, CLIENTMASK);
	XSelectInput(dpy, c->frame, FRAMEMASK);

	c->isbanned = True;
}

void buttonpress(XEvent *e) {
	Client *c;
	int i;
	XButtonPressedEvent *ev = &e->xbutton;

	if (!curmon())
		return;

	if (ev->window == root) {
		if (ev->type != ButtonRelease)
			return;

		switch (ev->button) {
			case Button3:
				spawn(options.rootrightclick);
				break;
			case Button4:
				// TODO: replace with prevtag
				spawn(options.rootscrolldown);
				break;
			case Button5:
				// TODO: replace with nexttag
				spawn(options.rootscrollup);
				break;
		}

		return;
	}

	if ((c = getclient(ev->window, clients, ClientTitle))) {
		DPRINTF("TITLE %s: 0x%x\n", c->name, (int) ev->window);
		focus(c);

		for (i = 0; i < LastBtn; i++) {
			if (button[i].action == NULL)
				continue;

			if ((ev->x > button[i].x)
			    && ((int)ev->x < (int)(button[i].x + style.titleheight))
			    && (button[i].x != -1) && (int)ev->y < style.titleheight) {
				if (ev->type == ButtonPress) {
					DPRINTF("BUTTON %d PRESSED\n", i);
					button[i].pressed = 1;
				} else {
					DPRINTF("BUTTON %d RELEASED\n", i);
					button[i].pressed = 0;
					button[i].action(NULL);
				}

				drawclient(c);
				return;
			}
		}

		for (i = 0; i < LastBtn; i++)
			button[i].pressed = 0;

		drawclient(c);

		if (ev->type == ButtonRelease)
			return;

		restack(curmon());

		if (ev->button == Button1)
			mousemove(c);
		else if (ev->button == Button3)
			mouseresize(c);
	} else if ((c = getclient(ev->window, clients, ClientWindow))) {
		DPRINTF("WINDOW %s: 0x%x\n", c->name, (int) ev->window);
		focus(c);
		restack(curmon());

		if (CLEANMASK(ev->state) != modkey) {
			XAllowEvents(dpy, ReplayPointer, CurrentTime);
			return;
		}

		if (ev->button == Button1) {
			if (!FEATURES(curlayout, OVERLAP) && !c->isfloating)
				togglefloatingwin(NULL);
			if (c->ismax)
				togglemax(NULL);

			mousemove(c);
		} else if (ev->button == Button2) {
			if (!FEATURES(curlayout, OVERLAP) && c->isfloating)
				togglefloatingwin(NULL);
			else
				zoom(NULL);
		} else if (ev->button == Button3) {
			if (!FEATURES(curlayout, OVERLAP) && !c->isfloating)
				togglefloatingwin(NULL);
			if (c->ismax)
				togglemax(NULL);

			mouseresize(c);
		}
	} else if ((c = getclient(ev->window, clients, ClientFrame))) {
		DPRINTF("FRAME %s: 0x%x\n", c->name, (int) ev->window);
		/* Not supposed to happen */
	}
}

void checkotherwm(void) {
	otherwm = False;
	XSetErrorHandler(xerrorstart);

	/* This causes an error if some other window manager is running */
	XSelectInput(dpy, root, SubstructureRedirectMask);
	XSync(dpy, False);
	if (otherwm)
		eprint("pegasus: another window manager is already running\n");
	XSync(dpy, False);
	XSetErrorHandler(NULL);
	xerrorxlib = XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void) {
	while (stack) {
		unban(stack);
		unmanage(stack);
	}

	free(tags);
	free(keys);
	initmons(NULL);
	/* Free resource database */
	XrmDestroyDatabase(xrdb);
	deinitstyle();
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XFreeCursor(dpy, cursor[CurNormal]);
	XFreeCursor(dpy, cursor[CurResize]);
	XFreeCursor(dpy, cursor[CurMove]);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
}

void configure(Client *c) {
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h - c->th;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & ce);
}

void configurenotify(XEvent *e) {
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window == root) {
		initmons(e);

		for (m = mons; m; m = m->next) {
			for (c = clients; c; c = c->next) {
				if (c->isbastard) {
					m = getmon(c->x + c->w/2, c->y);
					c->tags = m->seltags;
					updatestruts(m);
				}
			}
			updategeom(m);
		}

		focus(NULL);
		arrange(NULL);
	}
}

void configurerequest(XEvent *e) {
	Client *c;
	Monitor *cm;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	int x, y, w, h;

	if ((c = getclient(ev->window, clients, ClientWindow))) {
		cm = clientmon(c);

		if (ev->value_mask & CWBorderWidth)
			c->border = ev->border_width;

		if (c->isfixed || c->isfloating || MFEATURES(clientmon(c), OVERLAP)) {
			if (ev->value_mask & CWX)
				x = ev->x;
			if (ev->value_mask & CWY)
				y = ev->y;
			if (ev->value_mask & CWWidth)
				w = ev->width;
			if (ev->value_mask & CWHeight)
				h = ev->height + c->th;

			cm = getmon(x, y);

			if (!(ev->value_mask & (CWX | CWY)) /* Resize request */
			    && (ev->value_mask & (CWWidth | CWHeight))) {
				DPRINTF("RESIZE %s (%d,%d)->(%d,%d)\n", c->name, c->w, c->h, w, h);
				resize(c, c->x, c->y, w, h, True);
				save(c);
			} else if ((ev->value_mask & (CWX | CWY)) /* Move request */
			    && !(ev->value_mask & (CWWidth | CWHeight))) {
				DPRINTF("MOVE %s (%d,%d)->(%d,%d)\n", c->name, c->x, c->y, x, y);
				resize(c, x, y, c->w, c->h, True);
				save(c);
			} else if ((ev->value_mask & (CWX | CWY)) /* Move and resize request */
			    && (ev->value_mask & (CWWidth | CWHeight))) {
				DPRINTF("MOVE&RESIZE(MOVE) %s (%d,%d)->(%d,%d)\n", c->name, c->x, c->y, ev->x, ev->y);
				DPRINTF("MOVE&RESIZE(RESIZE) %s (%d,%d)->(%d,%d)\n", c->name, c->w, c->h, ev->width, ev->height);
				resize(c, x, y, w, h, True);
				save(c);
			} else if ((ev->value_mask & CWStackMode)) {
				DPRINTF("RESTACK %s ignoring\n", c->name);
				configure(c);
			}
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}

	XSync(dpy, False);
}

void destroynotify(XEvent *e) {
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if (!(c = getclient(ev->window, clients, ClientWindow)))
		return;

	unmanage(c);
	updateatom[ClientList] (NULL);
}

void detach(Client *c) {
	if (c->prev)
		c->prev->next = c->next;
	if (c->next)
		c->next->prev = c->prev;
	if (c == clients)
		clients = c->next;

	c->next = c->prev = NULL;
}

void detachstack(Client *c) {
	Client **tc, *t;

	for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;
}

void *emallocz(unsigned int size) {
	void *res = calloc(1, size);

	if (!res)
		eprint("fatal: could not malloc() %u bytes\n", size);

	return res;
}

void enternotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;

	if (ev->mode != NotifyNormal || ev->detail == NotifyInferior)
		return;

	if (!curmon())
		return;

	if ((c = getclient(ev->window, clients, ClientFrame))) {
		if (c->isbastard)
			return;

		/* Focus when switching monitors */
		if (!isvisible(sel, curmon()))
			focus(c);

		switch (options.focus) {
			case Clk2Focus:
				break;
			case SloppyFloat:
				if (FEATURES(curlayout, OVERLAP) || c->isfloating)
					focus(c);
				break;
			case AllSloppy:
				focus(c);
				break;
			case SloppyRaise:
				focus(c);
				restack(curmon());
				break;
			}
	} else if (ev->window == root) {
		selscreen = True;
		focus(NULL);
	}
}

void eprint(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void focusin(XEvent *e) {
	XFocusChangeEvent *ev = &e->xfocus;
	Client *c;

	if (sel && ((c = getclient(ev->window, clients, ClientWindow)) != sel))
		XSetInputFocus(dpy, sel->win, RevertToPointerRoot, CurrentTime);
}

void expose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;
	XEvent tmp;
	Client *c;

	while (XCheckWindowEvent(dpy, ev->window, ExposureMask, &tmp));
	if ((c = getclient(ev->window, clients, ClientTitle)))
		drawclient(c);
}

void givefocus(Client *c) {
	XEvent ce;

	if (checkatom(c->win, atom[WMProto], atom[WMTakeFocus])) {
		ce.xclient.type = ClientMessage;
		ce.xclient.message_type = atom[WMProto];
		ce.xclient.display = dpy;
		ce.xclient.window = c->win;
		ce.xclient.format = 32;
		ce.xclient.data.l[0] = atom[WMTakeFocus];
		ce.xclient.data.l[1] = CurrentTime; /* Incorrect */
		ce.xclient.data.l[2] = 0l;
		ce.xclient.data.l[3] = 0l;
		ce.xclient.data.l[4] = 0l;
		XSendEvent(dpy, c->win, False, NoEventMask, &ce);
	}
}

void focus(Client *c) {
	Client *o;

	o = sel;

	if ((!c && selscreen) || (c && (c->isbastard || !isvisible(c, curmon()))))
		for (c = stack;
			c && (c->isbastard || !isvisible(c, curmon())); c = c->snext);
	if (sel && sel != c)
		XSetWindowBorder(dpy, sel->frame, style.color.norm[ColBorder]);

	if (c) {
		detachstack(c);
		attachstack(c);
		//unban(c);
	}

	sel = c;

	if (!selscreen)
		return;

	if (c) {
		setclientstate(c, NormalState);
		if (c->isfocusable) {
			XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
			givefocus(c);
		}
		XSetWindowBorder(dpy, sel->frame, style.color.sel[ColBorder]);
		drawclient(c);
	} else
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);

	if (o)
		drawclient(o);

	updateatom[ActiveWindow] (sel);
	updateatom[ClientList] (NULL);
	updateatom[CurDesk] (NULL);
}

void iconify(const char *arg) {
	Client *c;

	if (!sel)
		return;

	c = sel;

	// TODO: Use th here
	resize(c, c->x, c->y, c->w, c->th + 1, False);
}

void incnmaster(const char *arg) {
	unsigned int i;

	if (!FEATURES(curlayout, NMASTER))
		return;

	if (!arg)
		views[curmontag].nmaster = DEFNMASTER;
	else {
		i = atoi(arg);
		if ((views[curmontag].nmaster + i) < 1
		    || curwah / (views[curmontag].nmaster + i) <= 2 * style.border)
			return;
		views[curmontag].nmaster += i;
	}

	if (sel)
		arrange(curmon());
}

Client *getclient(Window w, Client *list, int part) {
	Client *c;

	#define ClientPart(_c, _part) (((_part) == ClientWindow) ? (_c)->win : \
                       ((_part) == ClientTitle) ? (_c)->title : \
                       ((_part) == ClientFrame) ? (_c)->frame : 0)

	for (c = list; c && (ClientPart(c, part)) != w; c = c->next);

	return c;
}

long getstate(Window w) {
	long ret = -1;
	long *p = NULL;
	unsigned long n;

	p = (long*)getatom(w, atom[WMState], &n);

	if (n != 0)
		ret = *p;

	XFree(p);
	return ret;
}

const char *getresource(const char *resource, const char *defval) {
	static char name[256], class[256], *type;
	XrmValue value;

	snprintf(name, sizeof(name), "%s.%s", RESNAME, resource);
	snprintf(class, sizeof(class), "%s.%s", RESCLASS, resource);
	XrmGetResource(xrdb, name, class, &type, &value);

	if (value.addr)
		return value.addr;

	return defval;
}

Bool gettextprop(Window w, Atom atom, char *text, unsigned int size) {
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return False;

	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);

	if (!name.nitems)
		return False;

	if (name.encoding == XA_STRING)
		strncpy(text, (char *) name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success
		    && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}

	text[size - 1] = '\0';
	XFree(name.value);
	return True;
}

int idxoftag(const char *tag) {
	unsigned int i;

	for (i = 0; (i < ntags) && strcmp(tag, tags[i]); i++);
	return (i < ntags) ? i : 0;
}

Bool isvisible(Client *c, Monitor *m) {
	unsigned int i;

	if (!c)
		return False;

	if (!m) {
		for (m = mons; m; m = m->next) {
			for (i = 0; i < ntags; i++)
				if (c->tags[i] && m->seltags[i])
					return True;
		}
	} else {
		for (i = 0; i < ntags; i++)
			if (c->tags[i] && m->seltags[i])
				return True;
	}

	return False;
}

void grabkeys(void) {
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j;
	KeyCode code;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);

	for (i = 0; i < nkeys; i++) {
		if ((code = XKeysymToKeycode(dpy, keys[i]->keysym))) {
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabKey(dpy, code, keys[i]->mod | modifiers[j], root,
				         True, GrabModeAsync, GrabModeAsync);
		}
    }
}

void keypress(XEvent *e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	if (!curmon())
		return;

	ev = &e->xkey;
	keysym = XkbKeycodeToKeysym(dpy, (KeyCode) ev->keycode, 0, 0);

	for (i = 0; i < nkeys; i++)
		if (keysym == keys[i]->keysym
            && CLEANMASK(keys[i]->mod) == CLEANMASK(ev->state)) {
			if (keys[i]->func)
				keys[i]->func(keys[i]->arg);
			XUngrabKeyboard(dpy, CurrentTime);
		}
}

void killclient(const char *arg) {
	XEvent ev;

	if (!sel)
		return;

	if (checkatom(sel->win, atom[WMProto], atom[WMDelete])) {
		ev.type = ClientMessage;
		ev.xclient.window = sel->win;
		ev.xclient.message_type = atom[WMProto];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = atom[WMDelete];
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, sel->win, False, NoEventMask, &ev);
	} else
		XKillClient(dpy, sel->win);
}

void leavenotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->window == root) && !ev->same_screen) {
		selscreen = False;
		focus(NULL);
	}
}

void manage(Window w, XWindowAttributes *wa) {
	Client *c, *t = NULL;
	Monitor *cm = NULL;
	Window trans;
	XWindowChanges wc;
	XSetWindowAttributes twa;
	XWMHints *wmh;
	unsigned long mask = 0;

	c = emallocz(sizeof(Client));
	c->win = w;

	if (checkatom(c->win, atom[WindowType], atom[WindowTypeDesk]) ||
	    checkatom(c->win, atom[WindowType], atom[WindowTypeDock])) {
		c->isbastard = True;
		c->isfloating = True;
		c->isfixed = True;
	}
	if (checkatom(c->win, atom[WindowType], atom[WindowTypeDialog])) {
		c->isfloating = True;
		c->isfixed = True;
	}

	cm = curmon();
	c->title = c->isbastard ? (Window) NULL : 1;
	c->tags = emallocz(ntags * sizeof(cm->seltags[0]));
	c->isfocusable = c->isbastard ? False : True;
	c->border = c->isbastard ? 0 : style.border;
	c->oldborder = c->isbastard ? 0 : wa->border_width; /* XXX: why? */
	/*  XReparentWindow() unmaps *mapped* windows */
	c->ignoreunmap = wa->map_state == IsViewable ? 1 : 0;
	mwm_process_atom(c);
	updatesizehints(c);

	updatetitle(c);
	applyrules(c);
	applyatoms(c);

	if (XGetTransientForHint(dpy, w, &trans)) {
		if (t = getclient(trans, clients, ClientWindow)) {
			memcpy(c->tags, t->tags, ntags * sizeof(cm->seltags[0]));
			c->isfloating = True;
		}
	}

	c->th = c->title ? style.titleheight : 0;

	if (!c->isfloating)
		c->isfloating = c->isfixed;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		c->isfocusable = !(wmh->flags & InputHint) || wmh->input;
		XFree(wmh);
	}

	c->x = c->rx = wa->x;
	c->y = c->ry = wa->y;
	c->w = c->rw = wa->width;
	c->h = c->rh = wa->height + c->th;

	if (!wa->x && !wa->y && !c->isbastard)
		place(c);

	cm = c->isbastard ? getmon(wa->x, wa->y) : clientmon(c);
	if (!cm) {
		DPRINTF("Cannot find monitor for window 0x%x,"
				"requested coordinates %d,%d\n", w, wa->x, wa->y);
		cm = curmon();
	}
	c->hasstruts = getstruts(c); 
	if (c->isbastard) {
		free(c->tags);
		c->tags = cm->seltags;
	}

	XGrabButton(dpy, AnyButton, AnyModifier, c->win, True,
	            ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
	twa.override_redirect = True;
	twa.event_mask = FRAMEMASK;
	mask = CWOverrideRedirect | CWEventMask;
	if (wa->depth == 32) {
		mask |= CWColormap | CWBorderPixel | CWBackPixel;
		twa.colormap = XCreateColormap(dpy, root, wa->visual, AllocNone);
		twa.background_pixel = BlackPixel(dpy, screen);
		twa.border_pixel = BlackPixel(dpy, screen);
	}
	c->frame = XCreateWindow(dpy, root, c->x, c->y, c->w,
	                         c->h, c->border, wa->depth == 32 ? 32 : DefaultDepth(dpy, screen),
	                         InputOutput, wa->depth == 32 ? wa->visual : DefaultVisual(dpy,
	                         screen), mask, &twa);

	wc.border_width = c->border;
	XConfigureWindow(dpy, c->frame, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, c->frame, style.color.norm[ColBorder]);

	twa.event_mask = ExposureMask | MOUSEMASK;
	/* We create title as root's child as a workaround for 32bit visuals */
	if (c->title) {
		c->title = XCreateWindow(dpy, root, 0, 0, c->w, c->th,
		           0, DefaultDepth(dpy, screen), CopyFromParent,
		           DefaultVisual(dpy, screen), CWEventMask, &twa);
		c->drawable =
		    XCreatePixmap(dpy, root, c->w, c->th, DefaultDepth(dpy, screen));
	} else {
		c->title = (Window) NULL;
	}

	attach(c, options.attachaside);
	attachstack(c);

	twa.event_mask = CLIENTMASK;
	twa.do_not_propagate_mask = CLIENTNOPROPAGATEMASK;
	XChangeWindowAttributes(dpy, c->win, CWEventMask|CWDontPropagate, &twa);
	XSelectInput(dpy, c->win, CLIENTMASK);

	XReparentWindow(dpy, c->win, c->frame, 0, c->th);
	XReparentWindow(dpy, c->title, c->frame, 0, 0);
	XAddToSaveSet(dpy, c->win);
	XMapWindow(dpy, c->win);
	wc.border_width = 0;
	XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
	configure(c); /* Propagates border_width, if size doesn't change */

	ban(c);

	updateatom[ClientList] (NULL);
	updateatom[WindowDesk] (c);
	updateframe(c);

	if (!cm)
		return;

	if (c->hasstruts)
		updategeom(cm);

	arrange(cm);

	if (checkatom(c->win, atom[WindowState], atom[WindowStateFs]))
		ewmh_process_state_atom(c, atom[WindowStateFs], 1);

	if (!checkatom(c->win, atom[WindowType], atom[WindowTypeDesk]))
		focus(NULL);
}

void mappingnotify(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void maprequest(XEvent *e) {
	static XWindowAttributes wa;
	Client *c;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;

	if (wa.override_redirect)
		return;

	if (!(c = getclient(ev->window, clients, ClientWindow)))
		manage(ev->window, &wa);
}

void monocle(Monitor *m) {
	Client *c;

	for (c = nexttiled(clients, m); c; c = nexttiled(c->next, m)) {
			if (views[m->curtag].barpos != StrutsOn)
				resize(c, m->wax - c->border,
				       m->way - c->border, m->waw, m->wah, False);
			else
				resize(c, m->wax, m->way, m->waw - 2 * c->border,
				       m->wah - 2 * c->border, False);
	}
}

void moveresizekb(const char *arg) {
	int dw, dh, dx, dy;

	dw = dh = dx = dy = 0;

	if (!sel)
		return;

	if (!sel->isfloating)
		return;

	sscanf(arg, "%d %d %d %d", &dx, &dy, &dw, &dh);
	if (dw && (dw < sel->incw))
		dw = (dw / abs(dw)) * sel->incw;
	if (dh && (dh < sel->inch))
		dh = (dh / abs(dh)) * sel->inch;

	resize(sel, sel->x + dx, sel->y + dy, sel->w + dw,
	       sel->h + dh, True);
}

void getpointer(int *x, int *y) {
	int di;
	unsigned int dui;
	Window dummy;

	XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

Monitor *getmon(int x, int y) {
	Monitor *m;

	for (m = mons; m; m = m->next) {
		if ((x >= m->sx && x <= m->sx + m->sw) &&
		    (y >= m->sy && y <= m->sy + m->sh))
			return m;
	}

	return NULL;
}

Monitor * clientmon(Client *c) {
	Monitor *m;

	assert(c != NULL);

	for (m = mons; m; m = m->next)
		if (isvisible(c, m))
			return m;

	return NULL;
}

Monitor *curmon() {
	int x, y;

	getpointer(&x, &y);

	return getmon(x, y);
}

void mousemove(Client *c) {
	int x1, y1, ocx, ocy, nx, ny;
	unsigned int i;
	XEvent ev;
	Monitor *m, *nm;

	if (c->isbastard)
		return;

	m = curmon();
	ocx = c->x;
	ocy = c->y;

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync,
	    GrabModeAsync, None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;

	getpointer(&x1, &y1);

	for (;;) {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);

		switch (ev.type) {
			case ButtonRelease:
				XUngrabPointer(dpy, CurrentTime);
				XSync(dpy, False);
				return;
			case ConfigureRequest:
			case Expose:
			case MapRequest:
				handler[ev.type] (&ev);
				break;
			case MotionNotify:
				/* We are probably moving to a different monitor */
				if (!(nm = curmon()))
					break;
				nx = ocx + (ev.xmotion.x_root - x1);
				ny = ocy + (ev.xmotion.y_root - y1);
				if (abs(nx - nm->wax) < options.snap)
					nx = nm->wax;
				else if (abs((nm->wax + nm->waw) - (nx + c->w +
				         2 * c->border)) < options.snap)
					nx = nm->wax + nm->waw - c->w - 2 * c->border;
				if (abs(ny - nm->way) < options.snap)
					ny = nm->way;
				else if (abs((nm->way + nm->wah) - (ny + c->h +
				         2 * c->border)) < options.snap)
					ny = nm->way + nm->wah - c->h - 2 * c->border;
				resize(c, nx, ny, c->w, c->h, True);
				save(c);
				if (m != nm) {
					for (i = 0; i < ntags; i++)
						c->tags[i] = nm->seltags[i];
					updateatom[WindowDesk] (c);
					drawclient(c);
					arrange(NULL);
					m = nm;
				}
				break;
		}
	}
}

void mouseresize(Client *c) {
	int ocx, ocy, nw, nh;
	Monitor *cm;
	XEvent ev;

	if (c->isbastard || c->isfixed)
		return;

	cm = curmon();

	ocx = c->x;
	ocy = c->y;

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync,
	    GrabModeAsync, None, cursor[CurResize], CurrentTime) != GrabSuccess)
		return;

	c->ismax = False;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->border - 1,
	             c->h + c->border - 1);

	for (;;) {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
			case ButtonRelease:
				XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
				             c->w + c->border - 1, c->h + c->border - 1);
				XUngrabPointer(dpy, CurrentTime);
				XSync(dpy, False);
				while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
				return;
			case ConfigureRequest:
			case Expose:
			case MapRequest:
				handler[ev.type] (&ev);
				break;
			case MotionNotify:
				if ((nw = ev.xmotion.x - ocx - 2 * c->border + 1) <= 0)
					nw = MINWIDTH;
				if ((nh = ev.xmotion.y - ocy - 2 * c->border + 1) <= 0)
					nh = MINHEIGHT;
				resize(c, c->x, c->y, nw, nh, True);
				save(c);
				break;
		}
	}
}

Client *nexttiled(Client *c, Monitor *m) {
	for (; c && (c->isfloating || !isvisible(c, m) || c->isbastard); c = c->next);
		return c;
}

Client * prevtiled(Client *c, Monitor *m) {
	for (; c && (c->isfloating || !isvisible(c, m) || c->isbastard); c = c->prev);
		return c;
}

void reparentnotify(XEvent *e) {
	Client *c;
	XReparentEvent *ev = &e->xreparent;

	if ((c = getclient(ev->window, clients, ClientWindow)))
		if (ev->parent != c->frame)
			unmanage(c);
}

void place(Client *c) {
	int x, y;
	Monitor *m;
	int d = style.titleheight;

	/* XXX: Do something better */
	getpointer(&x, &y);
	DPRINTF("%d %d\n", x, y);
	m = getmon(x, y);
	x = x + rand()%d - c->w/2;
	y = y + rand()%d - c->h/2;
	if (x < m->wax)
		x = m->wax;
	DPRINTF("%d %d\n", x, y);
	if (y < m->way)
		y = m->way;
	DPRINTF("%d+%d > %d+%d\n", x, c->w, m->wax, m->waw);
	if (x + c->w > m->wax + m->waw)
		x = m->wax + m->waw - c->w - rand()%d;
	DPRINTF("%d %d\n", x, y);
	if (y + c->h > m->way + m->wah)
		y = m->way + m->wah - c->h - rand()%d;
	DPRINTF("%d %d\n", x, y);

	c->rx = c->x = x;
	c->ry = c->y = y;
}

void propertynotify(XEvent *e) {
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = getclient(ev->window, clients, ClientWindow))) {
		if (ev->atom == atom[StrutPartial]) {
			c->hasstruts = getstruts(c);
			updategeom(clientmon(c));
			arrange(clientmon(c));
		}
		if (ev->state == PropertyDelete) 
			return;
		if (ev->atom == atom[WindowName]) {
			updatetitle(c);
			drawclient(c);
		}

		switch (ev->atom) {
			case XA_WM_TRANSIENT_FOR:
				XGetTransientForHint(dpy, c->win, &trans);
				if (!c->isfloating && (c->isfloating =
				   (getclient(trans, clients, ClientWindow) != NULL)))
					arrange(clientmon(c));
				break;
			case XA_WM_NORMAL_HINTS:
				updatesizehints(c);
				break;
			case XA_WM_NAME:
				updatetitle(c);
				drawclient(c);
				break;
		}
	}
}

void quit(const char *arg) {
	running = False;

	if (arg) {
		cleanup();
		execvp(cargv[0], cargv);
		eprint("Can't exec: %s\n", strerror(errno));
	}
}

void resize(Client *c, int x, int y, int w, int h, Bool sizehints) {
	XWindowChanges wc;

	if (sizehints) {
		h -= c->th;
		/* Set minimum possible */
		if (w < 1)
			w = 1;
		if (h < 1)
			h = 1;

		/* Temporarily remove base dimensions */
		w -= c->basew;
		h -= c->baseh;

		/* Adjust for aspect limits */
		if (c->minay > 0 && c->maxay > 0 && c->minax > 0 && c->maxax > 0) {
			if (w * c->maxay > h * c->maxax)
				w = h * c->maxax / c->maxay;
			else if (w * c->minay < h * c->minax)
				h = w * c->minay / c->minax;
		}

		/* Adjust for increment value */
		if (c->incw)
			w -= w % c->incw;
		if (c->inch)
			h -= h % c->inch;

		/* Restore base dimensions */
		w += c->basew;
		h += c->baseh;

		if (c->minw > 0 && w < c->minw)
			w = c->minw;
		if (c->minh > 0 && h - c->th < c->minh)
			h = c->minh + c->th;
		if (c->maxw > 0 && w > c->maxw)
			w = c->maxw;
		if (c->maxh > 0 && h - c->th > c->maxh)
			h = c->maxh + c->th;
		h += c->th;
	}
	if (w <= 0 || h <= 0)
		return;

	/* Offscreen appearance fixes */
	if (x > DisplayWidth(dpy, screen))
		x = DisplayWidth(dpy, screen) - w - 2 * c->border;
	if (y > DisplayHeight(dpy, screen))
		y = DisplayHeight(dpy, screen) - h - 2 * c->border;
	if (w != c->w && c->th) {
		XMoveResizeWindow(dpy, c->title, 0, 0, w, c->th);
		XFreePixmap(dpy, c->drawable);
		c->drawable =
			XCreatePixmap(dpy, root, w, c->th, DefaultDepth(dpy, screen));
		drawclient(c);
	}

	if (c->x != x || c->y != y || c->w != w || c->h != h /* || sizehints */) {
		c->x = x;
		c->y = y;
		c->w = w;
		c->h = h;
		DPRINTF("x = %d y = %d w = %d h = %d\n", c->x, c->y, c->w, c->h);
		XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->w, c->h);
		XMoveResizeWindow(dpy, c->win, 0, c->th, c->w, c->h - c->th);
		configure(c);
	}
}

void restack(Monitor *m) {
	Client *c;
	XEvent ev;
	Window *wl;
	int i, n;

	if (!sel)
		return;

	for (n = 0, c = stack; c; c = c->snext) {
		if (isvisible(c, m)) {
			n++;
		}
	}

	if (!n)
		return;

	wl = malloc(sizeof(Window) * n);
	i = 0;

	for (c = stack; c && i < n; c = c->snext)
		if (isvisible(c, m))
			if (!c->isbastard && c->isfloating)
				wl[i++] = c->frame;
	for (c = stack; c && i < n; c = c->snext)
		if (isvisible(c, m) && c->isbastard &&
		    !checkatom(c->win, atom[WindowType], atom[WindowTypeDesk]))
			wl[i++] = c->frame;
	for (c = stack; c && i < n; c = c->snext) 
		if (isvisible(c, m))
			if (!c->isfloating && !c->isbastard)
				wl[i++] = c->frame;
	for (c = stack; c && i < n; c = c->snext)
		if (isvisible(c, m) && c->isbastard && 
			checkatom(c->win, atom[WindowType], atom[WindowTypeDesk]))
				wl[i++] = c->frame;

	assert(i == n);
	XRestackWindows(dpy, wl, n);
	free(wl);
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void run(void) {
	fd_set rd;
	int xfd;
	XEvent ev;

	/* Main event loop */
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);

	while (running) {
		FD_ZERO(&rd);
		FD_SET(xfd, &rd);
		if (select(xfd + 1, &rd, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			eprint("select failed\n");
		}

		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type])
				(handler[ev.type]) (&ev); /* Call handler */
		}
	}
}

void save(Client *c) {
	c->rx = c->x;
	c->ry = c->y;
	c->rw = c->w;
	c->rh = c->h;
}

void scan(void) {
	unsigned int i, num;
	Window *wins, d1, d2;
	XWindowAttributes wa;

	wins = NULL;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa) ||
			    wa.override_redirect
			    || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable
			    || getstate(wins[i]) == IconicState
			    || getstate(wins[i]) == NormalState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* Now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			    && (wa.map_state == IsViewable
			    || getstate(wins[i]) == IconicState
			    || getstate(wins[i]) == NormalState))
				manage(wins[i], &wa);
		}
	}

	if (wins)
		XFree(wins);
}

void setclientstate(Client *c, long state) {
	long data[] = { state, None };
	long winstate[2];

	XChangeProperty(dpy, c->win, atom[WMState], atom[WMState], 32,
	    PropModeReplace, (unsigned char *) data, 2);
	if (state == NormalState) {
		XDeleteProperty(dpy, c->win, atom[WindowState]);
	} else {
		winstate[0] = atom[WindowStateHidden];
		XChangeProperty(dpy, c->win, atom[WindowState], XA_ATOM, 32,
		    PropModeReplace, (unsigned char *) winstate, 1);
	}
}

void setlayout(const char *arg) {
	unsigned int i;
	Client *c;
	Bool wasfloat;

	wasfloat = FEATURES(curlayout, OVERLAP);

	if (arg) {
		for (i = 0; i < LENGTH(layouts); i++)
			if (*arg == layouts[i].symbol)
				break;
		if (i == LENGTH(layouts))
			return;
		views[curmontag].layout = &layouts[i];
	}

	if (sel) {
		for (c = clients; c; c = c->next) {
			if (isvisible(c, curmon())) {
				if (wasfloat)
					save(c);
				if (wasfloat != FEATURES(curlayout, OVERLAP))
					updateframe(c);
			}
		}
		arrange(curmon());
	}

	updateatom[ELayout] (NULL);
}

void setmax(Client *c) {
	XEvent ev;
	Monitor *m = curmon();

	c->ismax = !c->ismax;
	updateframe(c);

	save(c);
	resize(c, m->sx - c->border, m->sy - c->border - c->th, m->sw, m->sh + c->th, False);

	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void setmwfact(const char *arg) {
	double delta;

	if (!FEATURES(curlayout, MWFACT))
		return;

	/* Arg handling, manipulate mwfact */
	if (arg == NULL)
		views[curmontag].mwfact = DEFMWFACT;

	else if (sscanf(arg, "%lf", &delta) == 1) {
		if (arg[0] == '+' || arg[0] == '-')
			views[curmontag].mwfact += delta;
		else
			views[curmontag].mwfact = delta;
		if (views[curmontag].mwfact < 0.1)
			views[curmontag].mwfact = 0.1;
		else if (views[curmontag].mwfact > 0.9)
			views[curmontag].mwfact = 0.9;
	}

	arrange(curmon());
}

void setunmax(Client *c) {
	XEvent ev;

	c->ismax = !c->ismax;
	updateframe(c);

	resize(c, c->rx, c->ry, c->rw, c->rh, True);

	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void initlayouts() {
	unsigned int i, j;
	char conf[32], ltname;
	float mwfact;
	int nmaster;
	const char *deflayout;

	/* Init layouts */
	mwfact = atof(getresource("mwfact", STR(DEFMWFACT)));
	nmaster = atoi(getresource("nmaster", STR(DEFNMASTER)));
	deflayout = getresource("deflayout", "i");

	if (!nmaster)
		nmaster = 1;

	for (i = 0; i < ntags; i++) {
		views[i].layout = &layouts[0];
		snprintf(conf, sizeof(conf), "tags.layout%d", i);
		strncpy(&ltname, getresource(conf, deflayout), 1);

		for (j = 0; j < LENGTH(layouts); j++) {
			if (layouts[j].symbol == ltname) {
				views[i].layout = &layouts[j];
				break;
			}
		}

		views[i].mwfact = mwfact;
		views[i].nmaster = nmaster;
		views[i].barpos = StrutsOn;
	}

	updateatom[ELayout] (NULL);
}

void initmons(XEvent *e) {
	Monitor *m;
	m = emallocz(sizeof(Monitor));
	m->sx = m->wax = 0;
	m->sy = m->way = 0;
	m->sw = m->waw = DisplayWidth(dpy, screen);
	m->sh = m->wah = DisplayHeight(dpy, screen);
	m->mx = m->sx + m->sw/2;
	m->my = m->sy + m->sh/2;
	m->curtag = 0;
	m->prevtags = emallocz(ntags * sizeof(Bool));
	m->seltags = emallocz(ntags * sizeof(Bool));
	m->seltags[0] = True;
	m->next = NULL;
	mons = m;
	updateatom[WorkArea] (NULL);
}

void inittags() {
	unsigned int i;
	char tmp[25] = "\0";

	ntags = atoi(getresource("tags.number", "5"));
	views = emallocz(ntags * sizeof(View));
	tags = emallocz(ntags * sizeof(char *));
	for (i = 0; i < ntags; i++) {
		tags[i] = emallocz(25);
		snprintf(tmp, sizeof(tmp), "tags.name%d", i);
		snprintf(tags[i], sizeof(tags[i]), "%s", getresource(tmp,
		    "null"));
	}
}

void sighandler(int signum) {
	if (signum == SIGHUP)
		quit("HUP!");
	else
		quit(NULL);
}

void setup(char *conf) {
	int d;
	int i, j;
	unsigned int mask;
	Window w;
	Monitor *m;
	XModifierKeymap *modmap;
	XSetWindowAttributes wa;
	char oldcwd[256], path[256] = "/";
	char *home, *slash;

	const char *confs[] = {
		conf,
		"%s/.pegasus/config",
		NULL
	};

	/* Init cursors */
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_bottom_right_corner);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);

	/* Init modifier map */
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < modmap->max_keypermod; j++) {
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			    == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
		}
	}
	XFreeModifiermap(modmap);

	/* Select for events */
	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
	                | EnterWindowMask | LeaveWindowMask | StructureNotifyMask
	                | ButtonPressMask | ButtonReleaseMask;
	wa.cursor = cursor[CurNormal];
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);

	/* Init resource database */
	XrmInitialize();

	home = getenv("HOME");
	if (!home)
		*home = '/';
	if (!getcwd(oldcwd, sizeof(oldcwd)))
		eprint("pegasus: getcwd error: %s\n", strerror(errno));

	for (i = 0; confs[i] != NULL; i++) {
		if (*confs[i] == '\0')
			continue;
		snprintf(conf, 255, confs[i], home);
		/* Retrieve path to chdir(2) to it */
		slash = strrchr(conf, '/');
		if (slash)
			snprintf(path, slash - conf + 1, "%s", conf);
		chdir(path);
		xrdb = XrmGetFileDatabase(conf);
		/* Configuration file loaded successfully; break out */
		if (xrdb)
			break;
	}
	if (!xrdb)
		fprintf(stderr, "pegasus: no configuration file found, using defaults\n");

	initewmh();
	inittags();
	initmons(NULL);

	/* Init modkey */
	initrules();
	initkeys();
	initlayouts();
	updateatom[NumberOfDesk] (NULL);
	updateatom[DeskNames] (NULL);
	updateatom[CurDesk] (NULL);

	grabkeys();

	/* Init appearance */
	initstyle();
	options.attachaside = atoi(getresource("attachaside", "1"));
	strncpy(options.rootrightclick, getresource("rootrightclick", ROOTRIGHTCLICK), LENGTH(options.rootrightclick));
	options.rootrightclick[LENGTH(options.rootrightclick) - 1] = '\0';
	options.dectiled = atoi(getresource("decoratetiled", STR(DECORATETILED)));
	options.deftilinglayout = getresource("deftilinglayout", "s");
	options.hidebastards = atoi(getresource("hidebastards", "0"));
	options.focus = atoi(getresource("sloppy", "0"));
	options.gap = atoi(getresource("gap", STR(DEFGAP)));
	options.margin = atoi(getresource("margin", STR(DEFMARGIN)));
	strncpy(options.rootscrolldown, getresource("rootscrolldown", ROOTSCROLLDOWN), LENGTH(options.rootscrolldown));
	options.rootscrolldown[LENGTH(options.rootscrolldown) - 1] = '\0';
	strncpy(options.rootscrollup, getresource("rootscrollup", ROOTSCROLLUP), LENGTH(options.rootscrollup));
	options.rootscrollup[LENGTH(options.rootscrollup) - 1] = '\0';
	options.snap = atoi(getresource("snap", STR(SNAP)));

	for (m = mons; m; m = m->next) {
		m->struts[RightStrut] = m->struts[LeftStrut] =
		   m->struts[TopStrut] = m->struts[BotStrut] = 0;
		updategeom(m);
	}

	chdir(oldcwd);
}

void spawn(const char *arg) {
	static char shell[] = "/bin/sh";

	if (!arg)
		return;

	if (fork() == 0) {
		if (fork() == 0) {
			if (dpy)
				close(ConnectionNumber(dpy));
			setsid();
			execl(shell, shell, "-c", arg, (char *) NULL);
			fprintf(stderr, "pegasus: execl '%s -c %s'", shell, arg);
			perror(" failed");
		}
		exit(0);
	}
	wait(0);
}

void tag(const char *arg) {
	unsigned int i;

	if (!sel)
		return;

	for (i = 0; i < ntags; i++)
		 sel->tags[i] = (NULL == arg);
	sel->tags[idxoftag(arg)] = True;
	updateatom[WindowDesk] (sel);
	updateframe(sel);
	arrange(NULL);
	focus(NULL);
}

void bstack(Monitor *m) {
	int i, n, nx, ny, nw, nh, mh, tw;
	Client *c, *mc;

	for (n = 0, c = nexttiled(clients, m); c; c = nexttiled(c->next, m))
		n++;

	mh = (n == 1) ? m->wah : views[m->curtag].mwfact * m->wah;
	tw = (n > 1) ? m->waw / (n - 1) : 0;

	nx = m->wax;
	ny = m->way;
	nh = 0;

	for (i = 0, c = mc = nexttiled(clients, m); c; c = nexttiled(c->next, m), i++) {
		c->ismax = False;
		if (i == 0) {
			nh = mh - 2 * c->border;
			nw = m->waw - 2 * c->border;
			nx = m->wax;
		} else {
			if (i == 1) {
				nx = m->wax;
				ny += mc->h + c->border;
				nh = (m->way + m->wah) - ny - 2 * c->border;
			}
			if (i + 1 == n)
				nw = (m->wax + m->waw) - nx - 2 * c->border;
			else
				nw = tw - c->border;
		}
		resize(c, nx, ny, nw, nh, False);
		if (n > 1 && tw != curwaw)
			nx = c->x + c->w + c->border;
	}
}

void fibonacci(Monitor *m, int s) {
	int nx, ny, nw, nh;
	unsigned int i, n;
	Client *c, *mc;

	for (n = 0, c = nexttiled(clients, m); c; c = nexttiled(c->next, m), n++);
	if (n == 0)
		return;

	nx = m->wax - options.gap + options.margin;
	ny = 0;
	nw = m->waw + options.gap - options.margin * 2;
	nh = m->wah + options.gap - options.margin * 2;
	
	for (i = 0, c = mc = nexttiled(clients, m); c; c = nexttiled(c->next, m)) {
		c->ismax = False;
		if ((i % 2 && nh / 2 > 2 * c->border)
		   || (!(i % 2) && nw / 2 > 2 * c->border)) {
			if (i < n - 1) {
				if (i % 2)
					nh /= 2;
				else
					nw /= 2;
				if ((i % 4) == 2 && !s)
					nx += nw;
				else if ((i % 4) == 3 && !s)
					ny += nh;
			}
			if ((i % 4) == 0) {
				if (s)
					ny += nh;
				else
					ny -= nh;
			}
			else if ((i % 4) == 1)
				nx += nw;
			else if ((i % 4) == 2)
				ny += nh;
			else if ((i % 4) == 3) {
				if (s)
					nx += nw;
				else
					nx -= nw;
			}
			if (i == 0)
			{
				if(n != 1)
					nw = m->waw * views[m->curtag].mwfact;
				ny = m->way - options.gap + options.margin;
			}
			else if (i == 1)
				nw = m->waw - nw + options.gap - options.margin * 2;
			i++;
		}
		resize(c, nx + options.gap, ny + options.gap, nw - 2 * c->border - options.gap, nh - 2 * c->border - options.gap, False);
	}
}

void dwindle(Monitor * m) {
	fibonacci(m, 1);
}

void spiral(Monitor * m) {
	fibonacci(m, 0);
}

void togglestruts(const char *arg) {
	views[curmontag].barpos =
		(views[curmontag].barpos ==
		StrutsOn) ? (options.hidebastards ? StrutsHide : StrutsOff) : StrutsOn;
	updategeom(curmon());
	arrange(curmon());
}

void togglefloatingwin(const char *arg) {
	if (!sel)
		return;

	sel->isfloating = !sel->isfloating;
	updateframe(sel);

	if (sel->isfloating) {
		resize(sel, sel->rx, sel->ry, sel->rw, sel->rh, False);
	} else
		save(sel);

	arrange(curmon());
}

void togglefloatingtag(const char *arg) {
	const char *torf;

	if (FEATURES(curlayout, OVERLAP)) {
		torf = options.deftilinglayout;
	} else
		torf = "i";

	setlayout(torf);
}

void togglefill(const char *arg) {
	XEvent ev;
	Monitor *m = curmon();
	Client *c;
	int x1, x2, y1, y2, w, h;

	x1 = m->wax;
	x2 = m->sw;
	y1 = m->way;
	y2 = m->sh;

	if (!sel || sel->isfixed || !sel->isfloating || MFEATURES(m, OVERLAP))
		return;

	for (c = clients; c; c = c->next) {
		if (isvisible(c, m) && (c != sel) && !c->isbastard
					&& (c->isfloating || MFEATURES(m, OVERLAP))) {
			if (c->y + c->h > sel->y && c->y < sel->y + sel->h) {
				if (c->x < sel->x)
					x1 = max(x1, c->x + c->w + style.border);
				else
					x2 = min(x2, c->x - style.border);
			}
			if (c->x + c->w > sel->x && c->x < sel->x + sel->w) {
				if (c->y < sel->y)
					y1 = max(y1, c->y + c->h + style.border);
				else
					y2 = max(y2, c->y - style.border);
			}
		}
		DPRINTF("x1 = %d x2 = %d y1 = %d y2 = %d\n", x1, x2, y1, y2);
	}

	w = x2 - x1;
	h = y2 - y1;
	DPRINTF("x1 = %d w = %d y1 = %d h = %d\n", x1, w, y1, h);

	if ((w < sel->w) || (h < sel->h))
		return;

	if ((sel->isfill = !sel->isfill)) {
		save(sel);
		resize(sel, x1, y1, w, h, True);
	} else
		resize(sel, sel->rx, sel->ry, sel->rw, sel->rh, True);

	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void togglemax(const char *arg) {
	if (sel->ismax) {
		setmax(sel);
	} else {
		setunmax(sel);
	}
}

void toggletag(const char *arg) {
	unsigned int i, j;

	if (!sel)
		return;

	i = idxoftag(arg);
	sel->tags[i] = !sel->tags[i];

	for (j = 0; j < ntags && !sel->tags[j]; j++);
	if (j == ntags)
		sel->tags[i] = True; /* At least one tag must be enabled */

	drawclient(sel);
	arrange(NULL);
}

void togglemon(const char *arg) {
	Monitor *m, *cm;
	int x, y;

	getpointer(&x, &y);

	if (!(cm = getmon(x, y)))
		return;

	cm->mx = x;
	cm->my = y;
	for (m = mons; m == cm && m && m->next; m = m->next);

	if (!m)
		return;

	XWarpPointer(dpy, None, root, 0, 0, 0, 0, m->mx, m->my);
	focus(NULL);
}

void toggleview(const char *arg) {
	unsigned int i, j;
	Monitor *m, *cm;

	i = idxoftag(arg);
	cm = curmon();

	memcpy(cm->prevtags, cm->seltags, ntags * sizeof(cm->seltags[0]));
	cm->seltags[i] = !cm->seltags[i];

	for (m = mons; m; m = m->next) {
		if (m->seltags[i] && m != cm) {
			memcpy(m->prevtags, m->seltags, ntags * sizeof(m->seltags[0]));
			m->seltags[i] = False;
			for (j = 0; j < ntags && !m->seltags[j]; j++);

			if (j == ntags) {
				m->seltags[i] = True;   /* At least one tag must be viewed */
				cm->seltags[i] = False; /* Can't toggle */
				j = i;
			}

			if (m->curtag == i)
				m->curtag = j;

			arrange(m);
		}
	}

	arrange(cm);
	focus(NULL);
	updateatom[CurDesk] (NULL);
}

void focusview(const char *arg) {
	Client *c;
	int i;

	toggleview(arg);
	i = idxoftag(arg);

	if (!curseltags[i])
		return;

	for (c = stack; c; c = c->snext) {
		if (c->tags[i] && !c->isbastard) {
			focus(c);
			break;
		}
	}

	restack(curmon());
}

void unban(Client *c) {
	if (!c->isbanned)
		return;

	XSelectInput(dpy, c->win, CLIENTMASK & ~(StructureNotifyMask | EnterWindowMask));
	XSelectInput(dpy, c->frame, NoEventMask);
	XMapWindow(dpy, c->win);
	XMapWindow(dpy, c->frame);
	XSelectInput(dpy, c->win, CLIENTMASK);
	XSelectInput(dpy, c->frame, FRAMEMASK);
	setclientstate(c, NormalState);
	c->isbanned = False;
}

void unmanage(Client *c) {
	Monitor *m;
	XWindowChanges wc;
	Bool doarrange, dostruts;
	Window trans;

	m = clientmon(c);
	doarrange = !(c->isfloating || c->isfixed
	             || XGetTransientForHint(dpy, c->win, &trans)) || c->isbastard;
	dostruts = c->hasstruts;

	/* The server grab construct avoids race conditions. */
	XGrabServer(dpy);
	XSelectInput(dpy, c->frame, NoEventMask);
	XUnmapWindow(dpy, c->frame);
	XSetErrorHandler(xerrordummy);

	if (c->title) {
		XFreePixmap(dpy, c->drawable);
		XDestroyWindow(dpy, c->title);
		c->title = (Window) NULL;
	}

	XSelectInput(dpy, c->win, CLIENTMASK & ~(StructureNotifyMask | EnterWindowMask));
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	XReparentWindow(dpy, c->win, root, c->x, c->y);
	XMoveWindow(dpy, c->win, c->x, c->y);

	if (!running)
		XMapWindow(dpy, c->win);
	wc.border_width = c->oldborder;
	XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* Restore border */
	detach(c);
	detachstack(c);

	if (sel == c)
		focus(NULL);

	setclientstate(c, WithdrawnState);
	XDestroyWindow(dpy, c->frame);

	/* c->tags points to monitor */
	if (!c->isbastard)
		free(c->tags);
	free(c);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XUngrabServer(dpy);

	if (dostruts) {
		updatestruts(m);
		updategeom(m);
	}

	if (doarrange) 
		arrange(m);

	updateatom[ClientList] (NULL);
}

void updategeom(Monitor *m) {
	m->wax = m->sx;
	m->way = m->sy;
	m->waw = m->sw;
	m->wah = m->sh;

	switch (views[m->curtag].barpos) {
		default:
			m->wax += m->struts[LeftStrut];
			m->way += m->struts[TopStrut];
			m->waw -= (m->struts[RightStrut] + m->struts[LeftStrut]);
			m->wah = min(m->wah - m->struts[TopStrut],
			         (DisplayHeight(dpy, screen) - (m->struts[BotStrut] + m->struts[TopStrut])));
			break;
		case StrutsHide:
		case StrutsOff:
			break;
	}

	updateatom[WorkArea] (NULL);
}

void updatestruts(Monitor *m) {
	Client *c;

	m->struts[RightStrut] = m->struts[LeftStrut] = m->struts[TopStrut] =
		m->struts[BotStrut] = 0;
	for (c = clients; c; c = c->next)
		if (c->hasstruts)
			getstruts(c);
}

void unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = getclient(ev->window, clients, ClientWindow)) /* && ev->send_event */) {
		if (c->ignoreunmap--)
			return;
		DPRINTF("killing self-unmapped window (%s)\n", c->name);
		unmanage(c);
	}
}

void updateframe(Client *c) {
	int i, f = 0;

	if (!c->title)
		return;

	for (i = 0; i < ntags; i++) {
		if (c->tags[i])
			f += FEATURES(views[i].layout, OVERLAP);
	}

	//onodera
	//c->th = !c->ismax && (c->isfloating || options.dectiled || f) ?
	//			style.titleheight : 0;

	if (!c->th)
		XUnmapWindow(dpy, c->title);
	else
		XMapRaised(dpy, c->title);
}

void updatesizehints(Client *c) {
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize) || !size.flags)
		size.flags = PSize;

	c->flags = size.flags;

	if (c->flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (c->flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;

	if (c->flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;

	if (c->flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;

	if (c->flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (c->flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;

	if (c->flags & PAspect) {
		c->minax = size.min_aspect.x;
		c->maxax = size.max_aspect.x;
		c->minay = size.min_aspect.y;
		c->maxay = size.max_aspect.y;
	} else
		c->minax = c->maxax = c->minay = c->maxay = 0;

	c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
	    && c->maxw == c->minw && c->maxh == c->minh);
}

void updatetitle(Client *c) {
	if (!gettextprop(c->win, atom[WindowName], c->name, sizeof(c->name)))
	    gettextprop(c->win, atom[WMName], c->name, sizeof(c->name));
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (ebastardly on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dsply, XErrorEvent *ee) {
	if (ee->error_code == BadWindow
	    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;

	fprintf(stderr,
	        "pegasus: fatal error: request code=%d, error code=%d\n",
	        ee->request_code, ee->error_code);

	return xerrorxlib(dsply, ee); /* May call exit */
}

int xerrordummy(Display *dsply, XErrorEvent *ee) {
	return 0;
}

int xerrorstart(Display *dsply, XErrorEvent *ee) {
	otherwm = True;
	return -1;
}

void view(const char *arg) {
	int i, j;
	Monitor *m, *cm;
	int prevtag;

	i = idxoftag(arg);
	cm = curmon();

	if (cm->seltags[i])
		return;

	memcpy(cm->prevtags, cm->seltags, ntags * sizeof(cm->seltags[0]));

	for (j = 0; j < ntags; j++)
		cm->seltags[j] = (arg == NULL);

	cm->seltags[i] = True;
	prevtag = cm->curtag;
	cm->curtag = i;

	for (m = mons; m; m = m->next) {
		if (m->seltags[i] && m != cm) {
			m->curtag = prevtag;
			memcpy(m->prevtags, m->seltags, ntags * sizeof(m->seltags[0]));
			memcpy(m->seltags, cm->prevtags, ntags * sizeof(cm->seltags[0]));
			updategeom(m);
			arrange(m);
		}
	}

	updategeom(cm);
	arrange(cm);
	focus(NULL);
	updateatom[CurDesk] (NULL);
}

void viewprevtag(const char *arg) {
	Bool tmptags[ntags];
	unsigned int i = 0;
	int prevcurtag;

	while (i < ntags - 1 && !curprevtags[i])
		i++;

	prevcurtag = curmontag;
	curmontag = i;

	memcpy(tmptags, curseltags, ntags * sizeof(curseltags[0]));
	memcpy(curseltags, curprevtags, ntags * sizeof(curseltags[0]));
	memcpy(curprevtags, tmptags, ntags * sizeof(curseltags[0]));

	if (views[prevcurtag].barpos != views[curmontag].barpos)
		updategeom(curmon());

	arrange(NULL);
	focus(NULL);
	updateatom[CurDesk] (NULL);
}

void viewlefttag(const char *arg) {
	unsigned int i;

	for (i = 0; i < ntags; i++) {
		if (i && curseltags[i]) {
			view(tags[i - 1]);
			break;
		}
	}
}

void viewrighttag(const char *arg) {
	unsigned int i;

	for (i = 0; i < ntags - 1; i++) {
		if (curseltags[i]) {
			view(tags[i + 1]);
			break;
		}
	}
}

void zoom(const char *arg) {
	Client *c;

	if (!sel || !FEATURES(curlayout, ZOOM) || sel->isfloating)
		return;

	if ((c = sel) == nexttiled(clients, curmon()))
		if (!(c = nexttiled(c->next, curmon())))
			return;

	detach(c);
	attach(c, 0);
	arrange(curmon());
	focus(c);
}

int main(int argc, char *argv[]) {
	char conf[256] = "\0";

	if (argc == 3 && !strcmp("-f", argv[1]))
		snprintf(conf, sizeof(conf), "%s", argv[2]);
	else if (argc == 2 && !strcmp("-v", argv[1]))
		eprint("pegasus-" VERSION " (c) 2016 Camille Scholtz\n");
	else if (argc != 1)
		eprint("usage: pegasus [-v] [-f conf]\n");

	setlocale(LC_CTYPE, "");

	if (!(dpy = XOpenDisplay(0)))
		eprint("pegasus: cannot open display\n");

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);

	cargv = argv;
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	checkotherwm();
	setup(conf);
	scan();
	run();
	cleanup();

	XCloseDisplay(dpy);
	return 0;
}
