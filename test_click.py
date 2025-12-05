#!/usr/bin/env python3
import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gdk, Graphene
import cairo

class ClickCanvas(Gtk.DrawingArea):
    def __init__(self):
        super().__init__()
        self.clicks = []  # List of (x, y) click positions
        self.mouse_pos = None
        self.set_draw_func(self.draw)
        
        # Click controller
        click = Gtk.GestureClick()
        click.connect("pressed", self.on_click)
        self.add_controller(click)
        
        # Motion controller
        motion = Gtk.EventControllerMotion()
        motion.connect("motion", self.on_motion)
        self.add_controller(motion)
    
    def on_click(self, gesture, n_press, x, y):
        self.clicks.append((x, y))
        if len(self.clicks) > 20:  # Keep last 20 clicks
            self.clicks.pop(0)
        self.queue_draw()
    
    def on_motion(self, controller, x, y):
        self.mouse_pos = (x, y)
        self.queue_draw()
    
    def draw(self, area, cr, width, height):
        # Background
        cr.set_source_rgb(0.2, 0.2, 0.2)
        cr.paint()
        
        # Draw grid
        cr.set_source_rgba(0.4, 0.4, 0.4, 0.5)
        cr.set_line_width(1)
        for x in range(0, width, 50):
            cr.move_to(x, 0)
            cr.line_to(x, height)
            cr.stroke()
        for y in range(0, height, 50):
            cr.move_to(0, y)
            cr.line_to(width, y)
            cr.stroke()
        
        # Draw crosshair at mouse position
        if self.mouse_pos:
            mx, my = self.mouse_pos
            cr.set_source_rgba(0, 1, 0, 0.5)
            cr.set_line_width(1)
            cr.move_to(mx, 0)
            cr.line_to(mx, height)
            cr.move_to(0, my)
            cr.line_to(width, my)
            cr.stroke()
            
            # Coordinate label
            cr.set_source_rgb(1, 1, 1)
            cr.select_font_face("monospace", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
            cr.set_font_size(14)
            cr.move_to(10, 20)
            cr.show_text(f"Mouse: ({mx:.0f}, {my:.0f})")
        
        # Draw click markers
        for i, (x, y) in enumerate(self.clicks):
            alpha = 0.3 + 0.7 * (i + 1) / len(self.clicks) if self.clicks else 1
            
            # Red circle
            cr.set_source_rgba(1, 0, 0, alpha)
            cr.arc(x, y, 15, 0, 3.14159 * 2)
            cr.fill()
            
            # White center dot
            cr.set_source_rgba(1, 1, 1, alpha)
            cr.arc(x, y, 3, 0, 3.14159 * 2)
            cr.fill()
            
            # Coordinate label
            cr.set_source_rgba(1, 1, 0, alpha)
            cr.set_font_size(12)
            cr.move_to(x + 18, y + 4)
            cr.show_text(f"({x:.0f},{y:.0f})")

class ClickWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Click Test - Visual")
        self.set_default_size(500, 400)
        self.set_child(ClickCanvas())

def on_activate(app):
    win = ClickWindow(app)
    win.present()

app = Gtk.Application()
app.connect('activate', on_activate)
app.run()
