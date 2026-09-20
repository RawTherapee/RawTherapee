/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Jean-Christophe FRISCH <natureh.510@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#ifdef GUIVERSION

#include <cairomm/cairomm.h>
#include <glibmm/ustring.h>

#include "editcoordsys.h"
#include "rtsurface.h"
#include "rtengine/coord.h"
#include "rtengine/rt_math.h"

class ObjectMOBuffer;

/** @file
 *
 * The Edit mechanism is designed to let tools (subscribers) communicate with the preview area (provider).
 * Subscribers will be tools that need to create some graphics in the preview area, to let the user interact
 * with it in a more user friendly way.
 *
 * Do not confuse with _local_ editing, which is another topic implemented in another class. The Edit feature
 * is also not supported in batch editing from the File Browser.
 *
 * Edit tool can be of 2 types: pipette editing and object editing.
 *
 * ## Pipette edition
 *
 * By using this class, a pipette mechanism can be handled on the preview.
 *
 * Each pipette Edit tool must have a unique ID, that will identify them, and which let the ImProcCoordinator
 * or other mechanism act as appropriated. They are all defined in rtgui/editid.h. A buffer type has to be given
 * too, to know which kind of buffer to allocate (see EditSubscriber::BufferType).
 *
 * Only the first mouse button can be used to manipulate the pipette on the Preview, that's why the developer has
 * to implement at least the following 4 methods:
 *    - mouseOver
 *    - button1Pressed
 *    - drag1
 *    - button1Released
 *
 * Actually, only curves does use this class, and everything is handled for curve implementer (as much as possible).
 * See the curve's class documentation to see how to implement the curve's pipette feature.
 *
 * ### Event handling
 *
 * The mouseOver method is called on each mouse movement, excepted when dragging a point. This method can then access
 * the pipetteVal array values, which contain the mean of the pixel read in the buffer, or -1 if the cursor is outside
 * of the image. In this case, EditDataProvider::object is also set to 0 (and 1 if over the image).
 *
 * When the user will click on the left mouse button while pressing the CTRL key, button1Pressed will be called.
 * Setting "dragging" to true (or false) is not required for the pipette type editing.
 *
 * The drag1 method will be called on all subsequent mouse move. The pipetteVal[3] array will already be filled with
 * the mean of the read values under the cursor (actually a fixed square of 8px). If the BufferType is BT_SINGLEPLANE_FLOAT,
 * only the first array value will be filled.
 *
 * Then the button1Released will be called to stop the dragging.
 *
 * ## Object edition
 *
 * By using this class, objects can be drawn and manipulated on the preview.
 *
 * The developer has to handle the buttonPress, buttonRelease, drag and mouseOver methods that he needs. There
 * are buttonPress, buttonRelease and drag methods dedicated to each mouse button, for better flexibility
 * (e.g.button2Press, button2Release, drag2 will handle event when mouse button 2 is used first). RT actually
 * does not handle multiple mouse button event (e.g. button1 + button2), only one at a time. The first button pressed
 * set the mechanism, all other combined button press are ignored.
 *
 * The developer also have to fill 2 display list with object of the Geometry subclass. Each geometric shape
 * _can_ be used in one or the other, or both list at a time.
 *
 * The first list (visibleGeometry) is used to be displayed on the preview. The developer will have to set their state
 * manually (see Geometry::State), but the display shape, line width and color can be handled automatically, or with
 * specific values. To be displayed, the F_VISIBLE flag has to be set through the setActive or setVisible methods.
 *
 * The second list (mouseOverGeometry) is used in a backbuffer, the color used to draw the shape being the id of the
 * mouseOverGeometry. As an example, you could create a line to be shown in the preview, but create 2 filled Circle object
 * to be used as mouseOver detection, one on each end of the line. The association between both shape (visible and mouseOver)
 * is handled by the developer. To be displayed on this backbuffer, the F_HOVERABLE flag has to be set through the
 * setActive or setHoverable methods. For overlapping mouse over geometry, the priority is set by the order in the list :
 * the last item is detected first (think of it like a stack piled up).
 *
 *
 * ### Event handling
 *
 * RT will draw in the back buffer all mouseOverGeometry set by the developer once the Edit button is pressed, and handle
 * the events automatically.
 *
 * RT will call the mouseOver method on each mouse movement where no mouse button is pressed.
 *
 * On mouse button press over a mouseOverGeometry (that has F_HOVERABLE set), it will call the button press method corresponding
 * to the button (e.g. button1Pressed for mouse button 1), with the modifier key as parameter. Any other mouse button pressed at
 * the same time will be ignored. It's up to the developer to decide whether this action is starting a 'drag' or 'pick' action,
 * by setting the 'action' parameter to the appropriated value.
 *
 * If the user sets action to ES_ACTION_DRAGGING, RT will then send drag1 events (to stay with our button 1 pressed example) on each
 * mouse movement. It's up to the developer of the tool to handle the dragging. The EditProvider class will help you in this by
 * handling the actual position in various coordinate system and ways.
 *
 * When the user will release the mouse button, RT will call the button1Release event (in our example). The developer have
 * then to set action to ES_ACTION_NONE.
 *
 * If the user sets action to ES_ACTION_PICKING, RT will keep in memory the mouseOver object that was selected when pressing the mouse
 * (e.g. button 1), as well as the modifier keys.
 *
 * The element is said to be picked when the mouse button is released over the same mouse over object and with the same active
 * modifier keys. In this case, the corresponding picked event (e.g. picked1 in our example) and the 'picked' flag will be true.
 * If any of those condition is false, picked1 will still be be called to terminate the initiated picking action, but 'picked'
 * will be false. This is necessary because the user may want to update the geometry if the picking is aborted. The developer have
 * then to set action to ES_ACTION_NONE.
 *
 * Picking an on-screen element correspond to single-clicking on it. No double click is supported so far.
 *
 * Each of these methods have to returns a boolean value saying that the preview has to be refreshed or not (i.e. the displayed
 * geometry).
 *
 * ## Other general internal implementation notes
 *
 * When a tool is being constructed, unique IDs are affected to the EditSubscribers of the Pipette type.
 * Then the EditorPanel class will ask all ToolPanel to register the 'after' preview ImageArea object as data provider.
 * The Subscribers have now to provide a toggle button to click on to start the Edit listening. When toggling on, the Subscriber
 * register itself to the DataProvider, then an event is thrown through the standard ToolPanelListener::panelChanged
 * method to update the preview with new graphics to be displayed. If a previous Edit button was active, it will be deactivated
 * (the Edit buttons are mutually exclusive). For the Pipette type, a buffer will be created and has to be populated
 * by the developer in rtengine's pipeline. The unique pipette ID will be used to know where to fill the buffer, as each pipette
 * will need different data, corresponding to the state of the image right before the tool that needs pipette values. E.g for
 * the HSV tool, the Hue and Saturation and Value curves are applied on the current state of the image. That's why the pipette
 * of the H, S and V curve will share the same data of this "current state", otherwise the read value would be wrong.
 *
 * When the Edit process stops, the Subscriber is removed from the DataProvider, so buffers can be freed up.
 * A new ToolPanelListener::panelChanged event is also thrown to update the preview again, without the tool's
 * graphical objects. The Edit button is also toggled off (by the user or programmatically).
 *
 * It means that each Edit buttons toggled on will start an update of the preview which might or might not create
 * a new History entry, depending on the ProcEvent used.
 *
 */

class RGBColor
{
    double r;
    double g;
    double b;

public:
    RGBColor ();
    explicit RGBColor (double r, double g, double b);
    explicit RGBColor (char r, char g, char b);

    void setColor (double r, double g, double b);
    void setColor (char r, char g, char b);

    double getR ();
    double getG ();
    double getB ();
};

class RGBAColor : public RGBColor
{
    double a;

public:
    RGBAColor ();
    explicit RGBAColor (double r, double g, double b, double a);
    explicit RGBAColor (char r, char g, char b, char a);

    void setColor (double r, double g, double b, double a);
    void setColor (char r, char g, char b, char a);

    double getA ();
};

/// @brief Displayable and MouseOver geometry base class
class Geometry
{
public:
    /// @brief Graphical state of the element
    enum State {
        NORMAL,     /// Default state
        ACTIVE,     /// Focused state
        PRELIGHT,   /// Hovered state
        DRAGGED,    /// When being dragged
        INSENSITIVE /// Displayed but insensitive
    };

    /// @brief Coordinate space and origin of the point
    enum Datum {
        IMAGE,          /// Image coordinate system with image's top left corner as origin
        CLICKED_POINT,  /// Screen coordinate system with clicked point as origin
        CURSOR          /// Screen coordinate system with actual cursor position as origin
    };
    enum Flags {
        F_VISIBLE     = 1 << 0, /// true if the geometry have to be drawn on the visible layer
        F_HOVERABLE   = 1 << 1, /// true if the geometry have to be drawn on the "mouse over" layer
        F_AUTO_COLOR  = 1 << 2, /// true if the color depend on the state value, not the color field above
        F_DASHED      = 1 << 3, /// true if the geometry have to be drawn as a dash line
        // (TODO: add a F_LARGE_DASH to have two different dash size ?)
    };

    /// @brief Key point of the image's rectangle that is used to locate the icon copy to the target point:
    enum DrivenPoint {
        DP_CENTERCENTER,
        DP_TOPLEFT,
        DP_TOPCENTER,
        DP_TOPRIGHT,
        DP_CENTERRIGHT,
        DP_BOTTOMRIGHT,
        DP_BOTTOMCENTER,
        DP_BOTTOMLEFT,
        DP_CENTERLEFT
    };

protected:
    static const std::vector<double> dash;
    RGBColor innerLineColor;
    RGBColor outerLineColor;
    short flags;

    // Set MO Channel color according to Edit Widget id and MO Channel mode
    static void setMOChannelColor (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, unsigned short id);

public:
    float innerLineWidth;  // ...outerLineWidth = innerLineWidth+2
    Datum datum;
    State state;  // set by the Subscriber
    float opacity; // Percentage of opacity

    Geometry ();
    virtual ~Geometry() {}

    void setInnerLineColor (double r, double g, double b);
    void setInnerLineColor (char r, char g, char b);
    RGBColor getInnerLineColor ();
    void setOuterLineColor (double r, double g, double b);
    void setOuterLineColor (char r, char g, char b);
    RGBColor getOuterLineColor ();
    double getOuterLineWidth ();
    double getMouseOverLineWidth ();
    void setAutoColor (bool aColor);
    bool isVisible ();
    void setVisible (bool visible);
    bool isHoverable ();
    void setHoverable (bool visible);
    bool isDashed ();
    void setDashed (bool dashed);


    // setActive will enable/disable the visible and hoverable flags in one shot!
    void setActive (bool active);

    virtual void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) = 0;
    virtual void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) = 0;
    virtual void drawToMOChannel   (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) = 0;
};

class Circle : public Geometry
{
public:
    rtengine::Coord center;
    int radius;
    bool filled;
    bool radiusInImageSpace; /// If true, the radius depend on the image scale; if false, it is a fixed 'screen' size

    Circle ();
    Circle (rtengine::Coord& center, int radius, bool filled = false, bool radiusInImageSpace = false);
    Circle (int centerX, int centerY, int radius, bool filled = false, bool radiusInImageSpace = false);

    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel   (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
};

class Line : public Geometry
{
public:
    rtengine::Coord begin;
    rtengine::Coord end;

    Line ();
    Line (const rtengine::Coord& begin, const rtengine::Coord& end);
    Line (int beginX, int beginY, int endX, int endY);

    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel   (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
};

class Polyline : public Geometry
{
public:
    std::vector<rtengine::Coord> points;
    bool filled;

    Polyline ();

    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
};

class EditRectangle : public Geometry // New class name to avoid conflict elsewhere (exiv2), would be nicer to put in namespace?
{
public:
    rtengine::Coord topLeft;
    rtengine::Coord bottomRight;
    bool filled;

    EditRectangle ();

    void setXYWH(int left, int top, int width, int height);
    void setXYXY(int left, int top, int right, int bottom);
    void setXYWH(rtengine::Coord topLeft, rtengine::Coord widthHeight);
    void setXYXY(rtengine::Coord topLeft, rtengine::Coord bottomRight);
    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
};

class Ellipse : public Geometry
{
public:
    rtengine::Coord center;
    int radYT; // Ellipse half-radius for top y-axis
    int radY; // Ellipse half-radius for bottom y-axis
    int radXL; // Ellipse half-radius for left x-axis
    int radX; // Ellipse half-radius for right x-axis
    double angle = 0.; // Rotation angle around the center, in degrees
    bool filled;
    bool radiusInImageSpace; /// If true, the radius depend on the image scale; if false, it is a fixed 'screen' size

    Ellipse ();
    Ellipse (const rtengine::Coord& center, int radYT, int radY, int radXL, int radX, bool filled = false, bool radiusInImageSpace = false);

    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;

private:
    void drawEllipsePath (Cairo::RefPtr<Cairo::Context> &cr, const rtengine::Coord &center, double radYT, double radY, double radXL, double radX);
};

class OPIcon : public Geometry    // OP stands for "On Preview"
{

private:
    std::shared_ptr<RTSurface> normalImg;
    std::shared_ptr<RTSurface> prelightImg;
    std::shared_ptr<RTSurface> activeImg;
    std::shared_ptr<RTSurface> draggedImg;
    std::shared_ptr<RTSurface> insensitiveImg;

    void drawImage (std::shared_ptr<RTSurface> &img, Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem);
    void drawMOImage (std::shared_ptr<RTSurface> &img, Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem);
    void drivenPointToRectangle(const rtengine::Coord &pos, rtengine::Coord &topLeft, rtengine::Coord &bottomRight, int W, int H);

public:
    DrivenPoint drivenPoint;
    rtengine::Coord position;

    OPIcon (const std::shared_ptr<RTSurface> &normal,
            const std::shared_ptr<RTSurface> &active,
            const std::shared_ptr<RTSurface> &prelight = nullptr,
            const std::shared_ptr<RTSurface> &dragged = nullptr,
            const std::shared_ptr<RTSurface> &insensitive = nullptr,
            DrivenPoint drivenPoint = DP_CENTERCENTER);
    OPIcon (Glib::ustring normalImage, Glib::ustring activeImage, Glib::ustring  prelightImage = "", Glib::ustring  draggedImage = "", Glib::ustring insensitiveImage = "", DrivenPoint drivenPoint = DP_CENTERCENTER);
    const std::shared_ptr<RTSurface> getNormalImg();
    const std::shared_ptr<RTSurface> getPrelightImg();
    const std::shared_ptr<RTSurface> getActiveImg();
    const std::shared_ptr<RTSurface> getDraggedImg();
    const std::shared_ptr<RTSurface> getInsensitiveImg();
    void drawOuterGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawInnerGeometry (Cairo::RefPtr<Cairo::Context> &cr, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
    void drawToMOChannel (Cairo::RefPtr<Cairo::Context> &cr, unsigned short id, ObjectMOBuffer *objectBuffer, EditCoordSystem &coordSystem) override;
};

class OPAdjuster : public Geometry    // OP stands for "On Preview"
{

};

inline void RGBColor::setColor (double r, double g, double b) {
    this->r = r;
    this->g = g;
    this->b = b;
}

inline void RGBColor::setColor (char r, char g, char b) {
    this->r = double (r) / 255.;
    this->g = double (g) / 255.;
    this->b = double (b) / 255.;
}

inline double RGBColor::getR () {
    return r;
}

inline double RGBColor::getG () {
    return g;
}

inline double RGBColor::getB () {
    return b;
}

inline void RGBAColor::setColor (double r, double g, double b, double a) {
    RGBColor::setColor (r, g, b);
    this->a = a;
}

inline void RGBAColor::setColor (char r, char g, char b, char a) {
    RGBColor::setColor (r, g, b);
    this->a = double (a) / 255.;
}

inline double RGBAColor::getA () {
    return a;
}

inline void Geometry::setInnerLineColor (double r, double g, double b) {
    innerLineColor.setColor (r, g, b);
    flags &= ~F_AUTO_COLOR;
}

inline void Geometry::setInnerLineColor (char r, char g, char b) {
    innerLineColor.setColor (r, g, b);
    flags &= ~F_AUTO_COLOR;
}

inline void Geometry::setOuterLineColor (double r, double g, double b) {
    outerLineColor.setColor (r, g, b);
    flags &= ~F_AUTO_COLOR;
}

inline double Geometry::getOuterLineWidth () {
    return double (innerLineWidth) + 2.;
}

inline void Geometry::setOuterLineColor (char r, char g, char b) {
    outerLineColor.setColor (r, g, b);
    flags &= ~F_AUTO_COLOR;
}

inline double Geometry::getMouseOverLineWidth () {
    return rtengine::max(double(innerLineWidth), 1.) + 2.;
}

inline void Geometry::setAutoColor (bool aColor) {
    if (aColor) {
        flags |= F_AUTO_COLOR;
    } else {
        flags &= ~F_AUTO_COLOR;
    }
}

inline bool Geometry::isVisible () {
    return flags & F_VISIBLE;
}

inline void Geometry::setVisible (bool visible) {
    if (visible) {
        flags |= F_VISIBLE;
    } else {
        flags &= ~F_VISIBLE;
    }
}

inline bool Geometry::isHoverable () {
    return flags & F_HOVERABLE;
}

inline void Geometry::setHoverable (bool hoverable) {
    if (hoverable) {
        flags |= F_HOVERABLE;
    } else {
        flags &= ~F_HOVERABLE;
    }
}

inline bool Geometry::isDashed () {
    return flags & F_DASHED;
}

inline void Geometry::setDashed (bool dashed) {
    if (dashed) {
        flags |= F_DASHED;
    } else {
        flags &= ~F_DASHED;
    }
}

inline void Geometry::setActive (bool active) {
    if (active) {
        flags |= (F_VISIBLE | F_HOVERABLE);
    } else {
        flags &= ~(F_VISIBLE | F_HOVERABLE);
    }
}

inline Geometry::Geometry () :
        innerLineColor (char (255), char (255), char (255)), outerLineColor (
                char (0), char (0), char (0)), flags (
                F_VISIBLE | F_HOVERABLE | F_AUTO_COLOR), innerLineWidth (1.5f), datum (
                IMAGE), state (NORMAL), opacity(100.) {
}

inline RGBAColor::RGBAColor () :
        RGBColor (0., 0., 0.), a (0.) {
}

inline RGBColor::RGBColor () :
        r (0.), g (0.), b (0.) {
}

inline RGBColor::RGBColor (double r, double g, double b) :
        r (r), g (g), b (b) {
}

inline RGBColor::RGBColor (char r, char g, char b) :
        r (double (r) / 255.), g (double (g) / 255.), b (double (b) / 255.) {
}

inline RGBAColor::RGBAColor (double r, double g, double b, double a) :
        RGBColor (r, g, b), a (a) {
}

inline RGBAColor::RGBAColor (char r, char g, char b, char a) :
        RGBColor (r, g, b), a (double (a) / 255.) {
}

inline Circle::Circle () :
        center (100, 100), radius (10), filled (false), radiusInImageSpace (
                false) {
}

inline EditRectangle::EditRectangle () :
        topLeft (0, 0), bottomRight (10, 10), filled (false) {
}

inline Polyline::Polyline () :
        filled (false) {
}

inline Line::Line () :
        begin (10, 10), end (100, 100) {
}

inline Ellipse::Ellipse () :
        center (100, 100), radYT (5), radY (5), radXL (10), radX (10), filled (false), radiusInImageSpace (false) {
}

inline Circle::Circle (rtengine::Coord& center, int radius, bool filled,
        bool radiusInImageSpace) :
        center (center), radius (radius), filled (filled), radiusInImageSpace (
                radiusInImageSpace) {
}

inline Circle::Circle (int centerX, int centerY, int radius, bool filled,
        bool radiusInImageSpace) :
        center (centerX, centerY), radius (radius), filled (filled), radiusInImageSpace (
                radiusInImageSpace) {
}

inline Line::Line (const rtengine::Coord& begin, const rtengine::Coord& end) :
        begin (begin), end (end) {
}

inline Line::Line (int beginX, int beginY, int endX, int endY) :
        begin (beginX, beginY), end (endX, endY) {
}

inline Ellipse::Ellipse (const rtengine::Coord& center, int radYT, int radY, int radXL, int radX,
        bool filled, bool radiusInImageSpace) :
        center (center), radYT (radYT), radY (radY), radXL (radXL), radX (radX), filled (filled),
                radiusInImageSpace (radiusInImageSpace) {
}

#endif
