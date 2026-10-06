# Canvas API

The 2-D canvas subsystem facilitates the RawTherapee image viewer with pan,
zoom, and other interactive capabilities. The design is intended to be
extensible for simple annotations over an image.

## Coordinate Systems

There are 3 coordinate systems in the canvas implementation. In all coordinate
systems, the X-axis increases towards the right and the Y-axis increases
towards the bottom. This is consistent with GTK, Cairo, and image pixel access.

1. World
2. Camera
3. Widget

  (0, 0) ---> +x (column)
      |
      |
      v
  +y (row)

### World Coordinates/Units

World coordinates represent the true positioning and sizing of objects added
to the canvas environment. This coordinate system is used to work with objects
without having to adjust for panning and zoom by the user.

A unit in world space is equivalent to 1 image pixel length. This simplifies
calculations for image editing tasks.

For simplicity, the top-left of the image is placed at the origin (0, 0) in
world space such that the coordinates also correspond to the coordinates of
pixels in the image itself.

### Camera Coordinates

The camera (a.k.a viewpoint) is the observer which models which section of the
world is displayed on the canvas. The origin is located at the center of the
canvas (i.e. center of canvas widget in world space).

Just like world coordinates, 1 unit is 1 image pixel long. This coordinate
system is agnostic of HiDPI display/device pixel scaling.

When the display/device scale is 1, camera coordinates are the same as widget
coordinates. When the device scale is greater than 1 (most commonly 2),
camera coordinates still map 1:1 to physical display pixels.

### Widget Coordinates/Units

The logical/CSS pixel coordinate system. The origin is on the top right of the
widget. GTK and Cairo APIs use widget coordinates. GTK events are received in
widget coordinates. Cairo draw operations also use these coordinates.

For HiDPI support, widget coordinates are what is refered to as logical
coordinates. 1 unit in widget coordinates may span multiple physical display
pixels. If device scale is 2, 1 widget coordinate unit spans 2 world units.

### Coordinate System Types

The C++'s static type system is used to enforce the difference coordinate
spaces (see `coord.h`). A dedicated `SpaceTransform` utility is used to convert
between the different spaces.

`rt::geom::*` types stored in the canvas model and returned from public APIs
are in world space. If you need to use the `rt::geom` utilities in other
coordinate spaces, you'll need to convert/instantiate the types manually in
your local scope.
