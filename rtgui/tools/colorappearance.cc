/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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
 *  Jacques Desmis jdesmis@gmail.com  2012 - 2020 - 2026
 * 
 */
#include <cmath>

#include "colorappearance.h"

#include "guiutils.h"
#include "options.h"
#include "rtimage.h"
#include "eventmapper.h"
#include "widgets/basic/myexpander.h"
#include "widgets/curves/curveeditor.h"
#include "widgets/curves/curveeditorgroup.h"

#include "rtengine/color.h"
#include "rtengine/procparams.h"
#include "rtengine/utils.h"

#define MINTEMP0 2000   //1200
#define MAXTEMP0 12000  //12000
#define CENTERTEMP0 5003
#define MINGREEN0 0.8
#define MAXGREEN0 1.2

#define MINLA0 0.01
#define MAXLA0 16384
//#define CENTERLA0 500

using namespace rtengine;
using namespace rtengine::procparams;

const Glib::ustring ColorAppearance::TOOL_NAME = "colorappearance";

static double wbSlider2Temp (double sval)
{

    // slider range: 0 - 10000
    double temp;

    if (sval <= 5003) {
        // linear below center-temp
        temp = MINTEMP0 + (sval / 5003.0) * (CENTERTEMP0 - MINTEMP0);
    } else {
        const double slope = (double) (CENTERTEMP0 - MINTEMP0) / (MAXTEMP0 - CENTERTEMP0);
        double x = (sval - 5003) / 5003; // x 0..1
        double y = x * slope + (1.0 - slope) * pow (x, 4.0);
        //double y = pow(x, 4.0);
        temp = CENTERTEMP0 + y * (MAXTEMP0 - CENTERTEMP0);
    }

    if (temp < MINTEMP0) {
        temp = MINTEMP0;
    }

    if (temp > MAXTEMP0) {
        temp = MAXTEMP0;
    }

    return temp;
}


static double wbTemp2Slider (double temp)
{

    double sval;

    if (temp <= CENTERTEMP0) {
        sval = ((temp - MINTEMP0) / (CENTERTEMP0 - MINTEMP0)) * 5003.0;
    } else {
        const double slope = (double) (CENTERTEMP0 - MINTEMP0) / (MAXTEMP0 - CENTERTEMP0);
        const double y = (temp - CENTERTEMP0) / (MAXTEMP0 - CENTERTEMP0);
        double x = pow (y, 0.25); // rough guess of x, will be a little lower
        double k = 0.1;
        bool add = true;

        // the y=f(x) function is a mess to invert, therefore we have this trial-refinement loop instead.
        // from tests, worst case is about 20 iterations, ie no problem
        for (;;) {
            double y1 = x * slope + (1.0 - slope) * pow (x, 4.0);

            if (5003 * fabs (y1 - y) < 0.1) {
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

        sval = 5003.0 + x * 5003.0;
    }

    if (sval < 0) {
        sval = 0;
    }

    if (sval > 10000) {
        sval = 10000;
    }

    return sval;
}


ColorAppearance::ColorAppearance () : FoldableToolPanel (this, TOOL_NAME, M ("TP_COLORAPP_LABEL"), false, true)
{
    auto& options = App::get().mut_options();

    CurveListener::setMulti (true);
    std::vector<GradientMilestone> milestones;
    milestones.push_back ( GradientMilestone (0., 0., 0., 0.) );
    milestones.push_back ( GradientMilestone (1., 1., 1., 1.) );

    auto m = ProcEventMapper::getInstance();
    EvCATAutotempout = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_TEMPOUT");
    EvCATillum = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_ILLUM");
    EvCATcomplex = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATCOMPLEX");
    EvCATmodel = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATMODEL");
    EvCATcat = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATCAT");
    EvCATcolorhred = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATHUERED");
    EvCATschromared = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATSCHROMARED");
    EvCATcolorhgreen = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATHUEGREEN");
    EvCATschromagreen = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATSCHROMAGREEN");
    EvCATcolorhblue = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATHUEBLUE");
    EvCATschromablue = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATSCHROMABLUE");
    EvCATCurvered = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATCURVERED");
    EvCATCurvegreen = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATCURVEGREEN");
    EvCATCurveblue = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_CATCURVEBLUE");

    //preset button cat02/16
    Gtk::Frame *genFrame;
    Gtk::Box* genVBox;
    genFrame = Gtk::manage (new Gtk::Frame (M ("TP_COLORAPP_GEN")) );
    genFrame->set_label_align (0.025, 0.5);
    genFrame->set_tooltip_markup (M ("TP_COLORAPP_GEN_TOOLTIP"));
    genVBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    genVBox->set_spacing (2);

    complexmethod = Gtk::manage (new MyComboBoxText ());
    complexmethod->append(M("TP_WAVELET_COMPNORMAL"));
    complexmethod->append(M("TP_WAVELET_COMPEXPERT"));
    complexmethodconn = complexmethod->signal_changed().connect(sigc::mem_fun(*this, &ColorAppearance::complexmethodChanged));
    complexmethod->set_tooltip_text(M("TP_WAVELET_COMPLEX_TOOLTIP"));
    Gtk::Box* const complexHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* const complexLabel = Gtk::manage(new Gtk::Label(M("TP_WAVELET_COMPLEXLAB") + ":"));
    complexHBox->pack_start(*complexLabel, Gtk::PACK_SHRINK, 4);
    complexHBox->pack_start(*complexmethod);
    genVBox->pack_start (*complexHBox, Gtk::PACK_SHRINK);

    modelmethod = Gtk::manage (new MyComboBoxText ());
    modelmethod->append(M("TP_COLORAPP_MOD02"));//Old CIECAM02
    modelmethod->append(M("TP_COLORAPP_MOD16"));//new CIECAM16
    modelmethodconn = modelmethod->signal_changed().connect(sigc::mem_fun(*this, &ColorAppearance::modelmethodChanged));
    modelmethod->set_tooltip_text(M("TP_COLORAPP_MODELCAT_TOOLTIP"));
    Gtk::Box* const modelHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* const modelLabel = Gtk::manage(new Gtk::Label(M("TP_COLORAPP_MODELCAT") + ":"));
    modelHBox->pack_start(*modelLabel, Gtk::PACK_SHRINK, 4);
    modelHBox->pack_start(*modelmethod);
    genVBox->pack_start (*modelHBox, Gtk::PACK_SHRINK);

    catmethod = Gtk::manage (new MyComboBoxText ());//The choice of basic method is either 
    catmethod->append(M("TP_COLORAPP_CATCLASSIC"));//Classical (recommended),
    catmethod->append(M("TP_COLORAPP_CATSYMGEN")); //Symmetrical (allows for very good color matching)
    catmethod->append(M("TP_COLORAPP_CATSYMSPE")); //Or Mixed. In all three cases, the CIECAM characterizing values ​​such as absolute luminance, color matching, surround, etc., are different.

    catmethodconn = catmethod->signal_changed().connect(sigc::mem_fun(*this, &ColorAppearance::catmethodChanged));
    catmethod->set_tooltip_text(M("TP_COLORAPP_CATMET_TOOLTIP"));
    Gtk::Box* const catHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* const catLabel = Gtk::manage(new Gtk::Label(M("TP_COLORAPP_CATMOD") + ":"));
    catHBox->pack_start(*catLabel, Gtk::PACK_SHRINK, 4);
    catHBox->pack_start(*catmethod);
    genVBox->pack_start (*catHBox, Gtk::PACK_SHRINK);


    genFrame->add (*genVBox);
    pack_start (*genFrame, Gtk::PACK_EXPAND_WIDGET, 4);

    // ----------------------- Process #1: Converting to CIECAM


    // Process 1 frame
    Gtk::Frame *p1Frame;
    // Vertical box container for the content of the Process 1 frame
    Gtk::Box* p1VBox;

    p1Frame = Gtk::manage (new Gtk::Frame (M ("TP_COLORAPP_LABEL_SCENE")) );
    p1Frame->set_label_align (0.025, 0.5);
    p1Frame->set_tooltip_markup (M ("TP_COLORAPP_SOURCEF_TOOLTIP"));
    p1VBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    p1VBox->set_spacing (2);

    degree  = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CIECAT_DEGREE"),    0.,  100.,  1.,   90.));//first chromatic adaptation between Working profile to CAM (scene)
    degree->set_tooltip_markup (M ("TP_COLORAPP_DEGREE_TOOLTIP"));
    degree->setAdjusterListener(this);

    degree->addAutoButton (M ("TP_COLORAPP_CAT02ADAPTATION_TOOLTIP"));
    p1VBox->pack_start (*degree);

    Gtk::Box* surrHBox1 = Gtk::manage (new Gtk::Box ());
    surrHBox1->set_spacing (2);
    surrHBox1->set_tooltip_markup (M ("TP_COLORAPP_SURSOURCE_TOOLTIP"));
    Gtk::Label* surrLabel1 = Gtk::manage (new Gtk::Label (M ("TP_COLORAPP_SURROUNDSRC") + ":"));//surround scene
    surrHBox1->pack_start (*surrLabel1, Gtk::PACK_SHRINK);
    surrsrc = Gtk::manage (new MyComboBoxText ());
    surrsrc->append (M ("TP_COLORAPP_SURROUND_AVER"));//standard creates a slight lighting effect
    surrsrc->append (M ("TP_COLORAPP_SURROUND_DIM"));
    surrsrc->append (M ("TP_COLORAPP_SURROUND_DARK"));
    surrsrc->append (M ("TP_COLORAPP_SURROUND_EXDARK"));
    surrsrc->set_active (0);
    surrHBox1->pack_start (*surrsrc);
    p1VBox->pack_start (*surrHBox1);

    wbmHBox = Gtk::manage (new Gtk::Box ());

    wbmHBox->set_spacing (2);
    wbmHBox->set_tooltip_markup (M ("TP_COLORAPP_MODEL_TOOLTIP"));
    Gtk::Label* wbmLab = Gtk::manage (new Gtk::Label (M ("TP_COLORAPP_MODEL") + ":"));//white point model - The default balance point is D50, but D65 can be selected. In this case, the viewing temperature will be set to 6504K.
    wbmHBox->pack_start (*wbmLab, Gtk::PACK_SHRINK);
    wbmodel = Gtk::manage (new MyComboBoxText ());
    wbmodel->append (M ("TP_COLORAPP_WBRT"));
    wbmodel->append (M ("TP_COLORAPP_WBCAM"));
    wbmodel->append (M ("TP_COLORAPP_FREE"));

    wbmodel->set_active (0);
    wbmHBox->pack_start (*wbmodel);
    p1VBox->pack_start (*wbmHBox);


    illumHBox = Gtk::manage (new Gtk::Box ());
    illumHBox->set_spacing (2);
    illumHBox->set_tooltip_markup (M ("TP_COLORAPP_ILLUM_TOOLTIP"));
    Gtk::Label* illumLab = Gtk::manage (new Gtk::Label (M ("TP_COLORAPP_ILLUM") + ":"));//Choice of illuminant in non-standard cases
    illumHBox->pack_start (*illumLab, Gtk::PACK_SHRINK);
    illum = Gtk::manage (new MyComboBoxText ());
    illum->append (M ("TP_COLORAPP_ILA"));
    illum->append (M ("TP_COLORAPP_IL41"));
    illum->append (M ("TP_COLORAPP_IL50"));
    illum->append (M ("TP_COLORAPP_IL55"));
    illum->append (M ("TP_COLORAPP_IL60"));
    illum->append (M ("TP_COLORAPP_IL65"));
    illum->append (M ("TP_COLORAPP_IL75"));
    illum->append (M ("TP_COLORAPP_ILFREE"));

    illum->set_active (0);
    illumHBox->pack_start (*illum);
    p1VBox->pack_start (*illumHBox);

    Gtk::Image* itempL =  Gtk::manage (new RTImage ("circle-blue-small"));
    Gtk::Image* itempR =  Gtk::manage (new RTImage ("circle-yellow-small"));
    Gtk::Image* igreenL = Gtk::manage (new RTImage ("circle-magenta-small"));
    Gtk::Image* igreenR = Gtk::manage (new RTImage ("circle-green-small"));


    tempsc = Gtk::manage (new Adjuster (M ("TP_WBALANCE_TEMPERATURE"), MINTEMP0, MAXTEMP0, 5, CENTERTEMP0, itempL, itempR, &wbSlider2Temp, &wbTemp2Slider));
    greensc = Gtk::manage (new Adjuster (M ("TP_WBALANCE_GREEN"), MINGREEN0, MAXGREEN0, 0.001, 1.0, igreenL, igreenR));
    tempsc->set_tooltip_markup (M ("TP_COLORAPP_TEMP_TOOLTIP"));

    tempsc->show();
    greensc->show();
    p1VBox->pack_start (*tempsc);
    p1VBox->pack_start (*greensc);


    adapscen = Gtk::manage (new Adjuster (M ("TP_COLORAPP_ABSOLUTELUMINANCE"), MINLA0, MAXLA0, 0.01, 2000.));//An essential setting for CIECAM - Absolute Luminance scene - cd/m2
    adapscen->setLogScale(500, 0);

    adapscen->set_tooltip_markup (M ("TP_COLORAPP_ADAPSCEN_TOOLTIP"));

    adapscen->addAutoButton();
    p1VBox->pack_start (*adapscen);

    ybscen = Gtk::manage (new Adjuster (M ("TP_COLORAPP_MEANLUMINANCE"), 1, 90, 1, 18));//An other essential setting for CIECAM - Mean Luminance scene en %
    ybscen->set_tooltip_markup (M ("TP_COLORAPP_YBSCEN_TOOLTIP"));

    ybscen->addAutoButton();
    p1VBox->pack_start (*ybscen);

    p1Frame->add (*p1VBox);
    pack_start (*p1Frame, Gtk::PACK_EXPAND_WIDGET, 4);


    // ------------------------ Process #2: Modifying image inside CIECAM


    // Process 1 frame

    expadjust = Gtk::manage (new MyExpander (false, M ("TP_COLORAPP_LABEL_CAM02")));
    setExpandAlignProperties (expadjust, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    expadjust->signal_button_release_event().connect_notify ( sigc::bind ( sigc::mem_fun (this, &ColorAppearance::foldAllButMe), expadjust) );

    Gtk::Box* p2VBox;

    p2VBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    p2VBox->set_spacing (2);

    alHBox = Gtk::manage (new Gtk::Box ());
    alHBox->set_spacing (2);
    alHBox->set_tooltip_markup (M ("TP_COLORAPP_ALGO_TOOLTIP"));
    Gtk::Label* alLabel = Gtk::manage (new Gtk::Label (M ("TP_COLORAPP_ALGO") + ":"));
    alHBox->pack_start (*alLabel, Gtk::PACK_SHRINK);
    algo = Gtk::manage (new MyComboBoxText ());//I chose 4 possible combinations of the 6 variables... we can imagine others, but that will complicate the code
    algo->append (M ("TP_COLORAPP_ALGO_JC"));
    algo->append (M ("TP_COLORAPP_ALGO_JS"));
    algo->append (M ("TP_COLORAPP_ALGO_QM"));
    algo->append (M ("TP_COLORAPP_ALGO_ALL"));
    algo->set_active (0);
    alHBox->pack_start (*algo);
    p2VBox->pack_start (*alHBox);

    p2VBox->pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_EXPAND_WIDGET, 4);

    jlight = Gtk::manage (new Adjuster (M ("TP_COLORAPP_LIGHT"), -100.0, 100.0, 0.1, 0.));//Lightness
    jlight->setAdjusterListener  (this);

    jlight->set_tooltip_markup (M ("TP_COLORAPP_LIGHT_TOOLTIP"));
    p2VBox->pack_start (*jlight);

    qbright = Gtk::manage (new Adjuster (M ("TP_COLORAPP_BRIGHT"), -100.0, 100.0, 0.1, 0.));//brightness
    qbright->set_tooltip_markup (M ("TP_COLORAPP_BRIGHT_TOOLTIP"));
    p2VBox->pack_start (*qbright);

    chroma = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA"), -100.0, 100.0, 0.1, 0.));//chroma

    chroma->set_tooltip_markup (M ("TP_COLORAPP_CHROMA_TOOLTIP"));
    p2VBox->pack_start (*chroma);


    schroma = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA_S"), -100.0, 100.0, 0.1, 0.));//saturation

    schroma->set_tooltip_markup (M ("TP_COLORAPP_CHROMA_S_TOOLTIP"));
    p2VBox->pack_start (*schroma);

    mchroma = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA_M"), -100.0, 100.0, 0.1, 0.));//colorfullness

    mchroma->set_tooltip_markup (M ("TP_COLORAPP_CHROMA_M_TOOLTIP"));
    p2VBox->pack_start (*mchroma);

    rstprotection = Gtk::manage ( new Adjuster (M ("TP_COLORAPP_RSTPRO"), 0., 100., 0.1, 0.) );//protection red

    rstprotection->set_tooltip_markup (M ("TP_COLORAPP_RSTPRO_TOOLTIP"));
    p2VBox->pack_start (*rstprotection);

    contrast = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CONTRAST"), -100.0, 100.0, 0.1, 0.));//contrast J (lightness)

    contrast->set_tooltip_markup (M ("TP_COLORAPP_CONTRAST_TOOLTIP"));
    p2VBox->pack_start (*contrast);

    qcontrast = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CONTRAST_Q"), -100.0, 100.0, 0.1, 0.));//contrast Q (brightness)

    qcontrast->set_tooltip_markup (M ("TP_COLORAPP_CONTRAST_Q_TOOLTIP"));
    p2VBox->pack_start (*qcontrast);

    colorh = Gtk::manage (new Adjuster (M ("TP_COLORAPP_HUE"), -100.0, 100.0, 0.1, 0.));//rotation hue

    colorh->set_tooltip_markup (M ("TP_COLORAPP_HUE_TOOLTIP"));
    p2VBox->pack_start (*colorh);

    tonecie = Gtk::manage (new Gtk::CheckButton (M ("TP_COLORAPP_TONECIE")));//In case of tonemapping (usefull with CIECAM02)
    tonecie->set_tooltip_markup (M ("TP_COLORAPP_TONECIE_TOOLTIP"));
    tonecieconn = tonecie->signal_toggled().connect ( sigc::mem_fun (*this, &ColorAppearance::tonecie_toggled) );
    p2VBox->pack_start (*tonecie);
    p2VBox->pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_EXPAND_WIDGET, 4);

    //The code begins to modify hue, saturation, and brightness according to the three colors: Red, Green, and Blue.
    //curves
    curveEditorGred = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_BRIGHT_CUR_RED"));
    curveEditorGred->setCurveListener (this);
    shapered = static_cast<DiagonalCurveEditor*>(curveEditorGred->addCurve(CT_Diagonal, "", nullptr));
    curveEditorGred->curveListComplete();

    curveEditorGgreen = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_BRIGHT_CUR_GREEN"));
    curveEditorGgreen->setCurveListener (this);
    shapegreen = static_cast<DiagonalCurveEditor*>(curveEditorGgreen->addCurve(CT_Diagonal, "", nullptr));
    curveEditorGgreen->curveListComplete();

    curveEditorGblue = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_BRIGHT_CUR_BLUE"));
    curveEditorGblue->setCurveListener (this);
    shapeblue = static_cast<DiagonalCurveEditor*>(curveEditorGblue->addCurve(CT_Diagonal, "", nullptr));
    curveEditorGblue->curveListComplete();

    
    // Vertical box container for the content of the Process 3 frame
    Gtk::Box* pRGBVBox;

    pRGBFrame = Gtk::manage (new Gtk::Frame (M ("TP_COLORAPP_LABEL_RGB")) ); //Red Green Blue Hue and saturation
    pRGBFrame->set_label_align (0.025, 0.5);

    pRGBVBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    pRGBVBox->set_spacing (2);
    //hue red blue green
    colorhred = Gtk::manage (new Adjuster (M ("TP_COLORAPP_HUE_RED"), -25.0, 25.0, 0.1, 0.));//hue red
    colorhred->set_tooltip_markup (M ("TP_COLORAPP_RGB_TOOLTIP"));//I changed the location of this tooltip, placing it only on the first slider, rather than on the entire frame pRGBFrame. This improves usability.

    pRGBVBox->pack_start (*colorhred);
    schromared = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA_S_RED"), -40.0, 20.0, 0.1, 0.));//saturation red
    pRGBVBox->pack_start (*schromared);
    pRGBVBox->pack_start ( *curveEditorGred, Gtk::PACK_SHRINK, 2);//curve brightness red
    pRGBVBox->pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_EXPAND_WIDGET, 4);

    colorhgreen = Gtk::manage (new Adjuster (M ("TP_COLORAPP_HUE_GREEN"), -25.0, 25.0, 0.1, 0.));//hue green
    pRGBVBox->pack_start (*colorhgreen);
    schromagreen = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA_S_GREEN"), -40.0, 20.0, 0.1, 0.));//saturation green
    pRGBVBox->pack_start (*schromagreen);
    pRGBVBox->pack_start ( *curveEditorGgreen, Gtk::PACK_SHRINK, 2);//curve brightness green
    pRGBVBox->pack_start (*Gtk::manage (new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_EXPAND_WIDGET, 4);

    colorhblue = Gtk::manage (new Adjuster (M ("TP_COLORAPP_HUE_BLUE"), -25.0, 25.0, 0.1, 0.));//hue blue
    pRGBVBox->pack_start (*colorhblue);
    schromablue = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CHROMA_S_BLUE"), -40.0, 20.0, 0.1, 0.));//saturation blue
    pRGBVBox->pack_start (*schromablue);
    pRGBVBox->pack_start ( *curveEditorGblue, Gtk::PACK_SHRINK, 2);//curve brightness blue

    pRGBFrame->add (*pRGBVBox);
    p2VBox->pack_start (*pRGBFrame, Gtk::PACK_EXPAND_WIDGET, 4);

    toneCurveMode = Gtk::manage (new MyComboBoxText ());
    toneCurveMode->append (M ("TP_COLORAPP_TCMODE_LIGHTNESS"));
    toneCurveMode->append (M ("TP_COLORAPP_TCMODE_BRIGHTNESS"));
    toneCurveMode->set_active (0);
    toneCurveMode->set_tooltip_text (M ("TP_COLORAPP_TCMODE_LABEL1"));

    curveEditorG = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_CURVEEDITOR1"));
    curveEditorG->setCurveListener (this);
    curveEditorG->setTooltip (M ("TP_COLORAPP_CURVEEDITOR1_TOOLTIP"));
    shape = static_cast<DiagonalCurveEditor*> (curveEditorG->addCurve (CT_Diagonal, "", toneCurveMode));

    tcmodeconn = toneCurveMode->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::curveMode1Changed), true );

    toneCurveMode2 = Gtk::manage (new MyComboBoxText ());//curve type selection Lighness or brightness
    toneCurveMode2->append (M ("TP_COLORAPP_TCMODE_LIGHTNESS"));
    toneCurveMode2->append (M ("TP_COLORAPP_TCMODE_BRIGHTNESS"));
    toneCurveMode2->set_active (0);
    toneCurveMode2->set_tooltip_text (M ("TP_COLORAPP_TCMODE_LABEL2"));

    curveEditorG2 = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_CURVEEDITOR2"));
    curveEditorG2->setCurveListener (this);

    shape2 = static_cast<DiagonalCurveEditor*> (curveEditorG2->addCurve (CT_Diagonal, "", toneCurveMode2));

    tcmode2conn = toneCurveMode2->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::curveMode2Changed), true );

    toneCurveMode3 = Gtk::manage (new MyComboBoxText ());//curve type selection chroma, saturation, colorfullness
    toneCurveMode3->append (M ("TP_COLORAPP_TCMODE_CHROMA"));
    toneCurveMode3->append (M ("TP_COLORAPP_TCMODE_SATUR"));
    toneCurveMode3->append (M ("TP_COLORAPP_TCMODE_COLORF"));
    toneCurveMode3->set_active (0);
    toneCurveMode3->set_tooltip_text (M ("TP_COLORAPP_TCMODE_LABEL3"));

    curveEditorG3 = new CurveEditorGroup (options.lastToneCurvesDir, M ("TP_COLORAPP_CURVEEDITOR3"));
    curveEditorG3->setCurveListener (this);

    shape3 = static_cast<DiagonalCurveEditor*> (curveEditorG3->addCurve (CT_Diagonal, "", toneCurveMode3));
    shape3->setRangeLabels (
        M ("TP_LABCURVE_CURVEEDITOR_CC_RANGE1"), M ("TP_LABCURVE_CURVEEDITOR_CC_RANGE2"),
        M ("TP_LABCURVE_CURVEEDITOR_CC_RANGE3"), M ("TP_LABCURVE_CURVEEDITOR_CC_RANGE4")
    );
    shape3->setBottomBarColorProvider (this, 1);
    shape3->setLeftBarColorProvider (this, 1);
    shape3->setRangeDefaultMilestones (0.05, 0.2, 0.58);

    // The milestones are still the same than those define above
    //milestones.push_back( GradientMilestone(0., 0., 0., 0.) );
    //milestones.push_back( GradientMilestone(1., 1., 1., 1.) );
    shape->setBottomBarBgGradient (milestones);
    shape->setLeftBarBgGradient (milestones);
    shapered->setBottomBarBgGradient (milestones);
    shapered->setLeftBarBgGradient (milestones);
    shapegreen->setBottomBarBgGradient (milestones);
    shapegreen->setLeftBarBgGradient (milestones);
    shapeblue->setBottomBarBgGradient (milestones);
    shapeblue->setLeftBarBgGradient (milestones);
    shape2->setBottomBarBgGradient (milestones);
    shape2->setLeftBarBgGradient (milestones);

    std::vector<GradientMilestone> shape3Milestones;
    float R, G, B;

    for (int i = 0; i < 7; i++) {
        float x = float (i) * (1.0f / 6.f);
        Color::hsv2rgb01 (x, 0.5f, 0.5f, R, G, B);
        shape3Milestones.push_back ( GradientMilestone (double (x), double (R), double (G), double (B)) );
    }

    shape3->setBottomBarBgGradient (shape3Milestones);
    shape3->setLeftBarBgGradient (shape3Milestones);

    shape3->setRangeDefaultMilestones (0.05, 0.2, 0.58);

    curveEditorG->curveListComplete();

    curveEditorG2->curveListComplete();
    curveEditorG2->setTooltip (M ("TP_COLORAPP_CURVEEDITOR2_TOOLTIP"));

    curveEditorG3->curveListComplete();
    curveEditorG3->setTooltip (M ("TP_COLORAPP_CURVEEDITOR3_TOOLTIP"));//color curves
    tcmode3conn = toneCurveMode3->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::curveMode3Changed), true );

    p2VBox->pack_start ( *curveEditorG, Gtk::PACK_SHRINK, 2);
    p2VBox->pack_start ( *curveEditorG2, Gtk::PACK_SHRINK, 2);
    p2VBox->pack_start ( *curveEditorG3, Gtk::PACK_SHRINK, 2);

    // ------------------------ Choice CIECAM data


    datacie = Gtk::manage (new Gtk::CheckButton (M ("TP_COLORAPP_DATACIE")));
    datacie->set_tooltip_markup (M ("TP_COLORAPP_DATACIE_TOOLTIP"));
    datacieconn = datacie->signal_toggled().connect ( sigc::mem_fun (*this, &ColorAppearance::datacie_toggled) );
    p2VBox->pack_start (*datacie);

    //-------------------------



    expadjust->add (*p2VBox, false);
    expadjust->setLevel (2);
    pack_start (*expadjust);




    // ------------------------ Process #3: Converting back to Lab/RGB


    // Process 3 frame
    Gtk::Frame *p3Frame;
    // Vertical box container for the content of the Process 3 frame
    Gtk::Box* p3VBox;

    p3Frame = Gtk::manage (new Gtk::Frame (M ("TP_COLORAPP_LABEL_VIEWING")) );//Editing viewing conditions
    p3Frame->set_label_align (0.025, 0.5);
    p3Frame->set_tooltip_markup (M ("TP_COLORAPP_VIEWINGF_TOOLTIP"));

    p3VBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    p3VBox->set_spacing (2);

    Gtk::Image* itempL1 =  Gtk::manage (new RTImage ("circle-blue-small"));
    Gtk::Image* itempR1 =  Gtk::manage (new RTImage ("circle-yellow-small"));
    Gtk::Image* igreenL1 = Gtk::manage (new RTImage ("circle-magenta-small"));
    Gtk::Image* igreenR1 = Gtk::manage (new RTImage ("circle-green-small"));
    adaplum = Gtk::manage (new Adjuster (M ("TP_COLORAPP_ABSOLUTELUMINANCE"), MINLA0, MAXLA0, 0.01, 16.));//Absolute luminance viewing cd/m2
    adaplum->setLogScale(500, 0);

    adaplum->set_tooltip_markup (M ("TP_COLORAPP_VIEWING_ABSOLUTELUMINANCE_TOOLTIP"));
    p3VBox->pack_start (*adaplum);


    degreeout  = Gtk::manage (new Adjuster (M ("TP_COLORAPP_CIECAT_DEGREEOUT"),    0.,  100.,  1.,   90.));//Chromatic adaptation viewing

    degreeout->set_tooltip_markup (M ("TP_COLORAPP_DEGREOUT_TOOLTIP"));

    degreeout->addAutoButton (M ("TP_COLORAPP_CAT02ADAPTATION_TOOLTIP"));
    p3VBox->pack_start (*degreeout);
    tempout = Gtk::manage (new Adjuster (M ("TP_WBALANCE_TEMPERATURE"), MINTEMP0, MAXTEMP0, 5, CENTERTEMP0, itempR1, itempL1, &wbSlider2Temp, &wbTemp2Slider));//temperature viewing
    greenout = Gtk::manage (new Adjuster (M ("TP_WBALANCE_GREEN"), MINGREEN0, MAXGREEN0, 0.001, 1.0, igreenR1, igreenL1));//green viewing
    ybout = Gtk::manage (new Adjuster (M ("TP_COLORAPP_MEANLUMINANCE"), 5, 90, 1, 18));//Mean luminance viewing %
    ybout->set_tooltip_markup (M ("TP_COLORAPP_YBOUT_TOOLTIP"));

    tempout->set_tooltip_markup (M ("TP_COLORAPP_TEMP2_TOOLTIP"));
    tempout->addAutoButton (M ("TP_COLORAPP_TEMPOUT_TOOLTIP"));
    // I renable tempout with addautobutton to work properly (and all code disabled). There are certainly some redundancies, but it doesn't matter
    tempout->show();
    greenout->show();
    ybout->show();
    Gtk::Frame *tempgreenFrame;
    tempgreenFrame = Gtk::manage(new Gtk::Frame());
    tempgreenFrame->set_label_align (0.025, 0.5);
    Gtk::Box* tempgreenVBox;
    tempgreenVBox = Gtk::manage ( new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    tempgreenVBox->set_spacing (2);
    tempgreenVBox->pack_start (*tempout);
    tempgreenVBox->pack_start (*greenout);
    tempgreenFrame->add(*tempgreenVBox);
    p3VBox->pack_start(*tempgreenFrame);
    p3VBox->pack_start (*ybout);

    Gtk::Box* surrHBox = Gtk::manage (new Gtk::Box ());//Surround viewing
    surrHBox->set_spacing (2);
    surrHBox->set_tooltip_markup (M ("TP_COLORAPP_SURROUND_TOOLTIP"));
    Gtk::Label* surrLabel = Gtk::manage (new Gtk::Label (M ("TP_COLORAPP_SURROUND") + ":"));
    surrHBox->pack_start (*surrLabel, Gtk::PACK_SHRINK);
    surround = Gtk::manage (new MyComboBoxText ());
    surround->append (M ("TP_COLORAPP_SURROUND_AVER"));//standard
    surround->append (M ("TP_COLORAPP_SURROUND_DIM"));
    surround->append (M ("TP_COLORAPP_SURROUND_DARK"));
    surround->append (M ("TP_COLORAPP_SURROUND_EXDARK"));
    surround->set_active (1);
    surrHBox->pack_start (*surround);
    p3VBox->pack_start (*surrHBox);

    p3Frame->add (*p3VBox);
    pack_start (*p3Frame, Gtk::PACK_EXPAND_WIDGET, 4);


    // ------------------------ Lab Gamut control


    gamut = Gtk::manage (new Gtk::CheckButton (M ("TP_COLORAPP_GAMUT")));
    gamutconn = gamut->signal_toggled().connect ( sigc::mem_fun (*this, &ColorAppearance::gamut_toggled) );
    pack_start (*gamut, Gtk::PACK_SHRINK);

    // ------------------------ Bad pixel control

    /*
        badpix = Gtk::manage (new Gtk::CheckButton (M("TP_COLORAPP_BADPIX")));
        badpix->set_tooltip_markup (M("TP_COLORAPP_BADPIX_TOOLTIP"));
        badpixconn = badpix->signal_toggled().connect( sigc::mem_fun(*this, &ColorAppearance::badpix_toggled) );
        pack_start (*badpix, Gtk::PACK_SHRINK);
    */
    badpixsl = Gtk::manage (new Adjuster (M ("TP_COLORAPP_BADPIXSL"), 0,  2, 1,  0));

    badpixsl->set_tooltip_markup (M ("TP_COLORAPP_BADPIXSL_TOOLTIP"));
    pack_start (*badpixsl, Gtk::PACK_SHRINK);


    //reset button
    neutral = Gtk::manage (new Gtk::Button (M ("TP_COLORAPP_NEUTRAL")));
    setExpandAlignProperties (neutral, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    RTImage* const resetImg = Gtk::manage (new RTImage ("undo-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties (resetImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    neutral->set_image (*resetImg);
    neutral->set_tooltip_text (M ("TP_COLORAPP_NEUTRAL_TOOLTIP"));
    neutralconn = neutral->signal_pressed().connect ( sigc::mem_fun (*this, &ColorAppearance::neutral_pressed) );
    neutral->show();

    //-------------

    pack_start (*neutral);

    // ------------------------ Listening events


    wbmodelconn = wbmodel->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::wbmodelChanged) );
    illumconn = illum->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::illumChanged) );
    algoconn = algo->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::algoChanged) );
    surroundconn = surround->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::surroundChanged) );
    surrsrcconn = surrsrc->signal_changed().connect ( sigc::mem_fun (*this, &ColorAppearance::surrsrcChanged) );

    degree->setAdjusterListener  (this);
    degreeout->setAdjusterListener  (this);
    adapscen->setAdjusterListener (this);
    ybscen->setAdjusterListener (this);
    adaplum->setAdjusterListener (this);
    badpixsl->setAdjusterListener (this);
    jlight->setAdjusterListener  (this);
    qbright->setAdjusterListener  (this);
    colorh->setAdjusterListener  (this);
    colorhred->setAdjusterListener  (this);
    colorhgreen->setAdjusterListener  (this);
    colorhblue->setAdjusterListener  (this);
    chroma->setAdjusterListener  (this);
    schroma->setAdjusterListener  (this);
    schromared->setAdjusterListener  (this);
    schromagreen->setAdjusterListener  (this);
    schromablue->setAdjusterListener  (this);
    mchroma->setAdjusterListener  (this);
    contrast->setAdjusterListener  (this);
    qcontrast->setAdjusterListener  (this);
    rstprotection->setAdjusterListener  (this);
    tempout->setAdjusterListener  (this);
    greenout->setAdjusterListener  (this);
    ybout->setAdjusterListener  (this);
    tempsc->setAdjusterListener  (this);
    greensc->setAdjusterListener  (this);


    show_all();
}

ColorAppearance::~ColorAppearance ()
{
    idle_register.destroy();

    delete curveEditorG;
    delete curveEditorGred;
    delete curveEditorGgreen;
    delete curveEditorGblue;
    delete curveEditorG2;
    delete curveEditorG3;
}

void ColorAppearance::foldAllButMe (GdkEventButton* event, MyExpander *expander)
{
    if (event->button == 3) {
        expadjust->set_expanded (expadjust == expander);
    }
}

void ColorAppearance::writeOptions (std::vector<int> &tpOpen)
{
    tpOpen.push_back (expadjust->get_expanded ());
}

void ColorAppearance::updateToolState (std::vector<int> &tpOpen)
{
    if (tpOpen.size() >= 1) {
        expadjust->set_expanded (tpOpen.at (0));
    }
}

void ColorAppearance::neutral_pressed ()
{
    jlight->resetValue (false);
    qbright->resetValue (false);
    chroma->resetValue (false);
    schroma->resetValue (false);
    schromared->resetValue (false);
    schromagreen->resetValue (false);
    schromablue->resetValue (false);
    mchroma->resetValue (false);
    rstprotection->resetValue (false);
    contrast->resetValue (false);
    qcontrast->resetValue (false);
    colorh->resetValue (false);
    colorhred->resetValue (false);
    colorhgreen->resetValue (false);
    colorhblue->resetValue (false);
    tempout->resetValue (false);
    tempout->setAutoValue (true);
    greenout->resetValue (false);
    ybout->resetValue (false);
    tempsc->resetValue (false);
    greensc->resetValue (false);
    badpixsl->resetValue (false);
    wbmodel->set_active (0);
    illum->set_active (2);
    toneCurveMode->set_active (0);
    toneCurveMode2->set_active (1);
    toneCurveMode3->set_active (0);
    shape->reset();
    shapered->reset();
    shapegreen->reset();
    shapeblue->reset();
    shape2->reset();
    shape3->reset();
    gamutconn.block (true);
    gamut->set_active (true);
    gamutconn.block (false);
    degree->setAutoValue (true);
    degree->resetValue (false);
    adapscen->resetValue (false);
    adapscen->setAutoValue (true);
    degreeout->resetValue (false);
    degreeout->setAutoValue (true);
    ybscen->resetValue (false);
    ybscen->setAutoValue (true);
}

bool ColorAppearance::bgTTipQuery (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
    return true;
}

bool ColorAppearance::srTTipQuery (int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
{
    return true;
}

void ColorAppearance::read (const ProcParams* pp, const ParamsEdited* pedited)
{

    disableListener ();
    complexmethodconn.block(true);
    modelmethodconn.block(true);
    catmethodconn.block(true);
    tcmodeconn.block (true);
    tcmode2conn.block (true);
    tcmode3conn.block (true);
    shape->setCurve (pp->colorappearance.curve);
    shapered->setCurve (pp->colorappearance.curvered);
    shapegreen->setCurve (pp->colorappearance.curvegreen);
    shapeblue->setCurve (pp->colorappearance.curveblue);
    shape2->setCurve (pp->colorappearance.curve2);
    shape3->setCurve (pp->colorappearance.curve3);
    toneCurveMode->set_active (toUnderlying(pp->colorappearance.curveMode));
    toneCurveMode2->set_active (toUnderlying(pp->colorappearance.curveMode2));
    toneCurveMode3->set_active (toUnderlying(pp->colorappearance.curveMode3));
    curveMode3Changed(); // This will set the correct sensitive state of depending Adjusters

    nexttemp = pp->wb.temperature;
    nextgreen = pp->wb.green;

    if (pedited) {
        degree->setEditedState        (pedited->colorappearance.degree ? Edited : UnEdited);
        degreeout->setEditedState        (pedited->colorappearance.degreeout ? Edited : UnEdited);
        tempout->setEditedState        (pedited->colorappearance.tempout ? Edited : UnEdited);
        adapscen->setEditedState      (pedited->colorappearance.adapscen ? Edited : UnEdited);
        ybscen->setEditedState      (pedited->colorappearance.ybscen ? Edited : UnEdited);
        adaplum->setEditedState       (pedited->colorappearance.adaplum ? Edited : UnEdited);
        badpixsl->setEditedState      (pedited->colorappearance.badpixsl ? Edited : UnEdited);
        jlight->setEditedState        (pedited->colorappearance.jlight ? Edited : UnEdited);
        qbright->setEditedState       (pedited->colorappearance.qbright ? Edited : UnEdited);
        chroma->setEditedState        (pedited->colorappearance.chroma ? Edited : UnEdited);
        schroma->setEditedState       (pedited->colorappearance.schroma ? Edited : UnEdited);
        schromared->setEditedState    (pedited->colorappearance.schromared ? Edited : UnEdited);
        schromagreen->setEditedState    (pedited->colorappearance.schromagreen ? Edited : UnEdited);
        schromablue->setEditedState    (pedited->colorappearance.schromablue ? Edited : UnEdited);
        mchroma->setEditedState       (pedited->colorappearance.mchroma ? Edited : UnEdited);
        rstprotection->setEditedState (pedited->colorappearance.rstprotection ? Edited : UnEdited);
        tempout->setEditedState (pedited->colorappearance.tempout ? Edited : UnEdited);
        greenout->setEditedState (pedited->colorappearance.greenout ? Edited : UnEdited);
        ybout->setEditedState (pedited->colorappearance.ybout ? Edited : UnEdited);
        tempsc->setEditedState (pedited->colorappearance.tempsc ? Edited : UnEdited);
        greensc->setEditedState (pedited->colorappearance.greensc ? Edited : UnEdited);
        contrast->setEditedState      (pedited->colorappearance.contrast ? Edited : UnEdited);
        qcontrast->setEditedState     (pedited->colorappearance.qcontrast ? Edited : UnEdited);
        colorh->setEditedState        (pedited->colorappearance.colorh ? Edited : UnEdited);
        colorhred->setEditedState     (pedited->colorappearance.colorhred ? Edited : UnEdited);
        colorhgreen->setEditedState     (pedited->colorappearance.colorhgreen ? Edited : UnEdited);
        colorhblue->setEditedState     (pedited->colorappearance.colorhblue ? Edited : UnEdited);
        gamut->set_inconsistent       (!pedited->colorappearance.gamut);
        datacie->set_inconsistent     (!pedited->colorappearance.datacie);
        tonecie->set_inconsistent     (!pedited->colorappearance.tonecie);

        degree->setAutoInconsistent   (multiImage && !pedited->colorappearance.autodegree);
        degreeout->setAutoInconsistent   (multiImage && !pedited->colorappearance.autodegreeout);
        adapscen->setAutoInconsistent (multiImage && !pedited->colorappearance.autoadapscen);
        ybscen->setAutoInconsistent (multiImage && !pedited->colorappearance.autoybscen);
        set_inconsistent              (multiImage && !pedited->colorappearance.enabled);
        tempout->setAutoInconsistent   (multiImage && !pedited->colorappearance.autotempout);

        shape->setUnChanged (!pedited->colorappearance.curve);
        shapered->setUnChanged (!pedited->colorappearance.curvered);
        shapegreen->setUnChanged (!pedited->colorappearance.curvegreen);
        shapeblue->setUnChanged (!pedited->colorappearance.curveblue);
        shape2->setUnChanged (!pedited->colorappearance.curve2);
        shape3->setUnChanged (!pedited->colorappearance.curve3);

        if (!pedited->colorappearance.curveMode) {
            toneCurveMode->set_active (2);
        }
        if (!pedited->colorappearance.complexmethod) {
            complexmethod->set_active_text(M("GENERAL_UNCHANGED"));
        }
        if (!pedited->colorappearance.modelmethod) {
            modelmethod->set_active_text(M("GENERAL_UNCHANGED"));
        }
        if (!pedited->colorappearance.catmethod) {
            catmethod->set_active_text(M("GENERAL_UNCHANGED"));
        }

        if (!pedited->colorappearance.curveMode2) {
            toneCurveMode2->set_active (2);
        }

        if (!pedited->colorappearance.curveMode3) {
            toneCurveMode3->set_active (3);
        }


    }

    setEnabled (pp->colorappearance.enabled);

    if (pp->colorappearance.complexmethod == "normal") {
        complexmethod->set_active(0);
    } else if (pp->colorappearance.complexmethod == "expert") {
        complexmethod->set_active(1);
    }

    modelmethod->set_active(0);

    if (pp->colorappearance.modelmethod == "02") {
        modelmethod->set_active(0);
    } else if (pp->colorappearance.modelmethod == "16") {
        modelmethod->set_active(1);
    }

    catmethod->set_active(0);

    if (pp->colorappearance.catmethod == "clas") {
        catmethod->set_active(0);
    } else if (pp->colorappearance.catmethod == "symg") {
        catmethod->set_active(1);
    } else if (pp->colorappearance.catmethod == "symc") {
        catmethod->set_active(2);
    }

    surrsrcconn.block (true);

    if (pedited && !pedited->colorappearance.surrsrc) {
        surrsrc->set_active (4);
    } else if (pp->colorappearance.surrsrc == "Average") {
        surrsrc->set_active (0);
    } else if (pp->colorappearance.surrsrc == "Dim") {
        surrsrc->set_active (1);
    } else if (pp->colorappearance.surrsrc == "Dark") {
        surrsrc->set_active (2);
    } else if (pp->colorappearance.surrsrc == "ExtremelyDark") {
        surrsrc->set_active (3);
    }

    surrsrcconn.block (false);
    // Have to be manually called to handle initial state update
    surrsrcChanged();


    surroundconn.block (true);

    if (pedited && !pedited->colorappearance.surround) {
        surround->set_active (4);
    } else if (pp->colorappearance.surround == "Average") {
        surround->set_active (0);
    } else if (pp->colorappearance.surround == "Dim") {
        surround->set_active (1);
    } else if (pp->colorappearance.surround == "Dark") {
        surround->set_active (2);
    } else if (pp->colorappearance.surround == "ExtremelyDark") {
        surround->set_active (3);
    }

    surroundconn.block (false);
    // Have to be manually called to handle initial state update
    surroundChanged();



    wbmodelconn.block (true);

    if (pedited && !pedited->colorappearance.wbmodel) {
        wbmodel->set_active (3);
    } else if (pp->colorappearance.wbmodel == "RawT") {
        wbmodel->set_active (0);
    } else if (pp->colorappearance.wbmodel == "RawTCAT02") {
        wbmodel->set_active (1);
    } else if (pp->colorappearance.wbmodel == "free") {
        wbmodel->set_active (2);
    }

    wbmodelconn.block (false);
    // Have to be manually called to handle initial state update
    wbmodelChanged();

    illumconn.block (true);

    if (pedited && !pedited->colorappearance.illum) {
        illum->set_active (8);
    } else if (pp->colorappearance.illum == "iA") {
        illum->set_active (0);
    } else if (pp->colorappearance.illum == "i41") {
        illum->set_active (1);
    } else if (pp->colorappearance.illum == "i50") {
        illum->set_active (2);
    } else if (pp->colorappearance.illum == "i55") {
        illum->set_active (3);
    } else if (pp->colorappearance.illum == "i60") {
        illum->set_active (4);
    } else if (pp->colorappearance.illum == "i65") {
        illum->set_active (5);
    } else if (pp->colorappearance.illum == "i75") {
        illum->set_active (6);
    } else if (pp->colorappearance.illum == "ifree") {
        illum->set_active (7);
    }

    illumconn.block (false);
    // Have to be manually called to handle initial state update
    illumChanged();

    algoconn.block (true);

    if (pedited && !pedited->colorappearance.algo) {
        algo->set_active (4);
    } else if (pp->colorappearance.algo == "JC") {
        algo->set_active (0);
    } else if (pp->colorappearance.algo == "JS") {
        algo->set_active (1);
    } else if (pp->colorappearance.algo == "QM") {
        algo->set_active (2);
    } else if (pp->colorappearance.algo == "ALL") {
        algo->set_active (3);
    }

    algoconn.block (false);
    // Have to be manually called to handle initial state update
    algoChanged();

    gamutconn.block (true);
    gamut->set_active (pp->colorappearance.gamut);
    gamutconn.block (false);
//  badpixconn.block (true);
//  badpix->set_active (pp->colorappearance.badpix);
//  badpixconn.block (false);
    datacieconn.block (true);
    datacie->set_active (pp->colorappearance.datacie);
    datacieconn.block (false);
    tonecieconn.block (true);
    tonecie->set_active (pp->colorappearance.tonecie);
    tonecieconn.block (false);

    lastgamut = pp->colorappearance.gamut;
    lastdatacie = pp->colorappearance.datacie;
    lasttonecie = pp->colorappearance.tonecie;

    lastAutoDegree = pp->colorappearance.autodegree;
    lastAutoAdapscen = pp->colorappearance.autoadapscen;
    lastAutoDegreeout = pp->colorappearance.autodegreeout;
    lastAutoybscen = pp->colorappearance.autoybscen;
    lastAutotempout = pp->colorappearance.autotempout;

    degree->setValue (pp->colorappearance.degree);
    degree->setAutoValue (pp->colorappearance.autodegree);
    adapscen->setValue (pp->colorappearance.adapscen);
    adapscen->setAutoValue (pp->colorappearance.autoadapscen);
    degreeout->setValue (pp->colorappearance.degreeout);
    degreeout->setAutoValue (pp->colorappearance.autodegreeout);
    ybscen->setValue (pp->colorappearance.ybscen);
    ybscen->setAutoValue (pp->colorappearance.autoybscen);

    adaplum->setValue (pp->colorappearance.adaplum);
    badpixsl->setValue (pp->colorappearance.badpixsl);
    jlight->setValue (pp->colorappearance.jlight);
    qbright->setValue (pp->colorappearance.qbright);
    chroma->setValue (pp->colorappearance.chroma);
    schroma->setValue (pp->colorappearance.schroma);
    schromared->setValue (pp->colorappearance.schromared);
    schromagreen->setValue (pp->colorappearance.schromagreen);
    schromablue->setValue (pp->colorappearance.schromablue);
    mchroma->setValue (pp->colorappearance.mchroma);
    rstprotection->setValue (pp->colorappearance.rstprotection);
    contrast->setValue (pp->colorappearance.contrast);
    qcontrast->setValue (pp->colorappearance.qcontrast);
    colorh->setValue (pp->colorappearance.colorh);
    colorhred->setValue (pp->colorappearance.colorhred);
    colorhgreen->setValue (pp->colorappearance.colorhgreen);
    colorhblue->setValue (pp->colorappearance.colorhblue);
    tempout->setValue (pp->colorappearance.tempout);
    tempout->setAutoValue (pp->colorappearance.autotempout);
    greenout->setValue (pp->colorappearance.greenout);
    ybout->setValue (pp->colorappearance.ybout);
    tempsc->setValue (pp->colorappearance.tempsc);
    greensc->setValue (pp->colorappearance.greensc);

    if (complexmethod->get_active_row_number() == 0) {
        updateGUIToMode(0);
        convertParamToNormal();

    } else {
        updateGUIToMode(1);
    }

    tcmode3conn.block (false);
    tcmode2conn.block (false);
    tcmodeconn.block (false);
    catmethodconn.block(false);
    modelmethodconn.block(false);
    complexmethodconn.block(false);
    enableListener ();
}
void ColorAppearance::autoOpenCurve  ()
{
    shape->openIfNonlinear();
    shapered->openIfNonlinear();
    shapegreen->openIfNonlinear();
    shape2->openIfNonlinear();
    shape3->openIfNonlinear();

}


void ColorAppearance::write (ProcParams* pp, ParamsEdited* pedited)
{

    pp->colorappearance.degree        = degree->getValue ();
    pp->colorappearance.autodegree    = degree->getAutoValue ();
    pp->colorappearance.degreeout     = degreeout->getValue ();
    pp->colorappearance.autodegreeout = degreeout->getAutoValue ();
    pp->colorappearance.enabled       = getEnabled();
    pp->colorappearance.adapscen      = adapscen->getValue ();
    pp->colorappearance.autoadapscen  = adapscen->getAutoValue ();
    pp->colorappearance.ybscen        = ybscen->getValue ();
    pp->colorappearance.autoybscen    = ybscen->getAutoValue ();
    pp->colorappearance.adaplum       = adaplum->getValue ();
    pp->colorappearance.badpixsl      = badpixsl->getValue ();
    pp->colorappearance.jlight        = jlight->getValue ();
    pp->colorappearance.qbright       = qbright->getValue ();
    pp->colorappearance.chroma        = chroma->getValue ();
    pp->colorappearance.schroma       = schroma->getValue ();
    pp->colorappearance.schromared    = schromared->getValue ();
    pp->colorappearance.schromagreen  = schromagreen->getValue ();
    pp->colorappearance.schromablue   = schromablue->getValue ();
    pp->colorappearance.mchroma       = mchroma->getValue ();
    pp->colorappearance.contrast      = contrast->getValue ();
    pp->colorappearance.qcontrast     = qcontrast->getValue ();
    pp->colorappearance.colorh        = colorh->getValue ();
    pp->colorappearance.colorhred     = colorhred->getValue ();
    pp->colorappearance.colorhgreen   = colorhgreen->getValue ();
    pp->colorappearance.colorhblue    = colorhblue->getValue ();
    pp->colorappearance.rstprotection = rstprotection->getValue ();
    pp->colorappearance.gamut         = gamut->get_active();
//  pp->colorappearance.badpix        = badpix->get_active();
    pp->colorappearance.datacie       = datacie->get_active();
    pp->colorappearance.tonecie       = tonecie->get_active();
    pp->colorappearance.curve         = shape->getCurve ();
    pp->colorappearance.curvered      = shapered->getCurve ();
    pp->colorappearance.curvegreen    = shapegreen->getCurve ();
    pp->colorappearance.curveblue     = shapeblue->getCurve ();
    pp->colorappearance.curve2        = shape2->getCurve ();
    pp->colorappearance.curve3        = shape3->getCurve ();
    pp->colorappearance.tempout       = tempout->getValue ();
    pp->colorappearance.autotempout   = tempout->getAutoValue ();
    pp->colorappearance.greenout      = greenout->getValue ();
    pp->colorappearance.ybout         = ybout->getValue ();
    pp->colorappearance.tempsc        = tempsc->getValue ();
    pp->colorappearance.greensc       = greensc->getValue ();

    int tcMode = toneCurveMode->get_active_row_number();

    if      (tcMode == 0) {
        pp->colorappearance.curveMode = ColorAppearanceParams::TcMode::LIGHT;
    } else if (tcMode == 1) {
        pp->colorappearance.curveMode = ColorAppearanceParams::TcMode::BRIGHT;
    }

    tcMode = toneCurveMode2->get_active_row_number();

    if      (tcMode == 0) {
        pp->colorappearance.curveMode2 = ColorAppearanceParams::TcMode::LIGHT;
    } else if (tcMode == 1) {
        pp->colorappearance.curveMode2 = ColorAppearanceParams::TcMode::BRIGHT;
    }

    int tcMode3 = toneCurveMode3->get_active_row_number();

    if      (tcMode3 == 0) {
        pp->colorappearance.curveMode3 = ColorAppearanceParams::CtcMode::CHROMA;
    } else if (tcMode3 == 1) {
        pp->colorappearance.curveMode3 = ColorAppearanceParams::CtcMode::SATUR;
    } else if (tcMode3 == 2) {
        pp->colorappearance.curveMode3 = ColorAppearanceParams::CtcMode::COLORF;
    }

    if (pedited) {
        pedited->colorappearance.complexmethod = complexmethod->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->colorappearance.modelmethod   = modelmethod->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->colorappearance.catmethod     = catmethod->get_active_text() != M("GENERAL_UNCHANGED");
        pedited->colorappearance.degree        = degree->getEditedState ();
        pedited->colorappearance.degreeout     = degreeout->getEditedState ();
        pedited->colorappearance.adapscen      = adapscen->getEditedState ();
        pedited->colorappearance.adaplum       = adaplum->getEditedState ();
        pedited->colorappearance.ybscen        = ybscen->getEditedState ();
        pedited->colorappearance.badpixsl      = badpixsl->getEditedState ();
        pedited->colorappearance.jlight        = jlight->getEditedState ();
        pedited->colorappearance.qbright       = qbright->getEditedState ();
        pedited->colorappearance.chroma        = chroma->getEditedState ();
        pedited->colorappearance.schroma       = schroma->getEditedState ();
        pedited->colorappearance.schromared    = schromared->getEditedState ();
        pedited->colorappearance.schromagreen  = schromagreen->getEditedState ();
        pedited->colorappearance.schromablue   = schromablue->getEditedState ();
        pedited->colorappearance.mchroma       = mchroma->getEditedState ();
        pedited->colorappearance.contrast      = contrast->getEditedState ();
        pedited->colorappearance.qcontrast     = qcontrast->getEditedState ();
        pedited->colorappearance.colorh        = colorh->getEditedState ();
        pedited->colorappearance.colorhred     = colorhred->getEditedState ();
        pedited->colorappearance.colorhgreen   = colorhgreen->getEditedState ();
        pedited->colorappearance.colorhblue    = colorhblue->getEditedState ();
        pedited->colorappearance.rstprotection = rstprotection->getEditedState ();
        pedited->colorappearance.autodegree    = !degree->getAutoInconsistent();
        pedited->colorappearance.autodegreeout = !degreeout->getAutoInconsistent();
        pedited->colorappearance.autoadapscen  = !adapscen->getAutoInconsistent();
        pedited->colorappearance.autoybscen    = !ybscen->getAutoInconsistent();
        pedited->colorappearance.enabled       = !get_inconsistent();
        pedited->colorappearance.surround      = surround->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->colorappearance.surrsrc       = surrsrc->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->colorappearance.wbmodel       = wbmodel->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->colorappearance.illum         = illum->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->colorappearance.algo          = algo->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->colorappearance.gamut         = !gamut->get_inconsistent();
        pedited->colorappearance.datacie       = !datacie->get_inconsistent();
        pedited->colorappearance.tonecie       = !tonecie->get_inconsistent();
        pedited->colorappearance.curve         = !shape->isUnChanged ();
        pedited->colorappearance.curvered      = !shapered->isUnChanged ();
        pedited->colorappearance.curvegreen    = !shapegreen->isUnChanged ();
        pedited->colorappearance.curveblue     = !shapeblue->isUnChanged ();
        pedited->colorappearance.curve2        = !shape2->isUnChanged ();
        pedited->colorappearance.curve3        = !shape3->isUnChanged ();
        pedited->colorappearance.curveMode     = toneCurveMode->get_active_row_number() != 2;
        pedited->colorappearance.curveMode2    = toneCurveMode2->get_active_row_number() != 2;
        pedited->colorappearance.curveMode3    = toneCurveMode3->get_active_row_number() != 3;
        pedited->colorappearance.tempout       = tempout->getEditedState ();
        pedited->colorappearance.greenout      = greenout->getEditedState ();
        pedited->colorappearance.ybout         = ybout->getEditedState ();
        pedited->colorappearance.tempsc        = tempsc->getEditedState ();
        pedited->colorappearance.greensc       = greensc->getEditedState ();
        pedited->colorappearance.autotempout   = !tempout->getAutoInconsistent();

    }

    if (complexmethod->get_active_row_number() == 0) {
        pp->colorappearance.complexmethod = "normal";
    } else if (complexmethod->get_active_row_number() == 1) {
        pp->colorappearance.complexmethod = "expert";
    }

    if (modelmethod->get_active_row_number() == 0) {
        pp->colorappearance.modelmethod = "02";
    } else if (modelmethod->get_active_row_number() == 1) {
        pp->colorappearance.modelmethod = "16";
    }

    if (catmethod->get_active_row_number() == 0) {
        pp->colorappearance.catmethod = "clas";
    } else if (catmethod->get_active_row_number() == 1) {
        pp->colorappearance.catmethod = "symg";
    } else if (catmethod->get_active_row_number() == 2) {
        pp->colorappearance.catmethod = "symc";
    }

    if (surrsrc->get_active_row_number() == 0) {
        pp->colorappearance.surrsrc = "Average";
    } else if (surrsrc->get_active_row_number() == 1) {
        pp->colorappearance.surrsrc = "Dim";
    } else if (surrsrc->get_active_row_number() == 2) {
        pp->colorappearance.surrsrc = "Dark";
    } else if (surrsrc->get_active_row_number() == 3) {
        pp->colorappearance.surrsrc = "ExtremelyDark";
    }


    if (surround->get_active_row_number() == 0) {
        pp->colorappearance.surround = "Average";
    } else if (surround->get_active_row_number() == 1) {
        pp->colorappearance.surround = "Dim";
    } else if (surround->get_active_row_number() == 2) {
        pp->colorappearance.surround = "Dark";
    } else if (surround->get_active_row_number() == 3) {
        pp->colorappearance.surround = "ExtremelyDark";
    }

    if (wbmodel->get_active_row_number() == 0) {
        pp->colorappearance.wbmodel = "RawT";
    } else if (wbmodel->get_active_row_number() == 1) {
        pp->colorappearance.wbmodel = "RawTCAT02";
    } else if (wbmodel->get_active_row_number() == 2) {
        pp->colorappearance.wbmodel = "free";
    }

    if (illum->get_active_row_number() == 0) {
        pp->colorappearance.illum = "iA";
    } else if (illum->get_active_row_number() == 1) {
        pp->colorappearance.illum = "i41";
    } else if (illum->get_active_row_number() == 2) {
        pp->colorappearance.illum = "i50";
    } else if (illum->get_active_row_number() == 3) {
        pp->colorappearance.illum = "i55";
    } else if (illum->get_active_row_number() == 4) {
        pp->colorappearance.illum = "i60";
    } else if (illum->get_active_row_number() == 5) {
        pp->colorappearance.illum = "i65";
    } else if (illum->get_active_row_number() == 6) {
        pp->colorappearance.illum = "i75";
    } else if (illum->get_active_row_number() == 7) {
        pp->colorappearance.illum = "ifree";
    }

    if (algo->get_active_row_number() == 0) {
        pp->colorappearance.algo = "JC";
    } else if (algo->get_active_row_number() == 1) {
        pp->colorappearance.algo = "JS";
    } else if (algo->get_active_row_number() == 2) {
        pp->colorappearance.algo = "QM";
    } else if (algo->get_active_row_number() == 3) {
        pp->colorappearance.algo = "ALL";
    }

}


void ColorAppearance::updateGUIToMode(int mode)
{
    if(mode ==0) {
        alHBox->hide();
        wbmHBox->hide();
        curveEditorG->show();
        curveEditorG2->hide();
        curveEditorG3->hide();
        greenout->set_sensitive(false);
        badpixsl->hide();
        datacie->hide();
    } else {
        alHBox->show();
        wbmHBox->show();
        curveEditorG->show();
        curveEditorG2->show();
        curveEditorG3->show();
        greenout->set_sensitive(true);
        badpixsl->show();
        datacie->show();
    }

}


void ColorAppearance::convertParamToNormal()
{
    const ColorAppearanceParams def_params;
    disableListener();
    algo->set_active (0);
    shape2->setCurve(def_params.curve2);
    shape3->setCurve(def_params.curve3);
    shape2->reset();
    shape3->reset();
    wbmodel->set_active (0);
    if (catmethod->get_active_row_number() == 1  || catmethod->get_active_row_number() == 2) {
        wbmodel->set_active (2);
        illumChanged();
    }

    greenout->setValue(def_params.greenout);
    badpixsl->setValue(def_params.badpixsl);

    enableListener();

    // Update GUI based on converted widget parameters:
}

void ColorAppearance::complexmethodChanged()
{
    if (complexmethod->get_active_row_number() == 0) {
        updateGUIToMode(0);
        convertParamToNormal();

    } else {
        updateGUIToMode(1);
    }

    if (listener && (multiImage || getEnabled())) {
        listener->panelChanged(EvCATcomplex, complexmethod->get_active_text());
    }
}

void ColorAppearance::modelmethodChanged()
{

    if (listener && (multiImage || getEnabled())) {
        listener->panelChanged(EvCATmodel, modelmethod->get_active_text());
    }
}

void ColorAppearance::catmethodChanged()
{

    if (catmethod->get_active_row_number() == 1) {
        disableListener();
        jlight->resetValue (false);
        qbright->resetValue (false);
        chroma->resetValue (false);
        schroma->resetValue (false);
        schromared->resetValue (false);
        schromagreen->resetValue (false);
        schromablue->resetValue (false);
        mchroma->resetValue (false);
        rstprotection->resetValue (false);
        contrast->resetValue (false);
        qcontrast->resetValue (false);
        colorh->resetValue (false);
        colorhred->resetValue (false);
        colorhgreen->resetValue (false);
        colorhblue->resetValue (false);
        tempout->resetValue (false);
        greenout->resetValue (false);
        ybout->resetValue (false);
        tempsc->resetValue (false);
        greensc->resetValue (false);
        badpixsl->resetValue (false);
        illum->set_active (2);
        toneCurveMode->set_active (0);
        toneCurveMode2->set_active (1);
        toneCurveMode3->set_active (0);
        shape->reset();
        shape2->reset();
        shape3->reset();
        gamutconn.block (true);
        gamut->set_active (true);
        gamutconn.block (false);
        degree->setAutoValue (true);
        degree->resetValue (false);
        degree->setValue(90);
        adapscen->resetValue (false);
        adapscen->setAutoValue (true);
        degreeout->resetValue (false);
        degreeout->setAutoValue (true);
        ybscen->resetValue (false);
        ybscen->setAutoValue (true);
        surrsrc->set_active (0);
        wbmodel->set_active (2);
        tempsc->resetValue (false);
        greensc->resetValue (false);
        adapscen->setValue(400.);
        ybscen->setValue(18);
        surround->set_active (0);
        adaplum->setValue(400.);
        degreeout->setValue(90);
        ybout->setValue(18);
        tempout->setValue (nexttemp);

        if(tempout->getAutoValue()) {
            tempout->resetValue (false);
            tempout->setAutoValue (true);
        } else {
            tempout->resetValue (false);
            tempout->setValue (nexttemp);
            tempout->setAutoValue (true);
        }

        greenout->setValue (nextgreen);
        enableListener();

    }  else if (catmethod->get_active_row_number() == 0) {
        disableListener();
        degree->setAutoValue (true);
        degree->resetValue (false);
        adapscen->resetValue (false);
        adapscen->setAutoValue (true);
        degreeout->resetValue (false);
        degreeout->setAutoValue (true);
        ybscen->resetValue (false);
        ybscen->setAutoValue (true);
        surrsrc->set_active (0);
        wbmodel->set_active (0);
        illum->set_active (2);
        tempsc->resetValue (false);
        greensc->resetValue (false);
        adapscen->resetValue (false);
        ybscen->resetValue (false);
        surround->set_active (0);
        adaplum->resetValue (false);
        degreeout->resetValue (false);
        ybout->resetValue (false);
        tempout->resetValue (false);
        tempout->setAutoValue (true);
        greenout->resetValue (true);
        enableListener();
    }  else if (catmethod->get_active_row_number() == 2) {
        disableListener();
        degree->setAutoValue (true);
        degree->resetValue (false);
        adapscen->resetValue (false);
        adapscen->setAutoValue (true);
        degreeout->resetValue (false);
        degreeout->setAutoValue (true);
        ybscen->resetValue (false);
        ybscen->setAutoValue (true);
        surrsrc->set_active (0);
        wbmodel->set_active (2);
        illum->set_active (2);
        tempsc->resetValue (false);
        greensc->resetValue (false);
        adapscen->resetValue (false);
        ybscen->resetValue (false);
        surround->set_active (0);
        adaplum->resetValue (false);
        degreeout->resetValue (false);
        ybout->resetValue (false);
        tempout->resetValue (false);
        tempout->setValue (nexttemp);
        greenout->resetValue (false);
        enableListener();
    }

    if (listener && (multiImage || getEnabled())) {
        listener->panelChanged(EvCATcat, catmethod->get_active_text());
    }
}

void ColorAppearance::curveChanged (CurveEditor* ce)
{

    if (listener) {
        if (ce == shape) {
            listener->panelChanged (EvCATCurve1, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == shapered) {
            listener->panelChanged (EvCATCurvered, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == shapegreen) {
            listener->panelChanged (EvCATCurvegreen, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == shapeblue) {
            listener->panelChanged (EvCATCurveblue, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == shape2) {
            listener->panelChanged (EvCATCurve2, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == shape3) {
            listener->panelChanged (EvCATCurve3, M ("HISTORY_CUSTOMCURVE"));
        }
    }
}

void ColorAppearance::curveMode1Changed ()
{
    if (listener) {
        Glib::signal_idle().connect (sigc::mem_fun (*this, &ColorAppearance::curveMode1Changed_));
    }
}

bool ColorAppearance::curveMode1Changed_ ()
{
    if (listener) {
        listener->panelChanged (EvCATCurveMode1, toneCurveMode->get_active_text());
    }

    return false;
}

void ColorAppearance::curveMode2Changed ()
{
    if (listener) {
        Glib::signal_idle().connect (sigc::mem_fun (*this, &ColorAppearance::curveMode2Changed_));
    }
}

bool ColorAppearance::curveMode2Changed_ ()
{
    if (listener) {
        listener->panelChanged (EvCATCurveMode2, toneCurveMode2->get_active_text());
    }

    return false;
}

void ColorAppearance::curveMode3Changed ()
{
    int tcMode3 = toneCurveMode3->get_active_row_number();

    if (tcMode3 == 0) {
        chroma->set_sensitive (true);
        schroma->set_sensitive (true);
    } else if (tcMode3 == 2) {
        chroma->set_sensitive (false);
        schroma->set_sensitive (false);
    } else if (tcMode3 == 1) {
        chroma->set_sensitive (false);
        schroma->set_sensitive (true);
    }

    if (listener) {
        Glib::signal_idle().connect (sigc::mem_fun (*this, &ColorAppearance::curveMode3Changed_));
    }
}

bool ColorAppearance::curveMode3Changed_ ()
{
    if (listener) {
        listener->panelChanged (EvCATCurveMode3, toneCurveMode3->get_active_text());
    }

    return false;
}

void ColorAppearance::gamut_toggled ()
{

    if (batchMode) {
        if (gamut->get_inconsistent()) {
            gamut->set_inconsistent (false);
            gamutconn.block (true);
            gamut->set_active (false);
            gamutconn.block (false);
        } else if (lastgamut) {
            gamut->set_inconsistent (true);
        }

        lastgamut = gamut->get_active ();
    }

    if (listener) {
        if (gamut->get_active ()) {
            listener->panelChanged (EvCATgamut, M ("GENERAL_ENABLED"));
        } else {
            listener->panelChanged (EvCATgamut, M ("GENERAL_DISABLED"));
        }
    }


}
/*
void ColorAppearance::badpix_toggled () {

    if (batchMode) {
        if (badpix->get_inconsistent()) {
            badpix->set_inconsistent (false);
            badpixconn.block (true);
            badpix->set_active (false);
            badpixconn.block (false);
        }
        else if (lastbadpix)
            badpix->set_inconsistent (true);

        lastbadpix = badpix->get_active ();
    }
    if (listener) {
        if (badpix->get_active ())
            listener->panelChanged (EvCATbadpix, M("GENERAL_ENABLED"));
        else
            listener->panelChanged (EvCATbadpix, M("GENERAL_DISABLED"));
    }


}
*/

void ColorAppearance::datacie_toggled ()
{

    if (batchMode) {
        if (datacie->get_inconsistent()) {
            datacie->set_inconsistent (false);
            datacieconn.block (true);
            datacie->set_active (false);
            datacieconn.block (false);
        } else if (lastdatacie) {
            datacie->set_inconsistent (true);
        }

        lastdatacie = datacie->get_active ();
    }

    if (listener) {
        if (datacie->get_active ()) {
            listener->panelChanged (EvCATdatacie, M ("GENERAL_ENABLED"));
        } else {
            listener->panelChanged (EvCATdatacie, M ("GENERAL_DISABLED"));
        }
    }
}

void ColorAppearance::tonecie_toggled ()
{

    if (batchMode) {
        if (tonecie->get_inconsistent()) {
            tonecie->set_inconsistent (false);
            tonecieconn.block (true);
            tonecie->set_active (false);
            tonecieconn.block (false);
        } else if (lasttonecie) {
            tonecie->set_inconsistent (true);
        }

        lasttonecie = tonecie->get_active ();
    }

    if (listener) {
        if (tonecie->get_active ()) {
            listener->panelChanged (EvCATtonecie, M ("GENERAL_ENABLED"));
        } else {
            listener->panelChanged (EvCATtonecie, M ("GENERAL_DISABLED"));
        }
    }

}

void ColorAppearance::setDefaults (const ProcParams* defParams, const ParamsEdited* pedited)
{

    degree->setDefault (defParams->colorappearance.degree);
    degreeout->setDefault (defParams->colorappearance.degreeout);
    adapscen->setDefault (defParams->colorappearance.adapscen);
    ybscen->setDefault (defParams->colorappearance.ybscen);
    adaplum->setDefault (defParams->colorappearance.adaplum);
    badpixsl->setDefault (defParams->colorappearance.badpixsl);
    jlight->setDefault (defParams->colorappearance.jlight);
    qbright->setDefault (defParams->colorappearance.qbright);
    chroma->setDefault (defParams->colorappearance.chroma);
    schroma->setDefault (defParams->colorappearance.schroma);
    schromared->setDefault (defParams->colorappearance.schromared);
    schromagreen->setDefault (defParams->colorappearance.schromagreen);
    schromablue->setDefault (defParams->colorappearance.schromablue);
    mchroma->setDefault (defParams->colorappearance.mchroma);
    rstprotection->setDefault (defParams->colorappearance.rstprotection);
    contrast->setDefault (defParams->colorappearance.contrast);
    qcontrast->setDefault (defParams->colorappearance.qcontrast);
    colorh->setDefault (defParams->colorappearance.colorh);
    colorhred->setDefault (defParams->colorappearance.colorhred);
    colorhgreen->setDefault (defParams->colorappearance.colorhgreen);
    colorhblue->setDefault (defParams->colorappearance.colorhblue);
    tempout->setDefault (defParams->colorappearance.tempout);
    greenout->setDefault (defParams->colorappearance.greenout);
    ybout->setDefault (defParams->colorappearance.ybout);
    tempsc->setDefault (defParams->colorappearance.tempsc);
    greensc->setDefault (defParams->colorappearance.greensc);

    if (pedited) {
        degree->setDefaultEditedState (pedited->colorappearance.degree ? Edited : UnEdited);
        degreeout->setDefaultEditedState (pedited->colorappearance.degreeout ? Edited : UnEdited);
        adapscen->setDefaultEditedState (pedited->colorappearance.adapscen ? Edited : UnEdited);
        ybscen->setDefaultEditedState (pedited->colorappearance.ybscen ? Edited : UnEdited);
        adaplum->setDefaultEditedState (pedited->colorappearance.adaplum ? Edited : UnEdited);
        badpixsl->setDefaultEditedState (pedited->colorappearance.badpixsl ? Edited : UnEdited);
        jlight->setDefaultEditedState (pedited->colorappearance.jlight ? Edited : UnEdited);
        qbright->setDefaultEditedState (pedited->colorappearance.qbright ? Edited : UnEdited);
        chroma->setDefaultEditedState (pedited->colorappearance.chroma ? Edited : UnEdited);
        schroma->setDefaultEditedState (pedited->colorappearance.schroma ? Edited : UnEdited);
        schromared->setDefaultEditedState (pedited->colorappearance.schromared ? Edited : UnEdited);
        schromagreen->setDefaultEditedState (pedited->colorappearance.schromagreen ? Edited : UnEdited);
        schromablue->setDefaultEditedState (pedited->colorappearance.schromablue ? Edited : UnEdited);
        mchroma->setDefaultEditedState (pedited->colorappearance.mchroma ? Edited : UnEdited);
        rstprotection->setDefaultEditedState (pedited->colorappearance.rstprotection ? Edited : UnEdited);
        contrast->setDefaultEditedState (pedited->colorappearance.contrast ? Edited : UnEdited);
        qcontrast->setDefaultEditedState (pedited->colorappearance.qcontrast ? Edited : UnEdited);
        colorh->setDefaultEditedState (pedited->colorappearance.colorh ? Edited : UnEdited);
        colorhred->setDefaultEditedState (pedited->colorappearance.colorhred ? Edited : UnEdited);
        colorhgreen->setDefaultEditedState (pedited->colorappearance.colorhgreen ? Edited : UnEdited);
        colorhblue->setDefaultEditedState (pedited->colorappearance.colorhblue ? Edited : UnEdited);
        tempout->setDefaultEditedState (pedited->colorappearance.tempout ? Edited : UnEdited);
        greenout->setDefaultEditedState (pedited->colorappearance.greenout ? Edited : UnEdited);
        ybout->setDefaultEditedState (pedited->colorappearance.ybout ? Edited : UnEdited);
        tempsc->setDefaultEditedState (pedited->colorappearance.tempsc ? Edited : UnEdited);
        greensc->setDefaultEditedState (pedited->colorappearance.greensc ? Edited : UnEdited);

    } else {
        degree->setDefaultEditedState (Irrelevant);
        degreeout->setDefaultEditedState (Irrelevant);
        adapscen->setDefaultEditedState (Irrelevant);
        ybscen->setDefaultEditedState (Irrelevant);
        adaplum->setDefaultEditedState (Irrelevant);
        badpixsl->setDefaultEditedState (Irrelevant);
        jlight->setDefaultEditedState (Irrelevant);
        qbright->setDefaultEditedState (Irrelevant);
        chroma->setDefaultEditedState (Irrelevant);
        schroma->setDefaultEditedState (Irrelevant);
        schromared->setDefaultEditedState (Irrelevant);
        schromagreen->setDefaultEditedState (Irrelevant);
        schromablue->setDefaultEditedState (Irrelevant);
        mchroma->setDefaultEditedState (Irrelevant);
        contrast->setDefaultEditedState (Irrelevant);
        qcontrast->setDefaultEditedState (Irrelevant);
        rstprotection->setDefaultEditedState (Irrelevant);
        colorh->setDefaultEditedState (Irrelevant);
        colorhred->setDefaultEditedState (Irrelevant);
        colorhgreen->setDefaultEditedState (Irrelevant);
        colorhblue->setDefaultEditedState (Irrelevant);
        tempout->setDefaultEditedState (Irrelevant);
        greenout->setDefaultEditedState (Irrelevant);
        ybout->setDefaultEditedState (Irrelevant);
        tempsc->setDefaultEditedState (Irrelevant);
        greensc->setDefaultEditedState (Irrelevant);

    }
}

void ColorAppearance::autoCamChanged (double ccam, double ccamout)
{
    idle_register.add(
        [this, ccam, ccamout]() -> bool
        {
            disableListener();
            degree->setValue(ccam);
            degreeout->setValue(ccamout);
            enableListener();
            return false;
        }
    );
}

void ColorAppearance::adapCamChanged (double cadap)
{

    idle_register.add(
        [this, cadap]() -> bool
        {
            disableListener();
            adapscen->setValue(cadap);
            enableListener();
            return false;
        }
    );
}


void ColorAppearance::wbCamChanged (double temp, double tin, bool autotemp)
{//reactivate this function

    idle_register.add(
        [this, temp, tin, autotemp]() -> bool

        {
            if (temp != tempout->getValue()) {
                disableListener();
                tempout->setValue(temp);
                enableListener();
                listener->panelChanged (EvCATtempout, tempout->getTextValue());
            }
            if (tin != greenout->getValue()) {
                disableListener();
                greenout->setValue(tin);
                enableListener();
                listener->panelChanged (EvCATgreenout, greenout->getTextValue());
            }

            return false;
        }
    );
}

void ColorAppearance::ybCamChanged (int ybsc)
{

    idle_register.add(
        [this, ybsc]() -> bool
        {
            disableListener();
            ybscen->setValue(ybsc);
            enableListener();
            return false;
        }
    );
}

void ColorAppearance::colorForValue (double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller *caller)
{

    float R = 0.f, G = 0.f, B = 0.f;

    if (elemType == ColorCaller::CCET_VERTICAL_BAR) {
        valY = 0.5;
    }

    if (callerId == 1) {    // cc - bottom bar

        float value = (1.f - 0.7f) * float (valX) + 0.7f;
        // whole hue range
        // Y axis / from 0.15 up to 0.75 (arbitrary values; was 0.45 before)
        Color::hsv2rgb01 (float (valY*0.8), float (valX), value, R, G, B);
    }

    caller->ccRed = double (R);
    caller->ccGreen = double (G);
    caller->ccBlue = double (B);
}

void ColorAppearance::adjusterChanged(Adjuster* a, double newval)
{
    if (listener && (multiImage || getEnabled()) ) {
        if (a == degree) {
            listener->panelChanged (EvCATDegree, a->getTextValue());
        } else if (a == degreeout) {
            listener->panelChanged (EvCATDegreeout, a->getTextValue());
        } else if (a == adapscen) {
            listener->panelChanged (EvCATAdapscen, a->getTextValue());
        } else if (a == ybscen) {
            listener->panelChanged (EvCATybscen, a->getTextValue());
        } else if (a == adaplum) {
            listener->panelChanged (EvCATAdapLum, a->getTextValue());
        } else if (a == badpixsl) {
            listener->panelChanged (EvCATbadpix, a->getTextValue());
        } else if (a == jlight) {
            listener->panelChanged (EvCATJLight, a->getTextValue());
        } else if (a == qbright) {
            listener->panelChanged (EvCATQbright, a->getTextValue());
        } else if (a == chroma) {
            listener->panelChanged (EvCATChroma, a->getTextValue());
        } else if (a == schroma) {
            listener->panelChanged (EvCATSChroma, a->getTextValue());
        } else if (a == schromared) {
            listener->panelChanged (EvCATschromared, a->getTextValue());
        } else if (a == schromagreen) {
            listener->panelChanged (EvCATschromagreen, a->getTextValue());
        } else if (a == schromablue) {
            listener->panelChanged (EvCATschromablue, a->getTextValue());
        } else if (a == mchroma) {
            listener->panelChanged (EvCATMChroma, a->getTextValue());
        } else if (a == rstprotection) {
            listener->panelChanged (EvCATRstpro, a->getTextValue());
        } else if (a == contrast) {
            listener->panelChanged (EvCATContrast, a->getTextValue());
        } else if (a == colorh) {
            listener->panelChanged (EvCAThue, a->getTextValue());
        } else if (a == colorhred) {
            listener->panelChanged (EvCATcolorhred, a->getTextValue());
        } else if (a == colorhgreen) {
            listener->panelChanged (EvCATcolorhgreen, a->getTextValue());
        } else if (a == colorhblue) {
            listener->panelChanged (EvCATcolorhblue, a->getTextValue());
        } else if (a == qcontrast) {
            listener->panelChanged (EvCATQContrast, a->getTextValue());
        } else if (a == tempout) {
            listener->panelChanged (EvCATtempout, a->getTextValue());
        } else if (a == greenout) {
            listener->panelChanged (EvCATgreenout, a->getTextValue());
        } else if (a == ybout) {
            listener->panelChanged (EvCATybout, a->getTextValue());
        } else if (a == tempsc) {
            listener->panelChanged (EvCATtempsc, a->getTextValue());
        } else if (a == greensc) {
            listener->panelChanged (EvCATgreensc, a->getTextValue());
        }

    }
}

void ColorAppearance::adjusterAutoToggled(Adjuster* a, bool newval)
{
    if (multiImage) {
        if (degree->getAutoInconsistent()) {
            degree->setAutoInconsistent (false);
            degree->setAutoValue (false);
        } else if (lastAutoDegree) {
            degree->setAutoInconsistent (true);
        }

        lastAutoDegree = degree->getAutoValue();

        if (degreeout->getAutoInconsistent()) {
            degreeout->setAutoInconsistent (false);
            degreeout->setAutoValue (false);
        } else if (lastAutoDegreeout) {
            degreeout->setAutoInconsistent (true);
        }

        lastAutoDegreeout = degreeout->getAutoValue();

        if (adapscen->getAutoInconsistent()) {
            adapscen->setAutoInconsistent (false);
            adapscen->setAutoValue (false);
        } else if (lastAutoAdapscen) {
            adapscen->setAutoInconsistent (true);
        }

        lastAutoAdapscen = adapscen->getAutoValue();

        if (ybscen->getAutoInconsistent()) {
            ybscen->setAutoInconsistent (false);
            ybscen->setAutoValue (false);
        } else if (lastAutoybscen) {
            ybscen->setAutoInconsistent (true);
        }

        lastAutotempout = tempout->getAutoValue();

        if (tempout->getAutoInconsistent()) {
            tempout->setAutoInconsistent (false);
            tempout->setAutoValue (false);
        } else if (lastAutotempout) {
            tempout->setAutoInconsistent (true);
        }

        lastAutotempout = tempout->getAutoValue();

    }
    if (listener && (multiImage || getEnabled()) ) {

        if (a == degree) {
            if (degree->getAutoInconsistent()) {
                listener->panelChanged (EvCATAutoDegree, M ("GENERAL_UNCHANGED"));
            } else if (degree->getAutoValue()) {
                listener->panelChanged (EvCATAutoDegree, M ("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvCATAutoDegree, M ("GENERAL_DISABLED"));
            }
        }

        if (a == degreeout) {
            if (degreeout->getAutoInconsistent()) {
                listener->panelChanged (EvCATAutoDegreeout, M ("GENERAL_UNCHANGED"));
            } else if (degreeout->getAutoValue()) {
                listener->panelChanged (EvCATAutoDegreeout, M ("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvCATAutoDegreeout, M ("GENERAL_DISABLED"));
            }
        }


        if (a == adapscen) {
            if (adapscen->getAutoInconsistent()) {
                listener->panelChanged (EvCATAutoAdap, M ("GENERAL_UNCHANGED"));
            } else if (adapscen->getAutoValue()) {
                listener->panelChanged (EvCATAutoAdap, M ("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvCATAutoAdap, M ("GENERAL_DISABLED"));
            }
        }

        if (a == ybscen) {
            if (ybscen->getAutoInconsistent()) {
                listener->panelChanged (EvCATAutoyb, M ("GENERAL_UNCHANGED"));
            } else if (ybscen->getAutoValue()) {
                listener->panelChanged (EvCATAutoyb, M ("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvCATAutoyb, M ("GENERAL_DISABLED"));
            }
        }

        if (a == tempout) {
            if (tempout->getAutoInconsistent()) {
                listener->panelChanged (EvCATAutotempout, M ("GENERAL_UNCHANGED"));
            } else if (tempout->getAutoValue()) {
                listener->panelChanged (EvCATAutotempout, M ("GENERAL_ENABLED"));
            } else {
                listener->panelChanged (EvCATAutotempout, M ("GENERAL_DISABLED"));
            }
        }

    }
}
void ColorAppearance::enabledChanged ()
{

    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged (EvCATEnabled, M ("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged (EvCATEnabled, M ("GENERAL_ENABLED"));
            curveEditorG->set_sensitive (true);
            toneCurveMode->set_sensitive (true);
        } else {
            listener->panelChanged (EvCATEnabled, M ("GENERAL_DISABLED"));
        }
    }
}

void ColorAppearance::surrsrcChanged ()
{

    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged (EvCATsurr, surrsrc->get_active_text ());
    }
}


void ColorAppearance::surroundChanged ()
{

    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged (EvCATMethodsur, surround->get_active_text ());
    }
}

void ColorAppearance::wbmodelChanged ()
{
    if (wbmodel->get_active_row_number() == 0 || wbmodel->get_active_row_number() == 1) {
        illum->hide();
        illumHBox->hide();
        tempsc->hide();
        greensc->hide();
        tempsc->setValue (5003);
        greensc->setValue (1);
    }

    if (wbmodel->get_active_row_number() == 2) {
        illumHBox->show();
        tempsc->show();
        greensc->show();
        illum->show();
    }

    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged (EvCATMethodWB, wbmodel->get_active_text ());
    }
}

void ColorAppearance::illumChanged ()
{
    if (illum->get_active_row_number() == 0) {
            tempsc->setValue (2856);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 1) {
            tempsc->setValue (4100);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 2) {
            tempsc->setValue (5003);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 3) {
            tempsc->setValue (5503);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 4) {
            tempsc->setValue (6000);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 5) {
            tempsc->setValue (6504);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 6) {
            tempsc->setValue (7504);
            tempsc->set_sensitive(false);
            greensc->set_sensitive(false);
    } else if (illum->get_active_row_number() == 7) {
            tempsc->set_sensitive(true);
            greensc->set_sensitive(true);
    }

    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged (EvCATillum, illum->get_active_text ());
    }
}



void ColorAppearance::algoChanged ()
{

    if ( algo->get_active_row_number() == 0 ) {
        contrast->show();
        rstprotection->show();
        qcontrast->hide();
        jlight->show();
        mchroma->hide();
        chroma->show();
        schroma->hide();
        qbright->hide();
        colorh->show();
        tonecie->hide();
        curveEditorG->show();
        curveEditorG2->show();
        curveEditorG3->show();

    } else if ( algo->get_active_row_number() == 1 ) {
        rstprotection->show();
        contrast->show();
        qcontrast->hide();
        jlight->show();
        mchroma->hide();
        chroma->hide();
        schroma->show();
        qbright->hide();
        colorh->show();
        tonecie->hide();
        curveEditorG->show();
        curveEditorG2->show();
        curveEditorG3->show();
    } else if ( algo->get_active_row_number() == 2 ) {
        contrast->hide();
        rstprotection->show();
        qcontrast->show();
        jlight->hide();
        mchroma->show();
        chroma->hide();
        schroma->hide();
        qbright->show();
        colorh->show();
        tonecie->show();
        curveEditorG->show();
        curveEditorG2->show();
        curveEditorG3->show();

    } else if ( algo->get_active_row_number() >= 3 ) { // ">=3" because everything has to be visible with the "(unchanged)" option too
        contrast->show();
        rstprotection->show();
        qcontrast->show();
        jlight->show();
        mchroma->show();
        chroma->show();
        schroma->show();
        qbright->show();
        colorh->show();
        tonecie->show();
        curveEditorG->show();
        curveEditorG2->show();
        curveEditorG3->show();
    }

    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged (EvCATMethodalg, algo->get_active_text ());
    }
}

void ColorAppearance::setBatchMode (bool batchMode)
{

    ToolPanel::setBatchMode (batchMode);

    degree->showEditedCB ();
    degreeout->showEditedCB ();
    adapscen->showEditedCB ();
    adaplum->showEditedCB ();
    ybscen->showEditedCB ();
    badpixsl->showEditedCB ();
    jlight->showEditedCB ();
    qbright->showEditedCB ();
    chroma->showEditedCB ();
    schroma->showEditedCB ();
    schromared->showEditedCB ();
    schromagreen->showEditedCB ();
    schromablue->showEditedCB ();
    mchroma->showEditedCB ();
    rstprotection->showEditedCB ();
    contrast->showEditedCB ();
    qcontrast->showEditedCB ();
    colorh->showEditedCB ();
    colorhred->showEditedCB ();
    colorhgreen->showEditedCB ();
    colorhblue->showEditedCB ();
    tempout->showEditedCB ();
    greenout->showEditedCB ();
    ybout->showEditedCB ();
    tempsc->showEditedCB ();
    greensc->showEditedCB ();

    complexmethod->append(M("GENERAL_UNCHANGED"));
    modelmethod->append(M("GENERAL_UNCHANGED"));
    catmethod->append(M("GENERAL_UNCHANGED"));
    surround->append (M ("GENERAL_UNCHANGED"));
    surrsrc->append (M ("GENERAL_UNCHANGED"));
    wbmodel->append (M ("GENERAL_UNCHANGED"));
    illum->append (M ("GENERAL_UNCHANGED"));
    algo->append (M ("GENERAL_UNCHANGED"));
    toneCurveMode->append (M ("GENERAL_UNCHANGED"));
    toneCurveMode2->append (M ("GENERAL_UNCHANGED"));
    toneCurveMode3->append (M ("GENERAL_UNCHANGED"));

    curveEditorG->setBatchMode (batchMode);
    curveEditorGred->setBatchMode (batchMode);
    curveEditorGgreen->setBatchMode (batchMode);
    curveEditorGblue->setBatchMode (batchMode);
    curveEditorG2->setBatchMode (batchMode);
    curveEditorG3->setBatchMode (batchMode);
}

void ColorAppearance::updateCurveBackgroundHistogram(
    const LUTu& histToneCurve,
    const LUTu& histLCurve,
    const LUTu& histCCurve,
    const LUTu& histLCAM,
    const LUTu& histCCAM,
    const LUTu& histRed,
    const LUTu& histGreen,
    const LUTu& histBlue,
    const LUTu& histLuma,
    const LUTu& histLRETI
)
{
    shape->updateBackgroundHistogram(histLCAM);
    shape3->updateBackgroundHistogram(histCCAM);
}



void ColorAppearance::setAdjusterBehavior (bool degreeadd, bool adapscenadd, bool adaplumadd, bool badpixsladd, bool jlightadd, bool chromaadd, bool contrastadd, bool rstprotectionadd, bool qbrightadd, bool qcontrastadd, bool schromaadd, bool schromaredadd, bool schromagreenadd, bool schromablueadd, bool mchromaadd, bool colorhadd, bool colorhredadd, bool colorhgreenadd, bool colorhblueadd, bool degreeoutadd, bool tempoutadd)
{

    degree->setAddMode (degreeadd);
    adapscen->setAddMode (adapscenadd);
    adaplum->setAddMode (adaplumadd);
    badpixsl->setAddMode (badpixsladd);
    jlight->setAddMode (jlightadd);
    qbright->setAddMode (qbrightadd);
    chroma->setAddMode (chromaadd);
    schroma->setAddMode (schromaadd);
    schromared->setAddMode (schromaredadd);
    schromagreen->setAddMode (schromagreenadd);
    schromablue->setAddMode (schromablueadd);
    mchroma->setAddMode (mchromaadd);
    rstprotection->setAddMode (rstprotectionadd);
    contrast->setAddMode (contrastadd);
    qcontrast->setAddMode (qcontrastadd);
    colorh->setAddMode (colorhadd);
    colorhred->setAddMode (colorhredadd);
    colorhgreen->setAddMode (colorhgreenadd);
    colorhblue->setAddMode (colorhblueadd);
    degreeout->setAddMode (degreeoutadd);
    tempout->setAddMode (tempoutadd);

}

void ColorAppearance::trimValues (rtengine::procparams::ProcParams* pp)
{

    degree->trimValue (pp->colorappearance.degree);
    degreeout->trimValue (pp->colorappearance.degreeout);
    adapscen->trimValue (pp->colorappearance.adapscen);
    ybscen->trimValue (pp->colorappearance.ybscen);
    adaplum->trimValue (pp->colorappearance.adaplum);
    badpixsl->trimValue (pp->colorappearance.badpixsl);
    jlight->trimValue (pp->colorappearance.jlight);
    qbright->trimValue (pp->colorappearance.qbright);
    chroma->trimValue (pp->colorappearance.chroma);
    schroma->trimValue (pp->colorappearance.schroma);
    schromared->trimValue (pp->colorappearance.schromared);
    schromagreen->trimValue (pp->colorappearance.schromagreen);
    schromablue->trimValue (pp->colorappearance.schromablue);
    mchroma->trimValue (pp->colorappearance.mchroma);
    rstprotection->trimValue (pp->colorappearance.rstprotection);
    contrast->trimValue (pp->colorappearance.contrast);
    qcontrast->trimValue (pp->colorappearance.qcontrast);
    colorh->trimValue (pp->colorappearance.colorh);
    colorhred->trimValue (pp->colorappearance.colorhred);
    colorhgreen->trimValue (pp->colorappearance.colorhgreen);
    colorhblue->trimValue (pp->colorappearance.colorhblue);
    tempout->trimValue (pp->colorappearance.tempout);
    greenout->trimValue (pp->colorappearance.greenout);
    ybout->trimValue (pp->colorappearance.ybout);
    tempsc->trimValue (pp->colorappearance.tempsc);
    greensc->trimValue (pp->colorappearance.greensc);
}
