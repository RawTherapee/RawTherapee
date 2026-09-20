/*
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>frame
fft *
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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 *  2019-2020 Pierre Cabrera <pierre.cab@gmail.com>
 */
#include "tools/locallabtools.h"

#include "options.h"
#include "rtimage.h"
#include "tools/locallab.h"

#include "rtengine/color.h"
#include "rtengine/improcfun.h"
#include "rtengine/procparams.h"

#define MINRAD 1.5
#define MAXRAD 1000
#define CENTERRAD 100
#define MINCHRO 0.
#define MAXCHRO 150.
#define MAXCHROCC 100.
#define MINEXP -1.5
#define MAXEXP 1.5

namespace
{

void update_ghs_curve(
    double b,
    double d,
    double lp,
    double sp,
    double hp,
    bool inverse,
    LabGrid &lab_grid)
{
    const rtengine::ImProcFunctions::GHTStrType str_type =
        inverse
            ? rtengine::ImProcFunctions::GHTStrType::INVERSE
            : rtengine::ImProcFunctions::GHTStrType::NORMAL;
    const rtengine::ght_compute_params c = rtengine::ImProcFunctions::GHT_setup(
        b,
        d,
        lp,
        sp,
        hp,
        str_type);
    const LabGridArea::FunctionParams::Function function = [=](double x) -> double {
        return rtengine::ImProcFunctions::GHT(x, b, d, lp, sp, hp, c, str_type);
    };
    const LabGridArea::FunctionParams::ResolutionFunction resolution_function = [](int width) -> int {
        return std::min(width, 400);
    };
    lab_grid.setFunctionParams(LabGridArea::FunctionParams(0.0, 1.0, 0.0, 1.0, function, resolution_function));
}

} // namespace

using namespace rtengine;
using namespace procparams;

static double blurSlider2radius(double sval)
{
    // Slider range: 0 - 1000
    double radius;

    if (sval <= 100) {
        // Linear below center-temp
        radius = MINRAD + (sval / 100.0) * (CENTERRAD - MINRAD);
    } else {
        const double slope = (double)(CENTERRAD - MINRAD) / (MAXRAD - CENTERRAD);
        double x = (sval - 100) / 100; // x range: 0 - 1
        double y = x * slope + (1.0 - slope) * pow(x, 4.0);
        radius = CENTERRAD + y * (MAXRAD - CENTERRAD);
    }

    if (radius < MINRAD) {
        radius = MINRAD;
    }

    if (radius > MAXRAD) {
        radius = MAXRAD;
    }

    return radius;
}

static double blurRadius2Slider(double radius)
{
    double sval;

    if (radius <= CENTERRAD) {
        sval = ((radius - MINRAD) / (CENTERRAD - MINRAD)) * 100.0;
    } else {
        const double slope = (double)(CENTERRAD - MINRAD) / (MAXRAD - CENTERRAD);
        const double y = (radius - CENTERRAD) / (MAXRAD - CENTERRAD);
        double x = pow(y, 0.25); // Rough guess of x, will be a little lower
        double k = 0.1;
        bool add = true;

        // The y=f(x) function is a mess to invert, therefore we have this trial-refinement loop instead.
        // From tests, worst case is about 20 iterations, i.e. no problem
        for (;;) {
            double y1 = x * slope + (1.0 - slope) * pow(x, 4.0);

            if (100. * fabs(y1 - y) < 0.1) {
                break;
            }

            if (y1 < y) {
                if (!add) {
                    k /= 2;
                }

                x += k;
                add = true;
            } else {
                if (add) {
                    k /= 2;
                }

                x -= k;
                add = false;
            }
        }

        sval = 100.0 + x * 100.0;
    }

    if (sval < 0.) {
        sval = 0.;
    }

    if (sval > 10000.) {
        sval = 10000.;
    }

    return sval;
}

/* ==== LocallabTool ==== */
LocallabTool::LocallabTool(Gtk::Box* content, Glib::ustring toolName, Glib::ustring UILabel, bool need11, bool needMode):
    ToolPanel(toolName, need11),

    // LocallabTool parameters
    needMode(needMode),
    isLocActivated(false),
    locToolListener(nullptr),

    // LocallabTool generic widgets
    complexity(Gtk::manage(new MyComboBoxText()))
{
    // Create expander title bar
    Gtk::Box *titVBox;
    titVBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    titVBox->set_spacing(2);

    Gtk::Box* const titleBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* const titleLabel = Gtk::manage(new Gtk::Label());
    titleLabel->set_markup(Glib::ustring("<b>") + escapeHtmlChars(UILabel) + Glib::ustring("</b>"));
    titleLabel->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    titleBox->pack_start(*titleLabel, Gtk::PACK_EXPAND_WIDGET, 0);

    Gtk::EventBox* const removeEvBox = Gtk::manage(new Gtk::EventBox()); // Glue to manage mouse clicking event on remove image
    removeEvBox->set_can_focus(false);
    removeEvBox->set_above_child(false); // To have priority over expander title bar when mouse clicking on remove image
    removeEvBox->signal_button_release_event().connect(sigc::mem_fun(this, &LocallabTool::on_remove_change));
    RTImage* const removeImage = Gtk::manage(new RTImage("cancel-small", Gtk::ICON_SIZE_BUTTON));
    removeEvBox->add(*removeImage);
    titleBox->pack_end(*removeEvBox, Gtk::PACK_SHRINK, 1);
    if (needMode) {
        complexity->append(M("TP_LOCALLAB_MODE_EXPERT"));
        complexity->append(M("TP_LOCALLAB_MODE_NORMAL"));
        complexity->append(M("TP_LOCALLAB_MODE_SIMPLE"));
        complexity->set_active(2);
        complexityConn = complexity->signal_changed().connect(sigc::mem_fun(*this, &LocallabTool::complexityModeChanged));
    }

    Gtk::Separator* const separator = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_VERTICAL));
    titleBox->pack_end(*separator, Gtk::PACK_SHRINK, 0);

    if (need100Percent) {
        RTImage* const titleImage = Gtk::manage(new RTImage("one-to-one-small", Gtk::ICON_SIZE_BUTTON));
        titleImage->set_tooltip_text(M("TP_GENERAL_11SCALE_TOOLTIP"));
        titleBox->pack_end(*titleImage, Gtk::PACK_SHRINK, 0);
    }
    titVBox->pack_start(*titleBox, Gtk::PACK_SHRINK, 1);
    titVBox->pack_start(*complexity, Gtk::PACK_SHRINK, 1);

    exp = Gtk::manage(new MyExpander(true, titVBox));
    exp->signal_button_release_event().connect_notify(sigc::mem_fun(this, &LocallabTool::foldThemAll));
    enaExpConn = exp->signal_enabled_toggled().connect(sigc::mem_fun(*this, &LocallabTool::enabledChanged));

    ToolParamBlock* const totalBox = Gtk::manage(new ToolParamBlock());

    // Create panel for specific widget tools
    totalBox->pack_start(*content, Gtk::PACK_SHRINK, 0);
    exp->add(*totalBox, false);
}

LocallabTool::~LocallabTool()
{
    idle_register.destroy();
}

Glib::ustring LocallabTool::getSpotName() const
{
    if (spotNameSource) {
        return *spotNameSource;
    }
    return "";
}

void LocallabTool::addLocallabTool(bool raiseEvent)
{
    exp->set_visible(true);

    // Raise event if required
    if (raiseEvent) { // Note: Event is only raised when a tool is added by user
        // By default, activate newly added tool
        enaExpConn.block(true);
        exp->setEnabled(true);
        enaExpConn.block(false);

        if (needMode) {
            // Set complexity mode according to chosen default one
            complexityConn.block(true);
            complexity->set_active(App::get().options().complexity);
            complexityConn.block(false);

            // Update GUI accordingly
            if (complexity->get_active_row_number() == Normal) {
                convertParamToNormal();
                updateGUIToMode(Normal);
            } else if (complexity->get_active_row_number() == Expert) {
                updateGUIToMode(Expert);
            } else if (complexity->get_active_row_number() == Simple) {
                convertParamToNormal();
                updateGUIToMode(Normal);
                convertParamToSimple();
                updateGUIToMode(Simple);
            }

        }

        if (listener) {
            listener->panelChanged(EvlocallabToolAdded,
                                   toolName + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabTool::removeLocallabTool(bool raiseEvent)
{
    exp->set_visible(false);

    // Inform LocallabToolListener to update Locallab tools list
    if (locToolListener) {
        locToolListener->toolRemoved(this);
    }

    if (exp->getEnabled() || isMaskViewActive()) {
        // Disable tool while removing it
        disableListener();
        exp->setEnabled(false);
        enableListener();
        // Note: Mask views are all reset when removing tool (in toolpanelcoord.cc)

        // Raise event if required refreshing image
        if (raiseEvent && listener) {
            listener->panelChanged(EvlocallabToolRemovedWithRefresh,
                                   toolName + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    } else {
        // Raise event if required without refreshing image
        if (raiseEvent && listener) {
            listener->panelChanged(EvlocallabToolRemovedWithoutRefresh,
                                   toolName + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

bool LocallabTool::isLocallabToolAdded()
{
    return exp->get_visible();
}

void LocallabTool::refChanged(const double huer, const double lumar, const double chromar, const float fab)
{
    // Hue reference normalization (between 0 and 1)
    double normHuer = huer;
    float h = Color::huelab_to_huehsv2(normHuer);
    // h += 1.f / 6.f;

    if (h > 1.f) {
        h -= 1.f;
    }

    normHuer = h;

    double normHuerjz = huer;

    float hz = Color::huejz_to_huehsv2(normHuerjz);

    if (hz > 1.f) {
        hz -= 1.f;
    }
    normHuerjz = hz;

    // Luma reference normalization (between 0 and 1)
    const double normLumar = lumar / 100.f;

    // Chroma reference normalization (between 0 and 1)
    const double corfap = (65535.) / (double) fab;
    //printf("FAB=%f corfap=%f chromar=%f chroret=%f\n", (double) fab, corfap, chromar, (double) corfap * (chromar / 195.f));
    const double normChromar = LIM01(corfap * (chromar / 195.f));//195 a little more than 128 * 1.414 = 181

    // Update mask curve backgrounds
    updateMaskBackground(normChromar, normLumar, normHuer, normHuerjz);
}

Gtk::ToggleButton *LocallabTool::getPreviewDeltaEButton() const
{
    return nullptr;
}

sigc::connection *LocallabTool::getPreviewDeltaEButtonConnection()
{
    return nullptr;
}

void LocallabTool::colorForValue(double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller* caller)
{
    float R = 0.f;
    float G = 0.f;
    float B = 0.f;
    float x;

    if (elemType == ColorCaller::CCET_VERTICAL_BAR) {
        valY = 0.5;
    }

    switch (callerId) {
        case 1: // Mask CC shape (bottom bar) + Color CC/LC shape (left bar)
            Color::hsv2rgb01(float(valY), float(valX), 0.5f, R, G, B);

            break;

        case 2: // Mask HH shape (main curve and bottom bar)
            x = valX - 1.f / 6.f;

            if (x < 0.f) {
                x += 1.f;
            }

            x = log2lin(x, 3.f);
            Color::hsv2rgb01(x, 0.5f, 0.5f, R, G, B);

            break;

        case 3: // Color LH/HH shape (main curve)
            Color::hsv2rgb01(float (valX), float (valY), 0.5f, R, G, B);

            break;

        case 4: // Color CC/LC shape (bottom bar)
            const float value = (1.f - 0.7f) * float (valX) + 0.7f;
            // Whole hue range
            // Y axis / from 0.15 up to 0.75 (arbitrary values; was 0.45 before)
            Color::hsv2rgb01(float (valY), float (valX), value, R, G, B);
    }

    caller->ccRed = double (R);
    caller->ccGreen = double (G);
    caller->ccBlue = double (B);
}

void LocallabTool::disableListener()
{
    ToolPanel::disableListener();

    enaExpConn.block(true);

    if (needMode) {
        complexityConn.block(true);
    }
}
void LocallabTool::enableListener()
{
    ToolPanel::enableListener();

    enaExpConn.block(false);

    if (needMode) {
        complexityConn.block(false);
    }
}



bool LocallabTool::on_remove_change(GdkEventButton* event)
{
    if (event->button == GDK_BUTTON_PRIMARY) {
        // Remove Locallab tool raising an event
        removeLocallabTool(true);
    }

    return true; // No event propagation further (to avoid closing expander when mouse clicking on remove image)
}

void LocallabTool::foldThemAll(GdkEventButton* event)
{
    if (event->button == GDK_BUTTON_SECONDARY) {
        if (locToolListener) {
            (static_cast<Locallab*>(locToolListener))->foldAllButOne(this);
        }
    }
}

void LocallabTool::complexityModeChanged()
{
    if (complexity->get_active_row_number() == Simple) { // New selected mode is Simple one
        const bool maskPreviewActivated = isMaskViewActive();

        // Convert tool widget parameters
        convertParamToNormal(); // From Expert mode to Normal mode
        convertParamToSimple(); // From Normal mode to Simple mode
        // Update GUI based on new mode
        updateGUIToMode(Simple);

        if (maskPreviewActivated) {
            // This event is called to transmit reset mask state
            if (listener) {
                listener->panelChanged(EvlocallabshowmaskMethod, "");
            }
        }

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_SIMPLE") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    } else if (complexity->get_active_row_number() == Normal) { // New selected mode is Normal one
        // Convert tool widget parameters
        convertParamToNormal();
        // Update GUI based on new mode
        updateGUIToMode(Normal);

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_NORMAL") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    } else if (complexity->get_active_row_number() == Expert) { // New selected mode is Expert one
        // Update GUI based on new mode
        updateGUIToMode(Expert);

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_EXPERT") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

/* ==== LocallabColor ==== */
LocallabColor::LocallabColor():
    LocallabTool(this, M("TP_LOCALLAB_COLOR_TOOLNAME"), M("TP_LOCALLAB_COFR"), false),

    // Color & Light specific widgets
    lumFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LUMFRA")))),
    reparcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LOGREPART"), 1.0, 100.0, 1., 100.0))),
    gamc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMC"), 0.5, 3.0, 0.05, 1.))),
    lightness(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LIGHTNESS"), -100, 500, 1, 0))),
    contrast(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CONTRAST"), -100, 100, 1, 0))),
    chroma(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMA"), -100, 150, 1, 0))),
    curvactiv(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_CURV")))),
    gridFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LABGRID")))),
    labgrid(Gtk::manage(new LabGrid(EvLocallabLabGridValue, M("TP_LOCALLAB_LABGRID_VALUES"), true, false))),
    gridMethod(Gtk::manage(new MyComboBoxText())),
    strengthgrid(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRGRID"), 0, 100, 1, 30))),
    sensi(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 30))),
    previewcol(Gtk::manage(new Gtk::ToggleButton(M("TP_LOCALLAB_PREVIEW")))),
    
    structcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRUCCOL1"), 0, 100, 1, 0))),
    blurcolde(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLURDE"), 2, 100, 1, 5))),
    softradiuscol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SOFTRADIUSCOL"), 0.0, 100.0, 0.5, 0.))),
    exprecov(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    maskusablec(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusablec(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    recothresc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 0., 2., 0.01, 1.))),
    lowthresc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW"), 1., 80., 0.5, 12.))),
    higthresc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR"), 20., 99., 0.5, 85.))),
    decayc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKDDECAY"), 0.5, 4., 0.1, 2.))),
    invers(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVERS")))),
    expgradcol(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPGRAD")))),
    strcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTRLUM"), -4., 4., 0.05, 0.))),
    strcolab(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTRCHRO"), -6., 6., 0.05, 0.))),
    strcolh(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTRHUE"), -6., 6., 0.05, 0.))),
    angcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADANG"), -180, 180, 0.1, 0.))),
    feathercol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FEATVALUE"), 10., 100., 0.1, 25.))),
    expcurvcol(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPCURV")))),
    labqualcurv(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_QUALCURV_METHOD") + ":"))),
    qualitycurveMethod(Gtk::manage(new MyComboBoxText())),
    llCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_LUM"))),
    llshape(static_cast<DiagonalCurveEditor*>(llCurveEditorG->addCurve(CT_Diagonal, "L(L)"))),
    ccshape(static_cast<DiagonalCurveEditor*>(llCurveEditorG->addCurve(CT_Diagonal, "C(C)"))),
    clCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_CH"))),
    clshape(static_cast<DiagonalCurveEditor*>(clCurveEditorG->addCurve(CT_Diagonal, "C(L)"))),
    lcshape(static_cast<DiagonalCurveEditor*>(clCurveEditorG->addCurve(CT_Diagonal, "L(C)"))),
    HCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_HLH"))),
    LHshape(static_cast<FlatCurveEditor*>(HCurveEditorG->addCurve(CT_Flat, "L(h)", nullptr, false, true))),
    H3CurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_HLH"))),
    CHshape(static_cast<FlatCurveEditor*>(H3CurveEditorG->addCurve(CT_Flat, "C(h)", nullptr, false, true))),
    H2CurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_HLH"))),
    HHshape(static_cast<FlatCurveEditor*>(H2CurveEditorG->addCurve(CT_Flat, "h(h)", nullptr, false, true))),
    rgbCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_RGB"))),
    toneMethod(Gtk::manage(new MyComboBoxText())),
    rgbshape(static_cast<DiagonalCurveEditor*>(rgbCurveEditorG->addCurve(CT_Diagonal, "", toneMethod))),
    special(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_SPECIAL")))),
    expmaskcol1(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWC1")))),
    merMethod(Gtk::manage(new MyComboBoxText())),
    mask7(Gtk::manage(new ToolParamBlock())),
    mergecolMethod(Gtk::manage(new MyComboBoxText())),
    mercol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MERDCOL"), 0.0, 100.0, 0.5, 18.))),
    opacol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_OPACOL"), 0.0, 100.0, 0.5, 60.))),
    conthrcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CONTTHR"), 0.0, 100.0, 0.5, 0.))),
    gridmerFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LABGRIDMERG")))),
    labgridmerg(Gtk::manage(new LabGrid(EvLocallabLabGridmergValue, M("TP_LOCALLAB_LABGRID_VALUES"), false))),
    merlucol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MERLUCOL"), 0.0, 100.0, 0.5, 32., Gtk::manage(new RTImage("circle-black-small")), Gtk::manage(new RTImage("circle-white-small"))))),
    expmaskcol(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWC")))),
    mergecolFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_MERGECOLFRA")))),
    showmaskcolMethod(Gtk::manage(new MyComboBoxText())),
    showmaskcolMethodinv(Gtk::manage(new MyComboBoxText())),
    enaColorMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASK")))),
//    maskCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASKCOL"))),
    maskCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir,"", 1)),
    CCmaskshape(static_cast<FlatCurveEditor*>(maskCurveEditorG->addCurve(CT_Flat, "C", nullptr, false, false))),
    LLmaskshape(static_cast<FlatCurveEditor*>(maskCurveEditorG->addCurve(CT_Flat, "L", nullptr, false, false))),
    HHmaskshape(static_cast<FlatCurveEditor *>(maskCurveEditorG->addCurve(CT_Flat, "LC(h)", nullptr, false, true))),
    struFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LABSTRUM")))),
    strumaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRUMASKCOL"), 0., 200., 0.1, 0.))),
    toolcol(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_TOOLCOL")))),
    blurFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LABBLURM")))),
    fftColorMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_FFTCOL_MASK")))),
    contcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CONTCOL"), 0., 200., 0.5, 0.))),
    blurcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLURCOL"), 0.2, 100., 0.5, 0.2))),
    blendmaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLENDMASKCOL"), -100, 100, 1, 0))),
    toolcolFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_TOOLMASK")))),
    toolcolFrame2(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_TOOLMASK_2")))),
    radmaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    lapmaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    chromaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMASKCOL"), -100.0, 100.0, 0.1, 0.))),
    gammaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMMASKCOL"), 0.25, 4.0, 0.01, 1.))),
    slomaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOMASKCOL"), 0.0, 15.0, 0.1, 0.))),
    shadmaskcol(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SHAMASKCOL"), 0, 100, 1, 0))),
 //   maskHCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASKH"))),
    maskHCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, "")),
    HHhmaskshape(static_cast<FlatCurveEditor *>(maskHCurveEditorG->addCurve(CT_Flat, "h(h)", nullptr, false, true))),
    mask2CurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK2"))),
    Lmaskshape(static_cast<DiagonalCurveEditor*>(mask2CurveEditorG->addCurve(CT_Diagonal, "L(L)"))),
    mask2CurveEditorGwav(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_WAVMASK"))),
    LLmaskcolshapewav(static_cast<FlatCurveEditor*>(mask2CurveEditorGwav->addCurve(CT_Flat, "L(L)", nullptr, false, false))),
    csThresholdcol(Gtk::manage(new ThresholdAdjuster(M("TP_LOCALLAB_CSTHRESHOLDBLUR"), 0, 9, 0, 0, 6, 5, 0, false)))
{
    auto m = ProcEventMapper::getInstance();
    //rtengine::ProcEvent EvlocallabenacieMaskall;
    Evlocallabfeathercol = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_FEATHERCOL");
    Evlocallabpreviewcol = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_PREVIEWCOL");
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    float R, G, B;

    std::vector<GradientMilestone> six_shape;

    for (int i = 0; i < 6; i++) {
        const float x = static_cast<float>(i) * (1.f / 6.f);
        Color::hsv2rgb01(x, 0.5f, 0.5f, R, G, B);
        six_shape.emplace_back(x, R, G, B);
    }

    const LocallabParams::LocallabSpot defSpot;

    // Parameter Color & Light specific widgets
    lumFrame->set_label_align(0.025, 0.5);

    lightness->setAdjusterListener(this);

    gamc->setAdjusterListener(this);

    reparcol->setAdjusterListener(this);

    contrast->setAdjusterListener(this);

    chroma->setAdjusterListener(this);

    curvactivConn = curvactiv->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::curvactivChanged));

    gridFrame->set_label_align(0.025, 0.5);

    gridMethod->append(M("TP_LOCALLAB_GRIDONE"));
    gridMethod->append(M("TP_LOCALLAB_GRIDTWO"));
    gridMethod->set_active(0);
    gridMethodConn = gridMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::gridMethodChanged));

    strengthgrid->setAdjusterListener(this);

    sensi->setAdjusterListener(this);

    structcol->setAdjusterListener(this);

    blurcolde->setAdjusterListener(this);

    softradiuscol->setLogScale(10, 0);
    softradiuscol->setAdjusterListener(this);
    recothresc->setAdjusterListener(this);
    lowthresc->setAdjusterListener(this);
    higthresc->setAdjusterListener(this);
    decayc->setAdjusterListener(this);
    setExpandAlignProperties(exprecov, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    inversConn = invers->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::inversChanged));
    invers->set_tooltip_text(M("TP_LOCALLAB_INVERS_TOOLTIP"));

    setExpandAlignProperties(expgradcol, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    strcol->setAdjusterListener(this);

    strcolab->setAdjusterListener(this);
    strcolab->set_tooltip_text(M("TP_LOCALLAB_GRADSTRAB_TOOLTIP"));

    strcolh->setAdjusterListener(this);
    strcolh->set_tooltip_text(M("TP_LOCALLAB_GRADSTRHUE_TOOLTIP"));

    angcol->setAdjusterListener(this);
    feathercol->setAdjusterListener(this);

    setExpandAlignProperties(expcurvcol, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    previewcol->set_active(false);
    previewcolConn = previewcol->signal_clicked().connect(
                       sigc::mem_fun(
                           *this, &LocallabColor::previewcolChanged));

    qualitycurveMethod->append(M("TP_LOCALLAB_CURVNONE"));
    qualitycurveMethod->append(M("TP_LOCALLAB_CURVCURR"));
    qualitycurveMethod->set_active(0);
    qualitycurveMethodConn = qualitycurveMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::qualitycurveMethodChanged));

    llCurveEditorG->setCurveListener(this);

    llshape->setResetCurve(DiagonalCurveType(defSpot.llcurve.at(0)), defSpot.llcurve);
    llshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    llshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    llshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    ccshape->setResetCurve(DiagonalCurveType(defSpot.cccurve.at(0)), defSpot.cccurve);
    ccshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    ccshape->setBottomBarColorProvider(this, 4);
    ccshape->setLeftBarColorProvider(this, 1);

    llCurveEditorG->curveListComplete();

    clCurveEditorG->setCurveListener(this);

    clshape->setResetCurve(DiagonalCurveType(defSpot.clcurve.at(0)), defSpot.clcurve);
    clshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    clshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    clshape->setLeftBarColorProvider(this, 1);

    lcshape->setResetCurve(DiagonalCurveType(defSpot.lccurve.at(0)), defSpot.lccurve);
    lcshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    lcshape->setBottomBarColorProvider(this, 4);
    lcshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    clCurveEditorG->curveListComplete();

    HCurveEditorG->setCurveListener(this);

    LHshape->setIdentityValue(0.);
    LHshape->setResetCurve(FlatCurveType(defSpot.LHcurve.at(0)), defSpot.LHcurve);
    LHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    LHshape->setCurveColorProvider(this, 3);
    LHshape->setBottomBarBgGradient(six_shape);

    HCurveEditorG->curveListComplete();

    H3CurveEditorG->setCurveListener(this);

    CHshape->setIdentityValue(0.);
    CHshape->setResetCurve(FlatCurveType(defSpot.CHcurve.at(0)), defSpot.CHcurve);
    CHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    CHshape->setCurveColorProvider(this, 3);
    CHshape->setBottomBarBgGradient(six_shape);

    H3CurveEditorG->curveListComplete();

    H2CurveEditorG->setCurveListener(this);

    HHshape->setIdentityValue(0.);
    HHshape->setResetCurve(FlatCurveType(defSpot.HHcurve.at(0)), defSpot.HHcurve);
    HHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    HHshape->setCurveColorProvider(this, 3);
    HHshape->setBottomBarBgGradient(six_shape);

    H2CurveEditorG->curveListComplete();

    rgbCurveEditorG->setCurveListener(this);

    toneMethod->append(M("TP_EXPOSURE_TCMODE_STANDARD"));
    toneMethod->append(M("TP_EXPOSURE_TCMODE_WEIGHTEDSTD"));
    toneMethod->append(M("TP_EXPOSURE_TCMODE_LUMINANCE"));
    toneMethod->append(M("TP_EXPOSURE_TCMODE_FILMLIKE"));
    toneMethod->set_active(0);
    toneMethodConn = toneMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::toneMethodChanged));

    rgbshape->setResetCurve(DiagonalCurveType(defSpot.rgbcurve.at(0)), defSpot.rgbcurve);
    rgbshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    rgbshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    rgbshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    rgbCurveEditorG->curveListComplete();

    specialConn  = special->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::specialChanged));

    setExpandAlignProperties(expmaskcol1, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    merMethod->append(M("TP_LOCALLAB_MRONE"));
    // merMethod->append(M("TP_LOCALLAB_MRTWO"));
    merMethod->append(M("TP_LOCALLAB_MRTHR"));
    merMethod->append(M("TP_LOCALLAB_MRFOU"));
    merMethod->append(M("TP_LOCALLAB_MRFIV"));
    merMethod->set_active(0);
    merMethodConn = merMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::merMethodChanged));

    mergecolMethod->append(M("TP_LOCALLAB_MERONE"));
    mergecolMethod->append(M("TP_LOCALLAB_MERTWO"));
    mergecolMethod->append(M("TP_LOCALLAB_MERTHR"));
    mergecolMethod->append(M("TP_LOCALLAB_MERFOU"));
    mergecolMethod->append(M("TP_LOCALLAB_MERFIV"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSIX"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSEV"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSEV0"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSEV1"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSEV2"));
    mergecolMethod->append(M("TP_LOCALLAB_MERHEI"));
    mergecolMethod->append(M("TP_LOCALLAB_MERNIN"));
    mergecolMethod->append(M("TP_LOCALLAB_MERTEN"));
    mergecolMethod->append(M("TP_LOCALLAB_MERELE"));
    mergecolMethod->append(M("TP_LOCALLAB_MERTWE"));
    mergecolMethod->append(M("TP_LOCALLAB_MERTHI"));
    mergecolMethod->append(M("TP_LOCALLAB_MERFOR"));
    mergecolMethod->append(M("TP_LOCALLAB_MERHUE"));
    mergecolMethod->append(M("TP_LOCALLAB_MERSAT"));
    mergecolMethod->append(M("TP_LOCALLAB_MERCOL"));
    mergecolMethod->append(M("TP_LOCALLAB_MERLUM"));
    mergecolMethod->set_active(0);
    mergecolMethodConn = mergecolMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::mergecolMethodChanged));

    mercol->setAdjusterListener(this);

    opacol->setAdjusterListener(this);

    conthrcol->setAdjusterListener(this);

    merlucol->setAdjusterListener(this);

    setExpandAlignProperties(expmaskcol, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWMODIF"));
    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWMODIFMASK"));
    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWSTRUC"));
    showmaskcolMethod->append(M("TP_LOCALLAB_SHOWREF"));
    showmaskcolMethod->set_active(0);
    showmaskcolMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskcolMethodConn  = showmaskcolMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::showmaskcolMethodChanged));

    showmaskcolMethodinv->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskcolMethodinv->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskcolMethodinv->set_active(0);
    showmaskcolMethodinv->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskcolMethodConninv  = showmaskcolMethodinv->signal_changed().connect(sigc::mem_fun(*this, &LocallabColor::showmaskcolMethodChangedinv));

    enaColorMaskConn = enaColorMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::enaColorMaskChanged));

    maskCurveEditorG->setCurveListener(this);

    CCmaskshape->setIdentityValue(0.);
    CCmaskshape->setResetCurve(FlatCurveType(defSpot.CCmaskcurve.at(0)), defSpot.CCmaskcurve);
    CCmaskshape->setBottomBarColorProvider(this, 1);

    LLmaskshape->setIdentityValue(0.);
    LLmaskshape->setResetCurve(FlatCurveType(defSpot.LLmaskcurve.at(0)), defSpot.LLmaskcurve);
    LLmaskshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    HHmaskshape->setIdentityValue(0.);
    HHmaskshape->setResetCurve(FlatCurveType(defSpot.HHmaskcurve.at(0)), defSpot.HHmaskcurve);
    HHmaskshape->setCurveColorProvider(this, 2);
    HHmaskshape->setBottomBarColorProvider(this, 2);

    maskCurveEditorG->curveListComplete();

    struFrame->set_label_align(0.025, 0.5);

    strumaskcol->setAdjusterListener(this);

    toolcolConn  = toolcol->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::toolcolChanged));

    blurFrame->set_label_align(0.025, 0.5);

    fftColorMaskConn = fftColorMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabColor::fftColorMaskChanged));

    contcol->setAdjusterListener(this);

    blurcol->setAdjusterListener(this);

    blendmaskcol->setAdjusterListener(this);

    radmaskcol->setAdjusterListener(this);

    lapmaskcol->setAdjusterListener(this);

    chromaskcol->setAdjusterListener(this);

    gammaskcol->setAdjusterListener(this);

    slomaskcol->setAdjusterListener(this);

    shadmaskcol->setAdjusterListener(this);

    maskHCurveEditorG->setCurveListener(this);

    HHhmaskshape->setIdentityValue(0.);
    HHhmaskshape->setResetCurve(FlatCurveType(defSpot.HHhmaskcurve.at(0)), defSpot.HHhmaskcurve);
    HHhmaskshape->setCurveColorProvider(this, 2);
    HHhmaskshape->setBottomBarColorProvider(this, 2);

    maskHCurveEditorG->curveListComplete();

    mask2CurveEditorG->setCurveListener(this);

    Lmaskshape->setResetCurve(DiagonalCurveType(defSpot.Lmaskcurve.at(0)), defSpot.Lmaskcurve);
    Lmaskshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    Lmaskshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    mask2CurveEditorG->curveListComplete();

    mask2CurveEditorGwav->setCurveListener(this);

    LLmaskcolshapewav->setIdentityValue(0.);
    LLmaskcolshapewav->setResetCurve(FlatCurveType(defSpot.LLmaskcolcurvewav.at(0)), defSpot.LLmaskcolcurvewav);
//    LLmaskcolshapewav->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    mask2CurveEditorGwav->curveListComplete();

    csThresholdcol->setAdjusterListener(this);

    // Add Color & Light specific widgets to GUI
    pack_start(*reparcol);
    pack_start(*sensi);
    pack_start(*previewcol);
    
    pack_start(*invers);
    ToolParamBlock* const lumBox = Gtk::manage(new ToolParamBlock());
    lumBox->pack_start(*lightness);
    lumBox->pack_start(*contrast);
    lumBox->pack_start(*chroma);
    lumBox->pack_start(*gamc);
    lumFrame->add(*lumBox);
    pack_start(*lumFrame);
    Gtk::Frame* const superFrame = Gtk::manage(new Gtk::Frame());
    superFrame->set_label_align(0.025, 0.5);
    // superFrame->set_label_widget(*curvactiv);
    ToolParamBlock* const superBox = Gtk::manage(new ToolParamBlock());
    ToolParamBlock* const gridBox = Gtk::manage(new ToolParamBlock());
    gridBox->pack_start(*labgrid);
    gridBox->pack_start(*gridMethod);
    gridBox->pack_start(*strengthgrid);
    gridFrame->add(*gridBox);
    superBox->pack_start(*gridFrame);
    superFrame->add(*superBox);
    pack_start(*superFrame);
 //   pack_start(*sensi);
    pack_start(*structcol);
    pack_start(*blurcolde);
    pack_start(*softradiuscol);
//    pack_start(*invers);
    ToolParamBlock* const colBox3 = Gtk::manage(new ToolParamBlock());
    colBox3->pack_start(*maskusablec, Gtk::PACK_SHRINK, 0);
    colBox3->pack_start(*maskunusablec, Gtk::PACK_SHRINK, 0);
    colBox3->pack_start(*recothresc);
    colBox3->pack_start(*lowthresc);
    colBox3->pack_start(*higthresc);
    colBox3->pack_start(*decayc);
   // colBox3->pack_start(*invmaskc);
    exprecov->add(*colBox3, false);
    pack_start(*exprecov, false, false);


    ToolParamBlock* const gradcolBox = Gtk::manage(new ToolParamBlock());
    gradcolBox->pack_start(*strcol);
    gradcolBox->pack_start(*strcolab);
    gradcolBox->pack_start(*strcolh);
    gradcolBox->pack_start(*angcol);
    gradcolBox->pack_start(*feathercol);
    expgradcol->add(*gradcolBox, false);
    pack_start(*expgradcol, false, false);
    ToolParamBlock* const curvBox = Gtk::manage(new ToolParamBlock());
    Gtk::Box* const qualcurvbox = Gtk::manage(new Gtk::Box());
    qualcurvbox->pack_start(*labqualcurv, Gtk::PACK_SHRINK, 4);
    qualcurvbox->pack_start(*qualitycurveMethod);
    curvBox->pack_start(*qualcurvbox);
    curvBox->pack_start(*llCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*clCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*HCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*H3CurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*H2CurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*rgbCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    curvBox->pack_start(*special);
    expcurvcol->add(*curvBox, false);
    pack_start(*expcurvcol, false, false);
    ToolParamBlock* const mask7Box = Gtk::manage(new ToolParamBlock());
    mask7Box->pack_start(*merMethod);
    Gtk::Frame* const merge1colFrame = Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_MERGE1COLFRA")));
    merge1colFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const mergecolBox = Gtk::manage(new ToolParamBlock());
    Gtk::Separator* const separatormer = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    mergecolBox->pack_start(*separatormer, Gtk::PACK_SHRINK, 2);
    mergecolBox->pack_start(*mergecolMethod);
    mergecolBox->pack_start(*mercol);
    mergecolBox->pack_start(*opacol);
    mergecolBox->pack_start(*conthrcol);
    ToolParamBlock* const gridmerBox = Gtk::manage(new ToolParamBlock());
    gridmerFrame->set_label_align(0.025, 0.5);
    gridmerBox->pack_start(*labgridmerg);
    gridmerBox->pack_start(*merlucol);
    gridmerFrame->add(*gridmerBox);
    mergecolBox->pack_start(*gridmerFrame);
    merge1colFrame->add(*mergecolBox);
    mask7->pack_start(*merge1colFrame);
    mask7Box->pack_start(*mask7);
    expmaskcol1->add(*mask7Box, false);
    pack_start(*expmaskcol1, false, false);
    mergecolFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const maskcolBox = Gtk::manage(new ToolParamBlock());
    maskcolBox->pack_start(*showmaskcolMethod, Gtk::PACK_SHRINK, 4);
    maskcolBox->pack_start(*showmaskcolMethodinv, Gtk::PACK_SHRINK, 4);
    maskcolBox->pack_start(*enaColorMask, Gtk::PACK_SHRINK, 0);
    maskcolBox->pack_start(*maskCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    ToolParamBlock* const strumBox = Gtk::manage(new ToolParamBlock());
    strumBox->pack_start(*strumaskcol);
    strumBox->pack_start(*toolcol);
    struFrame->add(*strumBox);
    maskcolBox->pack_start(*struFrame, Gtk::PACK_SHRINK, 0);
    ToolParamBlock* const blurmBox = Gtk::manage(new ToolParamBlock());
    blurmBox->pack_start(*fftColorMask, Gtk::PACK_SHRINK, 0);
    blurmBox->pack_start(*contcol);
    blurmBox->pack_start(*blurcol);
    blurFrame->add(*blurmBox);
    maskcolBox->pack_start(*blurFrame, Gtk::PACK_SHRINK, 0);
    maskcolBox->pack_start(*blendmaskcol, Gtk::PACK_SHRINK, 0);
//    Gtk::Frame* const toolcolFrame = Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_TOOLMASK")));
    toolcolFrame->set_label_align(0.025, 0.5);
    toolcolFrame2->set_label_align(0.025, 0.5);
    ToolParamBlock* const toolcolBox = Gtk::manage(new ToolParamBlock());
    toolcolBox->pack_start(*radmaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*lapmaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*chromaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*gammaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*slomaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*shadmaskcol, Gtk::PACK_SHRINK, 0);
    toolcolBox->pack_start(*maskHCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    toolcolBox->pack_start(*mask2CurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    ToolParamBlock* const toolcolBox2 = Gtk::manage(new ToolParamBlock());
    toolcolBox2->pack_start(*mask2CurveEditorGwav, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    toolcolBox2->pack_start(*csThresholdcol, Gtk::PACK_SHRINK, 0);
    toolcolFrame2->add(*toolcolBox2);
    toolcolBox->pack_start(*toolcolFrame2);
    toolcolFrame->add(*toolcolBox);
    maskcolBox->pack_start(*toolcolFrame);
    mergecolFrame->add(*maskcolBox);
    expmaskcol->add(*mergecolFrame, false);
    pack_start(*expmaskcol, false, false);
}

LocallabColor::~LocallabColor()
{
    delete llCurveEditorG;
    delete clCurveEditorG;
    delete HCurveEditorG;
    delete H3CurveEditorG;
    delete H2CurveEditorG;
    delete rgbCurveEditorG;
    delete maskCurveEditorG;
    delete maskHCurveEditorG;
    delete mask2CurveEditorG;
    delete mask2CurveEditorGwav;
}

void LocallabColor::previewcolChanged()
{
   
    if(previewcol->get_active()) {
        showmaskcolMethod->set_active(5);
    } else {
        showmaskcolMethod->set_active(0);
    }
    
    if (isLocActivated) {
        if (listener) {
            listener->panelChanged(Evlocallabpreviewcol,"");
        }
    } 
}

void LocallabColor::setListener(ToolPanelListener* tpl)
{
    LocallabTool::setListener(tpl);

    labgrid->setListener(tpl);
    labgridmerg->setListener(tpl);
}

//new function Global
void LocallabColor::updateguicolor(int spottype)
{
    {
    // Disable all listeners
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                invers->hide();
                sensi->hide();
                expmaskcol->hide();
                exprecov->hide();
                enaColorMask->set_active(false);
                previewcol->hide();
                previewcol->set_active(false);
                blurcolde->hide();
            } else {
                invers->show();
                sensi->show();
                expmaskcol->show();
                exprecov->show();
                if(!invers->get_active()) {
                    previewcol->show();
                } else {
                    previewcol->hide();
                }
                updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

            }
            enableListener();
        return false;
        }
        );
    }
   
}

//new function scope
void LocallabColor::updateguiscopecolor(int scope)
{
    {
        idle_register.add(
        [this, scope]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            disableListener();
            sensi->setValue(scope);
            enableListener();

        return false;
        }
        );
    }
   
}


bool LocallabColor::isMaskViewActive()
{
    return ((showmaskcolMethod->get_active_row_number() != 0) || (showmaskcolMethodinv->get_active_row_number() != 0));
}

void LocallabColor::resetMaskView()
{
    showmaskcolMethodConn.block(true);
    showmaskcolMethodConninv.block(true);

    showmaskcolMethod->set_active(0);
    showmaskcolMethodinv->set_active(0);

    showmaskcolMethodConn.block(false);
    showmaskcolMethodConninv.block(false);
}

void LocallabColor::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    colorMask = showmaskcolMethod->get_active_row_number();
    colorMaskinv = showmaskcolMethodinv->get_active_row_number();
}

Gtk::ToggleButton *LocallabColor::getPreviewDeltaEButton() const
{
    return previewcol;
}

sigc::connection *LocallabColor::getPreviewDeltaEButtonConnection()
{
    return &previewcolConn;
}
 
void LocallabColor::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        lumFrame->set_tooltip_text(M("TP_LOCALLAB_EXPCOLOR_TOOLTIP"));
        recothresc->set_tooltip_text(M("TP_LOCALLAB_RECOTHRES02_TOOLTIP"));
        gamc->set_tooltip_text(M("TP_LOCALLAB_GAMCOL_TOOLTIP"));
        lightness->set_tooltip_text(M("TP_LOCALLAB_LIGHTN_TOOLTIP"));
        reparcol->set_tooltip_text(M("TP_LOCALLAB_REPARCOL_TOOLTIP"));
        gridMethod->set_tooltip_text(M("TP_LOCALLAB_GRIDMETH_TOOLTIP"));
        strengthgrid->set_tooltip_text(M("TP_LOCALLAB_STRENGRID_TOOLTIP"));
        blurcolde->set_tooltip_text(M("TP_LOCALLAB_BLURCOLDE_TOOLTIP"));
        softradiuscol->set_tooltip_text(M("TP_LOCALLAB_SOFTRADIUSCOL_TOOLTIP"));
        exprecov->set_tooltip_markup(M("TP_LOCALLAB_MASKRECOL_TOOLTIP"));
        expgradcol->set_tooltip_text(M("TP_LOCALLAB_EXPGRADCOL_TOOLTIP"));
        rgbCurveEditorG->set_tooltip_text(M("TP_LOCALLAB_RGBCURVE_TOOLTIP"));
        sensi->set_tooltip_text(M("TP_LOCALLAB_SENSI_TOOLTIP"));
        structcol->set_tooltip_text(M("TP_LOCALLAB_STRUCT_TOOLTIP"));
        strcol->set_tooltip_text(M("TP_LOCALLAB_GRADGEN_TOOLTIP"));
        angcol->set_tooltip_text(M("TP_LOCALLAB_GRADANG_TOOLTIP"));
        qualitycurveMethod->set_tooltip_markup(M("TP_LOCALLAB_CURVEMETHOD_TOOLTIP"));
        special->set_tooltip_text(M("TP_LOCALLAB_SPECIAL_TOOLTIP"));
        expmaskcol1->set_tooltip_text(M("TP_LOCALLAB_EXPMERGEFILE_TOOLTIP"));
        mercol->set_tooltip_text(M("TP_LOCALLAB_MERGEMER_TOOLTIP"));
        opacol->set_tooltip_text(M("TP_LOCALLAB_MERGEOPA_TOOLTIP"));
        conthrcol->set_tooltip_text(M("TP_LOCALLAB_MERGEOPA_TOOLTIP"));
        gridmerFrame->set_tooltip_text(M("TP_LOCALLAB_GRIDFRAME_TOOLTIP"));
        expmaskcol->set_tooltip_markup(M("TP_LOCALLAB_MASK_TOOLTIP"));
        CCmaskshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        LLmaskshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        HHmaskshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        struFrame->set_tooltip_text(M("TP_LOCALLAB_STRUMASK_TOOLTIP"));
        blurFrame->set_tooltip_text(M("TP_LOCALLAB_BLURMASK_TOOLTIP"));
        blendmaskcol->set_tooltip_text(M("TP_LOCALLAB_BLENDMASK_TOOLTIP"));
        radmaskcol->set_tooltip_text(M("TP_LOCALLAB_LAPRAD_TOOLTIP"));
        maskHCurveEditorG->set_tooltip_text(M("TP_LOCALLAB_HHMASK_TOOLTIP"));
        mask2CurveEditorG->set_tooltip_text(M("TP_LOCALLAB_CONTRASTCURVMASK_TOOLTIP"));
        Lmaskshape->setTooltip(M("TP_LOCALLAB_LMASK_LL_TOOLTIP"));
        mask2CurveEditorGwav->set_tooltip_text(M("TP_LOCALLAB_WAVMASK_TOOLTIP"));
        LLmaskcolshapewav->setTooltip(M("TP_LOCALLAB_LMASK_LEVEL_TOOLTIP"));
        mergecolFrame->set_tooltip_markup(M("TP_LOCALLAB_MERGECOLFRMASK_TOOLTIP"));
        maskCurveEditorG->set_tooltip_markup(M("TP_LOCALLAB_MASKCURVE_TOOLTIP"));
        strumaskcol->set_tooltip_text(M("TP_LOCALLAB_STRUSTRMASK_TOOLTIP"));
        toolcol->set_tooltip_text(M("TP_LOCALLAB_TOOLMASK_TOOLTIP"));
        toolcolFrame->set_tooltip_text(M("TP_LOCALLAB_TOOLCOLFRMASK_TOOLTIP"));
        fftColorMask->set_tooltip_text(M("TP_LOCALLAB_FFTMASK_TOOLTIP"));
        gammaskcol->set_tooltip_text(M("TP_LOCALLAB_GAMMASK_TOOLTIP"));
        chromaskcol->set_tooltip_text(M("TP_LOCALLAB_CHROMASK_TOOLTIP"));
        slomaskcol->set_tooltip_text(M("TP_LOCALLAB_SLOMASK_TOOLTIP"));
        shadmaskcol->set_tooltip_text(M("TP_LOCALLAB_SHADMASK_TOOLTIP"));
        contcol->set_tooltip_text(M("TP_LOCALLAB_CONTTHMASK_TOOLTIP"));
        blurcol->set_tooltip_text(M("TP_LOCALLAB_BLURRMASK_TOOLTIP"));
        lapmaskcol->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
        csThresholdcol->set_tooltip_text(M("TP_LOCALLAB_WAVEMASK_LEVEL_TOOLTIP"));
        decayc->set_tooltip_text(M("TP_LOCALLAB_MASKDECAY_TOOLTIP"));
        lowthresc->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRESC_TOOLTIP"));
        higthresc->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRESC_TOOLTIP"));
    } else {
        lumFrame->set_tooltip_text("");
        recothresc->set_tooltip_text("");
        lightness->set_tooltip_text("");
        gamc->set_tooltip_text("");
        reparcol->set_tooltip_text("");
        gridMethod->set_tooltip_text("");
        strengthgrid->set_tooltip_text("");
        blurcolde->set_tooltip_text("");
        softradiuscol->set_tooltip_text("");
        exprecov->set_tooltip_markup("");
        expgradcol->set_tooltip_text("");
        rgbCurveEditorG->set_tooltip_text("");
        sensi->set_tooltip_text("");
        structcol->set_tooltip_text("");
        strcol->set_tooltip_text("");
        angcol->set_tooltip_text("");
        qualitycurveMethod->set_tooltip_markup("");
        special->set_tooltip_text("");
        expmaskcol1->set_tooltip_text("");
        maskCurveEditorG->set_tooltip_markup("");
        mercol->set_tooltip_text("");
        opacol->set_tooltip_text("");
        conthrcol->set_tooltip_text("");
        gridmerFrame->set_tooltip_text("");
        expmaskcol->set_tooltip_markup("");
        mergecolFrame->set_tooltip_markup("");
        CCmaskshape->setTooltip("");
        LLmaskshape->setTooltip("");
        HHmaskshape->setTooltip("");
        struFrame->set_tooltip_text("");
        strumaskcol->set_tooltip_text("");
        toolcol->set_tooltip_text("");
        toolcolFrame->set_tooltip_text("");
        fftColorMask->set_tooltip_text("");
        gammaskcol->set_tooltip_text("");
        chromaskcol->set_tooltip_text("");
        slomaskcol->set_tooltip_text("");
        shadmaskcol->set_tooltip_text("");
        contcol->set_tooltip_text("");
        blurcol->set_tooltip_text("");
        blurFrame->set_tooltip_text("");
        blendmaskcol->set_tooltip_text("");
        radmaskcol->set_tooltip_text("");
        lapmaskcol->set_tooltip_text("");
        maskHCurveEditorG->set_tooltip_text("");
        mask2CurveEditorG->set_tooltip_text("");
        Lmaskshape->setTooltip("");
        mask2CurveEditorGwav->set_tooltip_text("");
        LLmaskcolshapewav->setTooltip("");
        csThresholdcol->set_tooltip_text("");
        decayc->set_tooltip_text("");
        lowthresc->set_tooltip_text("");
        higthresc->set_tooltip_text("");
    }
}

void LocallabColor::setDefaultExpanderVisibility()
{
    exprecov->set_expanded(false);
    expgradcol->set_expanded(false);
    expcurvcol->set_expanded(false);
    expmaskcol1->set_expanded(false);
    expmaskcol->set_expanded(false);
}

void LocallabColor::disableListener()
{
    LocallabTool::disableListener();

    curvactivConn.block(true);
    gridMethodConn.block(true);
    inversConn.block(true);
    qualitycurveMethodConn.block(true);
    toneMethodConn.block(true);
    specialConn.block(true);
    merMethodConn.block(true);
    mergecolMethodConn.block(true);
    showmaskcolMethodConn.block(true);
    showmaskcolMethodConninv.block(true);
    enaColorMaskConn.block(true);
    toolcolConn.block(true);
    fftColorMaskConn.block(true);
}

void LocallabColor::enableListener()
{
    LocallabTool::enableListener();

    curvactivConn.block(false);
    gridMethodConn.block(false);
    inversConn.block(false);
    qualitycurveMethodConn.block(false);
    toneMethodConn.block(false);
    specialConn.block(false);
    merMethodConn.block(false);
    mergecolMethodConn.block(false);
    showmaskcolMethodConn.block(false);
    showmaskcolMethodConninv.block(false);
    enaColorMaskConn.block(false);
    toolcolConn.block(false);
    fftColorMaskConn.block(false);
}

void LocallabColor::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();
    nbmaskcol = 0;
    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visicolor);
        exp->setEnabled(spot.expcolor);
        complexity->set_active(spot.complexcolor);

        lightness->setValue(spot.lightness);
        gamc->setValue(spot.gamc);
        reparcol->setValue(spot.reparcol);
        contrast->setValue(spot.contrast);
        chroma->setValue(spot.chroma);
        curvactiv->set_active(spot.curvactiv);
        labgrid->setParams(spot.labgridALow / LocallabParams::LABGRIDL_CORR_MAX,
                           spot.labgridBLow / LocallabParams::LABGRIDL_CORR_MAX,
                           spot.labgridAHigh / LocallabParams::LABGRIDL_CORR_MAX,
                           spot.labgridBHigh / LocallabParams::LABGRIDL_CORR_MAX,
                           0, 0, 0, 0, 0, 0, false);
       // printf("labgridlow=%f \n", spot.labgridALow);
        if (spot.gridMethod == "one") {
            gridMethod->set_active(0);
        } else if (spot.gridMethod == "two") {
            gridMethod->set_active(1);
        }

        strengthgrid->setValue(spot.strengthgrid);
        sensi->setValue(spot.sensi);
        structcol->setValue(spot.structcol);
        blurcolde->setValue(spot.blurcolde);
        softradiuscol->setValue(spot.softradiuscol);
        invers->set_active(spot.invers);
        strcol->setValue(spot.strcol);
        strcolab->setValue(spot.strcolab);
        strcolh->setValue(spot.strcolh);
        angcol->setValue(spot.angcol);
        feathercol->setValue(spot.feathercol);

        if (spot.qualitycurveMethod == "none") {
            qualitycurveMethod->set_active(0);
        } else if (spot.qualitycurveMethod == "std") {
            qualitycurveMethod->set_active(1);
        }

        llshape->setCurve(spot.llcurve);
        ccshape->setCurve(spot.cccurve);
        clshape->setCurve(spot.clcurve);
        lcshape->setCurve(spot.lccurve);
        LHshape->setCurve(spot.LHcurve);
        HHshape->setCurve(spot.HHcurve);
        CHshape->setCurve(spot.CHcurve);

        if (spot.toneMethod == "one") {
            toneMethod->set_active(0);
        } else if (spot.toneMethod == "two") {
            toneMethod->set_active(1);
        } else if (spot.toneMethod == "thr") {
            toneMethod->set_active(2);
        } else if (spot.toneMethod == "fou") {
            toneMethod->set_active(3);
        }

        rgbshape->setCurve(spot.rgbcurve);
        special->set_active(spot.special);

        if (spot.merMethod == "mone") {
            merMethod->set_active(0);
            // } else if (spot.merMethod == "mtwo") {
            //     merMethod->set_active(1);
        } else if (spot.merMethod == "mthr") {
            merMethod->set_active(1);
        } else if (spot.merMethod == "mfou") {
            merMethod->set_active(2);
        } else if (spot.merMethod == "mfiv") {
            merMethod->set_active(3);
        }

        recothresc->setValue((double)spot.recothresc);
        lowthresc->setValue((double)spot.lowthresc);
        higthresc->setValue((double)spot.higthresc);
        decayc->setValue((double)spot.decayc);

        if (spot.mergecolMethod == "one") {
            mergecolMethod->set_active(0);
        } else if (spot.mergecolMethod == "two") {
            mergecolMethod->set_active(1);
        } else if (spot.mergecolMethod == "thr") {
            mergecolMethod->set_active(2);
        } else if (spot.mergecolMethod == "fou") {
            mergecolMethod->set_active(3);
        } else if (spot.mergecolMethod == "fiv") {
            mergecolMethod->set_active(4);
        } else if (spot.mergecolMethod == "six") {
            mergecolMethod->set_active(5);
        } else if (spot.mergecolMethod == "sev") {
            mergecolMethod->set_active(6);
        } else if (spot.mergecolMethod == "sev0") {
            mergecolMethod->set_active(7);
        } else if (spot.mergecolMethod == "sev1") {
            mergecolMethod->set_active(8);
        } else if (spot.mergecolMethod == "sev2") {
            mergecolMethod->set_active(9);
        } else if (spot.mergecolMethod == "hei") {
            mergecolMethod->set_active(10);
        } else if (spot.mergecolMethod == "nin") {
            mergecolMethod->set_active(11);
        } else if (spot.mergecolMethod == "ten") {
            mergecolMethod->set_active(12);
        } else if (spot.mergecolMethod == "ele") {
            mergecolMethod->set_active(13);
        } else if (spot.mergecolMethod == "twe") {
            mergecolMethod->set_active(14);
        } else if (spot.mergecolMethod == "thi") {
            mergecolMethod->set_active(15);
        } else if (spot.mergecolMethod == "for") {
            mergecolMethod->set_active(16);
        } else if (spot.mergecolMethod == "hue") {
            mergecolMethod->set_active(17);
        } else if (spot.mergecolMethod == "sat") {
            mergecolMethod->set_active(18);
        } else if (spot.mergecolMethod == "col") {
            mergecolMethod->set_active(19);
        } else if (spot.mergecolMethod == "lum") {
            mergecolMethod->set_active(20);
        }

        mercol->setValue(spot.mercol);
        opacol->setValue(spot.opacol);
        conthrcol->setValue(spot.conthrcol);
        labgridmerg->setParams(0, 0,
                               spot.labgridAHighmerg / LocallabParams::LABGRIDL_CORR_MAX,
                               spot.labgridBHighmerg / LocallabParams::LABGRIDL_CORR_MAX,
                               0, 0, 0, 0, 0, 0, false);
        merlucol->setValue(spot.merlucol);
        enaColorMask->set_active(spot.enaColorMask);
        CCmaskshape->setCurve(spot.CCmaskcurve);
        LLmaskshape->setCurve(spot.LLmaskcurve);
        HHmaskshape->setCurve(spot.HHmaskcurve);
        strumaskcol->setValue(spot.strumaskcol);
        toolcol->set_active(spot.toolcol);
        fftColorMask->set_active(spot.fftColorMask);
        contcol->setValue(spot.contcol);
        // Update GUI according to fftColorMash button state
        // Note: Contrary to the others, shall be called before setting 'blurcol' value
        updateColorGUI3();
        blurcol->setValue(spot.blurcol);
        blendmaskcol->setValue(spot.blendmaskcol);
        radmaskcol->setValue(spot.radmaskcol);
        lapmaskcol->setValue(spot.lapmaskcol);
        chromaskcol->setValue(spot.chromaskcol);
        gammaskcol->setValue(spot.gammaskcol);
        slomaskcol->setValue(spot.slomaskcol);
        shadmaskcol->setValue(spot.shadmaskcol);
        HHhmaskshape->setCurve(spot.HHhmaskcurve);
        Lmaskshape->setCurve(spot.Lmaskcurve);
        LLmaskcolshapewav->setCurve(spot.LLmaskcolcurvewav);
        csThresholdcol->setValue<int>(spot.csthresholdcol);
    }

    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update GUI according to invers button state
    updateColorGUI1();

    // Update GUI according to merMethod combobox state
    updateColorGUI2();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabColor::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expcolor = exp->getEnabled();
        spot.visicolor = exp->get_visible();
        spot.complexcolor = complexity->get_active_row_number();

        spot.lightness = lightness->getIntValue();
        spot.gamc = gamc->getValue();
        spot.reparcol = reparcol->getValue();
        spot.contrast = contrast->getIntValue();
        spot.chroma = chroma->getIntValue();
        spot.curvactiv = curvactiv->get_active();
        double zerox = 0.;
        double zeroy = 0.;
        labgrid->getParams(spot.labgridALow,
                           spot.labgridBLow,
                           spot.labgridAHigh,
                           spot.labgridBHigh, zerox, zeroy, zerox, zeroy, zerox, zeroy);
        spot.labgridALow *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridAHigh *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridBLow *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridBHigh *= LocallabParams::LABGRIDL_CORR_MAX;

        if (gridMethod->get_active_row_number() == 0) {
            spot.gridMethod = "one";
        } else if (gridMethod->get_active_row_number() == 1) {
            spot.gridMethod = "two";
        }

        spot.strengthgrid = strengthgrid->getIntValue();
        spot.sensi = sensi->getIntValue();
        spot.structcol = structcol->getIntValue();
        spot.blurcolde = blurcolde->getIntValue();
        spot.softradiuscol = softradiuscol->getValue();
        spot.invers = invers->get_active();
        spot.strcol = strcol->getValue();
        spot.strcolab = strcolab->getValue();
        spot.strcolh = strcolh->getValue();
        spot.angcol = angcol->getValue();
        spot.feathercol = feathercol->getValue();

        spot.recothresc = recothresc->getValue();
        spot.lowthresc = lowthresc->getValue();
        spot.higthresc = higthresc->getValue();
        spot.decayc = decayc->getValue();

        if (qualitycurveMethod->get_active_row_number() == 0) {
            spot.qualitycurveMethod = "none";
        } else if (qualitycurveMethod->get_active_row_number() == 1) {
            spot.qualitycurveMethod = "std";
        }

        spot.llcurve = llshape->getCurve();
        spot.cccurve = ccshape->getCurve();
        spot.clcurve = clshape->getCurve();
        spot.lccurve = lcshape->getCurve();
        spot.LHcurve = LHshape->getCurve();
        spot.HHcurve = HHshape->getCurve();
        spot.CHcurve = CHshape->getCurve();

        if (toneMethod->get_active_row_number() == 0) {
            spot.toneMethod = "one";
        } else if (toneMethod->get_active_row_number() == 1) {
            spot.toneMethod = "two";
        } else if (toneMethod->get_active_row_number() == 2) {
            spot.toneMethod = "thr";
        } else if (toneMethod->get_active_row_number() == 3) {
            spot.toneMethod = "fou";
        }

        spot.rgbcurve = rgbshape->getCurve();
        spot.special = special->get_active();

        if (merMethod->get_active_row_number() == 0) {
            spot.merMethod = "mone";
            // } else if (merMethod->get_active_row_number() == 1) {
            //     spot.merMethod = "mtwo";
        } else if (merMethod->get_active_row_number() == 1) {
            spot.merMethod = "mthr";
        } else if (merMethod->get_active_row_number() == 2) {
            spot.merMethod = "mfou";
        } else if (merMethod->get_active_row_number() == 3) {
            spot.merMethod = "mfiv";
        }

        if (mergecolMethod->get_active_row_number() == 0) {
            spot.mergecolMethod = "one";
        } else if (mergecolMethod->get_active_row_number() == 1) {
            spot.mergecolMethod = "two";
        } else if (mergecolMethod->get_active_row_number() == 2) {
            spot.mergecolMethod = "thr";
        } else if (mergecolMethod->get_active_row_number() == 3) {
            spot.mergecolMethod = "fou";
        } else if (mergecolMethod->get_active_row_number() == 4) {
            spot.mergecolMethod = "fiv";
        } else if (mergecolMethod->get_active_row_number() == 5) {
            spot.mergecolMethod = "six";
        } else if (mergecolMethod->get_active_row_number() == 6) {
            spot.mergecolMethod = "sev";
        } else if (mergecolMethod->get_active_row_number() == 7) {
            spot.mergecolMethod = "sev0";
        } else if (mergecolMethod->get_active_row_number() == 8) {
            spot.mergecolMethod = "sev1";
        } else if (mergecolMethod->get_active_row_number() == 9) {
            spot.mergecolMethod = "sev2";
        } else if (mergecolMethod->get_active_row_number() == 10) {
            spot.mergecolMethod = "hei";
        } else if (mergecolMethod->get_active_row_number() == 11) {
            spot.mergecolMethod = "nin";
        } else if (mergecolMethod->get_active_row_number() == 12) {
            spot.mergecolMethod = "ten";
        } else if (mergecolMethod->get_active_row_number() == 13) {
            spot.mergecolMethod = "ele";
        } else if (mergecolMethod->get_active_row_number() == 14) {
            spot.mergecolMethod = "twe";
        } else if (mergecolMethod->get_active_row_number() == 15) {
            spot.mergecolMethod = "thi";
        } else if (mergecolMethod->get_active_row_number() == 16) {
            spot.mergecolMethod = "for";
        } else if (mergecolMethod->get_active_row_number() == 17) {
            spot.mergecolMethod = "hue";
        } else if (mergecolMethod->get_active_row_number() == 18) {
            spot.mergecolMethod = "sat";
        } else if (mergecolMethod->get_active_row_number() == 19) {
            spot.mergecolMethod = "col";
        } else if (mergecolMethod->get_active_row_number() == 20) {
            spot.mergecolMethod = "lum";
        }

        spot.mercol = mercol->getValue();
        spot.opacol = opacol->getValue();
        spot.conthrcol = conthrcol->getValue();
        double zerox1 = 0.;
        double zeroy1 = 0.;
        labgridmerg->getParams(spot.labgridALowmerg,
                               spot.labgridBLowmerg,
                               spot.labgridAHighmerg,
                               spot.labgridBHighmerg, zerox1, zeroy1, zerox1, zeroy1, zerox1, zeroy1);
        spot.labgridALowmerg *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridAHighmerg *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridBLowmerg *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.labgridBHighmerg *= LocallabParams::LABGRIDL_CORR_MAX;
        spot.merlucol = merlucol->getValue();
        spot.enaColorMask = enaColorMask->get_active();
        spot.CCmaskcurve = CCmaskshape->getCurve();
        spot.LLmaskcurve = LLmaskshape->getCurve();
        spot.HHmaskcurve = HHmaskshape->getCurve();
        spot.strumaskcol = strumaskcol->getValue();
        spot.toolcol = toolcol->get_active();
        spot.fftColorMask = fftColorMask->get_active();
        spot.contcol = contcol->getValue();
        spot.blurcol = blurcol->getValue();
        spot.blendmaskcol = blendmaskcol->getIntValue();
        spot.radmaskcol = radmaskcol->getValue();
        spot.lapmaskcol = lapmaskcol->getValue();
        spot.chromaskcol = chromaskcol->getValue();
        spot.gammaskcol = gammaskcol->getValue();
        spot.slomaskcol = slomaskcol->getValue();
        spot.shadmaskcol = shadmaskcol->getIntValue();
        spot.HHhmaskcurve = HHhmaskshape->getCurve();
        spot.Lmaskcurve = Lmaskshape->getCurve();
        spot.LLmaskcolcurvewav = LLmaskcolshapewav->getCurve();
        spot.csthresholdcol = csThresholdcol->getValue<int>();
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabColor::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default value for adjuster, labgrid and threshold adjuster widgets
        lightness->setDefault((double)defSpot.lightness);
        gamc->setDefault((double)defSpot.gamc);
        reparcol->setDefault(defSpot.reparcol);
        contrast->setDefault((double)defSpot.contrast);
        chroma->setDefault((double)defSpot.chroma);
        labgrid->setDefault(defSpot.labgridALow / LocallabParams::LABGRIDL_CORR_MAX,
                            defSpot.labgridBLow / LocallabParams::LABGRIDL_CORR_MAX,
                            defSpot.labgridAHigh / LocallabParams::LABGRIDL_CORR_MAX,
                            defSpot.labgridBHigh / LocallabParams::LABGRIDL_CORR_MAX, 0, 0, 0, 0, 0, 0);
        strengthgrid->setDefault((double) defSpot.strengthgrid);
        sensi->setDefault((double)defSpot.sensi);
        structcol->setDefault((double)defSpot.structcol);
        blurcolde->setDefault((double)defSpot.blurcolde);
        softradiuscol->setDefault(defSpot.softradiuscol);
        strcol->setDefault(defSpot.strcol);
        strcolab->setDefault(defSpot.strcolab);
        strcolh->setDefault(defSpot.strcolh);
        angcol->setDefault(defSpot.angcol);
        feathercol->setDefault(defSpot.feathercol);
        mercol->setDefault(defSpot.mercol);
        opacol->setDefault(defSpot.opacol);
        conthrcol->setDefault(defSpot.conthrcol);
        labgridmerg->setDefault(defSpot.labgridALowmerg / LocallabParams::LABGRIDL_CORR_MAX,
                                defSpot.labgridBLowmerg / LocallabParams::LABGRIDL_CORR_MAX,
                                defSpot.labgridAHighmerg / LocallabParams::LABGRIDL_CORR_MAX,
                                defSpot.labgridBHighmerg / LocallabParams::LABGRIDL_CORR_MAX, 0, 0, 0, 0, 0, 0);
        merlucol->setDefault(defSpot.merlucol);
        strumaskcol->setDefault(defSpot.strumaskcol);
        contcol->setDefault(defSpot.contcol);
        blurcol->setDefault(defSpot.blurcol);
        blendmaskcol->setDefault((double)defSpot.blendmaskcol);
        radmaskcol->setDefault(defSpot.radmaskcol);
        lapmaskcol->setDefault(defSpot.lapmaskcol);
        chromaskcol->setDefault(defSpot.chromaskcol);
        gammaskcol->setDefault(defSpot.gammaskcol);
        slomaskcol->setDefault(defSpot.slomaskcol);
        shadmaskcol->setDefault((double)defSpot.shadmaskcol);
        csThresholdcol->setDefault<int>(defSpot.csthresholdcol);
        recothresc->setDefault((double)defSpot.recothresc);
        lowthresc->setDefault((double)defSpot.lowthresc);
        higthresc->setDefault((double)defSpot.higthresc);
        decayc->setDefault((double)defSpot.decayc);

    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabColor::adjusterChanged(Adjuster* a, double newval)
{
    if (isLocActivated && exp->getEnabled()) {
        if (a == lightness) {      
            if (listener) {
                listener->panelChanged(Evlocallablightness,
                                       lightness->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gamc) {
            if (listener) {
                listener->panelChanged(Evlocallabgamc,
                                       gamc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == reparcol) {
            if (listener) {
                listener->panelChanged(Evlocallabreparcol,
                                       reparcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == contrast) {
            if (listener) {
                listener->panelChanged(Evlocallabcontrast,
                                       contrast->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chroma) {
            if (listener) {
                listener->panelChanged(Evlocallabchroma,
                                       chroma->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strengthgrid) {
            if (listener) {
                listener->panelChanged(EvLocallabLabstrengthgrid,
                                       strengthgrid->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensi) {
            if (listener) {
                listener->panelChanged(Evlocallabsensi,
                                       sensi->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");                                      
            }
        }

        if (a == structcol) {
            if (listener) {
                listener->panelChanged(Evlocallabstructcol,
                                       structcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blurcolde) {
            if (listener) {
                listener->panelChanged(Evlocallabblurcolde,
                                       blurcolde->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == softradiuscol) {
            if (listener) {
                listener->panelChanged(Evlocallabsoftradiuscol,
                                       softradiuscol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothresc) {

            if (listener) {
                listener->panelChanged(Evlocallabrecothresc,
                                       recothresc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthresc) {
            if (listener) {
                listener->panelChanged(Evlocallablowthresc,
                                       lowthresc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthresc) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthresc,
                                       higthresc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == decayc) {
            if (listener) {
                listener->panelChanged(Evlocallabdecayc,
                                       decayc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }


        if (a == strcol) {
            if (listener) {
                listener->panelChanged(Evlocallabstrcol,
                                       strcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strcolab) {
            if (listener) {
                listener->panelChanged(Evlocallabstrcolab,
                                       strcolab->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strcolh) {
            if (listener) {
                listener->panelChanged(Evlocallabstrcolh,
                                       strcolh->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == angcol) {
            if (listener) {
                listener->panelChanged(Evlocallabangcol,
                                       angcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == feathercol) {
            if (listener) {
                listener->panelChanged(Evlocallabfeathercol,
                                       feathercol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mercol) {
            if (listener) {
                listener->panelChanged(Evlocallabmercol,
                                       mercol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == opacol) {
            if (listener) {
                listener->panelChanged(Evlocallabopacol,
                                       opacol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == conthrcol) {
            if (listener) {
                listener->panelChanged(Evlocallabconthrcol,
                                       conthrcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == merlucol) {
            if (listener) {
                listener->panelChanged(Evlocallabmerlucol,
                                       merlucol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strumaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabstrumaskcol,
                                       strumaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == contcol) {
            if (listener) {
                listener->panelChanged(Evlocallabcontcol,
                                       contcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blurcol) {
            if (listener) {
                listener->panelChanged(Evlocallabblurcol,
                                       blurcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blendmaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabblendmaskcol,
                                       blendmaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == radmaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabradmaskcol,
                                       radmaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lapmaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallablapmaskcol,
                                       lapmaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chromaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabchromaskcol,
                                       chromaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gammaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabgammaskcol,
                                       gammaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == slomaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabslomaskcol,
                                       slomaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shadmaskcol) {
            if (listener) {
                listener->panelChanged(Evlocallabshadmaskcol,
                                       shadmaskcol->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR)
{
    if (isLocActivated && exp->getEnabled()) {
        if (a == csThresholdcol) {
            if (listener) {
                listener->panelChanged(EvlocallabcsThresholdcol,
                                       csThresholdcol->getHistoryString() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::curveChanged(CurveEditor* ce)
{
    if (isLocActivated && exp->getEnabled()) {
        if (ce == llshape) {
            if (listener) {
                listener->panelChanged(Evlocallabllshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == ccshape) {
            if (listener) {
                listener->panelChanged(Evlocallabccshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == clshape) {
            if (listener) {
                listener->panelChanged(Evlocallabclshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == lcshape) {
            if (listener) {
                listener->panelChanged(Evlocallablcshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == CHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == rgbshape) {
            if (listener) {
                listener->panelChanged(Evlocallabrgbshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == CCmaskshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCCmaskshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHmaskshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHmaskshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHhmaskshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHhmaskshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == Lmaskshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLmaskshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskcolshapewav) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskcolshapewav,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenacolor,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenacolor,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    gamc->setValue(defSpot.gamc);

    // Set hidden GUI widgets in Normal mode to default spot values
    blurcolde->setValue((double)defSpot.blurcolde);
    structcol->setValue((double)defSpot.structcol);
    strcolab->setValue(defSpot.strcolab);
    strcolh->setValue(defSpot.strcolh);
    clshape->setCurve(defSpot.clcurve);
    lcshape->setCurve(defSpot.lccurve);
    LHshape->setCurve(defSpot.LHcurve);
    CHshape->setCurve(defSpot.CHcurve);
    HHshape->setCurve(defSpot.HHcurve);

    if (defSpot.toneMethod == "one") {
        toneMethod->set_active(0);
    } else if (defSpot.toneMethod == "two") {
        toneMethod->set_active(1);
    } else if (defSpot.toneMethod == "thr") {
        toneMethod->set_active(2);
    } else if (defSpot.toneMethod == "fou") {
        toneMethod->set_active(3);
    }

    rgbshape->setCurve(defSpot.rgbcurve);
    special->set_active(defSpot.special);

    if (defSpot.mergecolMethod == "one") {
        mergecolMethod->set_active(0);
    } else if (defSpot.mergecolMethod == "two") {
        mergecolMethod->set_active(1);
    } else if (defSpot.mergecolMethod == "thr") {
        mergecolMethod->set_active(2);
    } else if (defSpot.mergecolMethod == "fou") {
        mergecolMethod->set_active(3);
    } else if (defSpot.mergecolMethod == "fiv") {
        mergecolMethod->set_active(4);
    } else if (defSpot.mergecolMethod == "six") {
        mergecolMethod->set_active(5);
    } else if (defSpot.mergecolMethod == "sev") {
        mergecolMethod->set_active(6);
    } else if (defSpot.mergecolMethod == "sev0") {
        mergecolMethod->set_active(7);
    } else if (defSpot.mergecolMethod == "sev1") {
        mergecolMethod->set_active(8);
    } else if (defSpot.mergecolMethod == "sev2") {
        mergecolMethod->set_active(9);
    } else if (defSpot.mergecolMethod == "hei") {
        mergecolMethod->set_active(10);
    } else if (defSpot.mergecolMethod == "nin") {
        mergecolMethod->set_active(11);
    } else if (defSpot.mergecolMethod == "ten") {
        mergecolMethod->set_active(12);
    } else if (defSpot.mergecolMethod == "ele") {
        mergecolMethod->set_active(13);
    } else if (defSpot.mergecolMethod == "twe") {
        mergecolMethod->set_active(14);
    } else if (defSpot.mergecolMethod == "thi") {
        mergecolMethod->set_active(15);
    } else if (defSpot.mergecolMethod == "for") {
        mergecolMethod->set_active(16);
    } else if (defSpot.mergecolMethod == "hue") {
        mergecolMethod->set_active(17);
    } else if (defSpot.mergecolMethod == "sat") {
        mergecolMethod->set_active(18);
    } else if (defSpot.mergecolMethod == "col") {
        mergecolMethod->set_active(19);
    } else if (defSpot.mergecolMethod == "lum") {
        mergecolMethod->set_active(20);
    }

    mercol->setValue(defSpot.mercol);
    opacol->setValue(defSpot.opacol);
    conthrcol->setValue(defSpot.conthrcol);
    labgridmerg->setParams(0, 0,
                           defSpot.labgridAHighmerg / LocallabParams::LABGRIDL_CORR_MAX,
                           defSpot.labgridBHighmerg / LocallabParams::LABGRIDL_CORR_MAX,
                           0, 0, 0, 0, 0, 0, false);
    merlucol->setValue(defSpot.merlucol);
    strumaskcol->setValue(defSpot.strumaskcol);
    toolcol->set_active(defSpot.toolcol);
    fftColorMask->set_active(defSpot.fftColorMask);
    contcol->setValue(defSpot.contcol);
    blurcol->setValue(defSpot.blurcol);
    lapmaskcol->setValue(defSpot.lapmaskcol);
    gammaskcol->setValue(defSpot.gammaskcol);
    slomaskcol->setValue(defSpot.slomaskcol);
    shadmaskcol->setValue((double)defSpot.shadmaskcol);
    HHhmaskshape->setCurve(defSpot.HHhmaskcurve);
    LLmaskcolshapewav->setCurve(defSpot.LLmaskcolcurvewav);
    csThresholdcol->setValue<int>(defSpot.csthresholdcol);
    decayc->setValue(defSpot.decayc);

    // Enable all listeners
    enableListener();

    // Update GUI based on converted widget parameters:
    // - Update GUI according to merMethod combobox state
    updateColorGUI2();
    // - Update GUI according to fftColorMash button state
    updateColorGUI3();
}

void LocallabColor::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();

    // Set hidden specific GUI widgets in Simple mode to default spot values
    softradiuscol->setValue(defSpot.softradiuscol);
  //  strcol->setValue(defSpot.strcol);
  //  angcol->setValue(defSpot.angcol);
  //  feathercol->setValue(defSpot.feathercol);
    gamc->setValue(defSpot.gamc);

    if (defSpot.qualitycurveMethod == "none") {
        qualitycurveMethod->set_active(0);
    } else if (defSpot.qualitycurveMethod == "std") {
        qualitycurveMethod->set_active(1);
    }
    if (defSpot.merMethod == "mone") {
        merMethod->set_active(0);
        // } else if (defSpot.merMethod == "mtwo") {
        //     merMethod->set_active(1);
    } else if (defSpot.merMethod == "mthr") {
        merMethod->set_active(1);
    } else if (defSpot.merMethod == "mfou") {
        merMethod->set_active(2);
    } else if (defSpot.merMethod == "mfiv") {
        merMethod->set_active(3);
    }

    llshape->setCurve(defSpot.llcurve);
    ccshape->setCurve(defSpot.cccurve);
    showmaskcolMethod->set_active(0);
    showmaskcolMethodinv->set_active(0);
    enaColorMask->set_active(defSpot.enaColorMask);
//    CCmaskshape->setCurve(defSpot.CCmaskcurve);
//    LLmaskshape->setCurve(defSpot.LLmaskcurve);
//    HHmaskshape->setCurve(defSpot.HHmaskcurve);
//    blendmaskcol->setValue((double)defSpot.blendmaskcol);
//    radmaskcol->setValue(defSpot.radmaskcol);
//    chromaskcol->setValue(defSpot.chromaskcol);
//    Lmaskshape->setCurve(defSpot.Lmaskcurve);
    recothresc->setValue(defSpot.recothresc);
    lowthresc->setValue(defSpot.lowthresc);
    higthresc->setValue(defSpot.higthresc);
    decayc->setValue(defSpot.decayc);

    // Enable all listeners
    enableListener();
}

void LocallabColor::updateGUIToMode(const modeType new_type)
{
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            structcol->hide();
            blurcolde->hide();
            softradiuscol->hide();
            expcurvcol->hide();
            expmaskcol1->hide();
            expmaskcol->hide();
            exprecov->hide();
            maskusablec->hide();
            maskunusablec->hide();
            decayc->hide();
            gamc->hide();
            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            structcol->hide();
            gamc->hide();
            blurcolde->hide();
            strcolab->hide();
            strcolh->hide();
            clCurveEditorG->hide();
            HCurveEditorG->hide();
            H3CurveEditorG->hide();
            H2CurveEditorG->hide();
            rgbCurveEditorG->hide();
            special->hide();
            exprecov->show();
           // expmaskcol1->hide();
            struFrame->hide();
            blurFrame->hide();
            lapmaskcol->hide();
            gammaskcol->hide();
            slomaskcol->hide();
            shadmaskcol->hide();
            maskHCurveEditorG->hide();
            mask2CurveEditorGwav->hide();
            csThresholdcol->hide();
            toolcolFrame2->hide();
            // Specific Simple mode widgets are shown in Normal mode
            softradiuscol->show();
            if (enaColorMask->get_active()) {
                maskusablec->show();
                maskunusablec->hide();

            } else {
                maskusablec->hide();
                maskunusablec->show();
            }

            if (!invers->get_active()) { // Keep widget hidden when invers is toggled
                expmaskcol1->show();
                exprecov->show();
                gamc->hide();
            }

            expcurvcol->show();
            expmaskcol->show();
            decayc->hide();

            break;

        case Expert:
            // Show widgets hidden in Normal and Simple mode
            structcol->show();
            blurcolde->show();
            gamc->show();

            if (!invers->get_active()) { // Keep widget hidden when invers is toggled
                softradiuscol->show();
                exprecov->show();
                gamc->show();
            }

            strcolab->show();
            strcolh->show();
            expcurvcol->show();
            if (enaColorMask->get_active()) {
                maskusablec->show();
                maskunusablec->hide();

            } else {
                maskusablec->hide();
                maskunusablec->show();
            }

            exprecov->show();
            decayc->show();

            if (!invers->get_active()) { // Keep widgets hidden when invers is toggled
                clCurveEditorG->show();
                HCurveEditorG->show();
                H3CurveEditorG->show();
            }

            H2CurveEditorG->show();
            rgbCurveEditorG->show();
            special->show();

            if (!invers->get_active()) { // Keep widget hidden when invers is toggled
                expmaskcol1->show();
            }

            expmaskcol->show();
            struFrame->show();
            blurFrame->show();
            lapmaskcol->show();
            gammaskcol->show();
            slomaskcol->show();
            shadmaskcol->show();
            maskHCurveEditorG->show();
            mask2CurveEditorGwav->show();
            csThresholdcol->show();
            toolcolFrame2->show();
    }
}

void LocallabColor::updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz)
{
    idle_register.add(
    [this, normHuer, normLumar, normChromar]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        // Update mask background
        CCmaskshape->updateLocallabBackground(normChromar);
        LLmaskshape->updateLocallabBackground(normLumar);
        HHmaskshape->updateLocallabBackground(normHuer);
        HHhmaskshape->updateLocallabBackground(normHuer);
        Lmaskshape->updateLocallabBackground(normLumar);
        HHshape->updateLocallabBackground(normHuer);
        CHshape->updateLocallabBackground(normHuer);
        LHshape->updateLocallabBackground(normHuer);
        llshape->updateLocallabBackground(normLumar);
        ccshape->updateLocallabBackground(normChromar);
        clshape->updateLocallabBackground(normLumar);
        lcshape->updateLocallabBackground(normChromar);

        return false;
    }
    );
}

void LocallabColor::curvactivChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (curvactiv->get_active()) {
                listener->panelChanged(Evlocallabcurvactiv,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabcurvactiv,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::gridMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvLocallabgridMethod,
                                   gridMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabColor::inversChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update GUI according to invers button state
    updateColorGUI1();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (invers->get_active()) {
                listener->panelChanged(Evlocallabinvers,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinvers,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::qualitycurveMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabqualitycurveMethod,
                                   qualitycurveMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabColor::toneMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvLocallabtoneMethod,
                                   toneMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabColor::specialChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (special->get_active()) {
                listener->panelChanged(EvLocallabspecial,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabspecial,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::merMethodChanged()
{
    updateColorGUI2(); // Update GUI according to merMethod combobox state

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvLocallabmerMethod,
                                   merMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabColor::mergecolMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvLocallabmergecolMethod,
                                   mergecolMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabColor::showmaskcolMethodChanged()
{
    // If mask preview is activated, deactivate other Color & Light mask preview
    showmaskcolMethodConninv.block(true);
    showmaskcolMethodinv->set_active(0);
    showmaskcolMethodConninv.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }
    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabColor::showmaskcolMethodChangedinv()
{
    // If mask preview is activated, deactivate other Color & Light mask preview
    showmaskcolMethodConn.block(true);
    showmaskcolMethod->set_active(0);
    showmaskcolMethodConn.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }
    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabColor::enaColorMaskChanged()
{
    if (enaColorMask->get_active()) {
        maskusablec->show();
        maskunusablec->hide();

    } else {
        maskusablec->hide();
        maskunusablec->show();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enaColorMask->get_active()) {
                listener->panelChanged(EvLocallabEnaColorMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnaColorMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::toolcolChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (toolcol->get_active()) {
                listener->panelChanged(EvLocallabtoolcol,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabtoolcol,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::fftColorMaskChanged()
{
    updateColorGUI3(); // Update GUI according to fftColorMash button state

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (fftColorMask->get_active()) {
                listener->panelChanged(EvLocallabfftColorMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabfftColorMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabColor::updateColorGUI1()
{
    const int mode = complexity->get_active_row_number();

    if (invers->get_active()) {
        gridFrame->hide();
        structcol->hide();
        softradiuscol->hide();
        expgradcol->hide();
        exprecov->hide();
        labqualcurv->hide();
        qualitycurveMethod->hide();
        clCurveEditorG->hide();
        HCurveEditorG->hide();
        H3CurveEditorG->hide();
        expmaskcol1->hide();
        showmaskcolMethod->hide();
        // Reset hidden mask combobox
        showmaskcolMethodConn.block(true);
        showmaskcolMethod->set_active(0);
        showmaskcolMethodConn.block(false);
        showmaskcolMethodinv->show();
        contcol->hide();
        blurcol->hide();
        reparcol->hide();
        gamc->hide();
    } else {
        gridFrame->show();
        gamc->hide();

        if (mode == Expert) { // Keep widget hidden in Normal and Simple mode
            structcol->show();
        }

        if (mode == Expert || mode == Normal) { // Keep widget hidden in Simple mode
            softradiuscol->show();
            exprecov->show();
            expmaskcol1->show();
        }

        labqualcurv->show();
        qualitycurveMethod->show();

        if (mode == Expert) { // Keep widgets hidden in Normal and Simple mode
            clCurveEditorG->show();
            HCurveEditorG->show();
            H3CurveEditorG->show();
            expmaskcol1->show();
            gamc->show();
        }

        showmaskcolMethod->show();
        showmaskcolMethodinv->hide();
        // Reset hidden mask combobox
        showmaskcolMethodConninv.block(true);
        showmaskcolMethodinv->set_active(0);
        showmaskcolMethodConninv.block(false);
        contcol->show();
        blurcol->show();
        reparcol->show();
    }
}

void LocallabColor::updateColorGUI2()
{
    // Note: When a merMethod is selected, invers button is set insensitive to avoid the combobox to disappear
    switch (merMethod->get_active_row_number()) {
        case 0:
            invers->set_sensitive(true);
            H2CurveEditorG->set_sensitive(true);
            rgbCurveEditorG->set_sensitive(true);
            special->set_sensitive(true);
            mask7->hide();
            conthrcol->hide();
            gridmerFrame->hide();
            break;

        /*
                case 1:
                    invers->set_sensitive(false);
                    H2CurveEditorG->set_sensitive(true);
                    rgbCurveEditorG->set_sensitive(true);
                    special->set_sensitive(true);
                    mask7->hide();
                    conthrcol->hide();
                    gridmerFrame->hide();
                    break;
        */
        case 1:
            invers->set_sensitive(false);
            H2CurveEditorG->set_sensitive(true);
            rgbCurveEditorG->set_sensitive(false);
            special->set_sensitive(false);
            mask7->show();
            conthrcol->show();
            gridmerFrame->hide();
            break;

        case 2:
            invers->set_sensitive(false);
            H2CurveEditorG->set_sensitive(true);
            rgbCurveEditorG->set_sensitive(false);
            special->set_sensitive(false);
            mask7->show();
            conthrcol->show();
            gridmerFrame->hide();
            break;

        case 3:
            invers->set_sensitive(false);
            H2CurveEditorG->set_sensitive(true);
            rgbCurveEditorG->set_sensitive(false);
            special->set_sensitive(false);
            mask7->show();
            conthrcol->hide();
            gridmerFrame->show();
    }
}

void LocallabColor::updateColorGUI3()
{
    const double temp = blurcol->getValue();

    if (fftColorMask->get_active()) {
        blurcol->setLimits(0.2, 1000., 0.5, 0.2);
    } else {
        blurcol->setLimits(0.2, 100., 0.5, 0.2);
    }

    blurcol->setValue(temp);
}

/* ==== LocallabExposure ==== */
LocallabExposure::LocallabExposure():
    LocallabTool(this, M("TP_LOCALLAB_EXP_TOOLNAME"), M("TP_LOCALLAB_EXPOSE"), false),

    // Exposure specific widgets
    expMethod(Gtk::manage(new MyComboBoxText())),
//    pdeFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_PDEFRA")))),
    exppde(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_PDEFRA")))),
    laplacexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPLACEXP"), 0.0, 100.0, 0.1, 0.))),
    reparexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LOGREPART"), 1.0, 100.0, 1., 100.0))),
    linear(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LINEAR"), 0.01, 1., 0.01, 0.05))),
    balanexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BALANEXP"), 0.5, 1.5, 0.01, 1.0))),
    gamm(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMM"), 0.2, 1.3, 0.01, 0.4))),
    labelexpmethod(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_NOISEMETH") + ":"))),
    exnoiseMethod(Gtk::manage(new MyComboBoxText())),
//    fatFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_FATFRA")))),
    expfat(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_FATFRA")))),
    fatamount(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATAMOUNT"), 1., 100., 1., 1.))),
    fatdetail(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATDETAIL"), -100., 300., 1., 0.))),
    fatsatur(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_FATSAT")))),
    norm(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_EQUIL")))),
    fatlevel(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATLEVEL"), 0.5, 2.0, 0.01, 1.))),
    fatanchor(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATANCHOR"), 0.1, 100.0, 0.01, 50., Gtk::manage(new RTImage("circle-black-small")), Gtk::manage(new RTImage("circle-white-small"))))),
    gamex(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMC"), 0.5, 3.0, 0.05, 1.))),
    sensiex(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 60))),
    previewexe(Gtk::manage(new Gtk::ToggleButton(M("TP_LOCALLAB_PREVIEW")))),
    
    structexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRUCCOL"), 0, 100, 1, 0))),
    blurexpde(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLURDE"), 2, 100, 1, 5))),
    exptoolexp(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPTOOL")))),
    expcomp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_EXPCOMP"), MINEXP, MAXEXP, 0.01, 0.))),
    black(Gtk::manage(new Adjuster(M("TP_EXPOSURE_BLACKLEVEL"), -16384, 32768, 10, 0))),
    hlcompr(Gtk::manage(new Adjuster(M("TP_EXPOSURE_COMPRHIGHLIGHTS"), 0, 500, 1, 0))),
    hlcomprthresh(Gtk::manage(new Adjuster(M("TP_EXPOSURE_COMPRHIGHLIGHTSTHRESHOLD"), 0, 100, 1, 0))),
    shadex(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SHADEX"), 0, 100, 1, 0))),
    shcompr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SHADEXCOMP"), 0, 100, 1, 50))),
    expchroma(Gtk::manage(new Adjuster(M("TP_LOCALLAB_EXPCHROMA"), -50, 100, 1, 5))),
    curveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_CURVEEDITOR_TONES_LABEL"))),
    shapeexpos(static_cast<DiagonalCurveEditor*>(curveEditorG->addCurve(CT_Diagonal, ""))),
    exprecove(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    maskusablee(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusablee(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    recothrese(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 0., 2., 0.01, 1.))),
    lowthrese(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW"), 1., 80., 0.5, 12.))),
    higthrese(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR"), 20., 99., 0.5, 85.))),
    decaye(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKDDECAY"), 0.5, 4., 0.1, 2.))),
    expgradexp(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPGRAD")))),
    strexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTR"), -4., 4., 0.05, 0.))),
    angexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADANG"), -180, 180, 0.1, 0.))),
    featherexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FEATVALUE"), 10., 100., 0.1, 25.))),
    softradiusexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SOFTRADIUSCOL"), 0.0, 100.0, 0.5, 0.))),
    inversex(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVERS")))),
    expmaskexp(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWE")))),
    showmaskexpMethod(Gtk::manage(new MyComboBoxText())),
    showmaskexpMethodinv(Gtk::manage(new MyComboBoxText())),
    enaExpMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASK")))),
    enaExpMaskaft(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASKAFT")))),
 //   maskexpCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK"))),
    maskexpCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, "", 1)),
    CCmaskexpshape(static_cast<FlatCurveEditor*>(maskexpCurveEditorG->addCurve(CT_Flat, "C", nullptr, false, false))),
    LLmaskexpshape(static_cast<FlatCurveEditor*>(maskexpCurveEditorG->addCurve(CT_Flat, "L", nullptr, false, false))),
    HHmaskexpshape(static_cast<FlatCurveEditor *>(maskexpCurveEditorG->addCurve(CT_Flat, "LC(h)", nullptr, false, true))),
    blendmaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLENDMASKCOL"), -100, 100, 1, 0))),
    radmaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    lapmaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    chromaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMASKCOL"), -100.0, 100.0, 0.1, 0.))),
    gammaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMMASKCOL"), 0.25, 4.0, 0.01, 1.))),
    slomaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOMASKCOL"), 0.0, 15.0, 0.1, 0.))),
    gradFramemask(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GRADFRA")))),
    strmaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTR"), -2., 2., 0.05, 0.))),
    angmaskexp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADANG"), -180., 180., 0.1, 0.))),
    mask2expCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK2"))),
    Lmaskexpshape(static_cast<DiagonalCurveEditor*>(mask2expCurveEditorG->addCurve(CT_Diagonal, "L(L)"))),
    Evlocallabtmosatur(ProcEventMapper::getInstance()->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_TMO_SATUR"))
    
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    const LocallabParams::LocallabSpot defSpot;
    auto m = ProcEventMapper::getInstance();
    Evlocallabpreviewexe = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_PREVIEWEXE");
    Evlocallabfeatherexp = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_FEATHEREXE");

    // Parameter Exposure specific widgets
    expMethod->append(M("TP_LOCALLAB_STD"));
    expMethod->append(M("TP_LOCALLAB_PDE"));
    expMethod->set_active(0);
    expMethodConn = expMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabExposure::expMethodChanged));

//    pdeFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties(exppde, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    setExpandAlignProperties(expfat, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    laplacexp->setAdjusterListener(this);
    reparexp->setAdjusterListener(this);

    linear->setAdjusterListener(this);

    balanexp->setAdjusterListener(this);

    gamm->setAdjusterListener(this);

    exnoiseMethod->append(M("TP_LOCALLAB_NONENOISE"));
    exnoiseMethod->append(M("TP_LOCALLAB_MEDIAN"));
    exnoiseMethod->append(M("TP_LOCALLAB_WEDIANHI"));
    exnoiseMethod->set_active(0);
    exnoiseMethodConn  = exnoiseMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabExposure::exnoiseMethodChanged));

 //   fatFrame->set_label_align(0.025, 0.5);

    fatamount->setAdjusterListener(this);

    fatdetail->setAdjusterListener(this);

    fatlevel->setAdjusterListener(this);

    fatanchor->setAdjusterListener(this);

    sensiex->setAdjusterListener(this);

    gamex->setAdjusterListener(this);

    structexp->setAdjusterListener(this);

    blurexpde->setAdjusterListener(this);

    setExpandAlignProperties(exptoolexp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    expcomp->setAdjusterListener(this);

    black->setAdjusterListener(this);

    hlcompr->setAdjusterListener(this);

    hlcomprthresh->setAdjusterListener(this);

    shadex->setAdjusterListener(this);

    shcompr->setAdjusterListener(this);

    expchroma->setAdjusterListener(this);

    curveEditorG->setCurveListener(this);

    shapeexpos->setResetCurve(DiagonalCurveType(defSpot.excurve.at(0)), defSpot.excurve);
    shapeexpos->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1}});
    shapeexpos->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1}});

    curveEditorG->curveListComplete();

    setExpandAlignProperties(expgradexp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    strexp->setAdjusterListener(this);

    angexp->setAdjusterListener(this);
    angexp->set_tooltip_text(M("TP_LOCALLAB_GRADANG_TOOLTIP"));
    featherexp->setAdjusterListener(this);

    softradiusexp->setLogScale(10, 0);
    softradiusexp->setAdjusterListener(this);
    recothrese->setAdjusterListener(this);
    lowthrese->setAdjusterListener(this);
    higthrese->setAdjusterListener(this);
    decaye->setAdjusterListener(this);
    
    previewexe->set_active(false);
    previewexeConn = previewexe->signal_clicked().connect(
                       sigc::mem_fun(
                           *this, &LocallabExposure::previewexeChanged));
    
    setExpandAlignProperties(exprecove, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    normConn  = norm->signal_toggled().connect(sigc::mem_fun(*this, &LocallabExposure::normChanged));
    fatsaturConn  = fatsatur->signal_toggled().connect(sigc::mem_fun(*this, &LocallabExposure::fatsaturChanged));

    inversexConn  = inversex->signal_toggled().connect(sigc::mem_fun(*this, &LocallabExposure::inversexChanged));
    inversex->set_tooltip_text(M("TP_LOCALLAB_INVERS_TOOLTIP"));

    setExpandAlignProperties(expmaskexp, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWMODIF"));
    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWMODIFMASK"));
    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWSTRUCEX"));
    showmaskexpMethod->append(M("TP_LOCALLAB_SHOWREF"));
    showmaskexpMethod->set_active(0);
    showmaskexpMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskexpMethodConn  = showmaskexpMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabExposure::showmaskexpMethodChanged));

    showmaskexpMethodinv->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskexpMethodinv->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskexpMethodinv->set_active(0);
    showmaskexpMethodinv->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskexpMethodConninv  = showmaskexpMethodinv->signal_changed().connect(sigc::mem_fun(*this, &LocallabExposure::showmaskexpMethodChangedinv));

    enaExpMaskConn = enaExpMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabExposure::enaExpMaskChanged));

    enaExpMaskaftConn = enaExpMaskaft->signal_toggled().connect(sigc::mem_fun(*this, &LocallabExposure::enaExpMaskaftChanged));

    maskexpCurveEditorG->setCurveListener(this);

    CCmaskexpshape->setIdentityValue(0.);
    CCmaskexpshape->setResetCurve(FlatCurveType(defSpot.CCmaskexpcurve.at(0)), defSpot.CCmaskexpcurve);
    CCmaskexpshape->setBottomBarColorProvider(this, 1);

    LLmaskexpshape->setIdentityValue(0.);
    LLmaskexpshape->setResetCurve(FlatCurveType(defSpot.LLmaskexpcurve.at(0)), defSpot.LLmaskexpcurve);
    LLmaskexpshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1}});

    HHmaskexpshape->setIdentityValue(0.);
    HHmaskexpshape->setResetCurve(FlatCurveType(defSpot.HHmaskexpcurve.at(0)), defSpot.HHmaskexpcurve);
    HHmaskexpshape->setCurveColorProvider(this, 2);
    HHmaskexpshape->setBottomBarColorProvider(this, 2);

    maskexpCurveEditorG->curveListComplete();

    blendmaskexp->setAdjusterListener(this);

    radmaskexp->setAdjusterListener(this);

    lapmaskexp->setAdjusterListener(this);

    chromaskexp->setAdjusterListener(this);

    gammaskexp->setAdjusterListener(this);

    slomaskexp->setAdjusterListener(this);

    gradFramemask->set_label_align(0.025, 0.5);

    strmaskexp->setAdjusterListener(this);

    angmaskexp->setAdjusterListener(this);
    angmaskexp->set_tooltip_text(M("TP_LOCALLAB_GRADANG_TOOLTIP"));

    mask2expCurveEditorG->setCurveListener(this);

    Lmaskexpshape->setResetCurve(DiagonalCurveType(defSpot.Lmaskexpcurve.at(0)), defSpot.Lmaskexpcurve);
    Lmaskexpshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1}});
    Lmaskexpshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1}});

    mask2expCurveEditorG->curveListComplete();

    // Add Color & Light specific widgets to GUI
    pack_start(*sensiex);
    pack_start(*previewexe);
    pack_start(*reparexp);
    pack_start(*inversex);
    ToolParamBlock* const pdeBox = Gtk::manage(new ToolParamBlock());
    pdeBox->pack_start(*laplacexp);
    pdeBox->pack_start(*linear);
    pdeBox->pack_start(*balanexp);
    pdeBox->pack_start(*gamm);
    Gtk::Box* const ctboxexpmethod = Gtk::manage(new Gtk::Box());
//    Gtk::Label* const labelexpmethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_NOISEMETH") + ":"));
    ctboxexpmethod->pack_start(*labelexpmethod, Gtk::PACK_SHRINK, 4);
    ctboxexpmethod->pack_start(*exnoiseMethod);
    pdeBox->pack_start(*ctboxexpmethod);
    exppde->add(*pdeBox, false);
//    pdeFrame->add(*pdeBox);
//    pack_start(*pdeFrame);
    pack_start(*exppde);
    ToolParamBlock* const fatBox = Gtk::manage(new ToolParamBlock());
    fatBox->pack_start(*fatamount);
    fatBox->pack_start(*fatdetail);
//    fatBox->pack_start(*norm);
//    fatBox->pack_start(*fatlevel);
    fatBox->pack_start(*fatanchor);
    fatBox->pack_start(*fatsatur);
//    fatFrame->add(*fatBox);
    expfat->add(*fatBox, false);
//    pack_start(*fatFrame);
    pack_start(*expfat);
    pack_start(*expcomp);
    pack_start(*gamex);
    pack_start(*structexp);
    pack_start(*blurexpde);
    ToolParamBlock* const toolBox = Gtk::manage(new ToolParamBlock());
    toolBox->pack_start(*black);
    toolBox->pack_start(*hlcompr);
    toolBox->pack_start(*hlcomprthresh);
    toolBox->pack_start(*shadex);
    toolBox->pack_start(*shcompr);
    toolBox->pack_start(*expchroma);
    toolBox->pack_start(*curveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    exptoolexp->add(*toolBox, false);
    pack_start(*exptoolexp);
    ToolParamBlock* const expBox3 = Gtk::manage(new ToolParamBlock());
    expBox3->pack_start(*maskusablee, Gtk::PACK_SHRINK, 0);
    expBox3->pack_start(*maskunusablee, Gtk::PACK_SHRINK, 0);
    expBox3->pack_start(*recothrese);
    expBox3->pack_start(*lowthrese);
    expBox3->pack_start(*higthrese);
    expBox3->pack_start(*decaye);
    exprecove->add(*expBox3, false);
    pack_start(*exprecove, false, false);

    ToolParamBlock* const gradBox = Gtk::manage(new ToolParamBlock());
    gradBox->pack_start(*strexp);
    gradBox->pack_start(*angexp);
    gradBox->pack_start(*featherexp);
    expgradexp->add(*gradBox, false);
    pack_start(*expgradexp);
    pack_start(*softradiusexp);
 //   pack_start(*inversex);
    ToolParamBlock* const maskexpBox = Gtk::manage(new ToolParamBlock());
    maskexpBox->pack_start(*showmaskexpMethod, Gtk::PACK_SHRINK, 4);
    maskexpBox->pack_start(*showmaskexpMethodinv, Gtk::PACK_SHRINK, 4);
    maskexpBox->pack_start(*enaExpMask, Gtk::PACK_SHRINK, 0);
    // maskexpBox->pack_start(*enaExpMaskaft, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*maskexpCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    maskexpBox->pack_start(*blendmaskexp, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*radmaskexp, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*lapmaskexp, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*chromaskexp, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*gammaskexp, Gtk::PACK_SHRINK, 0);
    maskexpBox->pack_start(*slomaskexp, Gtk::PACK_SHRINK, 0);
    ToolParamBlock* const gradmaskBox = Gtk::manage(new ToolParamBlock());
    gradmaskBox->pack_start(*strmaskexp);
    gradmaskBox->pack_start(*angmaskexp);
    gradFramemask->add(*gradmaskBox);
    maskexpBox->pack_start(*gradFramemask);
    maskexpBox->pack_start(*mask2expCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    expmaskexp->add(*maskexpBox, false);
    pack_start(*expmaskexp, false, false);
}

LocallabExposure::~LocallabExposure()
{
    delete curveEditorG;
    delete maskexpCurveEditorG;
    delete mask2expCurveEditorG;
}

bool LocallabExposure::isMaskViewActive()
{
    return ((showmaskexpMethod->get_active_row_number() != 0) || (showmaskexpMethodinv->get_active_row_number() != 0));
}

//new function Global
void LocallabExposure::updateguiexpos(int spottype) 
{
    {
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                inversex->hide();
                sensiex->hide();
                previewexe->hide();
                enaExpMask->set_active(false);
                enaExpMaskaft->set_active(false);
                previewexe->set_active(false);
                expmaskexp->hide();
                exprecove->hide();
                blurexpde->hide();
           } else {
                inversex->show();
                sensiex->show();
                expmaskexp->show();
                exprecove->show();

                if(!inversex->get_active()) {
                    previewexe->show();
                } else {
                    previewexe->hide();
                }
                updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));
            }
            enableListener();

        return false;
        }
        );
    }
   
}

void LocallabExposure::previewexeChanged()
{
    
    if(previewexe->get_active()) {
        showmaskexpMethod->set_active(5);
    } else {
        showmaskexpMethod->set_active(0);
    }
    
    if (isLocActivated) {
        if (listener) {
            listener->panelChanged(Evlocallabpreviewexe,"");
        }
    } 
}


void LocallabExposure::resetMaskView()
{
    showmaskexpMethodConn.block(true);
    showmaskexpMethodConninv.block(true);

    showmaskexpMethod->set_active(0);
    showmaskexpMethodinv->set_active(0);

    showmaskexpMethodConn.block(false);
    showmaskexpMethodConninv.block(false);
}

void LocallabExposure::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    expMask = showmaskexpMethod->get_active_row_number();
    expMaskinv = showmaskexpMethodinv->get_active_row_number();
}

Gtk::ToggleButton *LocallabExposure::getPreviewDeltaEButton() const
{
    return previewexe;
}

sigc::connection *LocallabExposure::getPreviewDeltaEButtonConnection()
{
    return &previewexeConn;
}

void LocallabExposure::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        exp->set_tooltip_text(M("TP_LOCALLAB_EXPOSURE_TOOLTIP"));
//        expMethod->set_tooltip_text(M("TP_LOCALLAB_EXPMETHOD_TOOLTIP"));
//        pdeFrame->set_tooltip_text(M("TP_LOCALLAB_PDEFRAME_TOOLTIP"));
        exppde->set_tooltip_text(M("TP_LOCALLAB_PDEFRAME_TOOLTIP"));
        recothrese->set_tooltip_text(M("TP_LOCALLAB_RECOTHRES02_TOOLTIP"));
        exprecove->set_tooltip_markup(M("TP_LOCALLAB_MASKREEXP_TOOLTIP"));
        decaye->set_tooltip_text(M("TP_LOCALLAB_MASKDECAY_TOOLTIP"));
        lowthrese->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRESE_TOOLTIP"));
        higthrese->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRESE_TOOLTIP"));
        blurexpde->set_tooltip_text(M("TP_LOCALLAB_BLURCOLDE_TOOLTIP"));
        laplacexp->set_tooltip_text(M("TP_LOCALLAB_EXPLAP_TOOLTIP"));
        reparexp->set_tooltip_text(M("TP_LOCALLAB_REPAREXP_TOOLTIP"));
        linear->set_tooltip_text(M("TP_LOCALLAB_EXPLAPLIN_TOOLTIP"));
        balanexp->set_tooltip_text(M("TP_LOCALLAB_EXPLAPBAL_TOOLTIP"));
        gamm->set_tooltip_text(M("TP_LOCALLAB_EXPLAPGAMM_TOOLTIP"));
        labelexpmethod->set_tooltip_text(M("TP_LOCALLAB_EXPNOISEMETHOD_TOOLTIP"));
        exnoiseMethod->set_tooltip_text(M("TP_LOCALLAB_EXPNOISEMETHOD_TOOLTIP"));
//        fatFrame->set_tooltip_text(M("TP_LOCALLAB_FATFRAME_TOOLTIP"));
        expfat->set_tooltip_text(M("TP_LOCALLAB_FATFRAME_TOOLTIP"));
        expcomp->set_tooltip_text(M("TP_LOCALLAB_EXPCOMP_TOOLTIP"));
        gamex->set_tooltip_text(M("TP_LOCALLAB_GAMCOL_TOOLTIP"));
        sensiex->set_tooltip_text(M("TP_LOCALLAB_SENSI_TOOLTIP"));
        structexp->set_tooltip_text(M("TP_LOCALLAB_STRUCT_TOOLTIP"));
        expchroma->set_tooltip_text(M("TP_LOCALLAB_EXPCHROMA_TOOLTIP"));
        shapeexpos->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_TONES_TOOLTIP"));
        expgradexp->set_tooltip_text(M("TP_LOCALLAB_EXPGRADCOL_TOOLTIP"));
        strexp->set_tooltip_text(M("TP_LOCALLAB_GRADGEN_TOOLTIP"));
        expmaskexp->set_tooltip_markup(M("TP_LOCALLAB_MASK_TOOLTIP"));
        CCmaskexpshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        LLmaskexpshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        HHmaskexpshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        blendmaskexp->set_tooltip_text(M("TP_LOCALLAB_BLENDMASK_TOOLTIP"));
        radmaskexp->set_tooltip_text(M("TP_LOCALLAB_LAPRAD_TOOLTIP"));
        lapmaskexp->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
        strmaskexp->set_tooltip_text(M("TP_LOCALLAB_GRADGEN_TOOLTIP"));
        mask2expCurveEditorG->set_tooltip_text(M("TP_LOCALLAB_CONTRASTCURVMASK_TOOLTIP"));
        Lmaskexpshape->setTooltip(M("TP_LOCALLAB_LMASK_LL_TOOLTIP"));
        maskexpCurveEditorG->set_tooltip_markup(M("TP_LOCALLAB_MASKCURVE_TOOLTIP"));
        gammaskexp->set_tooltip_text(M("TP_LOCALLAB_GAMMASK_TOOLTIP"));
        chromaskexp->set_tooltip_text(M("TP_LOCALLAB_CHROMASK_TOOLTIP"));
        slomaskexp->set_tooltip_text(M("TP_LOCALLAB_SLOMASK_TOOLTIP"));
        lapmaskexp->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
    } else {
        exp->set_tooltip_text("");
        recothrese->set_tooltip_text("");
        exppde->set_tooltip_text("");
        blurexpde->set_tooltip_text("");
        exprecove->set_tooltip_markup("");
        laplacexp->set_tooltip_text("");
        reparexp->set_tooltip_text("");
        linear->set_tooltip_text("");
        balanexp->set_tooltip_text("");
        gamm->set_tooltip_text("");
        labelexpmethod->set_tooltip_text("");
        exnoiseMethod->set_tooltip_text("");
        expfat->set_tooltip_text("");
        expcomp->set_tooltip_text("");
        sensiex->set_tooltip_text("");
        structexp->set_tooltip_text("");
        expchroma->set_tooltip_text("");
        shapeexpos->setTooltip("");
        strexp->set_tooltip_text("");
        expmaskexp->set_tooltip_markup("");
        CCmaskexpshape->setTooltip("");
        LLmaskexpshape->setTooltip("");
        HHmaskexpshape->setTooltip("");
        blendmaskexp->set_tooltip_text("");
        radmaskexp->set_tooltip_text("");
        strmaskexp->set_tooltip_text("");
        mask2expCurveEditorG->set_tooltip_text("");
        Lmaskexpshape->setTooltip("");
        gammaskexp->set_tooltip_text("");
        chromaskexp->set_tooltip_text("");
        slomaskexp->set_tooltip_text("");
        lapmaskexp->set_tooltip_text("");
        gamex->set_tooltip_text("");
        expgradexp->set_tooltip_text("");

    }
}

void LocallabExposure::setDefaultExpanderVisibility()
{
    exptoolexp->set_expanded(false);
    exprecove->set_expanded(false);
    exppde->set_expanded(false);
    expfat->set_expanded(false);
    expgradexp->set_expanded(false);
    expmaskexp->set_expanded(false);
}

void LocallabExposure::disableListener()
{
    LocallabTool::disableListener();

    expMethodConn.block(true);
    exnoiseMethodConn.block(true);
    inversexConn.block(true);
    normConn.block(true);
    fatsaturConn.block(true);
    showmaskexpMethodConn.block(true);
    showmaskexpMethodConninv.block(true);
    enaExpMaskConn.block(true);
    enaExpMaskaftConn.block(true);
}

void LocallabExposure::enableListener()
{
    LocallabTool::enableListener();

    expMethodConn.block(false);
    exnoiseMethodConn.block(false);
    inversexConn.block(false);
    normConn.block(false);
    fatsaturConn.block(false);
    showmaskexpMethodConn.block(false);
    showmaskexpMethodConninv.block(false);
    enaExpMaskConn.block(false);
    enaExpMaskaftConn.block(false);
}

void LocallabExposure::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();
    nbmaskexp = 0;
    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visiexpose);
        exp->setEnabled(spot.expexpose);
        complexity->set_active(spot.complexexpose);
/*
        if (spot.expMethod == "std") {
            expMethod->set_active(0);
        } else if (spot.expMethod == "pde") {
            expMethod->set_active(1);
        }
*/
        laplacexp->setValue(spot.laplacexp);
        reparexp->setValue(spot.reparexp);
        linear->setValue(spot.linear);
        balanexp->setValue(spot.balanexp);
        gamm->setValue(spot.gamm);

        if (spot.exnoiseMethod == "one") {
            exnoiseMethod->set_active(0);
        } else if (spot.exnoiseMethod == "med") {
            exnoiseMethod->set_active(1);
        } else if (spot.exnoiseMethod == "medhi") {
            exnoiseMethod->set_active(2);
        }

        recothrese->setValue((double)spot.recothrese);
        lowthrese->setValue((double)spot.lowthrese);
        higthrese->setValue((double)spot.higthrese);
        decaye->setValue((double)spot.decaye);

        fatamount->setValue(spot.fatamount);
        fatdetail->setValue(spot.fatdetail);
        fatlevel->setValue(spot.fatlevel);
        fatanchor->setValue(spot.fatanchor);
   //     fatlevel->setValue(1.);
   //     fatanchor->setValue(1.);
        sensiex->setValue(spot.sensiex);
        gamex->setValue(spot.gamex);
        structexp->setValue(spot.structexp);
        blurexpde->setValue(spot.blurexpde);
        expcomp->setValue(spot.expcomp);
        
        if(expcomp->getValue()== 0.) {
            black->hide();
            hlcompr->hide();
            hlcomprthresh->hide();
            shadex->hide();
            shcompr->hide();
            expchroma->hide();
       } else {
            black->show();
            hlcompr->show();
            hlcomprthresh->show();
            shadex->show();
            shcompr->show();
            expchroma->show();
        }

        black->setValue(spot.black);
        hlcompr->setValue(spot.hlcompr);
        hlcomprthresh->setValue(spot.hlcomprthresh);
        shadex->setValue(spot.shadex);
        shcompr->setValue(spot.shcompr);
        expchroma->setValue(spot.expchroma);
        shapeexpos->setCurve(spot.excurve);
        strexp->setValue(spot.strexp);
        angexp->setValue(spot.angexp);
        featherexp->setValue(spot.featherexp);
        softradiusexp->setValue(spot.softradiusexp);
        norm->set_active(spot.norm);
        fatsatur->set_active(spot.fatsatur);
        inversex->set_active(spot.inversex);
        enaExpMask->set_active(spot.enaExpMask);
        enaExpMaskaft->set_active(spot.enaExpMaskaft);
        CCmaskexpshape->setCurve(spot.CCmaskexpcurve);
        LLmaskexpshape->setCurve(spot.LLmaskexpcurve);
        HHmaskexpshape->setCurve(spot.HHmaskexpcurve);
        blendmaskexp->setValue(spot.blendmaskexp);
        radmaskexp->setValue(spot.radmaskexp);
        lapmaskexp->setValue(spot.lapmaskexp);
        chromaskexp->setValue(spot.chromaskexp);
        gammaskexp->setValue(spot.gammaskexp);
        slomaskexp->setValue(spot.slomaskexp);
        strmaskexp->setValue(spot.strmaskexp);
        angmaskexp->setValue(spot.angmaskexp);
        Lmaskexpshape->setCurve(spot.Lmaskexpcurve);
    }

    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update shcompr sensitivity according to black and shadex value
    updateExposureGUI1();

    // Update exposure GUI according to expMethod value
    updateExposureGUI2();

    // Update exposure GUI according to inversex button state
    updateExposureGUI3();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabExposure::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expexpose = exp->getEnabled();
        spot.visiexpose = exp->get_visible();
        spot.complexexpose = complexity->get_active_row_number();
/*
        if (expMethod->get_active_row_number() == 0) {
            spot.expMethod = "std";
        } else if (expMethod->get_active_row_number() == 1) {
            spot.expMethod = "pde";
        }
*/
        spot.laplacexp = laplacexp->getValue();
        spot.reparexp = reparexp->getValue();
        spot.linear = linear->getValue();
        spot.balanexp = balanexp->getValue();
        spot.gamm = gamm->getValue();

        if (exnoiseMethod->get_active_row_number() == 0) {
            spot.exnoiseMethod = "none";
        } else if (exnoiseMethod->get_active_row_number() == 1) {
            spot.exnoiseMethod = "med";
        } else if (exnoiseMethod->get_active_row_number() == 2) {
            spot.exnoiseMethod = "medhi";
        }
        spot.recothrese = recothrese->getValue();
        spot.lowthrese = lowthrese->getValue();
        spot.higthrese = higthrese->getValue();
        spot.decaye = decaye->getValue();

        spot.fatamount = fatamount->getValue();
        spot.fatdetail = fatdetail->getValue();
        spot.fatlevel = fatlevel->getValue();
        spot.fatanchor = fatanchor->getValue();
        spot.sensiex = sensiex->getIntValue();
        spot.gamex = gamex->getValue();
        spot.structexp = structexp->getIntValue();
        spot.blurexpde = blurexpde->getIntValue();
        spot.expcomp = expcomp->getValue();
        spot.black = black->getIntValue();
        spot.hlcompr = hlcompr->getIntValue();
        spot.hlcomprthresh = hlcomprthresh->getIntValue();
        spot.shadex = shadex->getIntValue();
        spot.shcompr = shcompr->getIntValue();
        spot.expchroma = expchroma->getIntValue();
        spot.excurve = shapeexpos->getCurve();
        spot.strexp = strexp->getValue();
        spot.angexp = angexp->getValue();
        spot.featherexp = featherexp->getValue();
        spot.softradiusexp = softradiusexp->getValue();
        spot.inversex = inversex->get_active();
        spot.norm = norm->get_active();
        spot.fatsatur = fatsatur->get_active();
        spot.enaExpMask = enaExpMask->get_active();
        spot.enaExpMaskaft = enaExpMaskaft->get_active();
        spot.CCmaskexpcurve = CCmaskexpshape->getCurve();
        spot.LLmaskexpcurve = LLmaskexpshape->getCurve();
        spot.HHmaskexpcurve = HHmaskexpshape->getCurve();
        spot.blendmaskexp = blendmaskexp->getIntValue();
        spot.radmaskexp = radmaskexp->getValue();
        spot.lapmaskexp = lapmaskexp->getValue();
        spot.chromaskexp = chromaskexp->getValue();
        spot.gammaskexp = gammaskexp->getValue();
        spot.slomaskexp = slomaskexp->getValue();
        spot.strmaskexp = strmaskexp->getValue();
        spot.angmaskexp = angmaskexp->getValue();
        spot.Lmaskexpcurve = Lmaskexpshape->getCurve();
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabExposure::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default values for adjuster widgets
        laplacexp->setDefault(defSpot.laplacexp);
        reparexp->setDefault(defSpot.reparexp);
        linear->setDefault(defSpot.linear);
        balanexp->setDefault(defSpot.balanexp);
        gamm->setDefault(defSpot.gamm);
        fatamount->setDefault(defSpot.fatamount);
        fatdetail->setDefault(defSpot.fatdetail);
        fatlevel->setDefault(defSpot.fatlevel);
        fatanchor->setDefault(defSpot.fatanchor);
        sensiex->setDefault((double)defSpot.sensiex);
        gamex->setDefault((double)defSpot.gamex);
        structexp->setDefault((double)defSpot.structexp);
        blurexpde->setDefault((double)defSpot.blurexpde);
        expcomp->setDefault(defSpot.expcomp);
        black->setDefault((double)defSpot.black);
        hlcompr->setDefault((double)defSpot.hlcompr);
        hlcomprthresh->setDefault((double)defSpot.hlcomprthresh);
        shadex->setDefault((double)defSpot.shadex);
        shcompr->setDefault((double)defSpot.shcompr);
        expchroma->setDefault((double)defSpot.expchroma);
        strexp->setDefault(defSpot.strexp);
        angexp->setDefault(defSpot.angexp);
        featherexp->setDefault(defSpot.featherexp);
        softradiusexp->setDefault(defSpot.softradiusexp);
        blendmaskexp->setDefault((double)defSpot.blendmaskexp);
        radmaskexp->setDefault(defSpot.radmaskexp);
        lapmaskexp->setDefault(defSpot.lapmaskexp);
        chromaskexp->setDefault(defSpot.chromaskexp);
        gammaskexp->setDefault(defSpot.gammaskexp);
        slomaskexp->setDefault(defSpot.slomaskexp);
        strmaskexp->setDefault(defSpot.strmaskexp);
        angmaskexp->setDefault(defSpot.angmaskexp);
        recothrese->setDefault((double)defSpot.recothrese);
        lowthrese->setDefault((double)defSpot.lowthrese);
        higthrese->setDefault((double)defSpot.higthrese);
        decaye->setDefault((double)defSpot.decaye);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabExposure::adjusterChanged(Adjuster* a, double newval)
{
    // Update shcompr sensitivity according to black and shadex value
    if (a == black || a == shadex) {
        updateExposureGUI1();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (a == laplacexp) {
            if (listener) {
                listener->panelChanged(Evlocallablaplacexp,
                                       laplacexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == reparexp) {
            if (listener) {
                listener->panelChanged(Evlocallabreparexp,
                                       reparexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == linear) {
            if (listener) {
                listener->panelChanged(Evlocallablinear,
                                       linear->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == balanexp) {
            if (listener) {
                listener->panelChanged(Evlocallabbalanexp,
                                       balanexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gamm) {
            if (listener) {
                listener->panelChanged(Evlocallabgamm,
                                       gamm->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == fatamount) {
            if (listener) {
                listener->panelChanged(Evlocallabfatamount,
                                       fatamount->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == fatdetail) {
            if (listener) {
                listener->panelChanged(Evlocallabfatdetail,
                                       fatdetail->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == fatlevel) {
            if (listener) {
                listener->panelChanged(Evlocallabfatlevel,
                                       fatlevel->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == fatanchor) {
            if (listener) {
                listener->panelChanged(Evlocallabfatanchor,
                                       fatanchor->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gamex) {
            if (listener) {
                listener->panelChanged(Evlocallabgamex,
                                       gamex->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothrese) {
            if (listener) {
                listener->panelChanged(Evlocallabrecothrese,
                                       recothrese->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthrese) {
            if (listener) {
                listener->panelChanged(Evlocallablowthrese,
                                       lowthrese->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthrese) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthrese,
                                       higthrese->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == decaye) {
            if (listener) {
                listener->panelChanged(Evlocallabdecaye,
                                       decaye->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensiex) {
            if (listener) {
                listener->panelChanged(Evlocallabsensiex,
                                       sensiex->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == structexp) {
            if (listener) {
                listener->panelChanged(Evlocallabstructexp,
                                       structexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blurexpde) {
            if (listener) {
                listener->panelChanged(Evlocallabblurexpde,
                                       blurexpde->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == expcomp) {
            if(expcomp->getValue()== 0.) {
                black->hide();
                hlcompr->hide();
                hlcomprthresh->hide();
                shadex->hide();
                shcompr->hide();
                expchroma->hide();
            } else {
                black->show();
                hlcompr->show();
                hlcomprthresh->show();
                shadex->show();
                shcompr->show();
                expchroma->show();
           }
               
            if (listener) {
                listener->panelChanged(Evlocallabexpcomp,
                                       expcomp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == black) {
            if (listener) {
                listener->panelChanged(Evlocallabblack,
                                       black->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == hlcompr) {
            if (listener) {
                listener->panelChanged(Evlocallabhlcompr,
                                       hlcompr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == hlcomprthresh) {
            if (listener) {
                listener->panelChanged(Evlocallabhlcomprthresh,
                                       hlcomprthresh->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shadex) {
            if (listener) {
                listener->panelChanged(Evlocallabshadex,
                                       shadex->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shcompr) {
            if (listener) {
                listener->panelChanged(Evlocallabshcompr,
                                       shcompr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == expchroma) {
            if (listener) {
                listener->panelChanged(Evlocallabexpchroma,
                                       expchroma->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strexp) {
            if (listener) {
                listener->panelChanged(Evlocallabstrexp,
                                       strexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == angexp) {
            if (listener) {
                listener->panelChanged(Evlocallabangexp,
                                       angexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == featherexp) {
            if (listener) {
                listener->panelChanged(Evlocallabfeatherexp,
                                       featherexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == softradiusexp) {
            if (listener) {
                listener->panelChanged(Evlocallabsoftradiusexp,
                                       softradiusexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blendmaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabblendmaskexp,
                                       blendmaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == radmaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabradmaskexp,
                                       radmaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lapmaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallablapmaskexp,
                                       lapmaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chromaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabchromaskexp,
                                       chromaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gammaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabgammaskexp,
                                       gammaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == slomaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabslomaskexp,
                                       slomaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strmaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabstrmaskexp,
                                       strmaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == angmaskexp) {
            if (listener) {
                listener->panelChanged(Evlocallabangmaskexp,
                                       angmaskexp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::curveChanged(CurveEditor* ce)
{
    if (isLocActivated && exp->getEnabled()) {
        if (ce == shapeexpos) {
            if (listener) {
                listener->panelChanged(Evlocallabshapeexpos,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == CCmaskexpshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCCmaskexpshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskexpshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskexpshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHmaskexpshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHmaskexpshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == Lmaskexpshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLmaskexpshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenaexpose,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenaexpose,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    gamex->setValue(defSpot.gamex);
    laplacexp->setValue(defSpot.laplacexp);
    // Set hidden GUI widgets in Normal mode to default spot values
    structexp->setValue((double)defSpot.structexp);
    blurexpde->setValue((double)defSpot.blurexpde);
    lapmaskexp->setValue(defSpot.lapmaskexp);
    gammaskexp->setValue(defSpot.gammaskexp);
    slomaskexp->setValue(defSpot.slomaskexp);
    strmaskexp->setValue(defSpot.strmaskexp);
    angmaskexp->setValue(defSpot.angmaskexp);
    decaye->setValue(defSpot.decaye);
//    norm->set_active(defSpot.enaExpMask);
    fatlevel->setValue(defSpot.fatlevel);

    // Enable all listeners
    enableListener();
}

void LocallabExposure::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    laplacexp->setValue(defSpot.laplacexp);
    fatlevel->setValue(defSpot.fatlevel);
    fatanchor->setValue(defSpot.fatanchor);
    norm->set_active(false);
    // Set hidden specific GUI widgets in Simple mode to default spot values
    //strexp->setValue(defSpot.strexp);
    //angexp->setValue(defSpot.angexp);
    //featherexp->setValue(defSpot.featherexp);
    softradiusexp->setValue(defSpot.softradiusexp);
    enaExpMask->set_active(defSpot.enaExpMask);
    enaExpMaskaft->set_active(defSpot.enaExpMaskaft);
    showmaskexpMethod->set_active(0);
    gamex->setValue(defSpot.gamex);
 //   CCmaskexpshape->setCurve(defSpot.CCmaskexpcurve);
 //   LLmaskexpshape->setCurve(defSpot.CCmaskexpcurve);
 //   HHmaskexpshape->setCurve(defSpot.HHmaskexpcurve);
 //   blendmaskexp->setValue((double)defSpot.blendmaskexp);
 //   radmaskexp->setValue(defSpot.radmaskexp);
//    chromaskexp->setValue(defSpot.chromaskexp);
//    Lmaskexpshape->setCurve(defSpot.Lmaskexpcurve);
    recothrese->setValue(defSpot.recothrese);
    lowthrese->setValue(defSpot.lowthrese);
    higthrese->setValue(defSpot.higthrese);
    decaye->setValue(defSpot.decaye);

    // Enable all listeners
    enableListener();
}

void LocallabExposure::updateGUIToMode(const modeType new_type)
{
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            structexp->hide();
            blurexpde->hide();
            expgradexp->hide();
            softradiusexp->hide();
            exprecove->hide();
            maskusablee->hide();
            maskunusablee->hide();
            decaye->hide();
            norm->hide();
            fatlevel->hide();
            fatanchor->hide();
            gamex->hide();
            exppde->hide();
            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            structexp->hide();
            gamex->hide();
            blurexpde->hide();
            lapmaskexp->hide();
            gammaskexp->hide();
            slomaskexp->hide();
            gradFramemask->hide();
            exprecove->show();
            if (enaExpMask->get_active()) {
                maskusablee->show();
                maskunusablee->hide();

            } else {
                maskusablee->hide();
                maskunusablee->show();
            }
            norm->show();
            fatlevel->hide();
            fatanchor->show();

            // Specific Simple mode widgets are shown in Normal mode
            softradiusexp->hide();
            blurexpde->hide();
            exppde->hide();

            if (!inversex->get_active()) { // Keep widget hidden when invers is toggled
              //  expgradexp->show();
                softradiusexp->show();
                exprecove->show();
                gamex->hide();
                blurexpde->show();
            }

            decaye->hide();

            break;

        case Expert:
            // Show widgets hidden in Normal and Simple mode
           structexp->hide();
           if (!inversex->get_active()) { // Keep widget hidden when invers is toggled
                structexp->show();
            }

            blurexpde->show();
            norm->show();
            fatlevel->show();
            fatanchor->show();
            softradiusexp->hide();
            gamex->show();

            if (!inversex->get_active()) { // Keep widget hidden when invers is toggled
                //expgradexp->show();
                softradiusexp->show();
                exprecove->show();
                gamex->show();
                blurexpde->show();

            }
            if (enaExpMask->get_active()) {
                maskusablee->show();
                maskunusablee->hide();

            } else {
                maskusablee->hide();
                maskunusablee->show();
            }
            exppde->show();

            lapmaskexp->show();
            gammaskexp->show();
            slomaskexp->show();
            gradFramemask->show();
            decaye->show();
    }
}

void LocallabExposure::updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz)
{
    idle_register.add(
    [this, normHuer, normLumar, normChromar]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        // Update mask background
        CCmaskexpshape->updateLocallabBackground(normChromar);
        LLmaskexpshape->updateLocallabBackground(normLumar);
        HHmaskexpshape->updateLocallabBackground(normHuer);
        shapeexpos->updateLocallabBackground(normLumar);
        Lmaskexpshape->updateLocallabBackground(normLumar);
        return false;
    }
    );
}

void LocallabExposure::expMethodChanged()
{
    // Update exposure GUI according to expMethod value
    updateExposureGUI2();

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabexpMethod,
                                   expMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabExposure::exnoiseMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabexnoiseMethod,
                                   exnoiseMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabExposure::normChanged()
{

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (norm->get_active()) {
                listener->panelChanged(Evlocallabnorm,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabnorm,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::fatsaturChanged()
{

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (fatsatur->get_active()) {
                listener->panelChanged(Evlocallabtmosatur,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabtmosatur,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::inversexChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update exposure GUI according to inversex button state
    updateExposureGUI3();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (inversex->get_active()) {
                listener->panelChanged(Evlocallabinversex,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinversex,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::showmaskexpMethodChanged()
{
    // If mask preview is activated, deactivate other Exposure mask preview
    showmaskexpMethodConninv.block(true);
    showmaskexpMethodinv->set_active(0);
    showmaskexpMethodConninv.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabExposure::showmaskexpMethodChangedinv()
{
    // If mask preview is activated, deactivate other Exposure mask preview
    showmaskexpMethodConn.block(true);
    showmaskexpMethod->set_active(0);
    showmaskexpMethodConn.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabExposure::enaExpMaskChanged()
{
    if (enaExpMask->get_active()) {
        maskusablee->show();
        maskunusablee->hide();
    } else {
        maskusablee->hide();
        maskunusablee->show();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enaExpMask->get_active()) {
                listener->panelChanged(EvLocallabEnaExpMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnaExpMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::enaExpMaskaftChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enaExpMaskaft->get_active()) {
                listener->panelChanged(EvLocallabEnaExpMaskaft,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnaExpMaskaft,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabExposure::updateExposureGUI1()
{
    // Update shcompr sensitivity according to black and shadex value
    if (black->getIntValue() == 0 && shadex->getIntValue() == 0) {
        shcompr->set_sensitive(false);
    } else {
        shcompr->set_sensitive(true);
    }
}

void LocallabExposure::updateExposureGUI2()
{  /*
    // Update exposure GUI according to expMethod value
    if (expMethod->get_active_row_number() == 0) {
//        pdeFrame->hide();
//        fatFrame->hide();
        exppde->hide();
        expfat->hide();
        softradiusexp->set_sensitive(true);
        sensiex->set_sensitive(true);
    } else if (expMethod->get_active_row_number() == 1) {
 //       pdeFrame->show();
 //       fatFrame->show();
        exppde->show();
        expfat->show();
        softradiusexp->set_sensitive(false);
        sensiex->set_sensitive(true);
    }
    */
}

void LocallabExposure::updateExposureGUI3()
{
    const int mode = complexity->get_active_row_number();

    // Update exposure GUI according to inversex button state
    if (inversex->get_active()) {
        expMethod->hide();
        expcomp->setLabel(M("TP_LOCALLAB_EXPCOMPINV"));
        exprecove->hide();
        reparexp->hide();
        gamex->hide();
        expfat->hide();
        exppde->hide();
        structexp->hide();
        blurexpde->hide();
        expgradexp->hide();
        
        // Manage specific case where expMethod is different from 0
        if (expMethod->get_active_row_number() > 0) {
            expMethodConn.block(true);
            expMethod->set_active(0);
            expMethodConn.block(false);

            // Update GUI accordingly
            updateExposureGUI2();
        }

        softradiusexp->hide();
        expgradexp->hide();
        showmaskexpMethod->hide();
        // Reset hidden mask combobox
        showmaskexpMethodConn.block(true);
        showmaskexpMethod->set_active(0);
        showmaskexpMethodConn.block(false);
        showmaskexpMethodinv->show();
    } else {
        expMethod->show();
        expcomp->setLabel(M("TP_LOCALLAB_EXPCOMP"));
        gamex->hide();
        expfat->show();
        exppde->hide();

        if (mode == Normal) { // Keep widgets hidden in Simple mode
            softradiusexp->show();
            //expgradexp->show();
            exprecove->show();
            blurexpde->show();
            exppde->hide();

        }
        if (mode == Expert) { // Keep widgets hidden in Simple mode
            softradiusexp->show();
            //expgradexp->show();
            exprecove->show();
            structexp->show();
            blurexpde->show();
            gamex->show();
            exppde->show();

        }

        reparexp->show();

        showmaskexpMethodinv->hide();
        // Reset hidden mask combobox
        showmaskexpMethodConninv.block(true);
        showmaskexpMethodinv->set_active(0);
        showmaskexpMethodConninv.block(false);
        showmaskexpMethod->show();
    }
}

/* ==== LocallabShadow ==== */
LocallabShadow::LocallabShadow():
    LocallabTool(this, M("TP_LOCALLAB_SH_TOOLNAME"), M("TP_LOCALLAB_SHADHIGH"), false),

    // Shadow highlight specific widgets
    shMethod(Gtk::manage(new MyComboBoxText())),
    reparsh(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LOGREPART"), 1.0, 100.0, 1., 100.0))),
    multipliersh([]() -> std::array<Adjuster *, 6>
    {
    std::array<Adjuster*, 6> res = {};

    for (unsigned int i = 0; i < res.size(); ++i) {
        Glib::ustring ss = Glib::ustring::format(i);

        if (i == 0) {
            ss += Glib::ustring::compose(" (%1)", M("TP_LOCALLAB_LUMADARKEST"));
        } else if (i == 5) {
            ss += Glib::ustring::compose(" (%1)", M("TP_LOCALLAB_LUMAWHITESEST"));
        }

        res[i] = Gtk::manage(new Adjuster(std::move(ss), -100, 100, 1, 0));
    }

    return res;
    }
    ()),
    detailSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DETAILSH"), -5, 5, 1, 0))),
    tePivot(Gtk::manage(new Adjuster(M("TP_LOCALLAB_TE_PIVOT"), -12, 12, 0.05, 0))),
    highlights(Gtk::manage(new Adjuster(M("TP_SHADOWSHLIGHTS_HIGHLIGHTS"), 0, 100, 1, 0))),
    h_tonalwidth(Gtk::manage(new Adjuster(M("TP_SHADOWSHLIGHTS_HLTONALW"), 10, 100, 1, 70))),
    shadows(Gtk::manage(new Adjuster(M("TP_SHADOWSHLIGHTS_SHADOWS"), 0, 100, 1, 0))),
    s_tonalwidth(Gtk::manage(new Adjuster(M("TP_SHADOWSHLIGHTS_SHTONALW"), 10, 100, 1, 30))),
    sh_radius(Gtk::manage(new Adjuster(M("TP_SHADOWSHLIGHTS_RADIUS"), 0, 100, 1, 40))),
    sensihs(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 30))),//reused - unused here, but used for normalize_mean_dt 
    previewsh(Gtk::manage(new Gtk::ToggleButton(M("TP_LOCALLAB_PREVIEW")))),
    blurSHde(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLURDE"), 2, 100, 1, 5))),
    exprecovs(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    maskusables(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusables(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    recothress(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 0., 2., 0.01, 1.))),
    lowthress(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW"), 1., 80., 0.5, 12.))),
    higthress(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR"), 20., 99., 0.5, 85.))),
    decays(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKDDECAY"), 0.5, 4., 0.1, 2.))),
    gamFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GAMFRA")))),
    gamSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMSH"), 0.25, 15.0, 0.01, 2.4))),
    sloSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOSH"), 0.0, 500.0, 0.01, 12.92))),
    //Generalized Hyperbolic Stretch - 2024 - 2026
    ghsMethod(Gtk::manage(new MyComboBoxText())),
    gridFrameghs(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHS_GHSDIAG")))),
    labgridghs(Gtk::manage(new LabGrid(EvlocallabGridciexy, M("TP_LOCALLAB_GHS_GHSDIAG"), true, false, true, false))),
    ghsFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHSFRA")))),
    matHBox(Gtk::manage(new Gtk::Box())),   
    ghs_agx(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_GHS_AGX")))),
    ghsMatmet(Gtk::manage(new MyComboBoxText())),
    ghs_D(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_D"), 0., 20.0, 0.001, 0.001))),
    Framemtf(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHS_MTFFRA")))),
    ghs_mtf(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_GHS_MTF")))),
    ghs_mtfstr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_MTFSTR"), 0., 1.0, 0.01, 0.25))),
    Lab_Frame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHSLABFRA")))),
    ghs_slope(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_SLOPE"), 1.0, 100.0, 0.01, 9.03296))),
    ghs_chro(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_CHRO"), -30., 100.0, 0.001, 0.))),
    ghs_B(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_B"), -5.0, 15.0, 0.001, 0.0))),
    ghs_SP(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_SP"), 0.0, 1.0, 0.00001, 0.015))),
    ghssymLabel(Gtk::manage(new Gtk::Label("---"))),
    ghsmidLabel(Gtk::manage(new Gtk::Label("---"))),
    ghsmaxrgbLabel(Gtk::manage(new Gtk::Label("---"))),
    ghs_LP(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_LP"), 0.0, 1.0, 0.00001, 0.0))),
    ghs_HP(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_HP"), 0.0, 1.0, 0.00001, 1.0))),
    LC_Frame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHS_LC_FRAME")))),
    ghs_LC(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_LC"), 0.0, 100.0, 0.1, 0.0))),
    ghs_MID(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_MID"), -100.0, 100.0, 0.1, 0.0))),
    BP_Frame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GHS_BLACKPOINT_FRAME")))),
    ghs_autobw(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_GHS_AUTOBW")))),
    ghs_BLP(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_BLP"), -0.2, 0.2, 0.0001, 0.0))),
    ghs_HLP(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_HLP"), 0.2002, 25.0, 0.0001, 1.))),
    ghsbpwpLabels(Gtk::manage(new Gtk::Label("---"))),
    ghsbpwpvalueLabels(Gtk::manage(new Gtk::Label("---"))),
    ghscolorLabels(Gtk::manage(new Gtk::Label("---"))),
    ghsDRLabels(Gtk::manage(new Gtk::Label("---"))),
    ghs_smooth(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_GHS_SMOOTH")))),
    ghs_inv(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_GHS_INV")))),
    //Michaelis-Menten 2026
    michFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_MICHFRA")))),
    michtone_Frame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_MICHTONEFRA")))),
    mich_exp(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHEXP"), -4., 4., 0.01, 0.))),//Exposure : Adjusts the input image brightness.
    mich_spar(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHSPAR"), 0.1, 5., 0.01, 1.))),//Output scale (S) : Controls the maximum asymptotic value of the curve; essentially the output white level.
    mich_kpar(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHKPAR"), 0.01, 2., 0.001, 0.18))),//Knee strength (K): Determines the “knee” of the curve. Lower values result in a sharper transition to the compressed highlight region.
    mich_jdx(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_MICHJDX")))),//Allows or disallows an LMS transformation with the JDx matrix.
    mich_sat(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHSAT"), 0.0, 2., 0.01, 1.15))),//Saturation : Adjusts color saturation post-tone mapping.
    mich_out(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHOUT"), 0.5, 10., 0.01, 1.))),//Output Max Clamp : Sets the final clipping point for the output values.
    michbwLabel(Gtk::manage(new Gtk::Label("---"))),//Display Subtract Black and White point
    michdataLabel(Gtk::manage(new Gtk::Label("---"))),//Display Midrey and Max RGB
    mich_black(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_MICHBLACK")))),//Allows or disallows the use of linear black subtraction.
    mich_white(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_MICHWHITE")))),//Allows or disallows the use of linear dynamic range.
    mich_high(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MICHHIGH"), 0., 3., 0.01, 0.))),//Reduces highlights.
    mich_mtf(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GHS_MTFFRA"), 0., 1., 0.01, 0.))),//MTF.
    expgradsh(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPGRAD")))),
    strSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTR"), -4., 4., 0.05, 0.))),
    angSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADANG"), -180, 180, 0.1, 0.))),
    featherSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FEATVALUE"), 10., 100., 0.1, 25.))),//10.
    inverssh(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVERS")))),
    expmasksh(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWS")))),
    showmaskSHMethod(Gtk::manage(new MyComboBoxText())),
    showmaskSHMethodinv(Gtk::manage(new MyComboBoxText())),
    enaSHMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASK")))),
//    maskSHCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK"))),
    maskSHCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, "", 1)),
    CCmaskSHshape(static_cast<FlatCurveEditor*>(maskSHCurveEditorG->addCurve(CT_Flat, "C", nullptr, false, false))),
    LLmaskSHshape(static_cast<FlatCurveEditor*>(maskSHCurveEditorG->addCurve(CT_Flat, "L", nullptr, false, false))),
    HHmaskSHshape(static_cast<FlatCurveEditor*>(maskSHCurveEditorG->addCurve(CT_Flat, "LC(h)", nullptr, false, true))),
    blendmaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLENDMASKCOL"), -100, 100, 1, 0))),
    radmaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    lapmaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    chromaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMASKCOL"), -100.0, 100.0, 0.1, 0.))),
    gammaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMMASKCOL"), 0.25, 4.0, 0.01, 1.))),
    slomaskSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOMASKCOL"), 0.0, 15.0, 0.1, 0.))),
    mask2SHCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK2"))),
    LmaskSHshape(static_cast<DiagonalCurveEditor*>(mask2SHCurveEditorG->addCurve(CT_Diagonal, "L(L)"))),
    fatSHFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_FATSHFRA")))),
    fatamountSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATAMOUNT"), 1., 100., 1., 1.))),
    fatanchorSH(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FATANCHOR"), 1., 100., 1., 50., Gtk::manage(new RTImage("circle-black-small")), Gtk::manage(new RTImage("circle-white-small"))))),
    EvlocallabTePivot(ProcEventMapper::getInstance()->newEvent(AUTOEXP, "HISTORY_MSG_LOCALLAB_TE_PIVOT"))
{
    auto m = ProcEventMapper::getInstance();
    Evlocallabpreviewsh = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_PREVIEWSH");
    EvlocallabfeatherSH = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_FEATHERSH");
    EvlocallabghsMethod = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHSMETHOD");
    Evlocallabghs_D = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_D");
    Evlocallabghs_slope = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_SLOPE");
    Evlocallabghs_chro = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_CHRO");
    Evlocallabghs_B = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_B");
    Evlocallabghs_SP = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_SP");
    EvlocallabautoSPson = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_AUTOSP");
    EvlocallabautoSPoff = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_AUTOSP");
    Evlocallabghs_LP = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_LP");
    Evlocallabghs_HP = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_HP");
    Evlocallabghs_LC = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_LC");
    Evlocallabghs_MID = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_MID");
    Evlocallabghs_HLP = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_HLP");
    Evlocallabghs_BLP = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_BLP");
    Evlocallabghs_smooth = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_SMOOTH");
    Evlocallabghs_autobw = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_AUTOBW");
    Evlocallabghs_agx = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_AGX");
    Evlocallabghs_mtf = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_MTF");
    Evlocallabghs_mtfstr = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_MTFSTR");
    Evlocallabghs_Matmet = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_MATMET");
    Evlocallabghs_inv = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_GHS_INV");
    EvlocallabGridghs = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_CIE_LABGRIDGHS");
    Evlocallabmich_exp = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_EXP");
    Evlocallabmich_spar = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_SPAR");
    Evlocallabmich_kpar = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_KPAR");
    Evlocallabmich_sat = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_SAT");
    Evlocallabmich_out = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_OUT");
    Evlocallabmich_black = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_BLACK");
    Evlocallabmich_white = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_WHITE");
    Evlocallabmich_high = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_HIGH");
    Evlocallabmich_jdx = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_JDX");
    Evlocallabmich_mtf = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_MICH_MTF");

    ghs_SP->addAutoButton(M("TP_LOCALLAB_SPRADIUS_TOOLTIP"));
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    const LocallabParams::LocallabSpot defSpot;

    // Parameter Shadow highlight specific widgets
    shMethod->append(M("TP_LOCALLAB_SH1"));
    shMethod->append(M("TP_LOCALLAB_SH2"));
    shMethod->append(M("TP_LOCALLAB_SH3"));
    shMethod->append(M("TP_LOCALLAB_SH4"));
    shMethod->set_active(3);
    shMethodConn = shMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabShadow::shMethodChanged));


    ghsMethod->append(M("TP_LOCALLAB_GHSRGBLUM"));
    ghsMethod->append(M("TP_LOCALLAB_GHSRGBSTD"));
    ghsMethod->append(M("TP_LOCALLAB_GHSLAB"));
    ghsMethod->append(M("TP_LOCALLAB_GHSLUM"));
    ghsMethod->append(M("TP_LOCALLAB_GHSSAT"));
    ghsMethod->append(M("TP_LOCALLAB_GHSHUE"));
    ghsMethod->set_active(1);
    ghsMethodConn = ghsMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabShadow::ghsMethodChanged));
    ghsMethod->set_tooltip_text(M("TP_LOCALLAB_GHS_METHOD_TOOLTIP"));


    ghsMatmet->append(M("TP_LOCALLAB_GHSMATO"));
    ghsMatmet->append(M("TP_LOCALLAB_GHSMAT1"));
    ghsMatmet->append(M("TP_LOCALLAB_GHSMAT2"));
    ghsMatmet->append(M("TP_LOCALLAB_GHSMAT3"));
    ghsMatmet->append(M("TP_LOCALLAB_GHSMAT4"));
    ghsMatmet->append(M("TP_LOCALLAB_GHSMAT5"));
    ghsMatmet->set_active(0);//I think it's better to keep 'none' (index 0) and let the user choose their preferred mode.
    ghsMatmetConn = ghsMatmet->signal_changed().connect(sigc::mem_fun(*this, &LocallabShadow::ghsMatmetChanged));

    for (const auto multiplier : multipliersh) {
        multiplier->setAdjusterListener(this);
    }

    detailSH->setAdjusterListener(this);
    tePivot->setAdjusterListener(this);
    reparsh->setAdjusterListener(this);

    highlights->setAdjusterListener(this);

    h_tonalwidth->setAdjusterListener(this);

    shadows->setAdjusterListener(this);

    s_tonalwidth->setAdjusterListener(this);

    sh_radius->setAdjusterListener(this);


    recothress->setAdjusterListener(this);
    lowthress->setAdjusterListener(this);
    higthress->setAdjusterListener(this);
    decays->setAdjusterListener(this);
    setExpandAlignProperties(exprecovs, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    sensihs->setAdjusterListener(this);

    blurSHde->setAdjusterListener(this);

    gamSH->setAdjusterListener(this);
    sloSH->setLogScale(16, 0);

    sloSH->setAdjusterListener(this);

    ghs_D->setAdjusterListener(this);
    ghs_mtfstr->setAdjusterListener(this);
    ghs_slope->setAdjusterListener(this);
    ghs_chro->setAdjusterListener(this);
    ghs_B->setAdjusterListener(this);
    ghs_SP->setAdjusterListener(this);
    ghssymLabel->set_line_wrap();
    ghssymLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghssymLabel, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);

    ghsmidLabel->set_line_wrap();
    ghsmidLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghsmidLabel, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);

    ghsmaxrgbLabel->set_line_wrap();
    ghsmaxrgbLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghsmaxrgbLabel, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);

    ghs_LP->setAdjusterListener(this);
    ghs_HP->setAdjusterListener(this);
    ghs_LC->setAdjusterListener(this);
    ghs_MID->setAdjusterListener(this);
    ghs_BLP->setAdjusterListener(this);
    ghs_HLP->setAdjusterListener(this);
    ghs_HLP->setLogScale(16, 0);
    mich_exp->setAdjusterListener(this);
    mich_spar->setAdjusterListener(this);
    mich_kpar->setAdjusterListener(this);
    mich_sat->setAdjusterListener(this);
    mich_out->setAdjusterListener(this);
    mich_kpar->setLogScale(10, 0);
    mich_out->setLogScale(10, 0);
    mich_high->setAdjusterListener(this);
    mich_mtf->setAdjusterListener(this);
    michbwLabel->set_line_wrap();
    michbwLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(michbwLabel, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    michdataLabel->set_line_wrap();
    michdataLabel->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(michdataLabel, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    ghsbpwpLabels->set_line_wrap();
    ghsbpwpLabels->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghsbpwpLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    ghsbpwpvalueLabels->set_line_wrap();
    ghsbpwpvalueLabels->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghsbpwpvalueLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    ghscolorLabels->set_line_wrap();
    ghscolorLabels->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghscolorLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);    
    ghsDRLabels->set_line_wrap();
    ghsDRLabels->set_justify(Gtk::Justification::JUSTIFY_CENTER);
    setExpandAlignProperties(ghsDRLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    ghs_D->setLogScale(10, 0);
    ghs_slope->setLogScale(10, 1);
    ghs_chro->setLogScale(10, -30);
    ghs_SP->setLogScale(10, 0);
    ghs_autobwConn = ghs_autobw->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::ghs_autobwChanged));
    ghs_smoothConn = ghs_smooth->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::ghs_smoothChanged));
    ghs_invConn = ghs_inv->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::ghs_invChanged));
    ghs_agxConn = ghs_agx->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::ghs_agxChanged));
    ghs_mtfConn = ghs_mtf->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::ghs_mtfChanged));
    mich_blackConn = mich_black->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::mich_blackChanged));
    mich_whiteConn = mich_white->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::mich_whiteChanged));
    mich_jdxConn = mich_jdx->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::mich_jdxChanged));


    setExpandAlignProperties(expgradsh, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    strSH->setAdjusterListener(this);

    angSH->setAdjusterListener(this);
    angSH->set_tooltip_text(M("TP_LOCALLAB_GRADANG_TOOLTIP"));
    featherSH->setAdjusterListener(this);

    inversshConn = inverssh->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::inversshChanged));
    inverssh->set_tooltip_text(M("TP_LOCALLAB_INVERS_TOOLTIP"));

    setExpandAlignProperties(expmasksh, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    previewsh->set_active(false);
    previewshConn = previewsh->signal_clicked().connect(
                       sigc::mem_fun(
                           *this, &LocallabShadow::previewshChanged));

    showmaskSHMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskSHMethod->append(M("TP_LOCALLAB_SHOWMODIF"));
    showmaskSHMethod->append(M("TP_LOCALLAB_SHOWMODIFMASK"));
    showmaskSHMethod->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskSHMethod->append(M("TP_LOCALLAB_SHOWREF"));
    showmaskSHMethod->set_active(0);
    showmaskSHMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskSHMethodConn = showmaskSHMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabShadow::showmaskSHMethodChanged));

    showmaskSHMethodinv->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskSHMethodinv->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskSHMethodinv->set_active(0);
    showmaskSHMethodinv->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskSHMethodConninv = showmaskSHMethodinv->signal_changed().connect(sigc::mem_fun(*this, &LocallabShadow::showmaskSHMethodChangedinv));

    enaSHMaskConn = enaSHMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabShadow::enaSHMaskChanged));

    maskSHCurveEditorG->setCurveListener(this);

    CCmaskSHshape->setIdentityValue(0.);
    CCmaskSHshape->setResetCurve(FlatCurveType(defSpot.CCmaskSHcurve.at(0)), defSpot.CCmaskSHcurve);
    CCmaskSHshape->setBottomBarColorProvider(this, 1);

    LLmaskSHshape->setIdentityValue(0.);
    LLmaskSHshape->setResetCurve(FlatCurveType(defSpot.LLmaskSHcurve.at(0)), defSpot.LLmaskSHcurve);
    LLmaskSHshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    HHmaskSHshape->setIdentityValue(0.);
    HHmaskSHshape->setResetCurve(FlatCurveType(defSpot.HHmaskSHcurve.at(0)), defSpot.HHmaskSHcurve);
    HHmaskSHshape->setCurveColorProvider(this, 2);
    HHmaskSHshape->setBottomBarColorProvider(this, 2);

    maskSHCurveEditorG->curveListComplete();

    blendmaskSH->setAdjusterListener(this);

    radmaskSH->setAdjusterListener(this);

    lapmaskSH->setAdjusterListener(this);

    chromaskSH->setAdjusterListener(this);

    gammaskSH->setAdjusterListener(this);

    slomaskSH->setAdjusterListener(this);

    mask2SHCurveEditorG->setCurveListener(this);

    LmaskSHshape->setResetCurve(DiagonalCurveType(defSpot.LmaskSHcurve.at(0)), defSpot.LmaskSHcurve);
    LmaskSHshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    LmaskSHshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    mask2SHCurveEditorG->curveListComplete();

    fatSHFrame->set_label_align(0.025, 0.5);

    fatamountSH->setAdjusterListener(this);

    fatanchorSH->setAdjusterListener(this);
    gridFrameghs->set_label_align(0.025, 0.5);
    ToolParamBlock* const gridBox = Gtk::manage(new ToolParamBlock());
    gridBox->pack_start(*labgridghs);
    gridFrameghs->add(*gridBox);

    // Add Shadow highlight specific widgets to GUI
    pack_start(*reparsh);
    pack_start(*sensihs);// reused / unused here, but used for normalize_mean_dt 
    pack_start(*previewsh);
    
    pack_start(*inverssh);
    pack_start(*shMethod);
    pack_start(*ghsMethod);
    ghsFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const ghsBox = Gtk::manage(new ToolParamBlock());
    ToolParamBlock* const ghsBox2 = Gtk::manage(new ToolParamBlock());
    BP_Frame->set_label_align(0.025, 0.5);
    ToolParamBlock* const BPBox = Gtk::manage(new ToolParamBlock());
    BPBox->pack_start(*ghs_autobw);
    BPBox->pack_start(*ghs_BLP);
    BPBox->pack_start(*ghs_HLP);
    BPBox->pack_start(*ghsbpwpLabels);
    BPBox->pack_start(*ghsbpwpvalueLabels);
    BPBox->pack_start(*ghscolorLabels);
    BPBox->pack_start(*ghsDRLabels);
    BP_Frame->add(*BPBox);
    ghsBox2->pack_start(*BP_Frame);
    ghsBox2->pack_start(*ghs_inv);

    pack_start(*ghsBox2);
    matHBox->set_spacing(2);
    Framemtf->set_label_align(0.025, 0.5);
    ToolParamBlock* const ghsmtfBox = Gtk::manage(new ToolParamBlock());

    Gtk::Label* matLabel = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_GHSMAT") + ":"));
    matHBox->pack_start(*matLabel, Gtk::PACK_SHRINK);
    matHBox->pack_start(*ghsMatmet);
    ghsBox->pack_start(*matHBox);

    ghsBox->pack_start(*gridFrameghs);
    ghsBox->pack_start(*ghs_D);
    ghsmtfBox->pack_start(*ghs_mtf);
    ghsmtfBox->pack_start(*ghs_mtfstr);
    Framemtf->add(*ghsmtfBox);
    ghsBox->pack_start(*Framemtf);

    Lab_Frame->set_label_align(0.025, 0.5);
    ToolParamBlock* const LabBox = Gtk::manage(new ToolParamBlock());
    LabBox->pack_start(*ghs_slope);
    LabBox->pack_start(*ghs_chro);
    Lab_Frame->add(*LabBox);
    ghsBox->pack_start(*Lab_Frame);
    ghsBox->pack_start(*ghs_B);
    ghsBox->pack_start(*ghs_SP);
    ghsBox->pack_start(*ghssymLabel);
    ghsBox->pack_start(*ghs_LP);
    ghsBox->pack_start(*ghs_HP);

    LC_Frame->set_label_align(0.025, 0.5);
    ToolParamBlock* const LCBox = Gtk::manage(new ToolParamBlock());
    LCBox->pack_start(*ghs_LC);
    LCBox->pack_start(*ghs_MID);
    LC_Frame->add(*LCBox);

    ghsBox->pack_start(*LC_Frame);
    ghsBox->pack_start(*ghs_smooth);
    ghsBox->pack_start(*ghsmidLabel);
    ghsBox->pack_start(*ghsmaxrgbLabel);

    ghsFrame->add(*ghsBox);
    pack_start(*ghsFrame);

    michFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const michBox = Gtk::manage(new ToolParamBlock());
    ToolParamBlock* const michBox2 = Gtk::manage(new ToolParamBlock());
    michtone_Frame->set_label_align(0.025, 0.5);
    michBox->pack_start(*mich_exp);
    michBox->pack_start(*mich_spar);
    michBox->pack_start(*mich_kpar);
    michBox->pack_start(*mich_jdx);
    michtone_Frame->add(*michBox);
    michBox2->pack_start(*michtone_Frame);
    michBox2->pack_start(*mich_sat);
    michBox2->pack_start(*mich_out);
    michBox2->pack_start(*mich_black);
    michBox2->pack_start(*mich_white);
    michBox2->pack_start(*mich_high);
    michBox2->pack_start(*mich_mtf);
    michBox2->pack_start(*michbwLabel);
    michBox2->pack_start(*michdataLabel);
    michFrame->add(*michBox2);
    pack_start(*michFrame);

    for (const auto multiplier : multipliersh) {
        pack_start(*multiplier);
    }

    pack_start(*detailSH);
    pack_start(*tePivot);
    pack_start(*highlights);
    pack_start(*h_tonalwidth);
    pack_start(*shadows);
    pack_start(*s_tonalwidth);
    pack_start(*sh_radius);
    pack_start(*blurSHde);
    ToolParamBlock* const shBox3 = Gtk::manage(new ToolParamBlock());
    shBox3->pack_start(*maskusables, Gtk::PACK_SHRINK, 0);
    shBox3->pack_start(*maskunusables, Gtk::PACK_SHRINK, 0);
    shBox3->pack_start(*recothress);
    shBox3->pack_start(*lowthress);
    shBox3->pack_start(*higthress);
    shBox3->pack_start(*decays);
    exprecovs->add(*shBox3, false);
    pack_start(*exprecovs, false, false);
    gamFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const gammBox = Gtk::manage(new ToolParamBlock());
    gammBox->pack_start(*gamSH);
    gammBox->pack_start(*sloSH);
    gamFrame->add(*gammBox);
    pack_start(*gamFrame);

    ToolParamBlock* const gradSHBox = Gtk::manage(new ToolParamBlock());
    gradSHBox->pack_start(*strSH);
    gradSHBox->pack_start(*angSH);
    gradSHBox->pack_start(*featherSH);
    expgradsh->add(*gradSHBox, false);
    pack_start(*expgradsh, false, false);
    ToolParamBlock* const maskSHBox = Gtk::manage(new ToolParamBlock());
    maskSHBox->pack_start(*showmaskSHMethod, Gtk::PACK_SHRINK, 4);
    maskSHBox->pack_start(*showmaskSHMethodinv, Gtk::PACK_SHRINK, 4);
    maskSHBox->pack_start(*enaSHMask, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*maskSHCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    maskSHBox->pack_start(*blendmaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*radmaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*lapmaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*chromaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*gammaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*slomaskSH, Gtk::PACK_SHRINK, 0);
    maskSHBox->pack_start(*mask2SHCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    ToolParamBlock* const fatSHBox = Gtk::manage(new ToolParamBlock());
    fatSHBox->pack_start(*fatamountSH);
    fatSHBox->pack_start(*fatanchorSH);
    fatSHFrame->add(*fatSHBox);
    expmasksh->add(*maskSHBox, false);
    pack_start(*expmasksh, false, false);
}

void LocallabShadow::adjusterAutoToggled(Adjuster* a, bool newval)
{    
    if (listener && a == ghs_SP) {
        if (newval == true) {
            nbsym2 = 1;
        } else {
            nbsym2 = 0;
        }
        auto e = (!newval) ? EvlocallabautoSPoff : EvlocallabautoSPson;
        listener->panelChanged(e, newval ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }
}

void LocallabShadow::autoSPChanged(float radius)
{
    idle_register.add(
        [this, radius]() -> bool
        {
            disableListener();
            if (radius < 0) {
                ghs_SP->delAutoButton();
            } else {
                ghs_SP->addAutoButton(M("TP_LOCALLAB_SPRADIUS_TOOLTIP"));
                ghs_SP->setValue(radius);
            }

            enableListener();

            if (nbsym2) {
                nbsym2 = 0;
                adjusterChanged(ghs_SP, 0);
            }
            return false;
        }
    );
}



LocallabShadow::~LocallabShadow()
{
    delete maskSHCurveEditorG;
    delete mask2SHCurveEditorG;
}

bool LocallabShadow::isMaskViewActive()
{
    return ((showmaskSHMethod->get_active_row_number() != 0) || (showmaskSHMethodinv->get_active_row_number() != 0));
}

void LocallabShadow::resetMaskView()
{
    showmaskSHMethodConn.block(true);
    showmaskSHMethodConninv.block(true);

    showmaskSHMethod->set_active(0);
    showmaskSHMethodinv->set_active(0);

    showmaskSHMethodConn.block(false);
    showmaskSHMethodConninv.block(false);
}

Gtk::ToggleButton *LocallabShadow::getPreviewDeltaEButton() const
{
    return previewsh;
}

sigc::connection *LocallabShadow::getPreviewDeltaEButtonConnection()
{
    return &previewshConn;
}

void LocallabShadow::previewshChanged()
{

    if(previewsh->get_active()) {
        showmaskSHMethod->set_active(4);
    } else {
        showmaskSHMethod->set_active(0);
    }

    if (isLocActivated) {
        if (listener) {
            listener->panelChanged(Evlocallabpreviewsh,"");
        }
    } 
}


//new function Global
void LocallabShadow::updateguishad(int spottype)
{
    {
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                inverssh->hide();
                sensihs->hide();
                previewsh->hide();
                exprecovs->hide();
                expmasksh->hide();
                enaSHMask->set_active(false);
                previewsh->set_active(false);
                blurSHde->hide();
            } else {
                sensihs->show();
                inverssh->show();
                if (shMethod->get_active_row_number() == 2  || shMethod->get_active_row_number() == 3) {
                    inverssh->hide();
                    inverssh->set_active(false);
                }

                exprecovs->show();
                expmasksh->show();
                if(!inverssh->get_active()) {
                    previewsh->show();
                } else {
                    previewsh->hide();
                }
                updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));
                
            }
            enableListener();

        return false;
        }
        );
    }
   
}

void LocallabShadow::updateguiscopesahd(int scope)
{
    {
        idle_register.add(
        [this, scope]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            disableListener();
            sensihs->setValue(scope);

            enableListener();
        return false;
        }
        );
    }
   
}



void LocallabShadow::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    shMask = showmaskSHMethod->get_active_row_number();
    shMaskinv = showmaskSHMethodinv->get_active_row_number();
}

void LocallabShadow::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        exp->set_tooltip_text(M("TP_LOCALLAB_SHADOWHIGHLIGHT_TOOLTIP"));

        for (const auto multiplier : multipliersh) {
            multiplier->set_tooltip_text(M("TP_LOCALLAB_MULTIPL_TOOLTIP"));
        }
        recothress->set_tooltip_text(M("TP_LOCALLAB_RECOTHRES02_TOOLTIP"));
        gamSH->set_tooltip_text(M("TP_LOCALLAB_SHTRC_TOOLTIP"));
        reparsh->set_tooltip_text(M("TP_LOCALLAB_REPARSH_TOOLTIP"));
        expgradsh->set_tooltip_text(M("TP_LOCALLAB_EXPGRADCOL_TOOLTIP"));

        sloSH->set_tooltip_text(M("TP_LOCALLAB_SHTRC_TOOLTIP"));
        strSH->set_tooltip_text(M("TP_LOCALLAB_GRADGEN_TOOLTIP"));
        exprecovs->set_tooltip_markup(M("TP_LOCALLAB_MASKRESH_TOOLTIP"));
        expmasksh->set_tooltip_markup(M("TP_LOCALLAB_MASK_TOOLTIP"));
        blurSHde->set_tooltip_text(M("TP_LOCALLAB_BLURCOLDE_TOOLTIP"));
        CCmaskSHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        LLmaskSHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        HHmaskSHshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        blendmaskSH->set_tooltip_text(M("TP_LOCALLAB_BLENDMASK_TOOLTIP"));
        radmaskSH->set_tooltip_text(M("TP_LOCALLAB_LAPRAD_TOOLTIP"));
        mask2SHCurveEditorG->set_tooltip_text(M("TP_LOCALLAB_CONTRASTCURVMASK_TOOLTIP"));
        LmaskSHshape->setTooltip(M("TP_LOCALLAB_LMASK_LL_TOOLTIP"));
        maskSHCurveEditorG->set_tooltip_markup(M("TP_LOCALLAB_MASKCURVE_TOOLTIP"));
        gammaskSH->set_tooltip_text(M("TP_LOCALLAB_GAMMASK_TOOLTIP"));
        chromaskSH->set_tooltip_text(M("TP_LOCALLAB_CHROMASK_TOOLTIP"));
        slomaskSH->set_tooltip_text(M("TP_LOCALLAB_SLOMASK_TOOLTIP"));
        lapmaskSH->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
        ghsmaxrgbLabel->set_tooltip_text(M("TP_LOCALLAB_GHSMAXRGB_TOOLTIP"));
        ghsmidLabel->set_tooltip_text(M("TP_LOCALLAB_GHSMAXRGB_TOOLTIP"));
        mich_exp->set_tooltip_text(M("TP_LOCALLAB_MICH_TOOLTIP"));
        mich_high->set_tooltip_text(M("TP_LOCALLAB_MICHDR_TOOLTIP"));
        mich_black->set_tooltip_text(M("TP_LOCALLAB_MICHDR_TOOLTIP"));
        mich_white->set_tooltip_text(M("TP_LOCALLAB_MICHDR_TOOLTIP"));
        mich_jdx->set_tooltip_text(M("TP_LOCALLAB_MICHJDX_TOOLTIP"));
        michbwLabel->set_tooltip_text(M("TP_LOCALLAB_MICHBLWH_TOOLTIP"));
        mich_mtf->set_tooltip_text(M("TP_LOCALLAB_MICHMTF_TOOLTIP"));
        /*
        highlights->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        h_tonalwidth->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        shadows->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        s_tonalwidth->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        sh_radius->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        */
        highlights->set_tooltip_text("");
        h_tonalwidth->set_tooltip_text("");
        shadows->set_tooltip_text("");
        s_tonalwidth->set_tooltip_text("");
        sh_radius->set_tooltip_text("");
        decays->set_tooltip_text(M("TP_LOCALLAB_MASKDECAY_TOOLTIP"));
        lowthress->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRESS_TOOLTIP"));
        higthress->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRESS_TOOLTIP"));
        ghs_D->set_tooltip_text(M("TP_LOCALLAB_GHS_D_TOOLTIP"));
        ghs_slope->set_tooltip_text(M("TP_LOCALLAB_GHS_SLOPE_TOOLTIP"));
        ghs_chro->set_tooltip_text(M("TP_LOCALLAB_GHS_CHRO_TOOLTIP"));
        ghs_B->set_tooltip_text(M("TP_LOCALLAB_GHS_B_TOOLTIP"));
        ghs_SP->set_tooltip_text(M("TP_LOCALLAB_GHS_SP_TOOLTIP"));
        ghs_LP->set_tooltip_text(M("TP_LOCALLAB_GHS_LP_TOOLTIP"));
        ghs_HP->set_tooltip_text(M("TP_LOCALLAB_GHS_HP_TOOLTIP"));
        ghs_LC->set_tooltip_text(M("TP_LOCALLAB_GHS_LC_TOOLTIP"));
        ghs_MID->set_tooltip_text(M("TP_LOCALLAB_GHS_MID_TOOLTIP"));
        ghs_BLP->set_tooltip_text(M("TP_LOCALLAB_GHS_BLP_TOOLTIP"));
        ghs_autobw->set_tooltip_text(M("TP_LOCALLAB_GHS_BLPHLPAUTO_TOOLTIP"));
        ghs_agx->set_tooltip_text(M("TP_LOCALLAB_GHS_AGX_TOOLTIP"));
        ghs_mtf->set_tooltip_text(M("TP_LOCALLAB_GHS_MTF_TOOLTIP"));
        matHBox->set_tooltip_text(M("TP_LOCALLAB_GHS_AGXMAT2_TOOLTIP"));
        ghsMatmet->set_tooltip_text(M("TP_LOCALLAB_GHS_AGX_MAT_TOOLTIP"));
        ghs_HLP->set_tooltip_text(M("TP_LOCALLAB_GHS_HLP_TOOLTIP"));
        ghs_smooth->set_tooltip_text(M("TP_LOCALLAB_GHS_SMOOTH_TOOLTIP"));
        ghs_inv->set_tooltip_text(M("TP_LOCALLAB_GHS_INV_TOOLTIP"));
        BP_Frame->set_tooltip_text(M("TP_LOCALLAB_GHS_BPFRAME_TOOLTIP"));
        ghsFrame->set_tooltip_text(M("TP_LOCALLAB_GHS_METHOD_TOOLTIP"));
        gridFrameghs->set_tooltip_text(M("TP_LOCALLAB_GHS_SIMUL_TOOLTIP"));
    } else {
        exp->set_tooltip_text("");

        for (const auto multiplier : multipliersh) {
            multiplier->set_tooltip_text("");
        }
        recothress->set_tooltip_text("");
        gamSH->set_tooltip_text("");
        reparsh->set_tooltip_text("");
        sloSH->set_tooltip_text("");
        strSH->set_tooltip_text("");
        blurSHde->set_tooltip_text("");
        expmasksh->set_tooltip_markup("");
        CCmaskSHshape->setTooltip("");
        LLmaskSHshape->setTooltip("");
        HHmaskSHshape->setTooltip("");
        blendmaskSH->set_tooltip_text("");
        radmaskSH->set_tooltip_text("");
        lapmaskSH->set_tooltip_text("");
        ghsmaxrgbLabel->set_tooltip_text("");
        ghsmidLabel->set_tooltip_text("");
        mask2SHCurveEditorG->set_tooltip_text("");
        LmaskSHshape->setTooltip("");
        maskSHCurveEditorG->set_tooltip_markup("");
        gammaskSH->set_tooltip_text("");
        chromaskSH->set_tooltip_text("");
        slomaskSH->set_tooltip_text("");
        highlights->set_tooltip_text("");
        h_tonalwidth->set_tooltip_text("");
        shadows->set_tooltip_text("");
        s_tonalwidth->set_tooltip_text("");
        sh_radius->set_tooltip_text("");
        exprecovs->set_tooltip_markup("");
        decays->set_tooltip_text("");
        lowthress->set_tooltip_text("");
        higthress->set_tooltip_text("");
        ghs_D->set_tooltip_text("");
        ghs_slope->set_tooltip_text("");
        ghs_chro->set_tooltip_text("");
        ghs_B->set_tooltip_text("");
        ghs_SP->set_tooltip_text("");
        ghs_LP->set_tooltip_text("");
        ghs_HP->set_tooltip_text("");
        ghs_LC->set_tooltip_text("");
        ghs_MID->set_tooltip_text("");
        ghs_BLP->set_tooltip_text("");
        ghs_HLP->set_tooltip_text("");
        ghs_autobw->set_tooltip_text("");
        ghsMatmet->set_tooltip_text("");
        ghs_agx->set_tooltip_text("");
        ghs_mtf->set_tooltip_text("");
        matHBox->set_tooltip_text("");
        ghs_smooth->set_tooltip_text("");
        ghs_inv->set_tooltip_text("");
        BP_Frame->set_tooltip_text("");
        ghsFrame->set_tooltip_text("");
        gridFrameghs->set_tooltip_text("");
        expgradsh->set_tooltip_text("");
        mich_exp->set_tooltip_text("");
        mich_high->set_tooltip_text("");
        mich_jdx->set_tooltip_text("");
        mich_black->set_tooltip_text("");
        mich_white->set_tooltip_text("");
        michbwLabel->set_tooltip_text("");
        mich_mtf->set_tooltip_text("");

    }
}

void LocallabShadow::setDefaultExpanderVisibility()
{
    exprecovs->set_expanded(false);
    expgradsh->set_expanded(false);
    expmasksh->set_expanded(false);
}

void LocallabShadow::disableListener()
{
    LocallabTool::disableListener();
    shMethodConn.block(true);
    ghsMethodConn.block(true);
    ghsMatmetConn.block(true);
    inversshConn.block(true);
    ghs_smoothConn.block(true);
    ghs_autobwConn.block(true);
    ghs_agxConn.block(true);
    ghs_mtfConn.block(true);
    ghs_invConn.block(true);
    mich_blackConn.block(true);
    mich_whiteConn.block(true);
    mich_jdxConn.block(true);
    showmaskSHMethodConn.block(true);
    showmaskSHMethodConninv.block(true);
    enaSHMaskConn.block(true);
}

void LocallabShadow::enableListener()
{
    LocallabTool::enableListener();
    shMethodConn.block(false);
    ghsMethodConn.block(false);
    ghsMatmetConn.block(false); 
    ghs_smoothConn.block(false);
    ghs_autobwConn.block(false);
    ghs_agxConn.block(false);
    ghs_mtfConn.block(false);
    ghs_invConn.block(false);
    mich_blackConn.block(false);
    mich_whiteConn.block(false);
    mich_jdxConn.block(false);
    inversshConn.block(false);
    showmaskSHMethodConn.block(false);
    showmaskSHMethodConninv.block(false);
    enaSHMaskConn.block(false);
}

void LocallabShadow::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();
    nbmasksh = 0;
    nbwb = 0;
    nbsym2 = 0;
    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visishadhigh);
        exp->setEnabled(spot.expshadhigh);
        complexity->set_active(spot.complexshadhigh);

        if (spot.shMethod == "std") {
            shMethod->set_active(0);
        } else if (spot.shMethod == "tone") {
            shMethod->set_active(1);
        } else if (spot.shMethod == "ghs") {//Generalized Hyperbolic Stretch
            shMethod->set_active(2);
        } else if (spot.shMethod == "micha") {//Michaelis-Menten algorithm
            shMethod->set_active(3);
        }

        if (spot.ghsMethod == "rgb") {
            ghsMethod->set_active(0);
            Framemtf->show();
        } else if (spot.ghsMethod == "rgbstd") {
            ghsMethod->set_active(1);
            Framemtf->show();
        } else if (spot.ghsMethod == "llab") {
            ghsMethod->set_active(2);
            Framemtf->hide();
        } else if (spot.ghsMethod == "lum") {
            ghsMethod->set_active(3);
            Framemtf->hide();
        } else if (spot.ghsMethod == "sat") {
            ghsMethod->set_active(4);
            Framemtf->hide();
        } else if (spot.ghsMethod == "hue") {
            ghsMethod->set_active(5);
            Framemtf->hide();
        }

        if (spot.ghsMatmet == "none") {
            ghsMatmet->set_active(0);
        } else if (spot.ghsMatmet == "agx") {
            ghsMatmet->set_active(1);
        } else if (spot.ghsMatmet == "JZxyz") {//Same comment as "JZ" under but without cognitive bias, by using XYZ data instead of RGB.
            ghsMatmet->set_active(2);
        } else if (spot.ghsMatmet == "cat16xyz") {//Same comment as "cat16" under but without cognitive bias, by using XYZ data instead of RGB.
            ghsMatmet->set_active(3);
        } else if (spot.ghsMatmet == "JZ") {//I chose JZ rather than JzAzBz because we're dealing with a cognitive bias in RGB mode.
                                            //it corresponds to original LMS JzAzBz matrix without PQ, whitout Absolute luminance, whitout "az and bz" and applied to RGB values
                                            //If I had chosen JzAzBz, the reader might believe we are using the JzAzBz model, which is not the case.
                                            //But to 'simplify' the GUI, JzAzBz and Cat16 appear... this reassures users, but it's more than likely false.
            ghsMatmet->set_active(4);
        } else if (spot.ghsMatmet == "cat16") {//It's essentially the same for Cat16 as for JZ. Note that this is not CIECAM, but only the use of the conversion matrix
                                               //it's a simplifying cognitive bias, but I kept the term here because the difference is often smaller.
            ghsMatmet->set_active(5);
        }

        for (int i = 0; i < 6; i++) {
            multipliersh[i]->setValue((double)spot.multsh[i]);
        }
        recothress->setValue((double)spot.recothress);
        lowthress->setValue((double)spot.lowthress);
        higthress->setValue((double)spot.higthress);
        decays->setValue((double)spot.decays);

        ghs_D->setValue((double)spot.ghs_D);
        ghs_mtfstr->setValue((double)spot.ghs_mtfstr);
        ghs_slope->setValue((double)spot.ghs_slope);
        ghs_chro->setValue((double)spot.ghs_chro);
        ghs_B->setValue((double)spot.ghs_B);
        ghs_SP->setValue((double)spot.ghs_SP);
        ghs_SP->setAutoValue(spot.SPAutoRadius);

        ghs_LP->setValue((double)spot.ghs_LP);
        ghs_HP->setValue((double)spot.ghs_HP);
        ghs_LC->setValue((double)spot.ghs_LC);
        ghs_MID->setValue((double)spot.ghs_MID);
        ghs_BLP->setValue((double)spot.ghs_BLP);
        ghs_HLP->setValue((double)spot.ghs_HLP);
        if (ghs_D->getValue() == 0.f) {
            ghs_BLP->set_sensitive(false);
            ghs_HLP->set_sensitive(false);
            ghs_inv->set_sensitive(false);
            ghs_autobw->set_sensitive(false);
            ghs_agx->set_sensitive(false);
            ghsMatmet->set_sensitive(false);
            ghs_LC->set_sensitive(true); 
            ghs_MID->set_sensitive(true);
        } else {
            if (ghs_autobw->get_active()) {
                ghs_BLP->set_sensitive(false);
                ghs_HLP->set_sensitive(false);
                ghs_inv->set_sensitive(false);
                ghs_agx->set_sensitive(false);
                ghsMatmet->set_sensitive(false);
            } else {
                ghs_BLP->set_sensitive(true);
                ghs_HLP->set_sensitive(true);
                ghs_agx->set_sensitive(true);
                ghs_inv->set_sensitive(true);
                ghsMatmet->set_sensitive(true);
            }
            ghs_autobw->set_sensitive(true);
            if(ghs_inv->get_active()) {
                ghs_autobw->set_sensitive(false);
                ghs_autobw->set_active(false);
            }
            ghs_LC->set_sensitive(true); 
            ghs_MID->set_sensitive(true); 
        }
        if (ghs_D->getValue() == 0.f) {
            ghs_LC->set_sensitive(false);
            ghs_MID->set_sensitive(false);
        }
        mich_exp->setValue((double)spot.mich_exp);
        mich_spar->setValue((double)spot.mich_spar);
        mich_kpar->setValue((double)spot.mich_kpar);
        mich_sat->setValue((double)spot.mich_sat);
        mich_out->setValue((double)spot.mich_out);
        mich_black->set_active(spot.mich_black);
        mich_white->set_active(spot.mich_white);
        mich_high->setValue((double)spot.mich_high);
        mich_mtf->setValue((double)spot.mich_mtf);
        mich_jdx->set_active(spot.mich_jdx);
        detailSH->setValue((double)spot.detailSH);
        tePivot->setValue(spot.tePivot);
        reparsh->setValue(spot.reparsh);
        highlights->setValue((double)spot.highlights);
        h_tonalwidth->setValue((double)spot.h_tonalwidth);
        shadows->setValue(spot.shadows);
        s_tonalwidth->setValue((double)spot.s_tonalwidth);
        sh_radius->setValue((double)spot.sh_radius);
        sensihs->setValue((double)spot.sensihs);
        blurSHde->setValue((double)spot.blurSHde);
        gamSH->setValue(spot.gamSH);
        sloSH->setValue(spot.sloSH);
        strSH->setValue(spot.strSH);
        angSH->setValue(spot.angSH);
        featherSH->setValue(spot.featherSH);
        inverssh->set_active(spot.inverssh);
        ghs_smooth->set_active(spot.ghs_smooth);
        ghs_autobw->set_active(spot.ghs_autobw);
        ghs_agx->set_active(spot.ghs_agx);
        ghs_mtf->set_active(spot.ghs_mtf);
        ghs_inv->set_active(spot.ghs_inv);
        enaSHMask->set_active(spot.enaSHMask);
        CCmaskSHshape->setCurve(spot.CCmaskSHcurve);
        LLmaskSHshape->setCurve(spot.LLmaskSHcurve);
        HHmaskSHshape->setCurve(spot.HHmaskSHcurve);
        blendmaskSH->setValue((double)spot.blendmaskSH);
        radmaskSH->setValue(spot.radmaskSH);
        lapmaskSH->setValue(spot.lapmaskSH);
        chromaskSH->setValue(spot.chromaskSH);
        gammaskSH->setValue(spot.gammaskSH);
        slomaskSH->setValue(spot.slomaskSH);
        LmaskSHshape->setCurve(spot.LmaskSHcurve);
        fatamountSH->setValue(spot.fatamountSH);
        fatanchorSH->setValue(spot.fatanchorSH);
    }
    ghsMethodChanged();
    ghsMatmetChanged();
    update_ghs_curve(
        ghs_B->getValue(),
        ghs_D->getValue(),
        ghs_LP->getValue(),
        ghs_SP->getValue(),
        ghs_HP->getValue(),
        ghs_inv->get_active(),
        *labgridghs);
        
    mich_whiteChanged();
    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    // Update shadow highlight GUI according to shMethod and ghsmethod combobox state
    updateShadowGUIshmet();
    updateShadowGUIsym();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabShadow::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expshadhigh = exp->getEnabled();
        spot.visishadhigh = exp->get_visible();
        spot.complexshadhigh = complexity->get_active_row_number();

        if (shMethod->get_active_row_number() == 0) {
            spot.shMethod = "std";
        } else if (shMethod->get_active_row_number() == 1) {
            spot.shMethod = "tone";
        } else if (shMethod->get_active_row_number() == 2) {//Generalized hyperbolic stretch
            spot.shMethod = "ghs";
        } else if (shMethod->get_active_row_number() == 3) {//Michaelis-Menten algorithm
            spot.shMethod = "micha";
        }

        if (ghsMethod->get_active_row_number() == 0) {
            spot.ghsMethod = "rgb";
        } else if (ghsMethod->get_active_row_number() == 1) {
            spot.ghsMethod = "rgbstd";
        } else if (ghsMethod->get_active_row_number() == 2) {
            spot.ghsMethod = "llab";
        } else if (ghsMethod->get_active_row_number() == 3) {
            spot.ghsMethod = "lum";
        } else if (ghsMethod->get_active_row_number() == 4) {
            spot.ghsMethod = "sat";
        } else if (ghsMethod->get_active_row_number() == 5) {
            spot.ghsMethod = "hue";
        }


        if (ghsMatmet->get_active_row_number() == 0) {
            spot.ghsMatmet = "none";
        } else if (ghsMatmet->get_active_row_number() == 1) {
            spot.ghsMatmet = "agx";
        } else if (ghsMatmet->get_active_row_number() == 2) {
            spot.ghsMatmet = "JZxyz";
        } else if (ghsMatmet->get_active_row_number() == 3) {
            spot.ghsMatmet = "cat16xyz";
        } else if (ghsMatmet->get_active_row_number() == 4) {
            spot.ghsMatmet = "JZ";
        } else if (ghsMatmet->get_active_row_number() == 5) {
            spot.ghsMatmet = "cat16";
        }

         for (int i = 0; i < 6; i++) {
            spot.multsh[i] = multipliersh[i]->getIntValue();
        }

        spot.ghs_D = ghs_D->getValue();
        spot.ghs_mtfstr = ghs_mtfstr->getValue();
        spot.ghs_slope = ghs_slope->getValue();
        spot.ghs_chro = ghs_chro->getValue();
        spot.ghs_B = ghs_B->getValue();
        spot.ghs_SP = ghs_SP->getValue();
        spot.SPAutoRadius = ghs_SP->getAutoValue();
        spot.ghs_LP = ghs_LP->getValue();
        spot.ghs_HP = ghs_HP->getValue();
        spot.ghs_LC = ghs_LC->getValue();
        spot.ghs_MID = ghs_MID->getValue();
        spot.ghs_BLP = ghs_BLP->getValue();
        spot.ghs_HLP = ghs_HLP->getValue();

        spot.detailSH = detailSH->getIntValue();
        spot.tePivot = tePivot->getValue();
        spot.reparsh = reparsh->getValue();
        spot.highlights = highlights->getIntValue();
        spot.h_tonalwidth = h_tonalwidth->getIntValue();
        spot.shadows = shadows->getIntValue();
        spot.s_tonalwidth = s_tonalwidth->getIntValue();
        spot.sh_radius = sh_radius->getIntValue();
        spot.sensihs = sensihs->getIntValue();
        spot.blurSHde = blurSHde->getIntValue();
        spot.gamSH = gamSH->getValue();
        spot.sloSH = sloSH->getValue();
        spot.strSH = strSH->getValue();
        spot.angSH = angSH->getValue();
        spot.featherSH = featherSH->getValue();
        spot.inverssh = inverssh->get_active();
        spot.ghs_smooth = ghs_smooth->get_active();
        spot.ghs_autobw = ghs_autobw->get_active();
        spot.ghs_agx = ghs_agx->get_active();
        spot.ghs_mtf = ghs_mtf->get_active();
        spot.ghs_inv = ghs_inv->get_active();
        spot.mich_exp = mich_exp->getValue();
        spot.mich_spar = mich_spar->getValue();
        spot.mich_kpar = mich_kpar->getValue();
        spot.mich_sat = mich_sat->getValue();
        spot.mich_out = mich_out->getValue();
        spot.mich_black = mich_black->get_active();
        spot.mich_white = mich_white->get_active();
        spot.mich_high = mich_high->getValue();
        spot.mich_jdx = mich_jdx->get_active();
        spot.mich_mtf = mich_mtf->getValue();

        spot.enaSHMask = enaSHMask->get_active();
        spot.LLmaskSHcurve = LLmaskSHshape->getCurve();
        spot.CCmaskSHcurve = CCmaskSHshape->getCurve();
        spot.HHmaskSHcurve = HHmaskSHshape->getCurve();
        spot.blendmaskSH = blendmaskSH->getIntValue();
        spot.radmaskSH = radmaskSH->getValue();
        spot.lapmaskSH = lapmaskSH->getValue();
        spot.chromaskSH = chromaskSH->getValue();
        spot.gammaskSH = gammaskSH->getValue();
        spot.slomaskSH = slomaskSH->getValue();
        spot.LmaskSHcurve = LmaskSHshape->getCurve();
        spot.fatamountSH = fatamountSH->getValue();
        spot.fatanchorSH = fatanchorSH->getValue();
        spot.recothress = recothress->getValue();
        spot.lowthress = lowthress->getValue();
        spot.higthress = higthress->getValue();
        spot.decays = decays->getValue();
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabShadow::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default values for adjuster widgets
        for (int i = 0; i < 6; i++) {
            multipliersh[i]->setDefault(defSpot.multsh[i]);
        }

        ghs_D->setDefault(defSpot.ghs_D);
        ghs_mtfstr->setDefault(defSpot.ghs_mtfstr);
        ghs_slope->setDefault(defSpot.ghs_slope);
        ghs_chro->setDefault(defSpot.ghs_chro);
        ghs_B->setDefault(defSpot.ghs_B);
        ghs_SP->setDefault(defSpot.ghs_SP);
        ghs_LP->setDefault(defSpot.ghs_LP);
        ghs_HP->setDefault(defSpot.ghs_HP);
        ghs_LC->setDefault(defSpot.ghs_LC);
        ghs_MID->setDefault(defSpot.ghs_MID);
        ghs_BLP->setDefault(defSpot.ghs_BLP);
        ghs_HLP->setDefault(defSpot.ghs_HLP);

        mich_exp->setDefault(defSpot.mich_exp);
        mich_spar->setDefault(defSpot.mich_spar);
        mich_kpar->setDefault(defSpot.mich_kpar);
        mich_sat->setDefault(defSpot.mich_sat);
        mich_out->setDefault(defSpot.mich_out);
        mich_high->setDefault(defSpot.mich_high);
        mich_mtf->setDefault(defSpot.mich_mtf);

        detailSH->setDefault((double)defSpot.detailSH);
        tePivot->setDefault(defSpot.tePivot);
        reparsh->setDefault(defSpot.reparsh);
        highlights->setDefault((double)defSpot.highlights);
        h_tonalwidth->setDefault((double)defSpot.h_tonalwidth);
        shadows->setDefault((double)defSpot.shadows);
        s_tonalwidth->setDefault((double)defSpot.s_tonalwidth);
        sh_radius->setDefault((double)defSpot.sh_radius);
        sensihs->setDefault((double)defSpot.sensihs);
        blurSHde->setDefault((double)defSpot.blurSHde);
        gamSH->setDefault(defSpot.gamSH);
        sloSH->setDefault(defSpot.sloSH);
        strSH->setDefault(defSpot.strSH);
        angSH->setDefault(defSpot.angSH);
        featherSH->setDefault(defSpot.featherSH);
        blendmaskSH->setDefault((double)defSpot.blendmaskSH);
        radmaskSH->setDefault(defSpot.radmaskSH);
        lapmaskSH->setDefault(defSpot.lapmaskSH);
        chromaskSH->setDefault(defSpot.chromaskSH);
        gammaskSH->setDefault(defSpot.gammaskSH);
        slomaskSH->setDefault(defSpot.slomaskSH);
        fatamountSH->setDefault(defSpot.fatamountSH);
        fatanchorSH->setDefault(defSpot.fatanchorSH);
        recothress->setDefault((double)defSpot.recothress);
        lowthress->setDefault((double)defSpot.lowthress);
        higthress->setDefault((double)defSpot.higthress);
        decays->setDefault((double)defSpot.decays);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabShadow::adjusterChanged(Adjuster* a, double newval)
{
    updateShadowGUIsym();

    if (isLocActivated && exp->getEnabled()) {
        if (a == multipliersh[0] || a == multipliersh[1] || a == multipliersh[2] || a == multipliersh[3] || a == multipliersh[4] || a == multipliersh[5]) {
            if (listener) {
                listener->panelChanged(EvlocallabEqualizersh,
                                       Glib::ustring::compose("%1, %2, %3, %4, %5, %6",
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[0]->getIntValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[1]->getIntValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[2]->getIntValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[3]->getIntValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[4]->getIntValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(2), multipliersh[5]->getIntValue())) + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_D) {
            if (ghs_D->getValue() == 0.f) {
                ghs_BLP->set_sensitive(false);
                ghs_HLP->set_sensitive(false);
                ghs_inv->set_sensitive(false);
                ghs_autobw->set_sensitive(false);
                ghs_agx->set_sensitive(false);
                ghsMatmet->set_sensitive(false);
                ghs_LC->set_sensitive(true);
                ghs_MID->set_sensitive(true);
            } else {
                if (ghs_autobw->get_active()) {
                    ghs_BLP->set_sensitive(false);
                    ghs_HLP->set_sensitive(false);
                    ghs_inv->set_sensitive(false);
                    ghs_agx->set_sensitive(false);
                    ghsMatmet->set_sensitive(false);
                } else {
                    ghs_BLP->set_sensitive(true);
                    ghs_HLP->set_sensitive(true);
                    ghs_inv->set_sensitive(true);
                    ghs_agx->set_sensitive(true);
                    ghsMatmet->set_sensitive(true);
                }
                ghs_autobw->set_sensitive(true);
                if (ghs_inv->get_active()) {
                    ghs_autobw->set_sensitive(false);
                    ghs_autobw->set_active(false);
                }
                ghs_LC->set_sensitive(true);
                ghs_MID->set_sensitive(true);
            }
            if (ghs_D->getValue() == 0.f) {
                ghs_LC->set_sensitive(false);
                ghs_MID->set_sensitive(false);
            }

            if (listener) {
                listener->panelChanged(Evlocallabghs_D,
                                       ghs_D->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_mtfstr) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_mtfstr,
                                       ghs_mtfstr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_slope) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_slope,
                                       ghs_slope->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_chro) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_chro,
                                       ghs_chro->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_B) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_B,
                                       ghs_B->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_SP) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_SP,
                                       ghs_SP->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_LP) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_LP,
                                       ghs_LP->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_HP) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_HP,
                                       ghs_HP->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_LC) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_LC,
                                       ghs_LC->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_MID) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_MID,
                                       ghs_MID->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_BLP) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_BLP,
                                       ghs_BLP->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_HLP) {
            if (listener) {
                listener->panelChanged(Evlocallabghs_HLP,
                                       ghs_HLP->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_exp) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_exp,
                                       mich_exp->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_spar) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_spar,
                                       mich_spar->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_kpar) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_kpar,
                                       mich_kpar->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_sat) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_sat,
                                       mich_sat->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_out) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_out,
                                       mich_out->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_high) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_high,
                                       mich_high->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == mich_mtf) {
            if (listener) {
                listener->panelChanged(Evlocallabmich_mtf,
                                       mich_mtf->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == detailSH) {
            if (listener) {
                listener->panelChanged(EvlocallabdetailSH,
                                       detailSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == tePivot) {
            if (listener) {
                listener->panelChanged(EvlocallabTePivot,
                                       tePivot->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == reparsh) {
            if (listener) {
                listener->panelChanged(Evlocallabreparsh,
                                       reparsh->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == highlights) {
            if (listener) {
                listener->panelChanged(Evlocallabhighlights,
                                       highlights->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == h_tonalwidth) {
            if (listener) {
                listener->panelChanged(Evlocallabh_tonalwidth,
                                       h_tonalwidth->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shadows) {
            if (listener) {
                listener->panelChanged(Evlocallabshadows,
                                       shadows->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == s_tonalwidth) {
            if (listener) {
                listener->panelChanged(Evlocallabs_tonalwidth,
                                       s_tonalwidth->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sh_radius) {
            if (listener) {
                listener->panelChanged(Evlocallabsh_radius,
                                       sh_radius->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothress) {

            if (listener) {
                listener->panelChanged(Evlocallabrecothress,
                                       recothress->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthress) {
            if (listener) {
                listener->panelChanged(Evlocallablowthress,
                                       lowthress->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthress) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthress,
                                       higthress->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == decays) {
            if (listener) {
                listener->panelChanged(Evlocallabdecays,
                                       decays->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensihs) {
            if (listener) {
                listener->panelChanged(Evlocallabsensihs,
                                       sensihs->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blurSHde) {
            if (listener) {
                listener->panelChanged(EvlocallabblurSHde,
                                       blurSHde->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gamSH) {
            if (listener) {
                listener->panelChanged(EvlocallabgamSH,
                                       gamSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sloSH) {
            if (listener) {
                listener->panelChanged(EvlocallabsloSH,
                                       sloSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strSH) {
            if (listener) {
                listener->panelChanged(EvlocallabstrSH,
                                       strSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == angSH) {
            if (listener) {
                listener->panelChanged(EvlocallabangSH,
                                       angSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == featherSH) {
            if (listener) {
                listener->panelChanged(EvlocallabfeatherSH,
                                       featherSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blendmaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallabblendmaskSH,
                                       blendmaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == radmaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallabradmaskSH,
                                       radmaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lapmaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallablapmaskSH,
                                       lapmaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chromaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallabchromaskSH,
                                       chromaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gammaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallabgammaskSH,
                                       gammaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == slomaskSH) {
            if (listener) {
                listener->panelChanged(EvlocallabslomaskSH,
                                       slomaskSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == fatamountSH) {
            if (listener) {
                listener->panelChanged(EvlocallabfatamountSH,
                                       fatamountSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }


        if (a == fatanchorSH) {
            if (listener) {
                listener->panelChanged(EvlocallabfatanchorSH,
                                       fatanchorSH->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == ghs_D ||
            a == ghs_B ||
            a == ghs_SP ||
            a == ghs_LP ||
            a == ghs_HP) {
            update_ghs_curve(
                ghs_B->getValue(),
                ghs_D->getValue(),
                ghs_LP->getValue(),
                ghs_SP->getValue(),
                ghs_HP->getValue(),
                ghs_inv->get_active(),
                *labgridghs);
        }
    }
}

void LocallabShadow::updatemichbw(double michb, double michw, double michmean, double michmax, bool michaut)//Information Black and White point Michaelis and Midgrey & Max RGB
{
    
    idle_register.add(
    [this, michb, michw, michmean, michmax, michaut]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        if (michaut) {//only if user choose one or the two checkbox
            michbwLabel->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_MICHBLWH"),
                                    Glib::ustring::format(std::fixed, std::setprecision(4), michb),
                                    Glib::ustring::format(std::fixed, std::setprecision(4), michw))
            );
        } else {
            michbwLabel->set_text(M("TP_LOCALLAB_MICHBLWHNO"));
        }

        michdataLabel->set_text( //Midgrey & Max RGB
            Glib::ustring::compose(M("TP_LOCALLAB_MICHDATA"),
                                Glib::ustring::format(std::fixed, std::setprecision(4), michmean),
                                Glib::ustring::format(std::fixed, std::setprecision(4), michmax))
        );
        
        
        return false;
    }
   );

}

void LocallabShadow::updateghsbw2(double ghsb, double ghsw, bool ghsaut)//auto GHS black point and white point
{
    idle_register.add(
    [this, ghsb, ghsw, ghsaut]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
        if (ghsaut) {
            if (nbwb) {
                nbwb = 0;
                if (ghsw != ghs_HLP->getValue()) {
                    disableListener();
                    ghs_HLP->setValue(ghsw);
                    enableListener();
                }

                if (ghsb != ghs_BLP->getValue()) {
                    disableListener();
                    ghs_BLP->setValue(ghsb);
                    enableListener();
                }
                listener->panelChanged(Evlocallabghs_autobw, M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
        return false;
    }
   );
  
}



void LocallabShadow::updateghsbw(int bp, int wp, double minbp, double maxwp, double symev, double midgrey, double maxrgb, double sig3, double maxR, double maxG, double maxB, double drghs, bool ghsau) //update informations for Black point and White point
{
    idle_register.add(
    [this, bp, wp, minbp, maxwp, symev, midgrey, maxrgb, sig3, maxR, maxG, maxB, drghs, ghsau]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
        
        if (ghsMethod->get_active_row_number() == 0 || ghsMethod->get_active_row_number() == 1) {//only in RGB mode
            ghssymLabel->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_GHSSYM"),
                                    Glib::ustring::format(std::fixed, std::setprecision(4), symev))
            );
        } else {
            ghssymLabel->set_text(M("TP_LOCALLAB_GHSSYMNO"));
        }

        if (ghsMethod->get_active_row_number() == 0 || ghsMethod->get_active_row_number() == 1) {//only in RGB mode
            ghsmidLabel->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_GHSMIDGREY"),
                                    Glib::ustring::format(std::fixed, std::setprecision(3), midgrey))
            );
        } else {
            ghsmidLabel->set_text(M("TP_LOCALLAB_GHSMIDGREYNO"));
        }

        if (ghsMethod->get_active_row_number() == 0 || ghsMethod->get_active_row_number() == 1) {//only in RGB mode
            ghsmaxrgbLabel->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_GHSMAXRGB"),
                                    Glib::ustring::format(std::fixed, std::setprecision(3), maxrgb),
                                    Glib::ustring::format(std::fixed, std::setprecision(3), sig3))
            );
        } else {
            ghsmaxrgbLabel->set_text(M("TP_LOCALLAB_GHSMAXRGBNO"));
        }

        ghsbpwpLabels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_GHSBPWP"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), bp),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), wp))
        );

        ghsbpwpvalueLabels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_GHSBPWPVALUE"),
                                   Glib::ustring::format(std::fixed, std::setprecision(2), minbp),
                                   Glib::ustring::format(std::fixed, std::setprecision(2), maxwp))
        );
        if (ghsau) {
            ghscolorLabels->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_GHSCOLORVALUE"),
                                    Glib::ustring::format(std::fixed, std::setprecision(2), maxR),
                                    Glib::ustring::format(std::fixed, std::setprecision(2), maxG), 
                                    Glib::ustring::format(std::fixed, std::setprecision(2), maxB))
            );

            ghsDRLabels->set_text(
                Glib::ustring::compose(M("TP_LOCALLAB_GHSDR"),
                                    Glib::ustring::format(std::fixed, std::setprecision(2), drghs))
            );      
        } else {
            ghscolorLabels->set_text(M("TP_LOCALLAB_GHSCOLORNO"));

            ghsDRLabels->set_text(M("TP_LOCALLAB_GHSDRNO"));
                     
        }

        return false;
    }
   );
    
}

void LocallabShadow::curveChanged(CurveEditor* ce)
{
    if (isLocActivated && exp->getEnabled()) {
        if (ce == CCmaskSHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCCmaskSHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskSHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskSHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHmaskSHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHmaskSHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LmaskSHshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLmaskSHshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenashadhigh,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenashadhigh,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    // Set hidden GUI widgets in Normal mode to default spot values
    blurSHde->setValue((double)defSpot.blurSHde);
    lapmaskSH->setValue(defSpot.lapmaskSH);
    gammaskSH->setValue(defSpot.gammaskSH);
    slomaskSH->setValue(defSpot.slomaskSH);
    fatamountSH->setValue(defSpot.fatamountSH);
    fatanchorSH->setValue(defSpot.fatanchorSH);
    decays->setValue(defSpot.decays);
    ghs_slope->setValue(defSpot.ghs_slope);
    updateShadowGUIsym();

    // Enable all listeners
    enableListener();
}

void LocallabShadow::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    // Set hidden specific GUI widgets in Simple mode to default spot values
    ghsMethod->set_active(1);
    ghs_inv->set_active(false);
    gamSH->setValue(defSpot.gamSH);
    sloSH->setValue(defSpot.sloSH);
    showmaskSHMethod->set_active(0);
    showmaskSHMethodinv->set_active(0);
    enaSHMask->set_active(defSpot.enaSHMask);
    recothress->setValue(defSpot.recothress);
    lowthress->setValue(defSpot.lowthress);
    higthress->setValue(defSpot.higthresc);
    decays->setValue(defSpot.decays);
    ghs_mtf->set_active(defSpot.ghs_mtf);
    // Enable all listeners
    enableListener();
}

void LocallabShadow::updateGUIToMode(const modeType new_type)
{
    const LocallabParams::LocallabSpot defSpot;
    
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            blurSHde->hide();
            gamFrame->hide();
            expmasksh->hide();
            exprecovs->hide();
            maskusables->hide();
            maskunusables->hide();
            decays->hide();
            ghsMethod->hide();
            ghs_slope->hide();
            Lab_Frame->hide();
            ghs_inv->hide();
            Framemtf->hide();
            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            blurSHde->hide();
            lapmaskSH->hide();
            gammaskSH->hide();
            slomaskSH->hide();
            fatSHFrame->hide();
            exprecovs->show();

            // Specific Simple mode widgets are shown in Normal mode
            if (shMethod->get_active_row_number() == 1) { // Keep widget hidden when shMethod is equal to 0
                gamFrame->show();
            }
            if (shMethod->get_active_row_number() != 1 ) { // Keep widget hidden when shMethod is equal to 0
                gamFrame->hide();
            }

            if (enaSHMask->get_active()) {
                maskusables->show();
                maskunusables->hide();

            } else {
                maskusables->hide();
                maskunusables->show();
            }

            if (!inverssh->get_active()) { // Keep widget hidden when inverssh is toggled
                exprecovs->show();
            }
            expmasksh->show();
            decays->hide();
            ghsMethod->show();
            Lab_Frame->hide();
            if (ghsMethod->get_active_row_number() == 2  && shMethod->get_active_row_number() == 2) {
                Lab_Frame->show();
            }
            ghs_slope->hide();
            ghs_inv->show();
            if (ghsMethod->get_active_row_number() <= 2) {
                Framemtf->show();
            }
            if (shMethod->get_active_row_number() != 2) {
                ghs_inv->hide();
                ghsMethod->hide();
            }
            break;

        case Expert:
            // Show widgets hidden in Normal and Simple mode
            blurSHde->show();

            if (shMethod->get_active_row_number() == 1) { // Keep widget hidden when shMethod is equal to 0
                gamFrame->show();
            }

            if (shMethod->get_active_row_number() != 1 ) { // Keep widget hidden when shMethod is equal to 0
                gamFrame->hide();
            }

            if (!inverssh->get_active()) { // Keep widget hidden when inverssh is toggled
                exprecovs->show();
            }

            if (enaSHMask->get_active()) {
                maskusables->show();
                maskunusables->hide();

            } else {
                maskusables->hide();
                maskunusables->show();
            }
            exprecovs->show();
            decays->show();
            expmasksh->show();
            lapmaskSH->show();
            gammaskSH->show();
            slomaskSH->show();
            fatSHFrame->show();
            ghsMethod->show();
            Lab_Frame->hide();

            if (ghsMethod->get_active_row_number() == 2  && shMethod->get_active_row_number() == 2) {
                Lab_Frame->show();
            }

            ghs_slope->show();
            ghs_inv->show();
            if (ghsMethod->get_active_row_number() <= 2) {
                Framemtf->show();
            }

            if (shMethod->get_active_row_number() != 2) {
                ghs_inv->hide();
                ghsMethod->hide();
            }
    }
}

void LocallabShadow::updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz)
{
    idle_register.add(
    [this, normHuer, normLumar, normChromar]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        // Update mask background
        CCmaskSHshape->updateLocallabBackground(normChromar);
        LLmaskSHshape->updateLocallabBackground(normLumar);
        HHmaskSHshape->updateLocallabBackground(normHuer);
        LmaskSHshape->updateLocallabBackground(normLumar);
        return false;
    }
    );
}

void LocallabShadow::shMethodChanged()
{

    // Update shadow highlight GUI according to shmethod combobox state
    updateShadowGUIshmet();

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshMethod,
                                   shMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabShadow::ghsMethodChanged()
{
    const int mode = complexity->get_active_row_number();

    // Update shadow highlight GUI according to ghsMethod combobox state
    updateShadowGUIshmet();
    Framemtf->hide();
    if (ghsMethod->get_active_row_number() < 2) {
        Framemtf->show();
    } else {
        Framemtf->hide();
    }

    if (ghsMethod->get_active_row_number() == 2) {
        Lab_Frame->show();
        ghs_slope->hide();
        Framemtf->hide();

        if (mode == Expert) {
            ghs_slope->show();
        }
        ghs_chro->show();
    } else {
        Lab_Frame->hide();
        ghs_slope->hide();
        ghs_chro->hide();
        Lab_Frame->hide();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabghsMethod,
                                   ghsMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabShadow::ghsMatmetChanged()
{

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(Evlocallabghs_Matmet,
                                   ghsMatmet->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabShadow::inversshChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();
    if (shMethod->get_active_row_number() == 2 || shMethod->get_active_row_number() == 3) {//GHS or Michaelis
        inverssh->hide();
        inverssh->set_active(false);
    }

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (inverssh->get_active()) {
                listener->panelChanged(Evlocallabinverssh,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinverssh,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::ghs_agxChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (ghs_agx->get_active()) {
                listener->panelChanged(Evlocallabghs_agx,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabghs_agx,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::ghs_mtfChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (ghs_mtf->get_active()) {
                listener->panelChanged(Evlocallabghs_mtf,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabghs_mtf,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}


void LocallabShadow::mich_jdxChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }


    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (mich_jdx->get_active()) {
                listener->panelChanged(Evlocallabmich_jdx,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabmich_jdx,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::mich_blackChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }


    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (mich_black->get_active()) {
                listener->panelChanged(Evlocallabmich_black,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabmich_black,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::mich_whiteChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (mich_white->get_active()) {
        mich_mtf->set_sensitive(true);
    } else {
        mich_mtf->set_sensitive(false);
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (mich_white->get_active()) {
                listener->panelChanged(Evlocallabmich_white,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabmich_white,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}


void LocallabShadow::ghs_autobwChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (ghs_autobw->get_active()) {
        ghs_BLP->set_sensitive(false);
        ghs_HLP->set_sensitive(false);
    } else {
        ghs_BLP->set_sensitive(true);
        ghs_HLP->set_sensitive(true);
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (ghs_autobw->get_active()) {
                nbwb = 1; // execute multi preview to automatically calculate black point and white point but only once
                listener->panelChanged(Evlocallabghs_autobw,
                                       M("TP_PREPROCESS_LABEL") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                nbwb = 0;
                listener->panelChanged(Evlocallabghs_autobw,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}



void LocallabShadow::ghs_smoothChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (ghs_smooth->get_active()) {
                listener->panelChanged(Evlocallabghs_smooth,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabghs_smooth,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::ghs_invChanged()

{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update shadow highlight GUI according to inverssh button state
    updateShadowGUImask();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (ghs_inv->get_active()) {
                listener->panelChanged(Evlocallabghs_inv,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabghs_inv,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }

    update_ghs_curve(
        ghs_B->getValue(),
        ghs_D->getValue(),
        ghs_LP->getValue(),
        ghs_SP->getValue(),
        ghs_HP->getValue(),
        ghs_inv->get_active(),
        *labgridghs);
}


void LocallabShadow::showmaskSHMethodChanged()
{
    // If mask preview is activated, deactivate other Shadow highlight mask preview
    showmaskSHMethodConninv.block(true);
    showmaskSHMethodinv->set_active(0);
    showmaskSHMethodConninv.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if (exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabShadow::showmaskSHMethodChangedinv()
{
    // If mask preview is activated, deactivate other Shadow highlight mask preview
    showmaskSHMethodConn.block(true);
    showmaskSHMethod->set_active(0);
    showmaskSHMethodConn.block(false);

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if (exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabShadow::enaSHMaskChanged()
{
    if (enaSHMask->get_active()) {
        maskusables->show();
        maskunusables->hide();

    } else {
        maskusables->hide();
        maskunusables->show();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enaSHMask->get_active()) {
                listener->panelChanged(EvLocallabEnaSHMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnaSHMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabShadow::updateShadowGUImask()
{
    const int mode = complexity->get_active_row_number();
    const LocallabParams::LocallabSpot defSpot;
    if (shMethod->get_active_row_number() == 2  || shMethod->get_active_row_number() == 3) {
        inverssh->hide();
        inverssh->set_active(false);
    }

    // Update shadow highlight GUI according to inverssh button state
    if (inverssh->get_active()) {
        expgradsh->hide();
        showmaskSHMethod->hide();
        // Reset hidden mask combobox
        showmaskSHMethodConn.block(true);
        showmaskSHMethod->set_active(0);
        showmaskSHMethodConn.block(false);
        showmaskSHMethodinv->show();
        exprecovs->hide();
        reparsh->hide();
    } else {
        ghsMethod->hide();
        ghs_slope->hide();
        Lab_Frame->hide();
        if (ghs_D->getValue() == 0.f) {
            ghs_BLP->set_sensitive(false);
            ghs_HLP->set_sensitive(false);
            ghs_inv->set_sensitive(false);
            ghs_autobw->set_sensitive(false);
            ghs_agx->set_sensitive(false);
            ghsMatmet->set_sensitive(false);
            ghs_LC->set_sensitive(true); 
            ghs_MID->set_sensitive(true); 
        } else {
            if (ghs_autobw->get_active()) {
                ghs_BLP->set_sensitive(false);
                ghs_HLP->set_sensitive(false);
                ghs_inv->set_sensitive(false);
                ghs_agx->set_sensitive(false);
                ghsMatmet->set_sensitive(false);
            } else {
                ghs_BLP->set_sensitive(true);
                ghs_HLP->set_sensitive(true);
                ghs_inv->set_sensitive(true);
                ghs_agx->set_sensitive(true);
                ghsMatmet->set_sensitive(true);
            }
            ghs_autobw->set_sensitive(true);
            if (ghs_inv->get_active()) {
                ghs_autobw->set_sensitive(false);
                ghs_autobw->set_active(false);
            }
            ghs_LC->set_sensitive(true); 
            ghs_MID->set_sensitive(true);
        }
        if (ghs_D->getValue() == 0.f) {
            ghs_LC->set_sensitive(false); 
            ghs_MID->set_sensitive(false);
        }

        if (mode == Expert || mode == Normal) { // Keep widget hidden in Simple mode
            exprecovs->show();
            ghsMethod->show();
            ghs_inv->show();
            Framemtf->show();

        } else {
            ghs_inv->hide();
            Framemtf->hide();

        }
        if (ghsMethod->get_active_row_number() == 2 && shMethod->get_active_row_number() == 2) {
            Lab_Frame->show();
            Framemtf->hide();
        }

        if (mode == Expert) {
            ghs_slope->show();  
        }
        reparsh->show();
        if (shMethod->get_active_row_number() != 2) {
            ghs_inv->hide();
            ghsMethod->hide();
            Framemtf->hide();

        }

        showmaskSHMethod->show();
        showmaskSHMethodinv->hide();
        // Reset hidden mask combobox
        showmaskSHMethodConninv.block(true);
        showmaskSHMethodinv->set_active(0);
        showmaskSHMethodConninv.block(false);
    }
}

void LocallabShadow::updateShadowGUIsym()
{
    // Update adjuster range to avoid black screen according to Symmetry ghs_SP

    double tempLP = ghs_LP->getValue();//Low values Protect shadows
    const double tempSP = rtengine::LIM(ghs_SP->getValue(), 0.0001, 0.9999);//avoid 0 and 1 no real sens for symmetry must be enough for most cases
    double tempHP = ghs_HP->getValue();//high values Protect highlight
    double secur = 0.001;//keep range security to avoid crash and wrong GUI - no or small incidence on usage
    double HPL = rtengine::LIM(tempSP - secur, 0.0001, 0.9999);
    double BPH = rtengine::LIM(tempSP + secur, 0.0001, 0.9999);
    ghs_LP->setLimits(0., HPL, 0.00001, 0.0);
    ghs_HP->setLimits(BPH, 1.0, 0.00001, 0.0);
    //avoid crash at limits
    if (tempHP - tempSP <= 0.) {
        tempHP = tempSP + 0.0001; 
    }
    if (tempSP - tempLP <= 0.) {
        tempLP = tempSP - 0.0001; 
    }
    ghs_LP->setValue(tempLP);
    ghs_HP->setValue(tempHP);

}


void LocallabShadow::updateShadowGUIshmet()
{
    const int mode = complexity->get_active_row_number();

    // Update shadow highlight GUI according to shMethod combobox state
    if (shMethod->get_active_row_number() == 0) {
        for (const auto multiplier : multipliersh) {
            multiplier->hide();
        }

        gamFrame->hide();
        detailSH->hide();
        tePivot->hide();
        highlights->show();
        h_tonalwidth->show();
        shadows->show();
        s_tonalwidth->show();
        sh_radius->show();
        ghsFrame->hide();
        ghsMethod->hide();
        BP_Frame->hide();
        ghs_inv->hide();
        michFrame->hide();
    } else if (shMethod->get_active_row_number() == 1) {
        for (const auto multiplier : multipliersh) {
            multiplier->show();
        }

        if (mode == Expert || mode == Normal) { // Keep widget hidden in Simple mode
            gamFrame->show();
        }

        detailSH->show();
        tePivot->show();
        highlights->hide();
        h_tonalwidth->hide();
        shadows->hide();
        s_tonalwidth->hide();
        sh_radius->hide();
        ghsFrame->hide();
        ghsMethod->hide();
        BP_Frame->hide();
        ghs_inv->hide();
        michFrame->hide();
        Framemtf->hide();

    } else if (shMethod->get_active_row_number() == 2) {
        for (const auto multiplier : multipliersh) {
            multiplier->hide();
        }
        gamFrame->hide();
        detailSH->hide();
        tePivot->hide();
        highlights->hide();
        h_tonalwidth->hide();
        shadows->hide();
        s_tonalwidth->hide();
        sh_radius->hide();
        ghsFrame->show();
        inverssh->hide();
        inverssh->set_active(false);
        ghsMethod->hide();
        ghs_slope->hide();
        Lab_Frame->hide();
        BP_Frame->show();
        ghs_inv->hide();
        michFrame->hide();
        Framemtf->hide();

        if (mode == Expert || mode == Normal) {
            ghs_inv->show();
            if (ghsMethod->get_active_row_number() < 2) {
                Framemtf->show();
            }

        }
        if (ghs_D->getValue() == 0.f) {
            ghs_BLP->set_sensitive(false);
            ghs_HLP->set_sensitive(false);
            ghs_inv->set_sensitive(false);
            ghs_autobw->set_sensitive(false);
            ghs_agx->set_sensitive(false);
            ghsMatmet->set_sensitive(false);
            ghs_LC->set_sensitive(true);
            ghs_MID->set_sensitive(true);
       } else {
            if (ghs_autobw->get_active()) {
                ghs_BLP->set_sensitive(false);
                ghs_HLP->set_sensitive(false);
                ghs_inv->set_sensitive(false);
                ghs_agx->set_sensitive(false);
                ghsMatmet->set_sensitive(false);
            } else {
                ghs_BLP->set_sensitive(true);
                ghs_HLP->set_sensitive(true);
                ghs_inv->set_sensitive(true);
                ghs_agx->set_sensitive(true);
                ghsMatmet->set_sensitive(true);
            }
            ghs_autobw->set_sensitive(true);
            if (ghs_inv->get_active()) {
               ghs_autobw->set_sensitive(false);
               ghs_autobw->set_active(false);
            }
            ghs_LC->set_sensitive(true); 
            ghs_MID->set_sensitive(true); 
        }
        if (ghs_D->getValue() == 0.f) {
            ghs_LC->set_sensitive(false);
            ghs_MID->set_sensitive(false);
        }

        if (mode == Expert || mode == Normal) { // Keep widget hidden in Simple mode
            ghsMethod->show();
            if (ghsMethod->get_active_row_number() == 2) {
                Lab_Frame->show();
            }
        }
        if (mode == Expert) {
            ghs_slope->show();
        }
    } else if (shMethod->get_active_row_number() == 3) {
        michFrame->show();
        gamFrame->hide();
        detailSH->hide();
        tePivot->hide();
        highlights->hide();
        h_tonalwidth->hide();
        shadows->hide();
        s_tonalwidth->hide();
        sh_radius->hide();
        ghsFrame->hide();
        ghsMethod->hide();
        BP_Frame->hide();
        ghs_inv->hide();
        Framemtf->hide();

        for (const auto multiplier : multipliersh) {
            multiplier->hide();
        }
    }

}

/* ==== LocallabVibrance ==== */
LocallabVibrance::LocallabVibrance():
    LocallabTool(this, M("TP_LOCALLAB_VIB_TOOLNAME"), M("TP_LOCALLAB_VIBRANCE"), false),

    // Vibrance specific widgets
    saturated(Gtk::manage(new Adjuster(M("TP_VIBRANCE_SATURATED"), -100., 100., 1., 0.))),
    pastels(Gtk::manage(new Adjuster(M("TP_VIBRANCE_PASTELS"), -100., 100., 1., 0.))),
    vibgam(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMC"), 0.5, 3., 0.05, 1.))),
    warm(Gtk::manage(new Adjuster(M("TP_LOCALLAB_WARM"), -100., 100., 1., 0., Gtk::manage(new RTImage("circle-blue-small")), Gtk::manage(new RTImage("circle-orange-small"))))),
    psThreshold(Gtk::manage(new ThresholdAdjuster(M("TP_VIBRANCE_PSTHRESHOLD"), -100., 100., 0., M("TP_VIBRANCE_PSTHRESHOLD_WEIGTHING"), 0, 0., 100., 75., M("TP_VIBRANCE_PSTHRESHOLD_SATTHRESH"), 0, this, false))),
    protectSkins(Gtk::manage(new Gtk::CheckButton(M("TP_VIBRANCE_PROTECTSKINS")))),
    avoidColorShift(Gtk::manage(new Gtk::CheckButton(M("TP_VIBRANCE_AVOIDCOLORSHIFT")))),
    pastSatTog(Gtk::manage(new Gtk::CheckButton(M("TP_VIBRANCE_PASTSATTOG")))),
    sensiv(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 30))),//reused - unused here, but used for normalize_mean_dt 
    previewvib(Gtk::manage(new Gtk::ToggleButton(M("TP_LOCALLAB_PREVIEW")))),
    curveEditorGG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_LABEL"))),
    skinTonesCurve(static_cast<DiagonalCurveEditor*>(curveEditorGG->addCurve(CT_Diagonal, M("TP_VIBRANCE_CURVEEDITOR_SKINTONES")))),
    exprecovv(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    maskusablev(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusablev(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    recothresv(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 0., 2., 0.01, 1.))),
    lowthresv(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW"), 1., 80., 0.5, 12.))),
    higthresv(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR"), 20., 99., 0.5, 85.))),
    decayv(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKDDECAY"), 0.5, 4., 0.1, 2.))),
    expgradvib(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_EXPGRAD")))),
    strvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTR"), -4., 4., 0.05, 0.))),
    strvibab(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTRCHRO"), -4., 4., 0.05, 0.))),
    strvibh(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADSTRHUE2"), -6., 6., 0.05, 0.))),
    angvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GRADANG"), -180, 180, 0.1, 0.))),
    feathervib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_FEATVALUE"), 10., 100., 0.1, 25.))),
    expmaskvib(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWVI")))),
    showmaskvibMethod(Gtk::manage(new MyComboBoxText())),
    enavibMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASK")))),
 //   maskvibCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK"))),
    maskvibCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, "", 1)),
    CCmaskvibshape(static_cast<FlatCurveEditor*>(maskvibCurveEditorG->addCurve(CT_Flat, "C", nullptr, false, false))),
    LLmaskvibshape(static_cast<FlatCurveEditor*>(maskvibCurveEditorG->addCurve(CT_Flat, "L", nullptr, false, false))),
    HHmaskvibshape(static_cast<FlatCurveEditor *>(maskvibCurveEditorG->addCurve(CT_Flat, "LC(h)", nullptr, false, true))),
    blendmaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLENDMASKCOL"), -100, 100, 1, 0))),
    radmaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    lapmaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    chromaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMASKCOL"), -100.0, 100.0, 0.1, 0.))),
    gammaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMMASKCOL"), 0.25, 4.0, 0.01, 1.))),
    slomaskvib(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOMASKCOL"), 0.0, 15.0, 0.1, 0.))),
    mask2vibCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK2"))),
    Lmaskvibshape(static_cast<DiagonalCurveEditor*>(mask2vibCurveEditorG->addCurve(CT_Diagonal, "L(L)")))
{
    auto m = ProcEventMapper::getInstance();
    Evlocallabpreviewvib = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_PREVIEWVIB");
    Evlocallabfeathervib = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_FEATHERVIB");
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    float R, G, B;

    const LocallabParams::LocallabSpot defSpot;

    // Parameter Vibrance specific widgets
    saturated->setAdjusterListener(this);

    pastels->setAdjusterListener(this);

    vibgam->setAdjusterListener(this);

    warm->setAdjusterListener(this);

    psThreshold->set_tooltip_markup(M("TP_VIBRANCE_PSTHRESHOLD_TOOLTIP"));
    psThreshold->setAdjusterListener(this);

    pskinsConn = protectSkins->signal_toggled().connect(sigc::mem_fun(*this, &LocallabVibrance::protectskins_toggled));

    ashiftConn = avoidColorShift->signal_toggled().connect(sigc::mem_fun(*this, &LocallabVibrance::avoidcolorshift_toggled));

    pastsattogConn = pastSatTog->signal_toggled().connect(sigc::mem_fun(*this, &LocallabVibrance::pastsattog_toggled));

    sensiv->setAdjusterListener(this);

    curveEditorGG->setCurveListener(this);

    skinTonesCurve->setTooltip(M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_TOOLTIP"));
    std::vector<GradientMilestone> mskinTonesCurve;
    // -0.1 rad < Hue < 1.6 rad
    Color::hsv2rgb01(0.92f, 0.45f, 0.6f, R, G, B);
    mskinTonesCurve.emplace_back(0.0, R, G, B);
    Color::hsv2rgb01(0.14056f, 0.45f, 0.6f, R, G, B);
    mskinTonesCurve.emplace_back(1.0, R, G, B);
    skinTonesCurve->setBottomBarBgGradient(mskinTonesCurve);
    skinTonesCurve->setLeftBarBgGradient(mskinTonesCurve);
    skinTonesCurve->setRangeLabels(
        M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_RANGE1"), M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_RANGE2"),
        M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_RANGE3"), M("TP_VIBRANCE_CURVEEDITOR_SKINTONES_RANGE4")
    );
    skinTonesCurve->setRangeDefaultMilestones(0.1, 0.4, 0.85);

    curveEditorGG->curveListComplete();

    recothresv->setAdjusterListener(this);
    lowthresv->setAdjusterListener(this);
    higthresv->setAdjusterListener(this);
    decayv->setAdjusterListener(this);
    setExpandAlignProperties(exprecovv, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    setExpandAlignProperties(expgradvib, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    strvib->setAdjusterListener(this);

    strvibab->set_tooltip_text(M("TP_LOCALLAB_GRADSTRAB_TOOLTIP"));
    strvibab->setAdjusterListener(this);

    strvibh->set_tooltip_text(M("TP_LOCALLAB_GRADSTRHUE_TOOLTIP"));
    strvibh->setAdjusterListener(this);

    angvib->set_tooltip_text(M("TP_LOCALLAB_GRADANG_TOOLTIP"));
    angvib->setAdjusterListener(this);
    feathervib->setAdjusterListener(this);

    previewvib->set_active(false);
    previewvibConn = previewvib->signal_clicked().connect(
                       sigc::mem_fun(
                           *this, &LocallabVibrance::previewvibChanged));

    setExpandAlignProperties(expmaskvib, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    showmaskvibMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskvibMethod->append(M("TP_LOCALLAB_SHOWMODIF"));
    showmaskvibMethod->append(M("TP_LOCALLAB_SHOWMODIFMASK"));
    showmaskvibMethod->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskvibMethod->append(M("TP_LOCALLAB_SHOWREF"));
    showmaskvibMethod->set_active(0);
    showmaskvibMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskvibMethodConn = showmaskvibMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabVibrance::showmaskvibMethodChanged));

    enavibMaskConn = enavibMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabVibrance::enavibMaskChanged));

    maskvibCurveEditorG->setCurveListener(this);

    CCmaskvibshape->setIdentityValue(0.);
    CCmaskvibshape->setResetCurve(FlatCurveType(defSpot.CCmaskvibcurve.at(0)), defSpot.CCmaskvibcurve);
    CCmaskvibshape->setBottomBarColorProvider(this, 1);

    LLmaskvibshape->setIdentityValue(0.);
    LLmaskvibshape->setResetCurve(FlatCurveType(defSpot.LLmaskvibcurve.at(0)), defSpot.LLmaskvibcurve);
    LLmaskvibshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    HHmaskvibshape->setIdentityValue(0.);
    HHmaskvibshape->setResetCurve(FlatCurveType(defSpot.HHmaskvibcurve.at(0)), defSpot.HHmaskvibcurve);
    HHmaskvibshape->setCurveColorProvider(this, 2);
    HHmaskvibshape->setBottomBarColorProvider(this, 2);

    maskvibCurveEditorG->curveListComplete();

    blendmaskvib->setAdjusterListener(this);

    radmaskvib->setAdjusterListener(this);

    lapmaskvib->setAdjusterListener(this);

    chromaskvib->setAdjusterListener(this);

    gammaskvib->setAdjusterListener(this);

    slomaskvib->setAdjusterListener(this);

    mask2vibCurveEditorG->setCurveListener(this);

    Lmaskvibshape->setResetCurve(DiagonalCurveType(defSpot.Lmaskvibcurve.at(0)), defSpot.Lmaskvibcurve);
    Lmaskvibshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    Lmaskvibshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    mask2vibCurveEditorG->curveListComplete();

    // Add Vibrance specific widgets to GUI
    pack_start(*sensiv, Gtk::PACK_SHRINK, 0);//reused - nused here, but used for normalize_mean_dt 
    pack_start(*previewvib, Gtk::PACK_SHRINK, 0);    
    pack_start(*saturated, Gtk::PACK_SHRINK, 0);
    pack_start(*pastels, Gtk::PACK_SHRINK, 0);
    pack_start(*vibgam, Gtk::PACK_SHRINK, 0);
    Gtk::Separator* const separatorvib = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    pack_start(*separatorvib, Gtk::PACK_SHRINK, 2);
    pack_start(*warm, Gtk::PACK_SHRINK, 0);
    pack_start(*psThreshold, Gtk::PACK_SHRINK, 0);
    pack_start(*protectSkins, Gtk::PACK_SHRINK, 0);
    pack_start(*avoidColorShift, Gtk::PACK_SHRINK, 0);
    pack_start(*pastSatTog, Gtk::PACK_SHRINK, 0);
//    pack_start(*sensiv, Gtk::PACK_SHRINK, 0);//reused - nused here, but used for normalize_mean_dt 
    pack_start(*curveEditorGG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    ToolParamBlock* const vibBox3 = Gtk::manage(new ToolParamBlock());
    vibBox3->pack_start(*maskusablev, Gtk::PACK_SHRINK, 0);
    vibBox3->pack_start(*maskunusablev, Gtk::PACK_SHRINK, 0);
    vibBox3->pack_start(*recothresv);
    vibBox3->pack_start(*lowthresv);
    vibBox3->pack_start(*higthresv);
    vibBox3->pack_start(*decayv);
   // colBox3->pack_start(*invmaskc);
    exprecovv->add(*vibBox3, false);
    pack_start(*exprecovv, false, false);

    ToolParamBlock* const gradvibBox = Gtk::manage(new ToolParamBlock());
    gradvibBox->pack_start(*strvib);
    gradvibBox->pack_start(*strvibab);
    gradvibBox->pack_start(*strvibh);
    gradvibBox->pack_start(*angvib);
    gradvibBox->pack_start(*feathervib);
    expgradvib->add(*gradvibBox, false);
    pack_start(*expgradvib);
    ToolParamBlock* const maskvibBox = Gtk::manage(new ToolParamBlock());
    maskvibBox->pack_start(*showmaskvibMethod, Gtk::PACK_SHRINK, 4);
    maskvibBox->pack_start(*enavibMask, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*maskvibCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    maskvibBox->pack_start(*blendmaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*radmaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*lapmaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*chromaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*gammaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*slomaskvib, Gtk::PACK_SHRINK, 0);
    maskvibBox->pack_start(*mask2vibCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    expmaskvib->add(*maskvibBox, false);
    pack_start(*expmaskvib, false, false);
}

LocallabVibrance::~LocallabVibrance()
{
    delete curveEditorGG;
    delete maskvibCurveEditorG;
    delete mask2vibCurveEditorG;
}

bool LocallabVibrance::isMaskViewActive()
{
    return (showmaskvibMethod->get_active_row_number() != 0);
}

void LocallabVibrance::resetMaskView()
{
    showmaskvibMethodConn.block(true);
    showmaskvibMethod->set_active(0);
    showmaskvibMethodConn.block(false);
}

Gtk::ToggleButton *LocallabVibrance::getPreviewDeltaEButton() const
{
    return previewvib;
}

sigc::connection *LocallabVibrance::getPreviewDeltaEButtonConnection()
{
    return &previewvibConn;
}

//new function Global
void LocallabVibrance::updateguivib(int spottype)
{
    {
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                sensiv->hide();
                previewvib->hide();
                enavibMask->set_active(false);
                previewvib->set_active(false);
                exprecovv->hide();
                expmaskvib->hide();
            } else {
                sensiv->show();
                previewvib->show();
                exprecovv->show();
                expmaskvib->show();
                updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

            }
            enableListener();
            
            
            
            

        return false;
        }
        );
    }
   
}

void LocallabVibrance::previewvibChanged()
{
    
    if(previewvib->get_active()) {
        showmaskvibMethod->set_active(4);
    } else {
        showmaskvibMethod->set_active(0);
    }
    
    if (isLocActivated) {
        if (listener) {
            listener->panelChanged(Evlocallabpreviewvib,"");
        }
    } 
}

//new function scope
void LocallabVibrance::updateguiscopevib(int scope)
{
    {
        idle_register.add(
        [this, scope]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            disableListener();
            sensiv->setValue(scope);
            enableListener();

        return false;
        }
        );
    }
   
}

void LocallabVibrance::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    vibMask = showmaskvibMethod->get_active_row_number();
}

void LocallabVibrance::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        exp->set_tooltip_text(M("TP_LOCALLAB_VIBRA_TOOLTIP"));
        warm->set_tooltip_text(M("TP_LOCALLAB_WARM_TOOLTIP"));
        recothresv->set_tooltip_text(M("TP_LOCALLAB_RECOTHRES02_TOOLTIP"));
        strvib->set_tooltip_text(M("TP_LOCALLAB_GRADGEN_TOOLTIP"));
        exprecovv->set_tooltip_markup(M("TP_LOCALLAB_MASKRESVIB_TOOLTIP"));
        expmaskvib->set_tooltip_markup(M("TP_LOCALLAB_MASK_TOOLTIP"));
        CCmaskvibshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        LLmaskvibshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        HHmaskvibshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        blendmaskvib->set_tooltip_text(M("TP_LOCALLAB_BLENDMASK_TOOLTIP"));
        mask2vibCurveEditorG->set_tooltip_text(M("TP_LOCALLAB_CONTRASTCURVMASK_TOOLTIP"));
        Lmaskvibshape->setTooltip(M("TP_LOCALLAB_LMASK_LL_TOOLTIP"));
        maskvibCurveEditorG->set_tooltip_markup(M("TP_LOCALLAB_MASKCURVE_TOOLTIP"));
        gammaskvib->set_tooltip_text(M("TP_LOCALLAB_GAMMASK_TOOLTIP"));
        chromaskvib->set_tooltip_text(M("TP_LOCALLAB_CHROMASK_TOOLTIP"));
        slomaskvib->set_tooltip_text(M("TP_LOCALLAB_SLOMASK_TOOLTIP"));
        lapmaskvib->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
        vibgam->set_tooltip_text(M("TP_LOCALLAB_GAMCOL_TOOLTIP"));
        expgradvib->set_tooltip_text(M("TP_LOCALLAB_EXPGRADCOL_TOOLTIP"));

/*
        saturated->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        pastels->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        psThreshold->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        protectSkins->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        avoidColorShift->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        pastSatTog->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        sensiv->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
        curveEditorGG->set_tooltip_text(M("TP_LOCALLAB_NUL_TOOLTIP"));
*/

        saturated->set_tooltip_text("");
        pastels->set_tooltip_text("");
        psThreshold->set_tooltip_text("");
        protectSkins->set_tooltip_text("");
        avoidColorShift->set_tooltip_text("");
        pastSatTog->set_tooltip_text("");
        sensiv->set_tooltip_text("");
        curveEditorGG->set_tooltip_text("");
        decayv->set_tooltip_text(M("TP_LOCALLAB_MASKDECAY_TOOLTIP"));
        lowthresv->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRESVIB_TOOLTIP"));
        higthresv->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRESVIB_TOOLTIP"));

    } else {
        exp->set_tooltip_text("");
        warm->set_tooltip_text("");
        strvib->set_tooltip_text("");
        recothresv->set_tooltip_text("");
        expmaskvib->set_tooltip_markup("");
        CCmaskvibshape->setTooltip("");
        LLmaskvibshape->setTooltip("");
        HHmaskvibshape->setTooltip("");
        blendmaskvib->set_tooltip_text("");
        mask2vibCurveEditorG->set_tooltip_text("");
        Lmaskvibshape->setTooltip("");
        maskvibCurveEditorG->set_tooltip_markup("");
        gammaskvib->set_tooltip_text("");
        chromaskvib->set_tooltip_text("");
        slomaskvib->set_tooltip_text("");
        lapmaskvib->set_tooltip_text("");
        saturated->set_tooltip_text("");
        pastels->set_tooltip_text("");
        psThreshold->set_tooltip_text("");
        protectSkins->set_tooltip_text("");
        avoidColorShift->set_tooltip_text("");
        pastSatTog->set_tooltip_text("");
        sensiv->set_tooltip_text("");
        curveEditorGG->set_tooltip_text("");
        exprecovv->set_tooltip_markup("");
        decayv->set_tooltip_text("");
        lowthresv->set_tooltip_text("");
        higthresv->set_tooltip_text("");
        vibgam->set_tooltip_text("");
        expgradvib->set_tooltip_text("");
    }
}

void LocallabVibrance::setDefaultExpanderVisibility()
{
    exprecovv->set_expanded(false);
    expgradvib->set_expanded(false);
    expmaskvib->set_expanded(false);
}

void LocallabVibrance::disableListener()
{
    LocallabTool::disableListener();

    pskinsConn.block(true);
    ashiftConn.block(true);
    pastsattogConn.block(true);
    showmaskvibMethodConn.block(true);
    enavibMaskConn.block(true);
}

void LocallabVibrance::enableListener()
{
    LocallabTool::enableListener();

    pskinsConn.block(false);
    ashiftConn.block(false);
    pastsattogConn.block(false);
    showmaskvibMethodConn.block(false);
    enavibMaskConn.block(false);
}

void LocallabVibrance::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();
    nbmaskvib = 0;
    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visivibrance);
        exp->setEnabled(spot.expvibrance);
        complexity->set_active(spot.complexvibrance);

        saturated->setValue(spot.saturated);
        pastels->setValue(spot.pastels);
        vibgam->setValue(spot.vibgam);
        warm->setValue(spot.warm);
        psThreshold->setValue<int>(spot.psthreshold);
        protectSkins->set_active(spot.protectskins);
        avoidColorShift->set_active(spot.avoidcolorshift);
        pastSatTog->set_active(spot.pastsattog);
        sensiv->setValue(spot.sensiv);
        skinTonesCurve->setCurve(spot.skintonescurve);
        strvib->setValue(spot.strvib);
        strvibab->setValue(spot.strvibab);
        strvibh->setValue(spot.strvibh);
        angvib->setValue(spot.angvib);
        feathervib->setValue(spot.feathervib);
        enavibMask->set_active(spot.enavibMask);
        CCmaskvibshape->setCurve(spot.CCmaskvibcurve);
        LLmaskvibshape->setCurve(spot.LLmaskvibcurve);
        HHmaskvibshape->setCurve(spot.HHmaskvibcurve);
        blendmaskvib->setValue(spot.blendmaskvib);
        radmaskvib->setValue(spot.radmaskvib);
        lapmaskvib->setValue(spot.lapmaskvib);
        chromaskvib->setValue(spot.chromaskvib);
        gammaskvib->setValue(spot.gammaskvib);
        slomaskvib->setValue(spot.slomaskvib);
        Lmaskvibshape->setCurve(spot.Lmaskvibcurve);
        recothresv->setValue((double)spot.recothresv);
        lowthresv->setValue((double)spot.lowthresv);
        higthresv->setValue((double)spot.higthresv);
        decayv->setValue((double)spot.decayv);
    }

    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update vibrance GUI according to pastsattog button state
    updateVibranceGUI();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabVibrance::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expvibrance = exp->getEnabled();
        spot.visivibrance = exp->get_visible();
        spot.complexvibrance = complexity->get_active_row_number();

        spot.saturated = saturated->getIntValue();
        spot.pastels = pastels->getIntValue();
        spot.vibgam = vibgam->getValue();
        spot.warm = warm->getIntValue();
        spot.psthreshold = psThreshold->getValue<int>();
        spot.protectskins = protectSkins->get_active();
        spot.avoidcolorshift = avoidColorShift->get_active();
        spot.pastsattog = pastSatTog->get_active();
        spot.sensiv = sensiv->getIntValue();
        spot.skintonescurve = skinTonesCurve->getCurve();
        spot.strvib = strvib->getValue();
        spot.strvibab = strvibab->getValue();
        spot.strvibh = strvibh->getValue();
        spot.angvib = angvib->getValue();
        spot.feathervib = feathervib->getValue();
        spot.enavibMask = enavibMask->get_active();
        spot.CCmaskvibcurve = CCmaskvibshape->getCurve();
        spot.LLmaskvibcurve = LLmaskvibshape->getCurve();
        spot.HHmaskvibcurve = HHmaskvibshape->getCurve();
        spot.blendmaskvib = blendmaskvib->getIntValue();
        spot.radmaskvib = radmaskvib->getValue();
        spot.lapmaskvib = lapmaskvib->getValue();
        spot.chromaskvib = chromaskvib->getValue();
        spot.gammaskvib = gammaskvib->getValue();
        spot.slomaskvib = slomaskvib->getValue();
        spot.Lmaskvibcurve = Lmaskvibshape->getCurve();
        spot.recothresv = recothresv->getValue();
        spot.lowthresv = lowthresv->getValue();
        spot.higthresv = higthresv->getValue();
        spot.decayv = decayv->getValue();
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabVibrance::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default values for adjuster and threshold adjuster widgets
        saturated->setDefault((double)defSpot.saturated);
        pastels->setDefault((double)defSpot.pastels);
        vibgam->setDefault((double)defSpot.vibgam);
        warm->setDefault((double)defSpot.warm);
        psThreshold->setDefault<int>(defSpot.psthreshold);
        sensiv->setDefault((double)defSpot.sensiv);
        strvib->setDefault(defSpot.strvib);
        strvibab->setDefault(defSpot.strvibab);
        strvibh->setDefault(defSpot.strvibh);
        angvib->setDefault(defSpot.angvib);
        feathervib->setDefault(defSpot.feathervib);
        blendmaskvib->setDefault((double)defSpot.blendmaskvib);
        radmaskvib->setDefault(defSpot.radmaskvib);
        lapmaskvib->setDefault(defSpot.lapmaskvib);
        chromaskvib->setDefault(defSpot.chromaskvib);
        gammaskvib->setDefault(defSpot.gammaskvib);
        slomaskvib->setDefault(defSpot.slomaskvib);
        recothresv->setDefault((double)defSpot.recothresv);
        lowthresv->setDefault((double)defSpot.lowthresv);
        higthresv->setDefault((double)defSpot.higthresv);
        decayv->setDefault((double)defSpot.decayv);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabVibrance::adjusterChanged(Adjuster* a, double newval)
{
    // Copy pastels adjuster value to saturated one according to pastSatTog button state
    if (a == pastels && pastSatTog->get_active()) {
        saturated->setValue(newval);
    }

    if (isLocActivated && exp->getEnabled()) {
        if (a == saturated && !pastSatTog->get_active()) {
            if (listener) {
                listener->panelChanged(EvlocallabSaturated,
                                       saturated->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == pastels) {
            if (listener) {
                listener->panelChanged(EvlocallabPastels,
                                       pastels->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == vibgam) {
            if (listener) {
                listener->panelChanged(Evlocallabvibgam,
                                       vibgam->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == warm) {
            if (listener) {
                listener->panelChanged(Evlocallabwarm,
                                       warm->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensiv) {
            if (listener) {
                listener->panelChanged(Evlocallabsensiv,
                                       sensiv->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothresv) {

            if (listener) {
                listener->panelChanged(Evlocallabrecothresv,
                                       recothresv->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthresv) {
            if (listener) {
                listener->panelChanged(Evlocallablowthresv,
                                       lowthresv->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthresv) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthresv,
                                       higthresv->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == decayv) {
            if (listener) {
                listener->panelChanged(Evlocallabdecayv,
                                       decayv->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strvib) {
            if (listener) {
                listener->panelChanged(Evlocallabstrvib,
                                       strvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strvibab) {
            if (listener) {
                listener->panelChanged(Evlocallabstrvibab,
                                       strvibab->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strvibh) {
            if (listener) {
                listener->panelChanged(Evlocallabstrvibh,
                                       strvibh->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == angvib) {
            if (listener) {
                listener->panelChanged(Evlocallabangvib,
                                       angvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == feathervib) {
            if (listener) {
                listener->panelChanged(Evlocallabfeathervib,
                                       feathervib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blendmaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallabblendmaskvi,
                                       blendmaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == radmaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallabradmaskvib,
                                       radmaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lapmaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallablapmaskvib,
                                       lapmaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chromaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallabchromaskvib,
                                       chromaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gammaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallabgammaskvib,
                                       gammaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == slomaskvib) {
            if (listener) {
                listener->panelChanged(Evlocallabslomaskvib,
                                       slomaskvib->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop)
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabPastSatThreshold,
                                   psThreshold->getHistoryString() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

std::vector<double> LocallabVibrance::getCurvePoints(ThresholdSelector* tAdjuster) const
{
    std::vector<double> points;
    double threshold, transitionWeighting;
    tAdjuster->getPositions<double>(transitionWeighting, threshold); // ( range -100;+100, range 0;+100 )
    transitionWeighting /= 100.; // range -1., +1.
    threshold /= 100.; // range  0., +1.

    // Initial point
    points.push_back(0.);
    points.push_back(0.);

    double p2 = 3.0 * threshold / 4.0; // same one than in ipvibrance.cc
    double s0 = threshold + (1.0 - threshold) / 4.0; // same one than in ipvibrance.cc

    // point at the beginning of the first linear transition
    points.push_back(p2);
    points.push_back(0.);

    // Y value of the chroma mean point, calculated to get a straight line between p2 and s0
    double chromaMean = (threshold / 4.0) / (s0 - p2);

    // move chromaMean up or down depending on transitionWeighting
    if (transitionWeighting > 0.0) {
        // positive values -> give more weight to Saturated
        chromaMean = (1.0 - chromaMean) * transitionWeighting + chromaMean;
    } else if (transitionWeighting < 0.0) {
        // negative values -> give more weight to Pastels
        chromaMean = chromaMean * transitionWeighting + chromaMean;
    }

    // point at the location of the Top cursor, at the end of the first linear transition and the beginning of the second one
    points.push_back(threshold);
    points.push_back(chromaMean);

    if (threshold < 1.0) {
        // point at the end of the second linear transition
        points.push_back(s0);
        points.push_back(1.0);

        // end point
        points.push_back(1.0);
        points.push_back(1.0);
    }

    return points;
}

void LocallabVibrance::curveChanged(CurveEditor* ce)
{
    if (isLocActivated && exp->getEnabled()) {
        if (ce == skinTonesCurve) {
            if (listener) {
                listener->panelChanged(EvlocallabSkinTonesCurve,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == CCmaskvibshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCCmaskvibshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskvibshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskvibshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHmaskvibshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHmaskvibshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == Lmaskvibshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLmaskvibshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenavibrance,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenavibrance,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();

    // Set hidden GUI widgets in Normal mode to default spot values
    saturated->setValue((double)defSpot.saturated);
    vibgam->setValue(defSpot.vibgam);

    psThreshold->setValue<int>(defSpot.psthreshold);
    protectSkins->set_active(defSpot.protectskins);
    avoidColorShift->set_active(defSpot.avoidcolorshift);
    pastSatTog->set_active(defSpot.pastsattog);
    skinTonesCurve->setCurve(defSpot.skintonescurve);
    strvibab->setValue(defSpot.strvibab);
    strvibh->setValue(defSpot.strvibh);
    lapmaskvib->setValue(defSpot.lapmaskvib);
    gammaskvib->setValue(defSpot.gammaskvib);
    slomaskvib->setValue(defSpot.slomaskvib);
    decayv->setValue(defSpot.decayv);

    // Enable all listeners
    enableListener();

    // Update GUI based on converted widget parameters:
    // - Update vibrance GUI according to pastsattog button state
    updateVibranceGUI();
}

void LocallabVibrance::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();

    // Set hidden specific GUI widgets in Simple mode to default spot values
   // strvib->setValue(defSpot.strvib);
   // angvib->setValue(defSpot.angvib);
   // feathervib->setValue(defSpot.feathervib);
    strvibab->setValue(defSpot.strvibab);
    strvibh->setValue(defSpot.strvibh);
  
    showmaskvibMethod->set_active(0);
    enavibMask->set_active(defSpot.enavibMask);
  //  CCmaskvibshape->setCurve(defSpot.CCmaskvibcurve);
  //  LLmaskvibshape->setCurve(defSpot.LLmaskvibcurve);
   // HHmaskvibshape->setCurve(defSpot.HHmaskvibcurve);
   // blendmaskvib->setValue((double)defSpot.blendmaskvib);
   // radmaskvib->setValue(defSpot.radmaskvib);
  //  chromaskvib->setValue(defSpot.chromaskvib);
  //  Lmaskvibshape->setCurve(defSpot.Lmaskvibcurve);
    recothresv->setValue(defSpot.recothresv);
    lowthresv->setValue(defSpot.lowthresv);
    higthresv->setValue(defSpot.higthresv);
    decayv->setValue(defSpot.decayv);

    // Enable all listener
    enableListener();
}

void LocallabVibrance::updateGUIToMode(const modeType new_type)
{
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            saturated->hide();
            pastels->setLabel(M("TP_LOCALLAB_PASTELS2"));
            vibgam->hide();
            psThreshold->hide();
            protectSkins->hide();
            avoidColorShift->hide();
            pastSatTog->hide();
            curveEditorGG->hide();
            expmaskvib->hide();
            exprecovv->hide();
            decayv->hide();
            maskusablev->hide();
            maskunusablev->hide();
            strvibab->hide();
            strvibh->hide();

            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            saturated->hide();
            vibgam->hide();
            pastels->setLabel(M("TP_LOCALLAB_PASTELS2"));
            psThreshold->hide();
            protectSkins->hide();
            avoidColorShift->hide();
            pastSatTog->hide();
            curveEditorGG->hide();
            strvibab->hide();
            strvibh->hide();
            lapmaskvib->hide();
            gammaskvib->hide();
            slomaskvib->hide();
            // Specific Simple mode widgets are shown in Normal mode
            expmaskvib->show();
            exprecovv->show();
            decayv->hide();
            if (enavibMask->get_active()) {
                maskusablev->show();
                maskunusablev->hide();

            } else {
                maskusablev->hide();
                maskunusablev->show();
            }

            break;

        case Expert:
            // Show widgets hidden in Normal and Simple mode
            saturated->show();
            vibgam->show();
            pastels->setLabel(M("TP_VIBRANCE_PASTELS"));
            psThreshold->show();
            protectSkins->show();
            avoidColorShift->show();
            pastSatTog->show();
            curveEditorGG->show();
            strvibab->show();
            strvibh->show();
            expmaskvib->show();
            lapmaskvib->show();
            gammaskvib->show();
            slomaskvib->show();
            exprecovv->show();
            decayv->show();
            if (enavibMask->get_active()) {
                maskusablev->show();
                maskunusablev->hide();

            } else {
                maskusablev->hide();
                maskunusablev->show();
            }
    }
}

void LocallabVibrance::updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz)
{
    idle_register.add(
    [this, normHuer, normLumar, normChromar]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        // Update mask background
        CCmaskvibshape->updateLocallabBackground(normChromar);
        LLmaskvibshape->updateLocallabBackground(normLumar);
        HHmaskvibshape->updateLocallabBackground(normHuer);
        Lmaskvibshape->updateLocallabBackground(normLumar);

        return false;
    }
    );
}

void LocallabVibrance::protectskins_toggled()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (protectSkins->get_active()) {
                listener->panelChanged(EvlocallabProtectSkins,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvlocallabProtectSkins,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::avoidcolorshift_toggled()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (avoidColorShift->get_active()) {
                listener->panelChanged(EvlocallabAvoidColorShift,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvlocallabAvoidColorShift,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::pastsattog_toggled()
{
    // Update vibrance GUI according to pastsattog button state
    updateVibranceGUI();

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (pastSatTog->get_active()) {
                listener->panelChanged(EvlocallabPastSatTog,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvlocallabPastSatTog,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::showmaskvibMethodChanged()
{
    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabVibrance::enavibMaskChanged()
{
    if (enavibMask->get_active()) {
        maskusablev->show();
        maskunusablev->hide();

    } else {
        maskusablev->hide();
        maskunusablev->show();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enavibMask->get_active()) {
                listener->panelChanged(EvLocallabEnavibMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnavibMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabVibrance::updateVibranceGUI()
{
    // Update vibrance GUI according to pastsattog button state
    if (pastSatTog->get_active()) {
        // Link both slider, so we set saturated and psThresholds unsensitive
        psThreshold->set_sensitive(false);
        saturated->set_sensitive(false);
        saturated->setValue(pastels->getValue()); // Pastels and Saturated are linked
    } else {
        // Separate sliders, so we set saturated and psThresholds sensitive again
        psThreshold->set_sensitive(true);
        saturated->set_sensitive(true);
    }
}

/* ==== LocallabSoft ==== */
LocallabSoft::LocallabSoft():
    LocallabTool(this, M("TP_LOCALLAB_SOFT_TOOLNAME"), M("TP_LOCALLAB_SOFT"), false),

    // Soft light specific widgets
    softMethod(Gtk::manage(new MyComboBoxText())),
    ctboxsoftmethod(Gtk::manage(new Gtk::Box())),
    showmasksoftMethod(Gtk::manage(new MyComboBoxText())),
    streng(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRENG"), 1, 100, 1, 1))),
    laplace(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPLACE"), 0., 100., 0.5, 25.))),
    sensisf(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 1, 100, 1, 30)))
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);

    // Parameter Soft light specific widgets
    softMethod->append(M("TP_LOCALLAB_SOFTM"));
    softMethod->append(M("TP_LOCALLAB_RETIM"));
    softMethod->set_active(0);
    softMethodConn = softMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabSoft::softMethodChanged));

    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWLAPLACE"));
    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWFOURIER"));
    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWPOISSON"));
    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWNORMAL"));
    showmasksoftMethod->append(M("TP_LOCALLAB_SHOWMODIF2"));
    showmasksoftMethod->set_active(0);
    showmasksoftMethodConn = showmasksoftMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabSoft::showmasksoftMethodChanged));

    streng->setAdjusterListener(this);

    laplace->setAdjusterListener(this);

    sensisf->setAdjusterListener(this);

    // Add Soft light specific widgets to GUI
    pack_start(*sensisf);
    pack_start(*softMethod);
    Gtk::Label* const labelsoftmethod = Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_SHOWDCT") + ":"));
    ctboxsoftmethod->pack_start(*labelsoftmethod, Gtk::PACK_SHRINK, 4);
    ctboxsoftmethod->pack_start(*showmasksoftMethod);
    pack_start(*ctboxsoftmethod);
    pack_start(*streng);
    pack_start(*laplace);
}

bool LocallabSoft::isMaskViewActive()
{
    return (showmasksoftMethod->get_active_row_number() != 0);
}

void LocallabSoft::resetMaskView()
{
    showmasksoftMethodConn.block(true);
    showmasksoftMethod->set_active(0);
    showmasksoftMethodConn.block(false);
}

//new function Global
void LocallabSoft::updateguisoft(int spottype) 
{
    {
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                sensisf->hide();
            } else {
                sensisf->show();
            }
            enableListener();

        return false;
        }
        );
    }
   
}


void LocallabSoft::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    softMask = showmasksoftMethod->get_active_row_number();
}

void LocallabSoft::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        exp->set_tooltip_markup(M("TP_LOCALLAB_SOFTMETHOD_TOOLTIP"));
        showmasksoftMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKSOFT_TOOLTIP"));
        streng->set_tooltip_text(M("TP_LOCALLAB_ORRETISTREN_TOOLTIP"));
        laplace->set_tooltip_text(M("TP_LOCALLAB_ORRETILAP_TOOLTIP"));
        sensisf->set_tooltip_text(M("TP_LOCALLAB_SENSI_TOOLTIP"));

    } else {
        exp->set_tooltip_text("");
        showmasksoftMethod->set_tooltip_markup("");
        streng->set_tooltip_text("");
        laplace->set_tooltip_text("");
        sensisf->set_tooltip_text("");
    }
}

void LocallabSoft::disableListener()
{
    LocallabTool::disableListener();

    softMethodConn.block(true);
    showmasksoftMethodConn.block(true);
}

void LocallabSoft::enableListener()
{
    LocallabTool::enableListener();

    softMethodConn.block(false);
    showmasksoftMethodConn.block(false);
}

void LocallabSoft::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();

    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visisoft);
        exp->setEnabled(spot.expsoft);
        complexity->set_active(spot.complexsoft);

        if (spot.softMethod == "soft") {
            softMethod->set_active(0);
        } else if (spot.softMethod == "reti") {
            softMethod->set_active(1);
        }

        streng->setValue((double)spot.streng);
        sensisf->setValue((double)spot.sensisf);
        laplace->setValue(spot.laplace);
    }

    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update soft light GUI according to softMethod combobox
    updateSoftGUI();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabSoft::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expsoft = exp->getEnabled();
        spot.visisoft = exp->get_visible();
        spot.complexsoft = complexity->get_active_row_number();

        if (softMethod->get_active_row_number() == 0) {
            spot.softMethod = "soft";
        } else if (softMethod->get_active_row_number() == 1) {
            spot.softMethod = "reti";
        }

        spot.streng = streng->getIntValue();
        spot.sensisf = sensisf->getIntValue();
        spot.laplace = laplace->getValue();
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabSoft::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default value for adjuster widgets
        streng->setDefault((double)defSpot.streng);
        laplace->setDefault(defSpot.laplace);
        sensisf->setDefault((double)defSpot.sensisf);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabSoft::adjusterChanged(Adjuster* a, double newval)
{
    if (isLocActivated && exp->getEnabled()) {
        if (a == streng) {
            if (listener) {
                listener->panelChanged(Evlocallabstreng,
                                       streng->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensisf) {
            if (listener) {
                listener->panelChanged(Evlocallabsensisf,
                                       sensisf->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == laplace) {
            if (listener) {
                listener->panelChanged(Evlocallablaplace,
                                       laplace->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabSoft::complexityModeChanged()
{
    if (complexity->get_active_row_number() == Simple) { // New selected mode is Simple one
        const bool maskPreviewActivated = isMaskViewActive();

        // Convert tool widget parameters
        convertParamToNormal(); // From Expert mode to Normal mode
        convertParamToSimple(); // From Normal mode to Simple mode
        // Update GUI based on new mode
        updateGUIToMode(Simple);

        if (maskPreviewActivated) {
            // This event is called to transmit reset mask state
            if (listener) {
                listener->panelChanged(EvlocallabshowmaskMethod, "");
            }
        }

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_SIMPLE") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    } else if (complexity->get_active_row_number() == Normal) { // New selected mode is Normal one
        const bool maskPreviewActivated = isMaskViewActive();

        // Convert tool widget parameters
        convertParamToNormal();
        // Update GUI based on new mode
        updateGUIToMode(Normal);

        if (maskPreviewActivated) {
            // This event is called to transmit reset mask state
            if (listener) {
                listener->panelChanged(EvlocallabshowmaskMethod, "");
            }
        }

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_NORMAL") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    } else if (complexity->get_active_row_number() == Expert) { // New selected mode is Expert one
        // Update GUI based on new mode
        updateGUIToMode(Expert);

        if (listener && isLocActivated) {
            listener->panelChanged(EvlocallabcomplexityWithRefresh,
                                   M("TP_LOCALLAB_MODE_EXPERT") + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabSoft::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenasoft,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenasoft,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabSoft::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();

    // Set hidden GUI widgets in Normal mode to default spot values
    showmasksoftMethod->set_active(0);

    // Enable all listeners
    enableListener();
}

void LocallabSoft::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();

    // Set hidden specific GUI widgets in Simple mode to default spot values
    if (defSpot.softMethod == "soft") {
        softMethod->set_active(0);
    } else if (defSpot.softMethod == "reti") {
        softMethod->set_active(1);
    }

    // Enable all listeners
    enableListener();

    // Update GUI based on converted widget parameters:
    // - Update soft light GUI according to softMethod combobox
    updateSoftGUI();
}

void LocallabSoft::updateGUIToMode(const modeType new_type)
{
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            softMethod->hide();
            ctboxsoftmethod->hide();

            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            ctboxsoftmethod->hide();
            // Specific Simple mode widgets are shown in Normal mode
            softMethod->show();

            break;

        case Expert:
            // Show widgets hidden in Normal and Simple mode
            softMethod->show();

            if (softMethod->get_active_row_number() != 0) { // Keep widget hidden when softMethod is equal to 0
                ctboxsoftmethod->show();
            }
    }
}

void LocallabSoft::softMethodChanged()
{
    const bool maskPreviewActivated = isMaskViewActive();

    // Update soft light GUI according to softMethod combobox
    updateSoftGUI();

    if (maskPreviewActivated) {
        // This event is called to transmit reset mask state
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabsoftMethod,
                                   softMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabSoft::showmasksoftMethodChanged()
{
    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }
    if(exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabshowmaskMethod, "");
        }
    }
}

void LocallabSoft::updateSoftGUI()
{
    // Update soft light GUI according to softMethod combobox
    const int comp = complexity->get_active_row_number();

    if (softMethod->get_active_row_number() == 0) {
        ctboxsoftmethod->hide();
        // Reset hidden mask combobox
        showmasksoftMethodConn.block(true);
        showmasksoftMethod->set_active(0);
        showmasksoftMethodConn.block(false);
        laplace->hide();
    } else {
        if (comp == Expert) { // Keep widget hidden in Simple and Normal mode
            ctboxsoftmethod->show();
        }

        laplace->show();
    }
}

/* ==== LocallabBlur ==== */
LocallabBlur::LocallabBlur():
    LocallabTool(this, M("TP_LOCALLAB_BLUR_TOOLNAME"), M("TP_LOCALLAB_BLUFR"), true),

    // Blur, Noise & Denoise specific widgets
    expblnoise(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_BLNOI_EXP")))),
    blMethod(Gtk::manage(new MyComboBoxText())),
    fftwbl(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_FFTWBLUR")))),
    radius(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADIUS"), MINRAD, MAXRAD, 0.1, 1.5, nullptr, nullptr, &blurSlider2radius, &blurRadius2Slider))),
    strength(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRENGTH"), 0, 100, 1, 0))),
    grainFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GRAINFRA")))),
    grainFrame2(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_GRAINFRA2")))),
    isogr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_ISOGR"), 20, 6400, 1, 400))),
    strengr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRENGR"), 0, 100, 1, 0))),
    scalegr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SCALEGR"), 0, 100, 1, 100))),
    divgr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DIVGR"), 0.2, 3., 0.1, 1.))),
    medMethod(Gtk::manage(new MyComboBoxText())),
    itera(Gtk::manage(new Adjuster(M("TP_DIRPYRDENOISE_MEDIAN_PASSES"), 1, 4, 1, 1))),
    guidbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GUIDBL"), 0, 1000, 1, 0))),
    strbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRBL"), 0, 100, 1, 50))),
    epsbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_EPSBL"), -10, 10, 1, 0))),
    expdenoise2(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    recothres(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 1., 2., 0.001, 1.))),
    lowthres(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW2"), 1., 80., 0.5, 12.))),
    higthres(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR2"), 20., 99., 0.5, 85.))),
    sensibn(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 40))),
    blurMethod(Gtk::manage(new MyComboBoxText())),
    invbl(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVBL")))),
    chroMethod(Gtk::manage(new MyComboBoxText())),
    activlum(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ACTIV")))),
    expdenoise(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI_EXP")))),
    denoFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_DENOIFRA")))),
    enacontrast(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_DENOIENA")))),
    denocontrast(Gtk::manage(new Adjuster(M("TP_SHARPENING_CONTRAST"), 0.5, 100., 0.5, 10.))),
    denoratio(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DENOIRATIO"), 0, 100, 1, 95))),
    contrshow(Gtk::manage(new Gtk::CheckButton(M("TP_PDSHARPENING_SHOWCAP")))),
    denomask(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DENOI_MASK"), 0., 100., 1., 30.))),
    quamethod(Gtk::manage(new MyComboBoxText())),
    expdenoisenl(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_NLFRA")))),
    expdenoiselum(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOIWAVLUM")))),
    expdenoisech(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOIWAVCH")))),
    LocalcurveEditorwavden(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_WAVDEN"))),
    wavshapeden(static_cast<FlatCurveEditor*>(LocalcurveEditorwavden->addCurve(CT_Flat, "", nullptr, false, false))),
  //  lCLabels(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_LCLABELS")))),
    lCLabels(Gtk::manage(new Gtk::Label("-----------------"))),
    lumLabels(Gtk::manage(new Gtk::Label("---"))),
    lum46Labels(Gtk::manage(new Gtk::Label("---"))),
    chroLabels(Gtk::manage(new Gtk::Label("---"))),
    chro46Labels(Gtk::manage(new Gtk::Label("---"))),
    lockmadl(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_LOCKMADL")))),
    madlFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_MADLFRA")))),

    madls([]() -> std::array<Adjuster *, 21>

    {
        std::array<Adjuster*, 21> res = {};

        for (unsigned int i = 0; i < res.size(); ++i) {
            Glib::ustring ss = Glib::ustring::format(i);

            if (i == 0) {
                ss += Glib::ustring::compose(" %1", M("TP_LOCALLAB_MADLDIRH"));
            } else if (i == 7) {
                ss += Glib::ustring::compose(" %1", M("TP_LOCALLAB_MADLDIRV"));          
            } else if (i == 14) {
                ss += Glib::ustring::compose(" %1", M("TP_LOCALLAB_MADLDIRD"));          
            }

            res[i] = Gtk::manage(new Adjuster(std::move(ss), 1.0, 2000000.0, 1., 100.0));
        }

        return res;
    }
    ()),
    madllock(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_LOCKMADL2")))),
    
    expdenoise1(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI1_EXP")))),
    maskusable(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusable(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    maskusable2(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusable2(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    maskusable3(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUSABLE")))),
    maskunusable3(Gtk::manage(new Gtk::Label(M("TP_LOCALLAB_MASKUNUSABLE")))),
    usemask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_USEMASK")))),
    lnoiselow(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLNOISELOW"), 0.7, 2., 0.01, 1.))),
    levelthr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR2"), 20., 99., 0.5, 85.))),
    levelthrlow(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW2"), 1., 80., 0.5, 12.))),
    noiselumf0(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELUMFINEZERO"), MINCHRO, MAXCHRO, 0.01, 0.))),
    noiselumf(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELUMFINE"), MINCHRO, MAXCHRO, 0.01, 0.))),
    noiselumf2(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELUMFINETWO"), MINCHRO, MAXCHRO, 0.01, 0.))),
    noiselumc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELUMCOARSE"), MINCHRO, MAXCHROCC, 0.01, 0.))),//unused here, but used for normalize_mean_dt
    noiselumdetail(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELUMDETAIL"), 0., 100., 0.01, 50.))),
    noiselequal(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISELEQUAL"), -2, 10, 1, 7, Gtk::manage(new RTImage("circle-white-small")), Gtk::manage(new RTImage("circle-black-small"))))),
    noisegam(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISEGAM"), 1.0, 5., 0.1, 1.))),
    LocalcurveEditorwavhue(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_WAVELET_DENOISEHUE"))),
    wavhue(static_cast<FlatCurveEditor*>(LocalcurveEditorwavhue->addCurve(CT_Flat, "", nullptr, false, true))),
    LocalcurveEditorwavhuecont(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_DENOIHUECONT"))),
    wavhuecont(static_cast<FlatCurveEditor*>(LocalcurveEditorwavhuecont->addCurve(CT_Flat, "", nullptr, false, true))),
    noisechrof(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISECHROFINE"), MINCHRO, MAXCHRO, 0.01, 0.))),
    noisechroc(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISECHROCOARSE"), MINCHRO, MAXCHROCC, 0.01, 0.))),
    noisechrodetail(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NOISECHRODETAIL"), 0., 100., 0.01, 50.))),
    detailFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_DETAILFRA")))),
    detailthr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_DETAILTHR"), 0, 100, 1, 50))),
    adjblur(Gtk::manage(new Adjuster(M("TP_LOCALLAB_ADJ"), -100., 100., 1., 0., Gtk::manage(new RTImage("circle-blue-yellow-small")), Gtk::manage(new RTImage("circle-red-green-small"))))),
    expdenoise3(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_DENOI2_EXP")))),
    recothresd(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKRECOTHRES"), 1., 2., 0.01, 1.))),
    lowthresd(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRLOW2"), 1., 80., 0.5, 12.))),
    midthresd(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRMID"), 0., 100., 0.5, 0.))),
    midthresdch(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHRMIDCH"), 0., 100., 0.5, 0.))),
    higthresd(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKLCTHR2"), 20., 99., 0.5, 85.))),
    decayd(Gtk::manage(new Adjuster(M("TP_LOCALLAB_MASKDDECAY"), 0.5, 4., 0.1, 2.))),
    invmaskd(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVMASK")))),
    invmask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_INVMASK")))),
    prevFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_LCLABELS")))),
    nlstr(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLLUM"), 0, 100, 1, 0))),
    nldet(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLDET"), 0, 100, 1, 50))),
    nlpat(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLPAT"), 1, 5, 1, 2))),
    nlrad(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLRAD"), 1, 10, 1, 3))),
    nlgam(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLGAM"), 2., 5., 0.1, 3.))),
    nliter(Gtk::manage(new Adjuster(M("TP_LOCALLAB_NLITER"), 1., 5., 1., 1.))),
    bilateral(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BILATERAL"), 0, 100, 1, 0))),
    sensiden(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SENSI"), 0, 100, 1, 60))),
    reparden(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LOGREPART"), 1.0, 100.0, 1., 100.0))),
    neutral(Gtk::manage (new Gtk::Button (M ("TP_RETINEX_NEUTRAL")))),
    expmaskbl(Gtk::manage(new MyExpander(false, M("TP_LOCALLAB_SHOWPLUS")))),
    showmaskblMethod(Gtk::manage(new MyComboBoxText())),
    showmaskblMethodtyp(Gtk::manage(new MyComboBoxText())),
    enablMask(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_ENABLE_MASK")))),
//    maskblCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK"))),
    maskblCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, "", 1)),
    CCmaskblshape(static_cast<FlatCurveEditor*>(maskblCurveEditorG->addCurve(CT_Flat, "C", nullptr, false, false))),
    LLmaskblshape(static_cast<FlatCurveEditor*>(maskblCurveEditorG->addCurve(CT_Flat, "L", nullptr, false, false))),
    HHmaskblshape(static_cast<FlatCurveEditor *>(maskblCurveEditorG->addCurve(CT_Flat, "LC(h)", nullptr, false, true))),
    strumaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_STRUMASKCOL"), 0., 200., 0.1, 0.))),
    toolbl(Gtk::manage(new Gtk::CheckButton(M("TP_LOCALLAB_TOOLCOL")))),
    toolblFrame(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_TOOLMASK")))),
    toolblFrame2(Gtk::manage(new Gtk::Frame(M("TP_LOCALLAB_TOOLMASK_2")))),
    blendmaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_BLENDMASKCOL"), -100, 100, 1, 0))),
    radmaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_RADMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    lapmaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_LAPMASKCOL"), 0.0, 100.0, 0.1, 0.))),
    chromaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_CHROMASKCOL"), -100.0, 100.0, 0.1, 0.))),
    gammaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_GAMMASKCOL"), 0.05, 5.0, 0.01, 1.))),
    slomaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SLOMASKCOL"), 0.0, 15.0, 0.1, 0.))),
    shadmaskbl(Gtk::manage(new Adjuster(M("TP_LOCALLAB_HIGHMASKCOL"), 0, 100, 1, 0))),
    shadmaskblsha(Gtk::manage(new Adjuster(M("TP_LOCALLAB_SHAMASKCOL"), 0, 100, 1, 0))),
    mask2blCurveEditorG(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_MASK"))),
    Lmaskblshape(static_cast<DiagonalCurveEditor*>(mask2blCurveEditorG->addCurve(CT_Diagonal, "L(L)"))),
    mask2blCurveEditorGwav(new CurveEditorGroup(App::get().mut_options().lastlocalCurvesDir, M("TP_LOCALLAB_WAVMASK"))),
    LLmaskblshapewav(static_cast<FlatCurveEditor*>(mask2blCurveEditorGwav->addCurve(CT_Flat, "L(L)", nullptr, false, false))),
    quaHBox(Gtk::manage(new Gtk::Box())),
    csThresholdblur(Gtk::manage(new ThresholdAdjuster(M("TP_LOCALLAB_CSTHRESHOLDBLUR"), 0, 9, 0, 0, 6, 5, 0, false)))
{
    auto m = ProcEventMapper::getInstance();
    Evlocallabnliter = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_NLITER");
    Evlocallabdenocontrast = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOCONTRAST");
    Evlocallabautodenoon = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOAUTO");
    Evlocallabautodenooff = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOAUTO");
    Evlocallabcontrshow = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOCONTRSHOW");
    Evlocallabdenoratio = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENORATIO");
    Evlocallabenacontrast = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOENA");
    Evlocallabdenomask = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOMASK");
    EvlocallabwavCurvehuecont = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_DENOMASKCURV");   
    Evlocallablockmadl = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_LOCKMADL");
    Evlocallablockmadls = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_LOCKMADLS");
    Evlocallabmadllock = m->newEvent(AUTOEXP, "HISTORY_MSG_LOCAL_LOCKMADL2");
   
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    denocontrast->addAutoButton(M("TP_LOCALLAB_DENORADIUS_TOOLTIP"));
    const LocallabParams::LocallabSpot defSpot;

    // Parameter Blur, Noise & Denoise specific widgets
    setExpandAlignProperties(expblnoise, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    float R, G, B;
    std::vector<GradientMilestone> six_shape;

    for (int i = 0; i < 6; i++) {
        const float x = static_cast<float>(i) * (1.f / 6.f);
        Color::hsv2rgb01(x, 0.5f, 0.5f, R, G, B);
        six_shape.emplace_back(x, R, G, B);
    }

    blMethod->append(M("TP_LOCALLAB_BLUR"));
    blMethod->append(M("TP_LOCALLAB_BLMED"));
    blMethod->append(M("TP_LOCALLAB_BLGUID"));
    blMethod->set_active(0);
    blMethodConn = blMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::blMethodChanged));

    fftwblConn = fftwbl->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::fftwblChanged));
    usemaskConn = usemask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::usemaskChanged));
    invblConn = invbl->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::invblChanged));
    invmaskdConn = invmaskd->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::invmaskdChanged));
    invmaskConn = invmask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::invmaskChanged));
    contrshowConn = contrshow->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::contrshowChanged));
    enacontrastConn = enacontrast->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::enacontrastChanged));
    lockmadlConn = lockmadl->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::lockmadlChanged));
    madllockConn = madllock->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::madllockChanged));
    for (const auto adj : madls) {
        adj->setAdjusterListener(this);
    }
    
    denocontrast->setAdjusterListener(this);
    
    denoratio->setAdjusterListener(this);

    denomask->setAdjusterListener(this);
    
    radius->setAdjusterListener(this);

    strength->setAdjusterListener(this);

    grainFrame->set_label_align(0.025, 0.5);
    grainFrame2->set_label_align(0.025, 0.5);

    isogr->setAdjusterListener(this);

    strengr->setAdjusterListener(this);

    scalegr->setAdjusterListener(this);
    divgr->setAdjusterListener(this);

    medMethod->append(M("TP_LOCALLAB_MEDNONE"));
    medMethod->append(M("TP_DIRPYRDENOISE_TYPE_3X3"));
    medMethod->append(M("TP_DIRPYRDENOISE_TYPE_5X5"));
    medMethod->append(M("TP_DIRPYRDENOISE_TYPE_7X7"));
    medMethod->append(M("TP_DIRPYRDENOISE_TYPE_9X9"));
    medMethod->set_active(0);
    medMethodConn = medMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::medMethodChanged));

    itera->setAdjusterListener(this);

    guidbl->setLogScale(100, 0);
    guidbl->setAdjusterListener(this);

    strbl->setAdjusterListener(this);

    epsbl->setAdjusterListener(this);
    setExpandAlignProperties(expdenoise2, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    recothres->setAdjusterListener(this);
    lowthres->setAdjusterListener(this);
    higthres->setAdjusterListener(this);
    recothres->setLogScale(10, 1);
    sensibn->setAdjusterListener(this);

    blurMethod->append(M("TP_LOCALLAB_BLNORM"));
    blurMethod->append(M("TP_LOCALLAB_BLINV"));
    blurMethod->set_active(0);
    blurMethodConn = blurMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::blurMethodChanged));

    chroMethod->append(M("TP_LOCALLAB_BLLO"));
    chroMethod->append(M("TP_LOCALLAB_BLCO"));
    chroMethod->append(M("TP_LOCALLAB_BLLC"));
    chroMethod->set_active(0);
    chroMethodConn = chroMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::chroMethodChanged));

    activlumConn = activlum->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::activlumChanged));

    setExpandAlignProperties(expdenoise, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);


    quamethod->append(M("TP_LOCALLAB_QUANONEALL"));
    quamethod->append(M("TP_LOCALLAB_QUACONSER"));
    quamethod->append(M("TP_LOCALLAB_QUAAGRES"));
    quamethod->append(M("TP_LOCALLAB_QUANONEWAV"));
    quamethodconn = quamethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::quamethodChanged));
    Gtk::Label* const quaLabel = Gtk::manage(new Gtk::Label(M("TP_WAVELET_DENQUA") + ":"));
    quaHBox->pack_start(*quaLabel, Gtk::PACK_SHRINK, 4);
    quaHBox->pack_start(*quamethod);
    setExpandAlignProperties(expdenoisenl, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    setExpandAlignProperties(expdenoiselum, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    setExpandAlignProperties(expdenoisech, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    LocalcurveEditorwavden->setCurveListener(this);

    wavshapeden->setIdentityValue(0.);
    wavshapeden->setResetCurve(FlatCurveType(defSpot.locwavcurveden.at(0)), defSpot.locwavcurveden);

    setExpandAlignProperties(lCLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    setExpandAlignProperties(lumLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    setExpandAlignProperties(lum46Labels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);

    setExpandAlignProperties(chroLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    setExpandAlignProperties(chro46Labels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);


    LocalcurveEditorwavden->curveListComplete();
    setExpandAlignProperties(expdenoise1, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    levelthr->setAdjusterListener(this);
    lnoiselow->setAdjusterListener(this);

    levelthrlow->setAdjusterListener(this);

    noiselumf0->setAdjusterListener(this);

    noiselumf->setAdjusterListener(this);

    noiselumf2->setAdjusterListener(this);

    noiselumc->setAdjusterListener(this);

    noiselumdetail->setAdjusterListener(this);

    noiselequal->setAdjusterListener(this);

    noisegam->setAdjusterListener(this);

    LocalcurveEditorwavhue->setCurveListener(this);

    wavhue->setIdentityValue(0.);
    wavhue->setResetCurve(FlatCurveType(defSpot.locwavcurvehue.at(0)), defSpot.locwavcurvehue);
    wavhue->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    wavhue->setCurveColorProvider(this, 3);
    wavhue->setBottomBarBgGradient(six_shape);



//    wavguid->setIdentityValue(0.);
//    wavguid->setResetCurve(FlatCurveType(defSpot.locwavcurveguid.at(0)), defSpot.locwavcurveguid);

    LocalcurveEditorwavhue->curveListComplete();
    
    LocalcurveEditorwavhuecont->setCurveListener(this);
    wavhuecont->setIdentityValue(0.);
    wavhuecont->setResetCurve(FlatCurveType(defSpot.locwavcurvehuecont.at(0)), defSpot.locwavcurvehuecont);
    wavhuecont->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_LL_TOOLTIP"));
    wavhuecont->setCurveColorProvider(this, 3);
    wavhuecont->setBottomBarBgGradient(six_shape);
       
    LocalcurveEditorwavhuecont->curveListComplete();

    noisechrof->setAdjusterListener(this);

    noisechroc->set_tooltip_text(M("TP_LOCALLAB_NOISECHROC_TOOLTIP"));
    noisechroc->setAdjusterListener(this);

    noisechrodetail->setAdjusterListener(this);

    detailFrame->set_label_align(0.025, 0.5);
    detailthr->setAdjusterListener(this);

    adjblur->setAdjusterListener(this);
    setExpandAlignProperties(expdenoise3, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    recothresd->setAdjusterListener(this);
    lowthresd->setAdjusterListener(this);
    midthresd->setAdjusterListener(this);
    midthresdch->setAdjusterListener(this);
    higthresd->setAdjusterListener(this);
    decayd->setAdjusterListener(this);

    bilateral->setAdjusterListener(this);
    prevFrame->set_label_align(0.025, 0.5);

    nlstr->setAdjusterListener(this);
    nldet->setAdjusterListener(this);
    nlpat->setAdjusterListener(this);
    nlrad->setAdjusterListener(this);
    nlgam->setAdjusterListener(this);
    nliter->setAdjusterListener(this);

    sensiden->setAdjusterListener(this);
    reparden->setAdjusterListener(this);


    setExpandAlignProperties (neutral, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    RTImage *resetImg = Gtk::manage (new RTImage ("undo-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties (resetImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    neutral->set_image (*resetImg);
    neutral->set_tooltip_text (M ("TP_RETINEX_NEUTRAL_TOOLTIP"));
    neutralconn = neutral->signal_pressed().connect ( sigc::mem_fun (*this, &LocallabBlur::neutral_pressed) );
    neutral->show();

    setExpandAlignProperties(expmaskbl, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    showmaskblMethod->append(M("TP_LOCALLAB_SHOWMNONE"));
    showmaskblMethod->append(M("TP_LOCALLAB_SHOWMODIF"));
    showmaskblMethod->append(M("TP_LOCALLAB_SHOWMODIFMASK"));
    showmaskblMethod->append(M("TP_LOCALLAB_SHOWMASK"));
    showmaskblMethod->set_active(0);
    showmaskblMethod->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKCOL_TOOLTIP"));
    showmaskblMethodConn = showmaskblMethod->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::showmaskblMethodChanged));

    showmaskblMethodtyp->append(M("TP_LOCALLAB_SHOWMASKTYP1"));
    showmaskblMethodtyp->append(M("TP_LOCALLAB_SHOWMASKTYP2"));
    showmaskblMethodtyp->append(M("TP_LOCALLAB_SHOWMASKTYP3"));
    showmaskblMethodtyp->set_active(1);
    showmaskblMethodtypConn = showmaskblMethodtyp->signal_changed().connect(sigc::mem_fun(*this, &LocallabBlur::showmaskblMethodtypChanged));

    enablMaskConn = enablMask->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::enablMaskChanged));

    maskblCurveEditorG->setCurveListener(this);

    CCmaskblshape->setIdentityValue(0.);
    CCmaskblshape->setResetCurve(FlatCurveType(defSpot.CCmaskblcurve.at(0)), defSpot.CCmaskblcurve);
    CCmaskblshape->setBottomBarColorProvider(this, 1);

    LLmaskblshape->setIdentityValue(0.);
    LLmaskblshape->setResetCurve(FlatCurveType(defSpot.LLmaskblcurve.at(0)), defSpot.LLmaskblcurve);
    LLmaskblshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    HHmaskblshape->setIdentityValue(0.);
    HHmaskblshape->setResetCurve(FlatCurveType(defSpot.HHmaskblcurve.at(0)), defSpot.HHmaskblcurve);
    HHmaskblshape->setCurveColorProvider(this, 2);
    HHmaskblshape->setBottomBarColorProvider(this, 2);

    maskblCurveEditorG->curveListComplete();

    strumaskbl->setAdjusterListener(this);

    toolblConn = toolbl->signal_toggled().connect(sigc::mem_fun(*this, &LocallabBlur::toolblChanged));

    blendmaskbl->setAdjusterListener(this);

    radmaskbl->setAdjusterListener(this);

    lapmaskbl->setAdjusterListener(this);

    chromaskbl->setAdjusterListener(this);

    gammaskbl->setAdjusterListener(this);

    slomaskbl->setAdjusterListener(this);

    shadmaskbl->setAdjusterListener(this);

    shadmaskblsha->setAdjusterListener(this);

    mask2blCurveEditorG->setCurveListener(this);

    Lmaskblshape->setResetCurve(DiagonalCurveType(defSpot.Lmaskblcurve.at(0)), defSpot.Lmaskblcurve);
    Lmaskblshape->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    Lmaskblshape->setLeftBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});
    mask2blCurveEditorG->curveListComplete();

    mask2blCurveEditorGwav->setCurveListener(this);

    LLmaskblshapewav->setIdentityValue(0.);
    LLmaskblshapewav->setResetCurve(FlatCurveType(defSpot.LLmaskblcurvewav.at(0)), defSpot.LLmaskblcurvewav);
    LLmaskblshapewav->setBottomBarBgGradient({{0., 0., 0., 0.}, {1., 1., 1., 1.}});

    mask2blCurveEditorGwav->curveListComplete();

    csThresholdblur->setAdjusterListener(this);

    // Add Blur, Noise & Denoise specific widgets to GUI
    ToolParamBlock* const blnoisebox = Gtk::manage(new ToolParamBlock());
    blnoisebox->pack_start(*sensibn);
    blnoisebox->pack_start(*invbl);
    blnoisebox->pack_start(*blMethod);
    blnoisebox->pack_start(*fftwbl, Gtk::PACK_SHRINK, 0);
    blnoisebox->pack_start(*radius);
    blnoisebox->pack_start(*strength);

    ToolParamBlock* const grain2Box = Gtk::manage(new ToolParamBlock());
    grain2Box->pack_start(*isogr);
    grain2Box->pack_start(*divgr);
    grainFrame2->add(*grain2Box);

    ToolParamBlock* const grainBox = Gtk::manage(new ToolParamBlock());
    grainBox->pack_start(*grainFrame2);
    grainBox->pack_start(*strengr);
    grainBox->pack_start(*scalegr);
    grainFrame->add(*grainBox);
    blnoisebox->pack_start(*grainFrame);
    blnoisebox->pack_start(*medMethod);
    blnoisebox->pack_start(*itera);
    blnoisebox->pack_start(*guidbl);
    blnoisebox->pack_start(*strbl);
    blnoisebox->pack_start(*epsbl);
    ToolParamBlock* const wavBox2 = Gtk::manage(new ToolParamBlock());
    wavBox2->pack_start(*maskusable2, Gtk::PACK_SHRINK, 0);
    wavBox2->pack_start(*maskunusable2, Gtk::PACK_SHRINK, 0);
    wavBox2->pack_start(*recothres);
    wavBox2->pack_start(*lowthres);
    wavBox2->pack_start(*higthres);
    wavBox2->pack_start(*invmask);
    expdenoise2->add(*wavBox2, false);
    blnoisebox->pack_start(*expdenoise2);
//    blnoisebox->pack_start(*sensibn);
//    blnoisebox->pack_start(*blurMethod);
//    blnoisebox->pack_start(*invbl);
    blnoisebox->pack_start(*chroMethod);
    // blnoisebox->pack_start(*activlum);
    expblnoise->add(*blnoisebox, false);
    pack_start(*expblnoise);
    ToolParamBlock* const denoisebox = Gtk::manage(new ToolParamBlock());
    Gtk::Frame* const wavFrame = Gtk::manage(new Gtk::Frame());
    ToolParamBlock* const wavBox = Gtk::manage(new ToolParamBlock());
    denoFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const denoBox = Gtk::manage(new ToolParamBlock());
    
    denoBox->pack_start(*quaHBox);
    denoBox->pack_start(*enacontrast);
    denoBox->pack_start(*denocontrast);
    denoBox->pack_start(*denoratio);
    denoBox->pack_start(*contrshow);
    denoBox->pack_start(*denomask);
    denoBox->pack_start(*LocalcurveEditorwavhuecont, Gtk::PACK_SHRINK, 4);
   // denoBox->pack_start(*quaHBox);
    denoFrame->add(*denoBox);
    wavBox->pack_start(*denoFrame);
    
    wavBox->pack_start(*sensiden);
    wavBox->pack_start(*reparden);
    ToolParamBlock* const prevBox = Gtk::manage(new ToolParamBlock());
    prevBox->pack_start(*lumLabels);
    prevBox->pack_start(*lum46Labels);
    prevBox->pack_start(*lCLabels);
    prevBox->pack_start(*chroLabels);
    prevBox->pack_start(*chro46Labels);
    prevBox->pack_start(*lockmadl);
    madlFrame->set_label_align(0.025, 0.5);
    ToolParamBlock* const madlBox = Gtk::manage(new ToolParamBlock());
    
    for (const auto adj : madls) {
        madlBox->pack_start(*adj);
        adj->setLogScale(10, 0);

    }
    madlBox->pack_start(*madllock);
    madlFrame->add(*madlBox);
    prevBox->pack_start(*madlFrame);
    prevFrame->add(*prevBox);
    wavBox->pack_start(*prevFrame);

    ToolParamBlock* const nlbox = Gtk::manage(new ToolParamBlock());
    nlbox->pack_start(*nlstr);
    nlbox->pack_start(*nldet);
    nlbox->pack_start(*nlgam);
    nlbox->pack_start(*nlpat);
    nlbox->pack_start(*nlrad);
    nlbox->pack_start(*nliter);
    expdenoisenl->add(*nlbox);

    wavBox->pack_start(*expdenoisenl);


    // wavBox->pack_start(*noiselumf0);
    // wavBox->pack_start(*noiselumf);
    // wavBox->pack_start(*noiselumf2);
    // wavBox->pack_start(*noiselumc);//unused here, but used for normalize_mean_dt
    ToolParamBlock* const wchBox = Gtk::manage(new ToolParamBlock());

    wchBox->pack_start(*LocalcurveEditorwavden, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    wchBox->pack_start(*noiselumdetail);
    wchBox->pack_start(*noiselequal);
    wchBox->pack_start(*noisegam);
    wchBox->pack_start(*LocalcurveEditorwavhue, Gtk::PACK_SHRINK, 4);
    ToolParamBlock* const wavBox1 = Gtk::manage(new ToolParamBlock());
    wavBox1->pack_start(*maskusable, Gtk::PACK_SHRINK, 0);
    wavBox1->pack_start(*maskunusable, Gtk::PACK_SHRINK, 0);
    wavBox1->pack_start(*lnoiselow, Gtk::PACK_SHRINK, 0);
    wavBox1->pack_start(*levelthrlow, Gtk::PACK_SHRINK, 0);
    wavBox1->pack_start(*levelthr, Gtk::PACK_SHRINK, 0);
    expdenoise1->add(*wavBox1, false);
    wchBox->pack_start(*expdenoise1);
    expdenoiselum->add(*wchBox);
    wavBox->pack_start(*expdenoiselum);
    ToolParamBlock* const chBox = Gtk::manage(new ToolParamBlock());

    chBox->pack_start(*noisechrof);
    chBox->pack_start(*noisechroc);
    chBox->pack_start(*noisechrodetail);
    chBox->pack_start(*adjblur);
    expdenoisech->add(*chBox);
    wavBox->pack_start(*expdenoisech);

    ToolParamBlock* const detailBox = Gtk::manage(new ToolParamBlock());
    detailBox->pack_start(*detailthr);
    detailBox->pack_start(*usemask, Gtk::PACK_SHRINK, 0);
    detailFrame->add(*detailBox);
    wavBox->pack_start(*detailFrame);
    wavFrame->add(*wavBox);
    denoisebox->pack_start(*wavFrame);

    ToolParamBlock* const wavBox3 = Gtk::manage(new ToolParamBlock());
    wavBox3->pack_start(*maskusable3, Gtk::PACK_SHRINK, 0);
    wavBox3->pack_start(*maskunusable3, Gtk::PACK_SHRINK, 0);
    wavBox3->pack_start(*recothresd);
    wavBox3->pack_start(*lowthresd);
    wavBox3->pack_start(*midthresd);
    wavBox3->pack_start(*midthresdch);
    wavBox3->pack_start(*higthresd);
    wavBox3->pack_start(*decayd);
    wavBox3->pack_start(*invmaskd);
    expdenoise3->add(*wavBox3, false);
    denoisebox->pack_start(*expdenoise3);
    denoisebox->pack_start(*bilateral);
    denoisebox->pack_start(*neutral);

    expdenoise->add(*denoisebox, false);
    pack_start(*expdenoise);
    ToolParamBlock* const maskblBox = Gtk::manage(new ToolParamBlock());
    maskblBox->pack_start(*showmaskblMethod, Gtk::PACK_SHRINK, 4);
    maskblBox->pack_start(*showmaskblMethodtyp, Gtk::PACK_SHRINK, 4);
    maskblBox->pack_start(*enablMask, Gtk::PACK_SHRINK, 0);
    maskblBox->pack_start(*maskblCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    maskblBox->pack_start(*strumaskbl, Gtk::PACK_SHRINK, 0);
    maskblBox->pack_start(*toolbl, Gtk::PACK_SHRINK, 0);
    Gtk::Separator* const separatorstrubl = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    maskblBox->pack_start(*separatorstrubl, Gtk::PACK_SHRINK, 2);
    maskblBox->pack_start(*blendmaskbl, Gtk::PACK_SHRINK, 0);
    toolblFrame->set_label_align(0.025, 0.5);
    toolblFrame2->set_label_align(0.025, 0.5);
    ToolParamBlock* const toolblBox = Gtk::manage(new ToolParamBlock());
    toolblBox->pack_start(*radmaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*lapmaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*chromaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*gammaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*slomaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*shadmaskblsha, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*shadmaskbl, Gtk::PACK_SHRINK, 0);
    toolblBox->pack_start(*mask2blCurveEditorG, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    ToolParamBlock* const toolblBox2 = Gtk::manage(new ToolParamBlock());
    toolblBox2->pack_start(*mask2blCurveEditorGwav, Gtk::PACK_SHRINK, 4); // Padding is mandatory to correct behavior of curve editor
    toolblBox2->pack_start(*csThresholdblur, Gtk::PACK_SHRINK, 0);
    toolblFrame2->add(*toolblBox2);
    toolblBox->pack_start(*toolblFrame2);
    toolblFrame->add(*toolblBox);
    maskblBox->pack_start(*toolblFrame);
    expmaskbl->add(*maskblBox, false);
    pack_start(*expmaskbl);
}

LocallabBlur::~LocallabBlur()
{

}

void LocallabBlur::adjusterAutoToggled(Adjuster* a, bool newval)
{
    if (listener && a == denocontrast) {
        auto e = (!newval) ? Evlocallabautodenooff : Evlocallabautodenoon ;
        listener->panelChanged(e, newval ? M("GENERAL_ENABLED") : M("GENERAL_DISABLED"));
    }

}

void LocallabBlur::autodenoContrastChanged(float autodenoContrast)
{
    idle_register.add(
        [this, autodenoContrast]() -> bool
        {
            disableListener();
            denocontrast->setValue(autodenoContrast);
            enableListener();
            return false;
        }
    );
}


bool LocallabBlur::isMaskViewActive()
{
    return (showmaskblMethod->get_active_row_number() != 0);
}

void LocallabBlur::resetMaskView()
{
    showmaskblMethodConn.block(true);
    showmaskblMethod->set_active(0);
    showmaskblMethodConn.block(false);
}

//new function Global
void LocallabBlur::updateguiblur(int spottype) 
{
    {
        idle_register.add(
        [this, spottype]() -> bool {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

            // Update GUI fullimage or main
            disableListener();

            if(spottype == 3) {
                sensibn->hide();
                sensiden->hide();
                invbl->hide();
                expmaskbl->hide();
                expdenoise2->hide();
                enablMask->set_active(false);
            } else {
                sensibn->show();
                sensiden->show();
                invbl->show();
                expmaskbl->show();
                expdenoise2->show();                
                updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

            }
            enableListener();

        return false;
        }
        );
    }
   
}



void LocallabBlur::getMaskView(int &colorMask, int &colorMaskinv, int &expMask, int &expMaskinv, int &shMask, int &shMaskinv, int &vibMask, int &softMask, int &blMask, int &tmMask, int &retiMask, int &sharMask, int &lcMask, int &cbMask, int &logMask, int &maskMask, int &cieMask)
{
    blMask = showmaskblMethod->get_active_row_number();
}

void LocallabBlur::updateAdviceTooltips(const bool showTooltips)
{
    if (showTooltips) {
        expblnoise->set_tooltip_markup(M("TP_LOCALLAB_BLUMETHOD_TOOLTIP"));
        radius->set_tooltip_text(M("TP_LOCALLAB_RADIUS_TOOLTIP"));
        strength->set_tooltip_text(M("TP_LOCALLAB_NOISE_TOOLTIP"));
        grainFrame->set_tooltip_text(M("TP_LOCALLAB_GRAIN_TOOLTIP"));
        sensibn->set_tooltip_text(M("TP_LOCALLAB_SENSI_TOOLTIP"));
        reparden->set_tooltip_text(M("TP_LOCALLAB_REPARDEN_TOOLTIP"));
        medMethod->set_tooltip_text(M("TP_LOCALLAB_MEDIAN_TOOLTIP"));
        itera->set_tooltip_text(M("TP_LOCALLAB_MEDIANITER_TOOLTIP"));
        fftwbl->set_tooltip_text(M("TP_LOCALLAB_FFTMASK_TOOLTIP"));
        guidbl->set_tooltip_text(M("TP_LOCALLAB_GUIDBL_TOOLTIP"));
        strbl->set_tooltip_text(M("TP_LOCALLAB_GUIDSTRBL_TOOLTIP"));
        epsbl->set_tooltip_text(M("TP_LOCALLAB_GUIDEPSBL_TOOLTIP"));
        blurMethod->set_tooltip_markup(M("TP_LOCALLAB_BLMETHOD_TOOLTIP"));
        invbl->set_tooltip_markup(M("TP_LOCALLAB_INVBL_TOOLTIP"));
        expdenoise->set_tooltip_markup(M("TP_LOCALLAB_DENOI_TOOLTIP"));
        quamethod->set_tooltip_markup(M("TP_LOCALLAB_DENOIQUA_TOOLTIP"));
        wavshapeden->setTooltip(M("TP_LOCALLAB_WASDEN_TOOLTIP"));
        wavhue->setTooltip(M("TP_LOCALLAB_WAVHUE_TOOLTIP"));
        expdenoise1->set_tooltip_markup(M("TP_LOCALLAB_MASKLC_TOOLTIP"));
        expdenoise2->set_tooltip_markup(M("TP_LOCALLAB_MASKGF_TOOLTIP"));
        expdenoise3->set_tooltip_markup(M("TP_LOCALLAB_MASKDE_TOOLTIP"));
        expdenoisenl->set_tooltip_markup(M("TP_LOCALLAB_NLFRAME_TOOLTIP"));
        invmask->set_tooltip_text(M("TP_LOCALLAB_MASKDEINV_TOOLTIP"));
        invmaskd->set_tooltip_text(M("TP_LOCALLAB_MASKDEINV_TOOLTIP"));
        LocalcurveEditorwavden->setTooltip(M("TP_LOCALLAB_WASDEN_TOOLTIP"));
        noiselequal->set_tooltip_text(M("TP_LOCALLAB_DENOIEQUAL_TOOLTIP"));
        noisegam->set_tooltip_text(M("TP_LOCALLAB_NOISEGAM_TOOLTIP"));
        noiselumdetail->set_tooltip_text(M("TP_LOCALLAB_DENOILUMDETAIL_TOOLTIP"));
        noisechrof->set_tooltip_text(M("TP_LOCALLAB_DENOICHROF_TOOLTIP"));
        noisechroc->set_tooltip_text(M("TP_LOCALLAB_DENOICHROC_TOOLTIP"));
        noisechrodetail->set_tooltip_text(M("TP_LOCALLAB_DENOICHRODET_TOOLTIP"));
        detailthr->set_tooltip_text(M("TP_LOCALLAB_DENOITHR_TOOLTIP"));
        adjblur->set_tooltip_text(M("TP_LOCALLAB_DENOIEQUALCHRO_TOOLTIP"));
        bilateral->set_tooltip_text(M("TP_LOCALLAB_DENOIBILAT_TOOLTIP"));
        prevFrame->set_tooltip_text(M("TP_LOCALLAB_LCLABELS_TOOLTIP"));
        nlstr->set_tooltip_text(M("TP_LOCALLAB_NLDENOISE_TOOLTIP"));
        nldet->set_tooltip_text(M("TP_LOCALLAB_NLDENOISE_TOOLTIP"));
        nlpat->set_tooltip_text(M("TP_LOCALLAB_NLDENOISENLPAT_TOOLTIP"));
        nlrad->set_tooltip_text(M("TP_LOCALLAB_NLDENOISENLRAD_TOOLTIP"));
        nlgam->set_tooltip_text(M("TP_LOCALLAB_NLDENOISENLGAM_TOOLTIP"));
        nliter->set_tooltip_text(M("TP_LOCALLAB_NLDENOISENLITER_TOOLTIP"));
        noiselumc->set_tooltip_text(M("TP_LOCALLAB_NOISECHROC_TOOLTIP"));
        expmaskbl->set_tooltip_markup(M("TP_LOCALLAB_MASK_TOOLTIP"));
        showmaskblMethodtyp->set_tooltip_markup(M("TP_LOCALLAB_SHOWMASKTYP_TOOLTIP"));
        CCmaskblshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        LLmaskblshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        HHmaskblshape->setTooltip(M("TP_LOCALLAB_CURVEEDITOR_CC_TOOLTIP"));
        blendmaskbl->set_tooltip_text(M("TP_LOCALLAB_BLENDMASK_TOOLTIP"));
        radmaskbl->set_tooltip_text(M("TP_LOCALLAB_LAPRAD_TOOLTIP"));
        lapmaskbl->set_tooltip_text(M("TP_LOCALLAB_LAPRAD_TOOLTIP"));
        Lmaskblshape->setTooltip(M("TP_LOCALLAB_LMASK_LL_TOOLTIP"));
        LLmaskblshapewav->setTooltip(M("TP_LOCALLAB_LMASK_LEVEL_TOOLTIP"));
        maskblCurveEditorG->set_tooltip_markup(M("TP_LOCALLAB_MASKCURVE_TOOLTIP"));
        strumaskbl->set_tooltip_text(M("TP_LOCALLAB_STRUSTRMASK_TOOLTIP"));
        toolbl->set_tooltip_text(M("TP_LOCALLAB_TOOLMASK_TOOLTIP"));
        toolblFrame->set_tooltip_text(M("TP_LOCALLAB_TOOLCOLFRMASK_TOOLTIP"));
        gammaskbl->set_tooltip_text(M("TP_LOCALLAB_GAMMASK_TOOLTIP"));
        chromaskbl->set_tooltip_text(M("TP_LOCALLAB_CHROMASK_TOOLTIP"));
        slomaskbl->set_tooltip_text(M("TP_LOCALLAB_SLOMASK_TOOLTIP"));
        shadmaskbl->set_tooltip_text(M("TP_LOCALLAB_SHADHMASK_TOOLTIP"));
        shadmaskblsha->set_tooltip_text(M("TP_LOCALLAB_SHADMASK_TOOLTIP"));
        lapmaskbl->set_tooltip_text(M("TP_LOCALLAB_LAPRAD1_TOOLTIP"));
        csThresholdblur->set_tooltip_text(M("TP_LOCALLAB_WAVEMASK_LEVEL_TOOLTIP"));
        sensiden->set_tooltip_text(M("TP_LOCALLAB_SENSI_TOOLTIP"));
        lowthres->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRES_TOOLTIP"));
        lowthresd->set_tooltip_text(M("TP_LOCALLAB_MASKLOWTHRESD_TOOLTIP"));
//        midthresd->set_tooltip_text(M("TP_LOCALLAB_MASKMIDTHRESD_TOOLTIP"));
        higthresd->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRESD_TOOLTIP"));
        higthres->set_tooltip_text(M("TP_LOCALLAB_MASKHIGTHRES_TOOLTIP"));
        decayd->set_tooltip_text(M("TP_LOCALLAB_MASKDECAY_TOOLTIP"));
        lCLabels->set_tooltip_text(M("TP_LOCALLAB_LCLABELS_TOOLTIP"));
        denoFrame->set_tooltip_text(M("TP_LOCALLAB_DENORADIUS_TOOLTIP"));
        lockmadl->set_tooltip_text(M("TP_LOCALLAB_LOCKMADL_TOOLTIP"));
        madllock->set_tooltip_text(M("TP_LOCALLAB_LOCKMADL2_TOOLTIP"));
        madlFrame->set_tooltip_text(M("TP_LOCALLAB_LOCKMADLFRA_TOOLTIP"));
    } else {

        expblnoise->set_tooltip_markup("");
        radius->set_tooltip_text("");
        strength->set_tooltip_text("");
        grainFrame->set_tooltip_text("");
        sensibn->set_tooltip_text("");
        reparden->set_tooltip_text("");
        medMethod->set_tooltip_text("");
        itera->set_tooltip_text("");
        fftwbl->set_tooltip_text("");
        guidbl->set_tooltip_text("");
        strbl->set_tooltip_text("");
        epsbl->set_tooltip_text("");
        blurMethod->set_tooltip_markup("");
        quamethod->set_tooltip_markup("");
        wavhue->setTooltip("");
        expdenoise1->set_tooltip_markup("");
        expdenoise2->set_tooltip_markup("");
        expdenoise3->set_tooltip_markup("");
        invmask->set_tooltip_text("");
        invmaskd->set_tooltip_text("");
        LocalcurveEditorwavden->setTooltip("");
        noiselequal->set_tooltip_text("");
        noisegam->set_tooltip_text("");
        noiselumdetail->set_tooltip_text("");
        noisechrof->set_tooltip_text("");
        noisechroc->set_tooltip_text("");
        noisechrodetail->set_tooltip_text("");
        detailthr->set_tooltip_text("");
        adjblur->set_tooltip_text("");
        bilateral->set_tooltip_text("");
        prevFrame->set_tooltip_text("");
        nlstr->set_tooltip_text("");
        nldet->set_tooltip_text("");
        nlpat->set_tooltip_text("");
        nlrad->set_tooltip_text("");
        nlgam->set_tooltip_text("");
        nliter->set_tooltip_text("");
        blurMethod->set_tooltip_markup("");
        expdenoise->set_tooltip_markup("");
        wavshapeden->setTooltip("");
        noiselumc->set_tooltip_text("");
        expmaskbl->set_tooltip_markup("");
        showmaskblMethodtyp->set_tooltip_markup("");
        CCmaskblshape->setTooltip("");
        LLmaskblshape->setTooltip("");
        HHmaskblshape->setTooltip("");
        blendmaskbl->set_tooltip_text("");
        radmaskbl->set_tooltip_text("");
        lapmaskbl->set_tooltip_text("");
        Lmaskblshape->setTooltip("");
        LLmaskblshapewav->setTooltip("");
        maskblCurveEditorG->set_tooltip_markup("");
        strumaskbl->set_tooltip_text("");
        toolbl->set_tooltip_text("");
        toolblFrame->set_tooltip_text("");
        gammaskbl->set_tooltip_text("");
        chromaskbl->set_tooltip_text("");
        slomaskbl->set_tooltip_text("");
        shadmaskbl->set_tooltip_text("");
        shadmaskblsha->set_tooltip_text("");
        csThresholdblur->set_tooltip_text("");
        sensiden->set_tooltip_text("");
        lowthres->set_tooltip_text("");
        lowthresd->set_tooltip_text("");
        higthresd->set_tooltip_text("");
        higthres->set_tooltip_text("");
//       midthresd->set_tooltip_text("");
        decayd->set_tooltip_text("");
        lCLabels->set_tooltip_text("");
        denoFrame->set_tooltip_text("");
        expdenoisenl->set_tooltip_markup("");
        lockmadl->set_tooltip_text("");
        madllock->set_tooltip_text("");
        madlFrame->set_tooltip_text("");

    }
}

void LocallabBlur::neutral_pressed ()
{
    const LocallabParams::LocallabSpot defSpot;
    lnoiselow->setValue(defSpot.lnoiselow);
    levelthr->setValue(defSpot.levelthr);
    levelthrlow->setValue(defSpot.levelthrlow);
    noiselumf0->setValue(defSpot.noiselumf0);
    noiselumdetail->setValue(defSpot.noiselumdetail);
    noiselequal->setValue(defSpot.noiselequal);
    noisegam->setValue(defSpot.noisegam);
    noisechrof->setValue(defSpot.noisechrof);
    noisechroc->setValue(defSpot.noisechroc);
    noisechrodetail->setValue(defSpot.noisechrodetail);
    detailthr->setValue(defSpot.detailthr);;
    adjblur->setValue(defSpot.adjblur);
    bilateral->setValue(defSpot.bilateral);
    nlstr->setValue(defSpot.nlstr);
    nldet->setValue(defSpot.nldet);
    nlpat->setValue(defSpot.nlpat);
    nlrad->setValue(defSpot.nlrad);
    nlgam->setValue(defSpot.nlgam);
    nliter->setValue(defSpot.nliter);
    sensiden->setValue(defSpot.sensiden);
    quamethod->set_active (0);
    wavshapeden->setCurve(defSpot.locwavcurveden);
    wavhue->setCurve(defSpot.locwavcurvehue);
    wavhuecont->setCurve(defSpot.locwavcurvehuecont);
    usemask->set_active(defSpot.usemask);
    invmaskd->set_active(defSpot.invmaskd);
    invmask->set_active(defSpot.invmask);
    recothresd->setValue(defSpot.recothresd);
    lowthresd->setValue(defSpot.lowthresd);
    midthresd->setValue(defSpot.midthresd);
    midthresdch->setValue(defSpot.midthresdch);
    higthresd->setValue(defSpot.higthresd);
    decayd->setValue(defSpot.decayd);
    recothres->setValue(defSpot.recothres);
    lowthres->setValue(defSpot.lowthres);
    higthres->setValue(defSpot.higthres);


}

void LocallabBlur::updatemadlc(const double m0, const double m1, const double m2, const double m3, const double m4, const double m5, const double m6, const double m7,
        const double m8, const double m9, const double m10, const double m11, const double m12, const double m13, const double m14, const double m15, const double m16,
        const double m17, const double m18, const double m19, const double m20, const bool madloc)
{
    idle_register.add(
    [this, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, madloc]() -> bool {
        
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
        
            disableListener();
            if(!madloc) {//allows manual mode
                madls[0]->setValue(m0);
                madls[1]->setValue(m1);
                madls[2]->setValue(m2);
                madls[3]->setValue(m3);
                madls[4]->setValue(m4);
                madls[5]->setValue(m5);
                madls[6]->setValue(m6);
                madls[7]->setValue(m7);
                madls[8]->setValue(m8);
                madls[9]->setValue(m9);
                madls[10]->setValue(m10);
                madls[11]->setValue(m11);
                madls[12]->setValue(m12);
                madls[13]->setValue(m13);
                madls[14]->setValue(m14);
                madls[15]->setValue(m15);
                madls[16]->setValue(m16);
                madls[17]->setValue(m17);
                madls[18]->setValue(m18);
                madls[19]->setValue(m19);
                madls[20]->setValue(m20);
            }
            enableListener();

            return false;
    }
    );



}

void LocallabBlur::updatedenlc(const double highres, const double nres, const double highres46, const double nres46, const double Lhighres, const double Lnres, const double Lhighres46, const double Lnres46)
{
    idle_register.add(
    [this, highres, nres, highres46, nres46, Lhighres, Lnres, Lhighres46, Lnres46]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
        
        double CLnres = Lnres;
        double CLhighres = Lhighres;
        double CLnres46 = Lnres46;
        double CLhighres46 = Lhighres46;
        double Cnres = nres;
        double Chighres = highres;
        double Cnres46 = nres46;
        double Chighres46 = highres46;
        
        if (quamethod->get_active_row_number() == 0) {
            CLnres = CLhighres = CLnres46 = CLhighres46 = Cnres = Chighres = Cnres46 = Chighres46 = 0.;
        }
            

        lumLabels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_LUMLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), CLnres),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), CLhighres))
        );
        lum46Labels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_LUM46LABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), CLnres46 ),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), CLhighres46))
        );
        chroLabels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_CHROLABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), Cnres),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), Chighres))
        );
        chro46Labels->set_text(
            Glib::ustring::compose(M("TP_LOCALLAB_CHRO46LABEL"),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), Cnres46),
                                   Glib::ustring::format(std::fixed, std::setprecision(0), Chighres46))
        );
        return false;
    }
    );
}
void LocallabBlur::setDefaultExpanderVisibility()
{
    expblnoise->set_expanded(false);
    expdenoise->set_expanded(false);
    expdenoise1->set_expanded(false);
    expdenoise2->set_expanded(false);
    expdenoise3->set_expanded(false);
    expmaskbl->set_expanded(false);
    expdenoisenl->set_expanded(false);
    expdenoiselum->set_expanded(false);
    expdenoisech->set_expanded(false);
}

void LocallabBlur::disableListener()
{
    LocallabTool::disableListener();

    blMethodConn.block(true);
    fftwblConn.block(true);
    usemaskConn.block(true);
    invmaskdConn.block(true);
    invmaskConn.block(true);
    invblConn.block(true);
    medMethodConn.block(true);
    blurMethodConn.block(true);
    chroMethodConn.block(true);
    quamethodconn.block(true);
    activlumConn.block(true);
    showmaskblMethodConn.block(true);
    showmaskblMethodtypConn.block(true);
    enablMaskConn.block(true);
    toolblConn.block(true);
    enacontrastConn.block(true);
    contrshowConn.block(true);
    lockmadlConn.block(true);
    madllockConn.block(true);
}

void LocallabBlur::enableListener()
{
    LocallabTool::enableListener();

    blMethodConn.block(false);
    fftwblConn.block(false);
    usemaskConn.block(false);
    invmaskdConn.block(false);
    invmaskConn.block(false);
    invblConn.block(false);
    medMethodConn.block(false);
    blurMethodConn.block(false);
    chroMethodConn.block(false);
    quamethodconn.block(false);
    activlumConn.block(false);
    showmaskblMethodConn.block(false);
    showmaskblMethodtypConn.block(false);
    enablMaskConn.block(false);
    toolblConn.block(false);
    enacontrastConn.block(false);
    contrshowConn.block(false);
    lockmadlConn.block(false);
    madllockConn.block(false);

}

void LocallabBlur::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();

    // Update GUI to selected spot value
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        exp->set_visible(spot.visiblur);
        exp->setEnabled(spot.expblur);
        complexity->set_active(spot.complexblur);

        if (spot.blMethod == "blur") {
            blMethod->set_active(0);
        } else if (spot.blMethod == "med") {
            blMethod->set_active(1);
        } else if (spot.blMethod == "guid") {
            blMethod->set_active(2);
        }
        for (int i = 0; i < 21; i++) {
            madls[i]->setValue(spot.madlsav[i]);
        }

        fftwbl->set_active(spot.fftwbl);
        usemask->set_active(spot.usemask);
        invmaskd->set_active(spot.invmaskd);
        invmask->set_active(spot.invmask);
        invbl->set_active(spot.invbl);
        radius->setValue(spot.radius);
        strength->setValue(spot.strength);
        isogr->setValue((double)spot.isogr);
        strengr->setValue((double)spot.strengr);
        scalegr->setValue((double)spot.scalegr);
        divgr->setValue((double)spot.divgr);

        if (spot.medMethod == "none") {
            medMethod->set_active(0);
        } else if (spot.medMethod == "33") {
            medMethod->set_active(1);
        } else if (spot.medMethod == "55") {
            medMethod->set_active(2);
        } else if (spot.medMethod == "77") {
            medMethod->set_active(3);
        } else if (spot.medMethod == "99") {
            medMethod->set_active(4);
        }

        itera->setValue((double)spot.itera);
        guidbl->setValue((double)spot.guidbl);
        strbl->setValue((double)spot.strbl);
        recothres->setValue((double)spot.recothres);
        lowthres->setValue((double)spot.lowthres);
        higthres->setValue((double)spot.higthres);
        epsbl->setValue((double)spot.epsbl);
        sensibn->setValue((double)spot.sensibn);
        reparden->setValue(spot.reparden);
        recothresd->setValue((double)spot.recothresd);
        lowthresd->setValue((double)spot.lowthresd);
        midthresd->setValue((double)spot.midthresd);
        midthresdch->setValue((double)spot.midthresdch);
        higthresd->setValue((double)spot.higthresd);
        decayd->setValue((double)spot.decayd);

        if (spot.blurMethod == "norm") {
            blurMethod->set_active(0);
        } else if (spot.blurMethod == "inv") {
            blurMethod->set_active(1);
        }

        if (spot.chroMethod == "lum") {
            chroMethod->set_active(0);
        } else if (spot.chroMethod == "chr") {
            chroMethod->set_active(1);
        } else if (spot.chroMethod == "all") {
            chroMethod->set_active(2);
        }

        if (spot.quamethod == "none") {
            quamethod->set_active(0);
        } else if (spot.quamethod == "cons") {
            quamethod->set_active(1);
        } else if (spot.quamethod == "agre") {
            quamethod->set_active(2);
        } else if (spot.quamethod == "nlmean") {
            quamethod->set_active(3);
        }

        activlum->set_active(spot.activlum);
        wavshapeden->setCurve(spot.locwavcurveden);
        wavhue->setCurve(spot.locwavcurvehue);
        wavhuecont->setCurve(spot.locwavcurvehuecont);
        noiselumf0->setValue(spot.noiselumf0);
        noiselumf->setValue(spot.noiselumf);
        noiselumf2->setValue(spot.noiselumf2);
        noiselumc->setValue(spot.noiselumc);
        noiselumdetail->setValue(spot.noiselumdetail);
        levelthr->setValue(spot.levelthr);
        lnoiselow->setValue(spot.lnoiselow);
        levelthrlow->setValue(spot.levelthrlow);
        noiselequal->setValue((double)spot.noiselequal);
        noisegam->setValue((double)spot.noisegam);
        noisechrof->setValue(spot.noisechrof);
        noisechroc->setValue(spot.noisechroc);
        noisechrodetail->setValue(spot.noisechrodetail);
        detailthr->setValue((double)spot.detailthr);
        adjblur->setValue((double)spot.adjblur);
        bilateral->setValue((double)spot.bilateral);
        nlstr->setValue((double)spot.nlstr);
        nldet->setValue((double)spot.nldet);
        nlpat->setValue((double)spot.nlpat);
        nlrad->setValue((double)spot.nlrad);
        nlgam->setValue((double)spot.nlgam);
        nliter->setValue((double)spot.nliter);
        sensiden->setValue((double)spot.sensiden);

        if (spot.showmaskblMethodtyp == "blur") {
            showmaskblMethodtyp ->set_active(0);
        } else if (spot.showmaskblMethodtyp == "nois") {
            showmaskblMethodtyp->set_active(1);
        } else if (spot.showmaskblMethodtyp == "all") {
            showmaskblMethodtyp->set_active(2);
        }

        enablMask->set_active(spot.enablMask);
        CCmaskblshape->setCurve(spot.CCmaskblcurve);
        LLmaskblshape->setCurve(spot.LLmaskblcurve);
        HHmaskblshape->setCurve(spot.HHmaskblcurve);
        strumaskbl->setValue(spot.strumaskbl);
        toolbl->set_active(spot.toolbl);
        blendmaskbl->setValue((double)spot.blendmaskbl);
        radmaskbl->setValue(spot.radmaskbl);
        lapmaskbl->setValue(spot.lapmaskbl);
        chromaskbl->setValue(spot.chromaskbl);
        gammaskbl->setValue(spot.gammaskbl);
        slomaskbl->setValue(spot.slomaskbl);
        shadmaskbl->setValue((double)spot.shadmaskbl);
        shadmaskblsha->setValue((double)spot.shadmaskblsha);
        Lmaskblshape->setCurve(spot.Lmaskblcurve);
        LLmaskblshapewav->setCurve(spot.LLmaskblcurvewav);
        csThresholdblur->setValue<int>(spot.csthresholdblur);
        denocontrast->setValue((double)spot.denocontrast);
        denocontrast->setAutoValue(spot.denoAutocontrast);
        contrshow->set_active(spot.contrshow);
        lockmadl->set_active(spot.lockmadl);
        madllock->set_active(spot.madllock);
        enacontrast->set_active(spot.enacontrast);
        denoratio->setValue((double)spot.denoratio);
        denomask->setValue((double)spot.denomask);

    }

    // Enable all listeners
    enableListener();

    // Update GUI according to complexity mode
    updateGUIToMode(static_cast<modeType>(complexity->get_active_row_number()));

    // Update Blur & Noise GUI according to blMethod combobox state
    updateBlurGUI();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabBlur::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    const int index = pp->locallab.selspot;

    if (index < (int)pp->locallab.spots.size()) {
        LocallabParams::LocallabSpot& spot = pp->locallab.spots.at(index);

        spot.expblur = exp->getEnabled();
        spot.visiblur = exp->get_visible();
        spot.complexblur = complexity->get_active_row_number();

        if (blMethod->get_active_row_number() == 0) {
            spot.blMethod = "blur";
        } else if (blMethod->get_active_row_number() == 1) {
            spot.blMethod = "med";
        } else if (blMethod->get_active_row_number() == 2) {
            spot.blMethod = "guid";
        }
        for (int i = 0; i < 21; i++) {
            spot.madlsav[i] =  madls[i]->getValue();
        }

        spot.fftwbl = fftwbl->get_active();
        spot.usemask = usemask->get_active();
        spot.invmaskd = invmaskd->get_active();
        spot.invmask = invmask->get_active();
        spot.invbl = invbl->get_active();
        spot.radius = radius->getValue();
        spot.strength = strength->getIntValue();
        spot.isogr = isogr->getIntValue();
        spot.strengr = strengr->getIntValue();
        spot.scalegr = scalegr->getIntValue();
        spot.divgr = divgr->getValue();

        if (medMethod->get_active_row_number() == 0) {
            spot.medMethod = "none";
        } else if (medMethod->get_active_row_number() == 1) {
            spot.medMethod = "33";
        } else if (medMethod->get_active_row_number() == 2) {
            spot.medMethod = "55";
        } else if (medMethod->get_active_row_number() == 3) {
            spot.medMethod = "77";
        } else if (medMethod->get_active_row_number() == 4) {
            spot.medMethod = "99";
        }

        spot.itera = itera->getIntValue();
        spot.guidbl = guidbl->getIntValue();
        spot.strbl = strbl->getIntValue();
        spot.recothres = recothres->getValue();
        spot.lowthres = lowthres->getValue();
        spot.higthres = higthres->getValue();
        spot.epsbl = epsbl->getIntValue();
        spot.sensibn = sensibn->getIntValue();
        spot.reparden = reparden->getValue();
        spot.recothresd = recothresd->getValue();
        spot.lowthresd = lowthresd->getValue();
        spot.midthresd = midthresd->getValue();
        spot.midthresdch = midthresdch->getValue();
        spot.higthresd = higthresd->getValue();
        spot.decayd = decayd->getValue();

        if (blurMethod->get_active_row_number() == 0) {
            spot.blurMethod = "norm";
        } else if (blurMethod->get_active_row_number() == 1) {
            spot.blurMethod = "inv";
        }

        if (chroMethod->get_active_row_number() == 0) {
            spot.chroMethod = "lum";
        } else if (chroMethod->get_active_row_number() == 1) {
            spot.chroMethod = "chr";
        } else if (chroMethod->get_active_row_number() == 2) {
            spot.chroMethod = "all";
        }

        if (quamethod->get_active_row_number() == 0) {
            spot.quamethod = "none";
        } else if (quamethod->get_active_row_number() == 1) {
            spot.quamethod = "cons";
        } else if (quamethod->get_active_row_number() == 2) {
            spot.quamethod = "agre";
        } else if (quamethod->get_active_row_number() == 3) {
            spot.quamethod = "nlmean";
        }

        spot.activlum = activlum->get_active();
        spot.locwavcurveden = wavshapeden->getCurve();
        spot.locwavcurvehue = wavhue->getCurve();
        spot.locwavcurvehuecont = wavhuecont->getCurve();
        spot.noiselumf0 = noiselumf0->getValue();
        spot.noiselumf = noiselumf->getValue();
        spot.noiselumf2 = noiselumf2->getValue();
        spot.noiselumc = noiselumc->getValue();
        spot.noiselumdetail = noiselumdetail->getValue();
        spot.levelthr    = levelthr->getValue();
        spot.lnoiselow    = lnoiselow->getValue();
        spot.levelthrlow    = levelthrlow->getValue();
        spot.noiselequal = noiselequal->getIntValue();
        spot.noisegam = noisegam->getValue();
        spot.noisechrof = noisechrof->getValue();
        spot.noisechroc = noisechroc->getValue();
        spot.noisechrodetail = noisechrodetail->getValue();
        spot.detailthr = detailthr->getIntValue();
        spot.adjblur = adjblur->getIntValue();
        spot.bilateral = bilateral->getIntValue();
        spot.sensiden = sensiden->getIntValue();
        spot.nlstr = nlstr->getIntValue();
        spot.nldet = nldet->getIntValue();
        spot.nlpat = nlpat->getIntValue();
        spot.nlrad = nlrad->getIntValue();
        spot.nliter = nliter->getIntValue();
        spot.nlgam = nlgam->getValue();

        if (showmaskblMethodtyp->get_active_row_number() == 0) {
            spot.showmaskblMethodtyp = "blur";
        } else if (showmaskblMethodtyp->get_active_row_number() == 1) {
            spot.showmaskblMethodtyp = "nois";
        } else if (showmaskblMethodtyp->get_active_row_number() == 2) {
            spot.showmaskblMethodtyp = "all";
        }

        spot.enablMask = enablMask->get_active();
        spot.LLmaskblcurve = LLmaskblshape->getCurve();
        spot.CCmaskblcurve = CCmaskblshape->getCurve();
        spot.HHmaskblcurve = HHmaskblshape->getCurve();
        spot.strumaskbl = strumaskbl->getValue();
        spot.toolbl = toolbl->get_active();
        spot.blendmaskbl = blendmaskbl->getIntValue();
        spot.radmaskbl = radmaskbl->getValue();
        spot.lapmaskbl = lapmaskbl->getValue();
        spot.chromaskbl = chromaskbl->getValue();
        spot.gammaskbl = gammaskbl->getValue();
        spot.slomaskbl = slomaskbl->getValue();
        spot.shadmaskbl = shadmaskbl->getIntValue();
        spot.shadmaskblsha = shadmaskblsha->getIntValue();
        spot.Lmaskblcurve = Lmaskblshape->getCurve();
        spot.LLmaskblcurvewav = LLmaskblshapewav->getCurve();
        spot.csthresholdblur = csThresholdblur->getValue<int>();
        spot.denocontrast = denocontrast->getValue();
        spot.denoAutocontrast = denocontrast->getAutoValue();
        spot.contrshow = contrshow->get_active();
        spot.lockmadl = lockmadl->get_active();
        spot.madllock = madllock->get_active();
        spot.enacontrast = enacontrast->get_active();
        spot.denoratio = denoratio->getValue();
        spot.denomask = denomask->getValue();

    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabBlur::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    const int index = defParams->locallab.selspot;

    if (index < (int)defParams->locallab.spots.size()) {
        const LocallabParams::LocallabSpot& defSpot = defParams->locallab.spots.at(index);

        // Set default value for adjuster and threshold adjuster widgets
        for (int i = 0; i < 21; i++) {
            madls[i]->setDefault(defSpot.madlsav[i]);
        }
        
        radius->setDefault(defSpot.radius);
        strength->setDefault((double)defSpot.strength);
        isogr->setDefault((double)defSpot.isogr);
        strengr->setDefault((double)defSpot.strengr);
        scalegr->setDefault((double)defSpot.scalegr);
        divgr->setDefault((double)defSpot.divgr);
        itera->setDefault((double)defSpot.itera);
        guidbl->setDefault((double)defSpot.guidbl);
        strbl->setDefault((double)defSpot.strbl);
        recothres->setDefault((double)defSpot.recothres);
        lowthres->setDefault((double)defSpot.lowthres);
        higthres->setDefault((double)defSpot.higthres);
        epsbl->setDefault((double)defSpot.epsbl);
        sensibn->setDefault((double)defSpot.sensibn);
        reparden->setDefault(defSpot.reparden);
        recothresd->setDefault((double)defSpot.recothresd);
        lowthresd->setDefault((double)defSpot.lowthresd);
        midthresd->setDefault((double)defSpot.midthresd);
        midthresdch->setDefault((double)defSpot.midthresdch);
        higthresd->setDefault((double)defSpot.higthresd);
        decayd->setDefault((double)defSpot.decayd);
        noiselumf0->setDefault(defSpot.noiselumf0);
        noiselumf->setDefault(defSpot.noiselumf);
        noiselumf2->setDefault(defSpot.noiselumf2);
        noiselumc->setDefault(defSpot.noiselumc);
        noiselumdetail->setDefault(defSpot.noiselumdetail);
        levelthr->setDefault(defSpot.levelthr);
        lnoiselow->setDefault(defSpot.lnoiselow);
        levelthrlow->setDefault(defSpot.levelthrlow);
        noiselequal->setDefault((double)defSpot.noiselequal);
        noisegam->setDefault(defSpot.noisegam);
        noisechrof->setDefault(defSpot.noisechrof);
        noisechroc->setDefault(defSpot.noisechroc);
        noisechrodetail->setDefault(defSpot.noisechrodetail);
        detailthr->setDefault((double)defSpot.detailthr);
        adjblur->setDefault((double)defSpot.adjblur);
        bilateral->setDefault((double)defSpot.bilateral);
        nlstr->setDefault((double)defSpot.nlstr);
        nldet->setDefault((double)defSpot.nldet);
        nlpat->setDefault((double)defSpot.nlpat);
        nlrad->setDefault((double)defSpot.nlrad);
        nliter->setDefault((double)defSpot.nliter);
        nlgam->setDefault(defSpot.nlgam);
        sensiden->setDefault((double)defSpot.sensiden);
        strumaskbl->setDefault(defSpot.strumaskbl);
        blendmaskbl->setDefault((double)defSpot.blendmaskbl);
        radmaskbl->setDefault(defSpot.radmaskbl);
        lapmaskbl->setDefault(defSpot.lapmaskbl);
        chromaskbl->setDefault(defSpot.chromaskbl);
        gammaskbl->setDefault(defSpot.gammaskbl);
        slomaskbl->setDefault(defSpot.slomaskbl);
        shadmaskbl->setDefault(defSpot.shadmaskbl);
        shadmaskblsha->setDefault(defSpot.shadmaskblsha);
        csThresholdblur->setDefault<int>(defSpot.csthresholdblur);
        denocontrast->setDefault(defSpot.denocontrast);
        denoratio->setDefault(defSpot.denoratio);
        denomask->setDefault(defSpot.denomask);
    }

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void LocallabBlur::enacontrastChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enacontrast->get_active()) {
                listener->panelChanged(Evlocallabenacontrast,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabenacontrast,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}



void LocallabBlur::contrshowChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (contrshow->get_active()) {
                listener->panelChanged(Evlocallabcontrshow,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabcontrshow,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::lockmadlChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (lockmadl->get_active()) {
                listener->panelChanged(Evlocallablockmadl,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallablockmadl,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::madllockChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (madllock->get_active()) {
                listener->panelChanged(Evlocallabmadllock,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabmadllock,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::adjusterChanged(Adjuster* a, double newval)
{
    if (isLocActivated && exp->getEnabled()) {
        if (a == radius) {
            if (listener) {
                listener->panelChanged(Evlocallabradius,
                                       radius->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == madls[0] || a == madls[1] || a == madls[2] || a == madls[3] || a == madls[4] || a == madls[5] || a == madls[6]){
            if (listener) {//MadL Horizontal
                listener->panelChanged(Evlocallablockmadls,
                                       Glib::ustring::compose("%1, %2, %3, %4, %5, %6, %7",
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[0]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[1]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[2]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[3]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[4]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[5]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[6]->getValue()))
                                       + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }


        if (a == madls[7] || a == madls[8] || a == madls[9] || a == madls[10] || a == madls[11] || a == madls[12] || a == madls[13]){
            if (listener) {//MadL Vertical
                listener->panelChanged(Evlocallablockmadls,
                                       Glib::ustring::compose("%1, %2, %3, %4, %5, %6, %7",
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[7]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[8]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[9]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[10]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[11]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[12]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[13]->getValue()))
                                       + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == madls[14] || a == madls[15] || a == madls[16] || a == madls[17] || a == madls[18] || a == madls[19] || a == madls[20]) {
            if (listener) {//MadL Diagonal
                listener->panelChanged(Evlocallablockmadls,
                                       Glib::ustring::compose("%1, %2, %3, %4 ,%5, %6, %7",
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[14]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[15]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[16]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[17]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[18]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[19]->getValue()),
                                               Glib::ustring::format(std::fixed, std::setprecision(0), madls[20]->getValue()))
                                       + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == denocontrast) {
            if (listener) {
                listener->panelChanged(Evlocallabdenocontrast,
                                       denocontrast->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == denoratio) {
            if (listener) {
                listener->panelChanged(Evlocallabdenoratio,
                                       denoratio->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == denomask) {
            if (listener) {
                listener->panelChanged(Evlocallabdenomask,
                                       denomask->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strength) {
            if (listener) {
                listener->panelChanged(Evlocallabstrength,
                                       strength->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == isogr) {
            if (listener) {
                listener->panelChanged(Evlocallabisogr,
                                       isogr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strengr) {
            if (listener) {
                listener->panelChanged(Evlocallabstrengr,
                                       strengr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == scalegr) {
            if (listener) {
                listener->panelChanged(Evlocallabscalegr,
                                       scalegr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == divgr) {
            if (listener) {
                listener->panelChanged(Evlocallabdivgr,
                                       divgr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == itera) {
            if (listener) {
                listener->panelChanged(Evlocallabitera,
                                       itera->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == guidbl) {
            if (listener) {
                listener->panelChanged(Evlocallabguidbl,
                                       guidbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strbl) {
            if (listener) {
                listener->panelChanged(Evlocallabstrbl,
                                       strbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothres) {
            if(recothres->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 1) {
                    showmaskblMethodtyp->set_active(2);
                }
            }

            if (listener) {
                listener->panelChanged(Evlocallabrecothres,
                                       recothres->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthres) {
            if (listener) {
                listener->panelChanged(Evlocallablowthres,
                                       lowthres->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthres) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthres,
                                       higthres->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == recothresd) {
            if(recothresd->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }

            if (listener) {
                listener->panelChanged(Evlocallabrecothresd,
                                       recothresd->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lowthresd) {
            if (listener) {
                listener->panelChanged(Evlocallablowthresd,
                                       lowthresd->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == midthresd) {
            if (listener) {
                listener->panelChanged(Evlocallabmidthresd,
                                       midthresd->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == midthresdch) {
            if (listener) {
                listener->panelChanged(Evlocallabmidthresdch,
                                       midthresdch->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == higthresd) {
            if (listener) {
                listener->panelChanged(Evlocallabhigthresd,
                                       higthresd->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == decayd) {
            if (listener) {
                listener->panelChanged(Evlocallabdecayd,
                                       decayd->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == epsbl) {
            if (listener) {
                listener->panelChanged(Evlocallabepsbl,
                                       epsbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensibn) {
            if (listener) {
                listener->panelChanged(Evlocallabsensibn,
                                       sensibn->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselumf0) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselumf0,
                                       noiselumf0->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselumf) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselumf,
                                       noiselumf->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselumf2) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselumf2,
                                       noiselumf2->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselumc) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselumc,
                                       noiselumc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselumdetail) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselumdetail,
                                       noiselumdetail->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noiselequal) {
            if (listener) {
                listener->panelChanged(Evlocallabnoiselequal,
                                       noiselequal->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noisegam) {
            if (listener) {
                listener->panelChanged(Evlocallabnoisegam,
                                       noisegam->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == levelthr) {
            if (listener) {
                listener->panelChanged(Evlocallablevelthr,
                                       levelthr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lnoiselow) {
            if(lnoiselow->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }
            if (listener) {
                listener->panelChanged(Evlocallablnoiselow,
                                       lnoiselow->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == levelthrlow) {
            if (listener) {
                listener->panelChanged(Evlocallablevelthrlow,
                                       levelthrlow->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noisechrof) {
            if (listener) {
                listener->panelChanged(Evlocallabnoisechrof,
                                       noisechrof->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noisechroc) {
            if (listener) {
                listener->panelChanged(Evlocallabnoisechroc,
                                       noisechroc->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == noisechrodetail) {
            if (listener) {
                listener->panelChanged(Evlocallabnoisechrodetail,
                                       noisechrodetail->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == detailthr) {
            if (listener) {
                listener->panelChanged(Evlocallabdetailthr,
                                       detailthr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == adjblur) {
            if (listener) {
                listener->panelChanged(Evlocallabadjblur,
                                       adjblur->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == bilateral) {
            if (listener) {
                listener->panelChanged(Evlocallabbilateral,
                                       bilateral->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nlstr) {
            if (listener) {
                listener->panelChanged(Evlocallabnlstr,
                                       nlstr->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nliter) {
            if (listener) {
                listener->panelChanged(Evlocallabnliter,
                                       nliter->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nldet) {
            if (listener) {
                listener->panelChanged(Evlocallabnldet,
                                       nldet->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nlpat) {
            if (listener) {
                listener->panelChanged(Evlocallabnlpat,
                                       nlpat->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nlrad) {
            if (listener) {
                listener->panelChanged(Evlocallabnlrad,
                                       nlrad->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == nlgam) {
            if (listener) {
                listener->panelChanged(Evlocallabnlgam,
                                       nlgam->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == sensiden) {
            if (listener) {
                listener->panelChanged(Evlocallabsensiden,
                                       sensiden->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == reparden) {
            if (listener) {
                listener->panelChanged(Evlocallabreparden,
                                       reparden->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == strumaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabstrumaskbl,
                                       strumaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == blendmaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabblendmaskbl,
                                       blendmaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == radmaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabradmaskbl,
                                       radmaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == lapmaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallablapmaskbl,
                                       lapmaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == chromaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabchromaskbl,
                                       chromaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == gammaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabgammaskbl,
                                       gammaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == slomaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabslomaskbl,
                                       slomaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shadmaskbl) {
            if (listener) {
                listener->panelChanged(Evlocallabshadmaskbl,
                                       shadmaskbl->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (a == shadmaskblsha) {
            if (listener) {
                listener->panelChanged(Evlocallabshadmaskblsha,
                                       shadmaskblsha->getTextValue() + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

    }
}



void LocallabBlur::adjusterChanged2(ThresholdAdjuster* a, int newBottomL, int newTopL, int newBottomR, int newTopR)
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabcsThresholdblur,
                                   csThresholdblur->getHistoryString() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabBlur::curveChanged(CurveEditor* ce)
{
    if (isLocActivated && exp->getEnabled()) {
        if (ce == wavshapeden) {
            if (listener) {
                listener->panelChanged(EvlocallabwavCurveden,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == wavhue) {
            if (listener) {
                listener->panelChanged(EvlocallabwavCurvehue,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == wavhuecont) {
            if (listener) {
                listener->panelChanged(EvlocallabwavCurvehuecont,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == CCmaskblshape) {
            if (listener) {
                listener->panelChanged(EvlocallabCCmaskblshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskblshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskblshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == HHmaskblshape) {
            if (listener) {
                listener->panelChanged(EvlocallabHHmaskblshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == Lmaskblshape) {
            if (listener) {
                listener->panelChanged(EvlocallabLmaskblshape,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }

        if (ce == LLmaskblshapewav) {
            if (listener) {
                listener->panelChanged(EvlocallabLLmaskblshapewav,
                                       M("HISTORY_CUSTOMCURVE") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::enabledChanged()
{
    if (isLocActivated) {
        if (listener) {
            if (exp->getEnabled()) {
                listener->panelChanged(EvLocenablur,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocenablur,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::convertParamToNormal()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    invmask->set_active(defSpot.invmask);
    invmaskd->set_active(defSpot.invmaskd);
    // Set hidden GUI widgets in Normal mode to default spot values
    fftwbl->set_active(defSpot.fftwbl);
    strumaskbl->setValue(defSpot.strumaskbl);
    toolbl->set_active(defSpot.toolbl);
    decayd->setValue(defSpot.decayd);
    lapmaskbl->setValue(defSpot.lapmaskbl);
    shadmaskbl->setValue((double)defSpot.shadmaskbl);
    shadmaskblsha->setValue((double)defSpot.shadmaskblsha);
    LLmaskblshapewav->setCurve(defSpot.LLmaskblcurvewav);
    csThresholdblur->setValue<int>(defSpot.csthresholdblur);
    lnoiselow->setValue(defSpot.lnoiselow);
    nlrad->setValue(defSpot.nlrad);
    noisegam->setValue(defSpot.noisegam);
    madllock->set_active(defSpot.madllock);

    // Enable all listeners
    enableListener();
}

void LocallabBlur::convertParamToSimple()
{
    const LocallabParams::LocallabSpot defSpot;

    // Disable all listeners
    disableListener();
    invmask->set_active(defSpot.invmask);
    invmaskd->set_active(defSpot.invmaskd);
    scalegr->setValue(defSpot.scalegr);
    // Set hidden specific GUI widgets in Simple mode to default spot values
    showmaskblMethod->set_active(0);
    madllock->set_active(defSpot.madllock);
    
    if (defSpot.showmaskblMethodtyp == "blur") {
        showmaskblMethodtyp ->set_active(0);
    } else if (defSpot.showmaskblMethodtyp == "nois") {
        showmaskblMethodtyp->set_active(1);
    } else if (defSpot.showmaskblMethodtyp == "all") {
        showmaskblMethodtyp->set_active(2);
    }
    lnoiselow->setValue(defSpot.lnoiselow);
    enablMask->set_active(defSpot.enablMask);
 //   CCmaskblshape->setCurve(defSpot.CCmaskblcurve);
 //   LLmaskblshape->setCurve(defSpot.LLmaskblcurve);
 //   HHmaskblshape->setCurve(defSpot.HHmaskblcurve);
 //   blendmaskbl->setValue((double)defSpot.blendmaskbl);
 //   radmaskbl->setValue(defSpot.radmaskbl);
 //   chromaskbl->setValue(defSpot.chromaskbl);
 //   gammaskbl->setValue(defSpot.gammaskbl);
 //   slomaskbl->setValue(defSpot.slomaskbl);
 //   Lmaskblshape->setCurve(defSpot.Lmasklccurve);
    levelthr->setValue(defSpot.levelthr);
    lnoiselow->setValue(defSpot.lnoiselow);
    levelthrlow->setValue(defSpot.levelthrlow);
    usemask->set_active(defSpot.usemask);
    invmaskd->set_active(defSpot.invmaskd);
    invmask->set_active(defSpot.invmask);
    recothresd->setValue(defSpot.recothresd);
    lowthresd->setValue(defSpot.lowthresd);
    midthresd->setValue(defSpot.midthresd);
    midthresdch->setValue(defSpot.midthresdch);
    higthresd->setValue(defSpot.higthresd);
    decayd->setValue(defSpot.decayd);
    recothres->setValue(defSpot.recothres);
    lowthres->setValue(defSpot.lowthres);
    higthres->setValue(defSpot.higthres);
    adjblur->setValue(defSpot.adjblur);
    noisechrodetail->setValue(defSpot.noisechrodetail);
    nlpat->setValue(defSpot.nlpat);
    nlrad->setValue(defSpot.nlrad);
    nlgam->setValue(defSpot.nlgam);
    noisegam->setValue(defSpot.noisegam);

    // Enable all listeners
    enableListener();
}

void LocallabBlur::updateGUIToMode(const modeType new_type)
{
    switch (new_type) {
        case Simple:
            // Expert and Normal mode widgets are hidden in Simple mode
            fftwbl->hide();
            expmaskbl->hide();
            expdenoise1->hide();
            expdenoise2->hide();
            expdenoise3->hide();
            maskusable->hide();
            maskunusable->hide();
            maskusable2->hide();
            maskunusable2->hide();
            maskusable3->hide();
            maskunusable3->hide();
            decayd->hide();
            invmask->hide();
            invmaskd->hide();
            adjblur->hide();
            noisechrodetail->hide();
            usemask->hide();
            nlpat->hide();
            nlrad->hide();
            nlgam->hide();
            scalegr->hide();
            noisegam->hide();
            madlFrame->hide();

            break;

        case Normal:
            // Expert mode widgets are hidden in Normal mode
            fftwbl->hide();
            strumaskbl->hide();
            toolbl->hide();
            lapmaskbl->hide();
            shadmaskbl->hide();
            shadmaskblsha->hide();
            mask2blCurveEditorGwav->hide();
            csThresholdblur->hide();
            toolblFrame2->hide();
            // Specific Simple mode widgets are shown in Normal mode
            expmaskbl->show();
            expdenoise1->hide();
            expdenoise2->show();
            expdenoise3->show();
            adjblur->show();
            noisechrodetail->show();
            usemask->show();
            nlpat->show();
            nlrad->hide();
            nlgam->show();
            scalegr->show();
            noisegam->hide();
            madlFrame->hide();


          //  if (blMethod->get_active_row_number() == 2) {
          //      expdenoise2->show();
          //  }

            decayd->hide();
            invmask->hide();
            invmaskd->hide();
            if(lnoiselow->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }
            if(recothres->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 1) {
                    showmaskblMethodtyp->set_active(2);
                }
            }
            if(recothresd->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }

            if (enablMask->get_active()) {
                maskusable->show();
                maskunusable->hide();
                maskusable2->show();
                maskunusable2->hide();
                maskusable3->show();
                maskunusable3->hide();

            } else {
                maskusable->hide();
                maskunusable->show();
                maskusable2->hide();
                maskunusable2->show();
                maskusable3->hide();
                maskunusable3->show();
            }

            break;

        case Expert:

            // Show widgets hidden in Normal and Simple mode
/* 
            if (blMethod->get_active_row_number() == 0) { // Keep widget hidden when blMethod is > 0
                fftwbl->show();
                expdenoise2->hide();
            }
            if (blMethod->get_active_row_number() == 1) {
                expdenoise2->hide();
            }
            if (blMethod->get_active_row_number() == 2) {
                expdenoise2->show();
            }
*/
            expdenoise2->show();
            expdenoise1->show();
            expdenoise3->show();
            decayd->show();
            invmask->show();
            invmaskd->show();
            adjblur->show();
            noisechrodetail->show();
            usemask->show();
            scalegr->show();

            expmaskbl->show();
            strumaskbl->show();
            toolbl->show();
            lapmaskbl->show();
            shadmaskbl->show();
            shadmaskblsha->show();
            mask2blCurveEditorGwav->show();
            csThresholdblur->show();
            toolblFrame2->show();
            nlpat->show();
            nlrad->show();
            nlgam->show();
            noisegam->show();
            madlFrame->show();

            if(lnoiselow->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }
            if(recothres->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 1) {
                    showmaskblMethodtyp->set_active(2);
                }
            }
            if(recothresd->getValue()!= 1.) {
                if (showmaskblMethodtyp->get_active_row_number() == 0) {
                    showmaskblMethodtyp->set_active(2);
                }
            }

            if (enablMask->get_active()) {
                maskusable->show();
                maskunusable->hide();
                maskusable2->show();
                maskunusable2->hide();
                maskusable3->show();
                maskunusable3->hide();
            } else {
                maskusable->hide();
                maskunusable->show();
                maskusable2->hide();
                maskunusable2->show();
                maskusable3->show();
                maskunusable3->hide();
            }

    }
}

void LocallabBlur::updateMaskBackground(const double normChromar, const double normLumar, const double normHuer, const double normHuerjz)
{
    idle_register.add(
    [this, normHuer, normLumar, normChromar]() -> bool {
        GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

        // Update mask background
        CCmaskblshape->updateLocallabBackground(normChromar);
        LLmaskblshape->updateLocallabBackground(normLumar);
        HHmaskblshape->updateLocallabBackground(normHuer);
        Lmaskblshape->updateLocallabBackground(normLumar);
        wavhue->updateLocallabBackground(normHuer);
        return false;
    }
    );
}

void LocallabBlur::blMethodChanged()
{
    // Update Blur & Noise GUI according to blMethod combobox state
    updateBlurGUI();
    const LocallabParams::LocallabSpot defSpot;
    
    if(blMethod->get_active_row_number() == 2) {
        epsbl->show();
    } else {
        epsbl->hide();      
    }
    if (invbl->get_active()  &&  blMethod->get_active_row_number() == 2) {
        radius->setValue(defSpot.radius);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 0) {
        guidbl->setValue(defSpot.guidbl);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 1) {
        radius->setValue(defSpot.radius);
        guidbl->setValue(defSpot.guidbl);
    }


    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabblMethod,
                                   blMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabBlur::fftwblChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (fftwbl->get_active()) {
                listener->panelChanged(Evlocallabfftwbl,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabfftwbl,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::usemaskChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (usemask->get_active()) {
                listener->panelChanged(Evlocallabusemask1,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabusemask1,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::invmaskdChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (invmaskd->get_active()) {
                listener->panelChanged(Evlocallabinvmaskd,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinvmaskd,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::invmaskChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (invmask->get_active()) {
                listener->panelChanged(Evlocallabinvmask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinvmask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}


void LocallabBlur::invblChanged()
{
    const LocallabParams::LocallabSpot defSpot;

    if (invbl->get_active()  &&  blMethod->get_active_row_number() == 2) {
        radius->setValue(defSpot.radius);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 0) {
        guidbl->setValue(defSpot.guidbl);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 1) {
        radius->setValue(defSpot.radius);
        guidbl->setValue(defSpot.guidbl);
    }



    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (invbl->get_active()) {
                listener->panelChanged(Evlocallabinvbl,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabinvbl,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::medMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabmedMethod,
                                   medMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabBlur::blurMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabblurMethod,
                                   blurMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabBlur::chroMethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabchroMethod,
                                   chroMethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}

void LocallabBlur::quamethodChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            listener->panelChanged(EvlocallabquaMethod,
                                   quamethod->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
        }
    }
}


void LocallabBlur::activlumChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (activlum->get_active()) {
                listener->panelChanged(Evlocallabactivlum,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabactivlum,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::showmaskblMethodChanged()
{
    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if (listener) {
        listener->panelChanged(EvlocallabshowmaskMethod, "");
    }
}

void LocallabBlur::showmaskblMethodtypChanged()
{
    if(lnoiselow->getValue()!= 1.) {
        if (showmaskblMethodtyp->get_active_row_number() == 0) {
            showmaskblMethodtyp->set_active(2);
        }
    }
    if(recothres->getValue()!= 1.) {
        if (showmaskblMethodtyp->get_active_row_number() == 1) {
            showmaskblMethodtyp->set_active(2);
        }
    }
    if(recothresd->getValue()!= 1.) {
        if (showmaskblMethodtyp->get_active_row_number() == 0) {
            showmaskblMethodtyp->set_active(2);
        }
    }

    // If mask preview is activated, deactivate all other tool mask preview
    if (locToolListener) {
        locToolListener->resetOtherMaskView(this);
    }

    if (listener) {
        listener->panelChanged(EvlocallabshowmasktypMethod,
                               showmaskblMethodtyp->get_active_text() + " (" + escapeHtmlChars(getSpotName()) + ")");
    }
}

void LocallabBlur::enablMaskChanged()
{
    if (enablMask->get_active()) {
        maskusable->show();
        maskunusable->hide();
        maskusable2->show();
        maskunusable2->hide();
        maskusable3->show();
        maskunusable3->hide();
    } else {
        maskusable->hide();
        maskunusable->show();
        maskusable2->hide();
        maskunusable2->show();
        maskusable3->hide();
        maskunusable3->show();
    }

    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (enablMask->get_active()) {
                listener->panelChanged(EvLocallabEnablMask,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(EvLocallabEnablMask,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::toolblChanged()
{
    if (isLocActivated && exp->getEnabled()) {
        if (listener) {
            if (toolbl->get_active()) {
                listener->panelChanged(Evlocallabtoolbl,
                                       M("GENERAL_ENABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            } else {
                listener->panelChanged(Evlocallabtoolbl,
                                       M("GENERAL_DISABLED") + " (" + escapeHtmlChars(getSpotName()) + ")");
            }
        }
    }
}

void LocallabBlur::updateBlurGUI()
{
    const LocallabParams::LocallabSpot defSpot;

    if (invbl->get_active()  &&  blMethod->get_active_row_number() == 2) {
        radius->setValue(defSpot.radius);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 0) {
        guidbl->setValue(defSpot.guidbl);
        medMethod->set_active(0);
    } else if(invbl->get_active()  &&  blMethod->get_active_row_number() == 1) {
        radius->setValue(defSpot.radius);
        guidbl->setValue(defSpot.guidbl);
    }


    const int mode = complexity->get_active_row_number();

    if (blMethod->get_active_row_number() == 0) {
        if (mode == Expert) { // Keep widget hidden in Normal and Simple mode
            fftwbl->show();
        }
//        expdenoise2->hide();
        radius->show();
        strength->show();
        grainFrame->show();
        medMethod->hide();
        itera->hide();
        guidbl->hide();
        strbl->hide();
        if (mode == Expert  || mode == Normal){
            expdenoise2->show();
            recothres->show();
            lowthres->show();
            higthres->show();
        }
        epsbl->hide();
        activlum->show();
    } else if (blMethod->get_active_row_number() == 1) {
        fftwbl->hide();
        radius->hide();
        strength->hide();
        grainFrame->hide();
        medMethod->show();
        itera->show();
        guidbl->hide();
        strbl->hide();
        if (mode == Expert  || mode == Normal){
            expdenoise2->show();
            recothres->show();
            lowthres->show();
            higthres->show();
        }
        epsbl->hide();
        activlum->show();
    } else if (blMethod->get_active_row_number() == 2) {
        fftwbl->hide();
        radius->hide();
        strength->hide();
        grainFrame->hide();
        medMethod->hide();
        itera->hide();
        guidbl->show();
        strbl->show();
        if (mode == Expert  || mode == Normal){
            expdenoise2->show();
            recothres->show();
            lowthres->show();
            higthres->show();
        }
        epsbl->show();
        activlum->hide();
    }
}
