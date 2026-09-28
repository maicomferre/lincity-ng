#!/usr/bin/env python3
"""Rasterize an SVG icon with librsvg (requires PyGObject and pycairo)."""
import sys

import cairo
import gi

gi.require_version("Rsvg", "2.0")
from gi.repository import Rsvg

svg = Rsvg.Handle.new_from_file(sys.argv[1])
surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 32, 32)
viewport = Rsvg.Rectangle()
viewport.x = viewport.y = 0
viewport.width = viewport.height = 32
svg.render_document(cairo.Context(surface), viewport)
surface.write_to_png(sys.stdout.buffer)
