
/******************************************************************************
* MODULE     : x_loop.cpp
* DESCRIPTION: The main event loop
* COPYRIGHT  : (C) 1999  Joris van der Hoeven
*******************************************************************************
* This software falls under the GNU general public license and comes WITHOUT
* ANY WARRANTY WHATSOEVER. See the file $TEXMACS_PATH/LICENSE for more details.
* If you don't have this file, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
******************************************************************************/

#include "X/x_display.hpp"
#include "X/x_window.hpp"
#include "iterator.hpp"

#ifdef OS_WIN32
#include <sys/time.h>
#include <sys/misc.h>
#include <sys/_types.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern hashmap<Window,pointer> Window_to_window;
int nr_windows=0;

/******************************************************************************
* Look up keys and mouse
******************************************************************************/

string
x_display_rep::look_up_key (XKeyEvent* ev) {
  KeySym key= 0;
  //cout << ev->state << ", " << ev->keycode << LF;

  /*
  // decode non standard keys
  if (ev->state >= 256) {
    if ((ev->state&3)==0) key= XLookupKeysym (ev, 2);
    else {
      key= XLookupKeysym (ev, 3);
      if (key==0) key= XLookupKeysym (ev, 2);
    }
  }

  // decode standard keys
  if (key == 0) {
    if ((ev->state&3)==0) key= XLookupKeysym (ev, 0);
    else {
      key= XLookupKeysym (ev, 1);
      if (key==0) key= XLookupKeysym (ev, 0);
    }
  }

  // special treatment for num-lock
  string s= ((ev->state&3)? upper_key [key]: lower_key [key]);
  if ((N(s)>=2) && (s[0]=='K') && (s[1]=='-')) {
    if (ev->state&16) {
      if ((ev->state&3)==0) key= XLookupKeysym (ev, 1);
      else {
	key= XLookupKeysym (ev, 0);
	if (key==0) key= XLookupKeysym (ev, 1);
      }
      s= ((ev->state&3)? upper_key [key]: lower_key [key]);
    }
    if ((N(s)>=2) && (s[0]=='K') && (s[1]=='-')) s= s (2, N(s));
  }
  */

  XLookupString (ev, NULL, 0, &key, NULL);
  string s= ((ev->state&3)? upper_key [key]: lower_key [key]);
  if ((N(s)>=2) && (s[0]=='K') && (s[1]=='-')) s= s (2, N(s));

  /* other keyboard modifiers */
  if (N(s)==0) return s;
  if (ev->state&4) s= "C-" * s;
  if (ev->state&8) s= "Mod1-" * s;
  if (ev->state&16) s= "Mod2-" * s;
  if (ev->state&32) s= "Mod3-" * s;
  if (ev->state&64) s= "Mod4-" * s;
  if (ev->state&128) s= "Mod5-" * s;
  // cout << "key press: " << s << LF;
  return s;
}

string
x_display_rep::look_up_mouse (XButtonEvent* ev) {
  switch (ev->button) {
  case Button1: return "left";
  case Button2: return "middle";
  case Button3: return "right";
  case Button4: return "up";
  case Button5: return "down";
  default: return "unknown";
  }
}

unsigned int
x_display_rep::get_button_mask (XButtonEvent* ev) {
  switch (ev->button) {
  case Button1: return Button1Mask;
  case Button2: return Button2Mask;
  case Button3: return Button3Mask;
  case Button4: return Button4Mask;
  case Button5: return Button5Mask;
  default: return 0;
  }
}

/******************************************************************************
* Process events
******************************************************************************/

char* event_name[]= {
  "?",
  "?",
  "Key press",
  "Key release",
  "Button press",
  "Button release",
  "Motion notify",
  "Enter notify",
  "Leave notify",
  "Focus in",
  "Focus out",
  "Keymap notify",
  "Expose",
  "Graphics expose",
  "No expose",
  "Visibility notify",
  "Create notify",
  "Destroy notify",
  "Unmap notify",
  "Map request",
  "Reparent notify",
  "Configure notify",
  "Configure request",
  "Gravity notify",
  "Resize request",
  "Circulate notify",
  "Circulate request",
  "Property notify",
  "Selection clear",
  "Selection request",
  "Selection notify",
  "Colormap notify",
  "Client message",
  "Mapping notify"
};

void
x_display_rep::process_event (x_window win, XEvent* ev) {
  //if (ev->type != NoExpose)
  //cout << "Event: " << event_name[ev->type] << "\n";
  switch (ev->type) {
  case Expose:
    {
      XExposeEvent& ee= ev->xexpose;
      /*
      cout << "Expose: " << ee.x << "," << ee.y << ","
	   << ee.x+ee.width << "," << ee.y+ee.height << "\n";
	   */
      win->invalidate_event (ee.x, ee.y, ee.x+ee.width, ee.y+ee.height);
      break;
    }
  case GraphicsExpose:
    {
      XGraphicsExposeEvent& ee= ev->xgraphicsexpose;
      /*
      cout << "Expose: " << ee.x << "," << ee.y << ","
	   << ee.x+ee.width << "," << ee.y+ee.height << "\n";
	   */
      win->invalidate_event (ee.x, ee.y, ee.x+ee.width, ee.y+ee.height);
      break;
    }
  case NoExpose:
    // cout << "No expose\n";
    break;
  case ConfigureNotify:
    /*
    cout << "Configure move  : " << ev->xconfigure.x << ","
	 << ev->xconfigure.y << "\n";
    cout << "Configure resize: " << ev->xconfigure.width << ","
	 << ev->xconfigure.height << "\n";
	 */
    if ((ev->xconfigure.x!=0) || (ev->xconfigure.y!=0) ||
	((ev->xconfigure.width == win->win_w) &&
	 (ev->xconfigure.height == win->win_h)))
      win->move_event (ev->xconfigure.x, ev->xconfigure.y);
    win->resize_event (ev->xconfigure.width, ev->xconfigure.height);
    break;
  case CreateNotify:
    break;
  case DestroyNotify:
    // cout << "Destroy\n";
    win->destroy_event ();
    event_loop ();
    exit (0);
  case UnmapNotify:
    // cout << "Unmap\n";
    break;
  case ButtonPress:
    unmap_balloon ();
    set_button_state (ev->xbutton.state ^ get_button_mask (&ev->xbutton));
    win->mouse_event ("press-" * look_up_mouse (&ev->xbutton),
		      ev->xbutton.x, ev->xbutton.y, ev->xbutton.time);
    break;
  case ButtonRelease:
    unmap_balloon ();
    set_button_state (ev->xbutton.state ^ get_button_mask (&ev->xbutton));
    win->mouse_event ("release-" * look_up_mouse (&ev->xbutton),
		      ev->xbutton.x, ev->xbutton.y, ev->xbutton.time);
    break;
  case EnterNotify:
    unmap_balloon ();
    if (ev->xcrossing.mode == NotifyNormal) {
      // cout << "Enter at (" <<ev->xcrossing.x<<","<<ev->xcrossing.y << ")\n";
      set_button_state (ev->xcrossing.state);
      win->mouse_event ("enter", ev->xcrossing.x, ev->xcrossing.y,
			ev->xcrossing.time);
    }
    break;
  case LeaveNotify:
    unmap_balloon ();
    if (ev->xcrossing.mode == NotifyNormal) {
      // cout << "Leave at (" <<ev->xcrossing.x<<","<<ev->xcrossing.y << ")\n";
      set_button_state (ev->xcrossing.state);
      win->mouse_event ("leave", ev->xcrossing.x, ev->xcrossing.y,
			ev->xcrossing.time);
    }
    break;
  case FocusIn:
    win->focus_in_event ();
    break;
  case FocusOut:
    win->focus_out_event ();
    break;
  case MotionNotify:
    // cout << "Move to (" << ev->xmotion.x << "," << ev->xmotion.y << ")\n";
    unmap_balloon ();
    set_button_state (ev->xmotion.state);
    win->mouse_event ("move", ev->xmotion.x, ev->xmotion.y, ev->xmotion.time);

    break;
  case KeyPress:
    unmap_balloon ();
    {
      string key= look_up_key (&ev->xkey);
      // cout << "Press " << key << " at " << ev->xkey.time
      // << " (" << texmacs_time() << ")\n";
      if (N(key)>0) win->key_event (key);
      break;
    }
  case SelectionRequest:
    {
      bool flag=true;
      XSelectionRequestEvent& req= ev->xselectionrequest;
      if (selection==NULL) flag=false;
      if ((req.target!=AnyPropertyType) && (req.target!=XA_STRING)) flag=false;
      if (flag)
	XChangeProperty (dpy, req.requestor, req.property, XA_STRING,
			 8, PropModeReplace,
			 (unsigned char*) selection,
			 strlen (selection));
      XSelectionEvent sel;
      sel.type      = SelectionNotify;
      sel.serial    = req.serial;
      sel.send_event= true;
      sel.display   = dpy;
      sel.requestor = req.requestor;
      sel.selection = req.selection;
      sel.target    = req.target;
      sel.property  = flag?req.property:None;
      sel.time      = req.time;
      XSendEvent (dpy, InputFocus, false, 0, (XEvent*) &sel);
      break;
    }
  case SelectionClear:
    clear_selection ("primary");
    break;
  case ClientMessage:
    {
      Atom wm_protocols     = XInternAtom(win->dpy, "WM_PROTOCOLS",     1);
      Atom wm_delete_window = XInternAtom(win->dpy, "WM_DELETE_WINDOW", 1);
      if ((ev->xclient.message_type == wm_protocols) &&
	  ((Atom)ev->xclient.data.l[0] == wm_delete_window))
	win->destroy_event();
      break;
    }
  }
}

/******************************************************************************
* Main event loop
******************************************************************************/

static void (*the_interpose_handler) (void) = NULL;
void set_interpose_handler (void (*r) (void)) { the_interpose_handler= r; }

#define MIN_DELAY   10
#define MAX_DELAY   1000
#define SLEEP_AFTER 120000

void
x_display_rep::event_loop () {
  bool wait  = true;
  int count  = 0;
  int delay  = MIN_DELAY;

  while (nr_windows>0) {
    // Get events
    XEvent report;
    if (XPending (dpy) > 0) {
      XNextEvent (dpy, &report);
      // cout << "Event: " << event_name[report.type] << "\n";
      x_window win= (x_window) Window_to_window[report.xany.window];
      if (win!=NULL) process_event (win, &report);
      count= 0;
      delay= MIN_DELAY;
      wait = false;
    }

    // Don't typeset when resizing window
    if (XPending (dpy) > 0)
      if (report.type == ConfigureNotify ||
	  report.type == Expose ||
	  report.type == NoExpose) continue;

    // Wait for events on all channels and interpose
    if (wait) {
      struct timeval tv;
      tv.tv_sec  = delay/1000;
      tv.tv_usec = 1000 * (delay%1000);
      select (0, NULL, NULL, NULL, &tv);
      count += delay;
      if (count >= SLEEP_AFTER) delay= MAX_DELAY;
    }
    else wait= true;
    if (the_interpose_handler != NULL) the_interpose_handler ();

    // Popup help balloons
    if (!nil (balloon_wid))
      if (texmacs_time () - balloon_time >= 666)
	if (balloon_win == NULL)
	  map_balloon ();

    // Redraw invalid windows
    interrupted= false;
    interrupt_time= texmacs_time () + (100 / (XPending (dpy) + 1));
    iterator<Window> it= iterate (Window_to_window);
    while (it->busy()) { // first the window which has the focus
      x_window win= (x_window) Window_to_window[it->next()];
      if (win->has_focus) win->repaint_invalid_regions();
    }
    it= iterate (Window_to_window);
    while (it->busy()) { // and then the other windows
      x_window win= (x_window) Window_to_window[it->next()];
      if (!win->has_focus) win->repaint_invalid_regions();
    }

    // Handle alarm messages
    while (!nil (messages)) {
      time_t ct= texmacs_time ();
      message m= messages->item;
      if ((m->t - ct) <= 0) {
	messages= messages->next;
	m->wid << emit_alarm (m->s, m->t);
      }
      else break;
    }
  }
}
