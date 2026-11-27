/*
 *  This file is part of RawTherapee.
 */
#include "retinex.h"

#include "eventmapper.h"
#include "options.h"
#include "rtimage.h"
#include "widgets/basic/myexpander.h"
#include "widgets/curves/curveeditor.h"
#include "widgets/curves/curveeditorgroup.h"
#include "widgets/curves/mycurve.h"

#include "rtengine/color.h"

using namespace rtengine;
using namespace rtengine::procparams;

const Glib::ustring Retinex::TOOL_NAME = "retinex";

Retinex::Retinex () : FoldableToolPanel (this, TOOL_NAME, M ("TP_RETINEX_LABEL"), false, true), lastmedianmap (false)
{
    CurveListener::setMulti (true);
    std::vector<GradientMilestone> milestones;
    nextmin = 0.;
    nextmax = 0.;
    nextminiT = 0.;
    nextmaxiT = 0.;
    nextmeanT = 0.;
    nextsigma = 0.;
    nextminT = 0.;
    nextmaxT = 0.;
    auto m = ProcEventMapper::getInstance();
    EvReticomplex = m->newEvent(DEMOSAIC, "HISTORY_MSG_COMPLEXRETI");


    const RetinexParams default_params;


    // MAIN Expander ==================================================================

    complexmethod = Gtk::manage (new MyComboBoxText ());

    complexmethod->append(M("TP_WAVELET_COMPNORMAL"));
    complexmethod->append(M("TP_WAVELET_COMPEXPERT"));
    complexmethodconn = complexmethod->signal_changed().connect(sigc::mem_fun(*this, &Retinex::complexmethodChanged));
    complexmethod->set_tooltip_text(M("TP_WAVELET_COMPLEX_TOOLTIP"));
    Gtk::Box* const complexHBox = Gtk::manage(new Gtk::Box());
    Gtk::Label* const complexLabel = Gtk::manage(new Gtk::Label(M("TP_WAVELET_COMPLEXLAB") + ":"));
    complexHBox->pack_start(*complexLabel, Gtk::PACK_SHRINK, 4);
    complexHBox->pack_start(*complexmethod);
    pack_start(*complexHBox);


    Gtk::Grid *retinexGrid = Gtk::manage ( new Gtk::Grid());
    setExpandAlignProperties (retinexGrid, false, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    dhgrid = Gtk::manage (new Gtk::Grid ());
    setExpandAlignProperties (dhgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    labmdh = Gtk::manage (new Gtk::Label (M ("TP_RETINEX_METHOD") + ":"));
    setExpandAlignProperties (labmdh, false, false, Gtk::ALIGN_START, Gtk::ALIGN_BASELINE);
    dhgrid->attach (*labmdh, 0, 0, 1, 1);

    retinexMethod = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties (retinexMethod, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    retinexMethod->append (M ("TP_RETINEX_LOW"));
    retinexMethod->append (M ("TP_RETINEX_UNIFORM"));
    retinexMethod->append (M ("TP_RETINEX_HIGH"));
    retinexMethod->append (M ("TP_RETINEX_HIGHLIG"));
//  retinexMethod->append (M("TP_RETINEX_HIGHLIGPLUS"));
    retinexMethod->set_active (0);
    retinexMethodConn = retinexMethod->signal_changed().connect ( sigc::mem_fun (*this, &Retinex::retinexMethodChanged) );
    retinexMethod->set_tooltip_markup (M ("TP_RETINEX_METHOD_TOOLTIP"));
    dhgrid->attach (*retinexMethod, 1, 0, 1, 1);

    retinexcolorspace = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties (retinexcolorspace, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    retinexcolorspace->append (M ("TP_RETINEX_LABSPACE"));
    retinexcolorspace->append (M ("TP_RETINEX_HSLSPACE_LOG"));
    retinexcolorspace->append (M ("TP_RETINEX_HSLSPACE_LIN"));
    retinexcolorspace->set_active (0);
    retinexColorSpaceConn = retinexcolorspace->signal_changed().connect ( sigc::mem_fun (*this, &Retinex::retinexColorSpaceChanged) );
    dhgrid->attach (*retinexcolorspace, 2, 0, 1, 1);
    retinexGrid->attach (*dhgrid, 0, 0, 1, 1);

    str = Gtk::manage (new Adjuster (M ("TP_RETINEX_STRENGTH"), 0, 100., 1., 20.));
    setExpandAlignProperties (str, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    retinexGrid->attach (*str, 0, 1, 1, 1);
    str->show ();

    neigh = Gtk::manage (new Adjuster (M ("TP_RETINEX_NEIGHBOR"), 6, 100., 1., 80.));
    setExpandAlignProperties (neigh, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    retinexGrid->attach (*neigh, 0, 2, 1, 1);
    neigh->show ();

    vart   = Gtk::manage (new Adjuster (M ("TP_RETINEX_VARIANCE"), 50, 500, 1, 200));
    setExpandAlignProperties (vart, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    vart->set_tooltip_markup (M ("TP_RETINEX_VARIANCE_TOOLTIP"));
    retinexGrid->attach (*vart, 0, 3, 1, 1);
    vart->show ();

    highl   = Gtk::manage (new Adjuster (M ("TP_RETINEX_HIGHLIGHT"), 1, 20, 1, 4));
    setExpandAlignProperties (highl, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    highl->set_tooltip_markup (M ("TP_RETINEX_HIGHLIGHT_TOOLTIP"));
    retinexGrid->attach (*highl, 0, 4, 1, 1);
    highl->show ();

    viewgrid = Gtk::manage (new Gtk::Grid ());
    setExpandAlignProperties (viewgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    labview = Gtk::manage (new Gtk::Label (M ("TP_RETINEX_VIEW") + ":"));
    setExpandAlignProperties (labview, false, false, Gtk::ALIGN_START, Gtk::ALIGN_BASELINE);
    viewgrid->attach (*labview, 0, 0, 1, 1);

    viewMethod = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties (viewMethod, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    viewMethod->append (M ("TP_RETINEX_VIEW_NONE"));
    viewMethod->append (M ("TP_RETINEX_VIEW_UNSHARP"));
    viewMethod->append (M ("TP_RETINEX_VIEW_MASK"));
    viewMethod->append (M ("TP_RETINEX_VIEW_TRAN"));
    viewMethod->append (M ("TP_RETINEX_VIEW_TRAN2"));
    viewMethod->set_active (0);
    viewMethodConn = viewMethod->signal_changed().connect ( sigc::mem_fun (*this, &Retinex::viewMethodChanged) );
    viewMethod->set_tooltip_markup (M ("TP_RETINEX_VIEW_METHOD_TOOLTIP"));
    viewgrid->attach (*viewMethod, 1, 0, 1, 1);
    retinexGrid->attach (*viewgrid, 0, 5, 1, 1);

    //-------------

    pack_start (*retinexGrid);


    // MAP (MASK) Frame ---------------------------------------------------------------


 //   Gtk::Frame *maskFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_LABEL_MASK")) );
    maskFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_LABEL_MASK")) );
    maskFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties (maskFrame, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    Gtk::Grid *maskGrid = Gtk::manage ( new Gtk::Grid());
    setExpandAlignProperties (maskGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    // Map Method
    mapgrid = Gtk::manage (new Gtk::Grid ());
    setExpandAlignProperties (mapgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    labmap = Gtk::manage (new Gtk::Label (M ("TP_RETINEX_MAP") + ":"));
    setExpandAlignProperties (labmap, false, false, Gtk::ALIGN_START, Gtk::ALIGN_BASELINE);
    mapgrid->attach (*labmap, 0, 0, 1, 1);

    mapMethod = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties (mapMethod, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    mapMethod->append (M ("TP_RETINEX_MAP_NONE"));
    mapMethod->append (M ("TP_RETINEX_MAP_GAUS"));
    mapMethod->append (M ("TP_RETINEX_MAP_MAPP"));
    mapMethod->append (M ("TP_RETINEX_MAP_MAPT"));
    mapMethod->set_active (0);
    mapMethodConn = mapMethod->signal_changed().connect ( sigc::mem_fun (*this, &Retinex::mapMethodChanged) );
    mapMethod->set_tooltip_markup (M ("TP_RETINEX_MAP_METHOD_TOOLTIP"));
    mapgrid->attach (*mapMethod, 1, 0, 1, 1);

    maskGrid->attach (*mapgrid, 0, 0, 1, 1);
    mapgrid->show();

    auto& options = App::get().mut_options();

    // Map Equalizer
    curveEditormap = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_CONTEDIT_MAP"));
    setExpandAlignProperties (curveEditormap, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    curveEditormap->setCurveListener (this);
    std::vector<GradientMilestone> milestones222;
    milestones222.push_back ( GradientMilestone (0., 0., 0., 0.) );
    milestones222.push_back ( GradientMilestone (1., 1., 1., 1.) );
    mapshape = static_cast<DiagonalCurveEditor*> (curveEditormap->addCurve (CT_Diagonal, M ("TP_RETINEX_CURVEEDITOR_MAP")));
    mapshape->setTooltip (M ("TP_RETINEX_CURVEEDITOR_MAP_TOOLTIP"));
    mapshape->setBottomBarBgGradient (milestones222);
    mapshape->setLeftBarBgGradient (milestones222);
    curveEditormap->curveListComplete();
    maskGrid->attach (*curveEditormap, 0, 1, 1, 1);
    curveEditormap->show();

    // Adjusters
    highlights = Gtk::manage (new Adjuster (M ("TP_SHADOWSHLIGHTS_HIGHLIGHTS"), 0, 100, 1, 0));
    setExpandAlignProperties (highlights, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    maskGrid->attach (*highlights, 0, 2, 1, 1);
    highlights->show();

    h_tonalwidth = Gtk::manage (new Adjuster (M ("TP_SHADOWSHLIGHTS_HLTONALW"), 10, 100, 1, 80));
    setExpandAlignProperties (h_tonalwidth, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    maskGrid->attach (*h_tonalwidth, 0, 3, 1, 1);
    h_tonalwidth->show();

    shadows = Gtk::manage (new Adjuster (M ("TP_SHADOWSHLIGHTS_SHADOWS"), 0, 100, 1, 0));
    setExpandAlignProperties (shadows, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    maskGrid->attach (*shadows, 0, 4, 1, 1);
    shadows->show();

    s_tonalwidth = Gtk::manage (new Adjuster (M ("TP_SHADOWSHLIGHTS_SHTONALW"), 10, 100, 1, 80));
    setExpandAlignProperties (s_tonalwidth, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    maskGrid->attach (*s_tonalwidth, 0, 5, 1, 1);
    s_tonalwidth->show();

    radius = Gtk::manage (new Adjuster (M ("TP_SHADOWSHLIGHTS_RADIUS"), 5, 100, 1, 40));
    setExpandAlignProperties (radius, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    maskGrid->attach (*radius, 0, 6, 1, 1);
    radius->show();

    //-------------

    maskFrame->add (*maskGrid);
    pack_start (*maskFrame, Gtk::PACK_EXPAND_WIDGET, 4);




    // SETTINGS Expander ==============================================================




    expsettings = Gtk::manage(new MyExpander(false, M ("TP_RETINEX_SETTINGS")));
    setExpandAlignProperties (expsettings, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    expsettings->signal_button_release_event().connect_notify ( sigc::bind ( sigc::mem_fun (this, &Retinex::foldAllButMe), expsettings) );

    Gtk::Grid *settingsGrid = Gtk::manage ( new Gtk::Grid());
    setExpandAlignProperties (settingsGrid, false, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    mMLabels = Gtk::manage (new Gtk::Label ("---"));
    setExpandAlignProperties (mMLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    mMLabels->set_tooltip_markup (M ("TP_RETINEX_MLABEL_TOOLTIP"));
    settingsGrid->attach (*mMLabels, 0, 0, 1, 1);
    mMLabels->show ();

    transLabels = Gtk::manage (new Gtk::Label ("---"));
    setExpandAlignProperties (transLabels, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    transLabels->set_tooltip_markup (M ("TP_RETINEX_TLABEL_TOOLTIP"));
    settingsGrid->attach (*transLabels, 0, 1, 1, 1);
    transLabels->show ();

    transLabels2 = Gtk::manage (new Gtk::Label ("---"));
    setExpandAlignProperties (transLabels2, true, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_START);
    settingsGrid->attach (*transLabels2, 0, 2, 1, 1);
    transLabels2->show ();


    // EQUALIZER Frame ----------------------------------------------------------------


    equalFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_EQUAL")));
    equalFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties (equalFrame, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
    equalFrame->set_border_width (5);
#endif
//GTK318

    Gtk::Grid *equalGrid = Gtk::manage (new Gtk::Grid());
    setExpandAlignProperties (equalGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    // Histogram equalizer Lab curve
    curveEditorGD = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_CONTEDIT_LAB"));
    setExpandAlignProperties (curveEditorGD, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    curveEditorGD->setCurveListener (this);
    std::vector<GradientMilestone> milestones22;
    milestones22.push_back ( GradientMilestone (0., 0., 0., 0.) );
    milestones22.push_back ( GradientMilestone (1., 1., 1., 1.) );
    cdshape = static_cast<DiagonalCurveEditor*> (curveEditorGD->addCurve (CT_Diagonal, M ("TP_RETINEX_CURVEEDITOR_CD")));
    cdshape->setTooltip (M ("TP_RETINEX_CURVEEDITOR_CD_TOOLTIP"));
    cdshape->setBottomBarBgGradient (milestones22);
    cdshape->setLeftBarBgGradient (milestones22);
    curveEditorGD->curveListComplete();
    equalGrid->attach (*curveEditorGD, 0, 0, 1, 1);
    curveEditorGD->show();

    // Histogram equalizer HSL curve
    curveEditorGDH = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_CONTEDIT_HSL"));
    setExpandAlignProperties (curveEditorGDH, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    curveEditorGDH->setCurveListener (this);
    std::vector<GradientMilestone> milestones22H;
    milestones22H.push_back ( GradientMilestone (0., 0., 0., 0.) );
    milestones22H.push_back ( GradientMilestone (1., 1., 1., 1.) );
    cdshapeH = static_cast<DiagonalCurveEditor*> (curveEditorGDH->addCurve (CT_Diagonal, M ("TP_RETINEX_CURVEEDITOR_CD")));
    cdshapeH->setTooltip (M ("TP_RETINEX_CURVEEDITOR_CD_TOOLTIP"));
    cdshapeH->setBottomBarBgGradient (milestones22H);
    cdshapeH->setLeftBarBgGradient (milestones22H);
    curveEditorGDH->curveListComplete();
    equalGrid->attach (*curveEditorGDH, 0, 1, 1, 1);
    curveEditorGDH->show();

    // Hue equalizer
    curveEditorGH = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_CONTEDIT_LH"));
    setExpandAlignProperties (curveEditorGH, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    curveEditorGH->setCurveListener (this);
    lhshape = static_cast<FlatCurveEditor*> (curveEditorGH->addCurve (CT_Flat, M ("TP_RETINEX_CURVEEDITOR_LH")));
    lhshape->setTooltip (M ("TP_RETINEX_CURVEEDITOR_LH_TOOLTIP"));
    lhshape->setCurveColorProvider (this, 4);
    milestones.clear();

    for (int i = 0; i < 7; i++) {
        float R, G, B;
        float x = i / 6.0;
        Color::hsv2rgb01 (x, 0.5f, 0.5f, R, G, B);
        milestones.push_back ( GradientMilestone (double (x), double (R), double (G), double (B)) );
    }

    lhshape->setBottomBarBgGradient (milestones);
    curveEditorGH->curveListComplete();
    equalGrid->attach (*curveEditorGH, 0, 2, 1, 1);
    curveEditorGH->show();

    // Gamma settings
    gamgrid = Gtk::manage (new Gtk::Grid ());
    setExpandAlignProperties (gamgrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    labgam = Gtk::manage (new Gtk::Label (M ("TP_RETINEX_GAMMA") + ":"));
    setExpandAlignProperties (labgam, false, false, Gtk::ALIGN_START, Gtk::ALIGN_BASELINE);
    gamgrid->attach (*labgam, 0, 0, 1, 1);

    gammaretinex = Gtk::manage (new MyComboBoxText ());
    setExpandAlignProperties (gammaretinex, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_BASELINE);
    gammaretinex->append (M ("TP_RETINEX_GAMMA_NONE"));
    gammaretinex->append (M ("TP_RETINEX_GAMMA_LOW"));
    gammaretinex->append (M ("TP_RETINEX_GAMMA_MID"));
    gammaretinex->append (M ("TP_RETINEX_GAMMA_HIGH"));
    gammaretinex->append (M ("TP_RETINEX_GAMMA_FREE"));
    gammaretinex->set_active (0);
    gammaretinexConn = gammaretinex->signal_changed().connect ( sigc::mem_fun (*this, &Retinex::gammaretinexChanged) );
    gammaretinex->set_tooltip_markup (M ("TP_RETINEX_GAMMA_TOOLTIP"));
    gamgrid->attach (*gammaretinex, 1, 0, 1, 1);
    equalGrid->attach (*gamgrid, 0, 3, 1, 1);
    gammaretinex->show();

    gam = Gtk::manage (new Adjuster (M ("TP_RETINEX_FREEGAMMA"), 0.6, 3.0, 0.01, 1.30));
    setExpandAlignProperties (gam, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    equalGrid->attach (*gam, 0, 4, 1, 1);
    gam->show ();

    slope = Gtk::manage (new Adjuster (M ("TP_RETINEX_SLOPE"), 1., 20., 0.1, 3.));
    setExpandAlignProperties (slope, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    equalGrid->attach (*slope, 0, 5, 1, 1);
    slope->show ();

    //-------------

    equalFrame->add (*equalGrid);
    settingsGrid->attach (*equalFrame, 0, 3, 1, 1);


    // TONE MAPPING Frame -------------------------------------------------------------


    iterFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_ITERF")));
    iterFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties (iterFrame, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
    iterFrame->set_border_width (5);
#endif
//GTK318

    Gtk::Grid *iterGrid = Gtk::manage (new Gtk::Grid());
    setExpandAlignProperties (iterGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    iter   = Gtk::manage (new Adjuster (M ("TP_RETINEX_ITER"), 1, 5., 1., 1.));
    setExpandAlignProperties (iter, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    iter->set_tooltip_markup (M ("TP_RETINEX_ITER_TOOLTIP"));
    iterGrid->attach (*iter, 0, 0, 1, 1);
    iter->show ();

    scal   = Gtk::manage (new Adjuster (M ("TP_RETINEX_SCALES"), -1, 6., 1., 3.));
    setExpandAlignProperties (scal, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    scal->set_tooltip_markup (M ("TP_RETINEX_SCALES_TOOLTIP"));
    iterGrid->attach (*scal, 0, 1, 1, 1);
    scal->show ();

    grad   = Gtk::manage (new Adjuster (M ("TP_RETINEX_GRAD"), -2., 2., 1., 1.));
    setExpandAlignProperties (grad, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    grad->set_tooltip_markup (M ("TP_RETINEX_GRAD_TOOLTIP"));
    iterGrid->attach (*grad, 0, 2, 1, 1);
    grad->show ();

    grads   = Gtk::manage (new Adjuster (M ("TP_RETINEX_GRADS"), -2., 2., 1., 1.));
    setExpandAlignProperties (grads, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    grads->set_tooltip_markup (M ("TP_RETINEX_GRADS_TOOLTIP"));
    iterGrid->attach (*grads, 0, 3, 1, 1);
    grads->show ();

    //-------------

    iterFrame->add (*iterGrid);
    settingsGrid->attach (*iterFrame, 0, 4, 1, 1);


    // TRANSMISSION Frame -------------------------------------------------------------


    tranFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_TRANF")));
    tranFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties (tranFrame, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
    tranFrame->set_border_width (5);
#endif
//GTK318

    Gtk::Grid *tranGrid = Gtk::manage (new Gtk::Grid());
    setExpandAlignProperties (tranGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);


    // Transmission map curve
    transmissionCurveEditorG = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_TRANSMISSION"));
    setExpandAlignProperties (transmissionCurveEditorG, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    transmissionCurveEditorG->setCurveListener (this);
    transmissionShape = static_cast<FlatCurveEditor*> (transmissionCurveEditorG->addCurve (CT_Flat, "", nullptr, false, false));
    transmissionShape->setIdentityValue (0.);
    transmissionShape->setResetCurve (FlatCurveType (default_params.transmissionCurve.at (0)), default_params.transmissionCurve);
    // transmissionShape->setBottomBarBgGradient(milestones);
    transmissionCurveEditorG->curveListComplete();
    transmissionCurveEditorG->set_tooltip_markup (M ("TP_RETINEX_TRANSMISSION_TOOLTIP"));
    tranGrid->attach ( *transmissionCurveEditorG, 0, 0, 1, 1);
    transmissionCurveEditorG->show();

    // Scale
    skal = Gtk::manage (new Adjuster (M ("TP_RETINEX_SKAL"), 1, 8, 1, 3));
    setExpandAlignProperties (skal, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    tranGrid->attach (*skal, 0, 1, 1, 1);
    skal->show ();

    // Threshold
    limd = Gtk::manage (new Adjuster (M ("TP_RETINEX_THRESHOLD"), 2, 100, 1, 8));
    setExpandAlignProperties (limd, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    limd->set_tooltip_markup (M ("TP_RETINEX_THRESHOLD_TOOLTIP"));
    tranGrid->attach (*limd, 0, 2, 1, 1);
    limd->show ();

    // Transmission median filter
    medianmap = Gtk::manage (new Gtk::CheckButton (M ("TP_RETINEX_MEDIAN")));
    setExpandAlignProperties (medianmap, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    medianmap->set_active (true);
    medianmapConn  = medianmap->signal_toggled().connect ( sigc::mem_fun (*this, &Retinex::medianmapChanged) );
    tranGrid->attach (*medianmap, 0, 3, 1, 1);
    medianmap->show ();

    //-------------

    tranFrame->add (*tranGrid);
    settingsGrid->attach (*tranFrame, 0, 5, 1, 1);


    // GAIN AND OFFSET Frame ----------------------------------------------------------


    gainFrame = Gtk::manage (new Gtk::Frame (M ("TP_RETINEX_GAINOFFS")));
    gainFrame->set_label_align(0.025, 0.5);
    setExpandAlignProperties (gainFrame, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
    gainFrame->set_border_width (5);
#endif
//GTK318

    Gtk::Grid *gainGrid = Gtk::manage (new Gtk::Grid());
    setExpandAlignProperties (gainGrid, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);

    // Gain Transmission map curve
    gaintransmissionCurve = new CurveEditorGroup (options.lastRetinexDir, M ("TP_RETINEX_GAINTRANSMISSION"));
    setExpandAlignProperties (gaintransmissionCurve, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    gaintransmissionCurve->setCurveListener (this);
    gaintransmissionShape = static_cast<FlatCurveEditor*> (gaintransmissionCurve->addCurve (CT_Flat, "", nullptr, false, false));
    gaintransmissionShape->setIdentityValue (0.);
    gaintransmissionShape->setResetCurve (FlatCurveType (default_params.gaintransmissionCurve.at (0)), default_params.gaintransmissionCurve);
    //gaintransmissionShape->setBottomBarBgGradient(milestones);
    gaintransmissionCurve->set_tooltip_markup (M ("TP_RETINEX_GAINTRANSMISSION_TOOLTIP"));
    gaintransmissionCurve->curveListComplete();

    gainGrid->attach ( *gaintransmissionCurve, 0, 0, 1, 1);
    gaintransmissionCurve->show();

    offs   = Gtk::manage (new Adjuster (M ("TP_RETINEX_OFFSET"), -1000, 5000, 1, 0));
    setExpandAlignProperties (offs, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    gainGrid->attach (*offs, 0, 1, 1, 1);
    offs->show ();

    gainFrame->add (*gainGrid);
    settingsGrid->attach (*gainFrame, 0, 6, 1, 1);

    expsettings->add (*settingsGrid, false);
    expsettings->setLevel (2);
    pack_start (*expsettings);

    // End of SETTINGS Expander =======================================================

    // Reset button

    neutral = Gtk::manage (new Gtk::Button (M ("TP_RETINEX_NEUTRAL")));
    setExpandAlignProperties (neutral, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_START);
    RTImage *resetImg = Gtk::manage (new RTImage ("undo-small", Gtk::ICON_SIZE_BUTTON));
    setExpandAlignProperties (resetImg, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER);
    neutral->set_image (*resetImg);
    neutral->set_tooltip_text (M ("TP_RETINEX_NEUTRAL_TOOLTIP"));
    neutralconn = neutral->signal_pressed().connect ( sigc::mem_fun (*this, &Retinex::neutral_pressed) );
    neutral->show();

    //-------------

    pack_start (*neutral);


    // Setting Adjusters'delay


    str->setAdjusterListener (this);
    str->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    scal->setAdjusterListener (this);
    scal->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    iter->setAdjusterListener (this);
    iter->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    grad->setAdjusterListener (this);
    grad->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    grads->setAdjusterListener (this);
    grads->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    gam->setAdjusterListener (this);
    gam->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay) * 2.5f);

    slope->setAdjusterListener (this);
    slope->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay) * 2.5f);

    neigh->setAdjusterListener (this);
    neigh->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    offs->setAdjusterListener (this);
    offs->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    vart->setAdjusterListener (this);
    offs->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    limd->setAdjusterListener (this);
    limd->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    highl->setAdjusterListener (this);
    highl->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    radius->setAdjusterListener (this);
    radius->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    highlights->setAdjusterListener (this);
    highlights->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    h_tonalwidth->setAdjusterListener (this);
    h_tonalwidth->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    shadows->setAdjusterListener (this);
    shadows->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    s_tonalwidth->setAdjusterListener (this);
    s_tonalwidth->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    skal->setAdjusterListener (this);
    skal->setDelay(std::max(options.adjusterMinDelay, options.adjusterMaxDelay));

    disableListener();
    retinexColorSpaceChanged();
    gammaretinexChanged();
    medianmapChanged();
    enableListener();

}

Retinex::~Retinex()
{
    idle_register.destroy();

    delete curveEditorGD;
    delete curveEditorGDH;
    delete transmissionCurveEditorG;
    delete gaintransmissionCurve;
    delete curveEditorGH;
    delete curveEditormap;

}
void Retinex::neutral_pressed ()
{
    neigh->resetValue (false);
    offs->resetValue (false);
    str->resetValue (false);
    scal->resetValue (false);
    iter->resetValue (false);
    grad->resetValue (false);
    grads->resetValue (false);
    vart->resetValue (false);
    limd->resetValue (false);
    highl->resetValue (false);
    gam->resetValue (false);
    skal->resetValue (false);
    slope->resetValue (false);
    highlights->resetValue (false);
    h_tonalwidth->resetValue (false);
    shadows->resetValue (false);
    s_tonalwidth->resetValue (false);
    radius->resetValue (false);
    medianmap->set_active (false);
    mapMethod->set_active (0);
    viewMethod->set_active (0);
    retinexMethod->set_active (2);
    retinexcolorspace->set_active (0);
    gammaretinex->set_active (0);
    transmissionShape->reset();
    gaintransmissionShape->reset();
    cdshape->reset();
    cdshapeH->reset();
    lhshape->reset();
    mapshape->reset();
}

void Retinex::foldAllButMe (GdkEventButton* event, MyExpander *expander)
{
    if (event->button == 3) {
        expsettings->set_expanded (expsettings == expander);
    }
}

void Retinex::writeOptions (std::vector<int> &tpOpen)
{
    tpOpen.push_back (expsettings->get_expanded ());
}

void Retinex::updateToolState(const std::vector<int>& tpOpen)
{
    if (tpOpen.empty()) {
        expsettings->set_expanded(false);

        return;
    }

    if (tpOpen.size() >= 10) {
        expsettings->set_expanded(tpOpen[9]);
    }
}

void Retinex::minmaxChanged (double cdma, double cdmin, double mini, double maxi, double Tmean, double Tsigma, double Tmin, double Tmax)
{
    nextmin = cdmin;
    nextmax = cdma;
    nextminiT = mini;
    nextmaxiT = maxi;
    nextmeanT = Tmean;
    nextsigma = Tsigma;
    nextminT = Tmin;
    nextmaxT = Tmax;

    idle_register.add(
        [this]() -> bool
        {
            GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected
            // FIXME: The above can't be true?!
            disableListener();
            enableListener();
            updateLabel();
            updateTrans();
            return false;
        }
    );
}

void Retinex::updateLabel ()
{
    if (!batchMode) {
        float nX, nY;
        nX = nextmin;
        nY = nextmax;
        {
            mMLabels->set_text (
                Glib::ustring::compose (M ("TP_RETINEX_MLABEL"),
                                        Glib::ustring::format (std::fixed, std::setprecision (0), nX),
                                        Glib::ustring::format (std::fixed, std::setprecision (0), nY))
            );
        }
    }
}

void Retinex::updateTrans ()
{
    if (!batchMode) {
        float nm, nM, nZ, nA, nB, nS;
        nm = nextminiT;
        nM = nextmaxiT;
        nZ = nextmeanT;
        nA = nextminT;
        nB = nextmaxT;
        nS = nextsigma;
        {
            transLabels->set_text (
                Glib::ustring::compose (M ("TP_RETINEX_TLABEL"),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nm),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nM),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nZ),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nS))
            );
            transLabels2->set_text (
                Glib::ustring::compose (M ("TP_RETINEX_TLABEL2"),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nA),
                                        Glib::ustring::format (std::fixed, std::setprecision (1), nB))
            );


        }
    }
}

void Retinex::convertParamToNormal()
{
    const RetinexParams def_params;
    disableListener();
    iter->setValue(def_params.iter);
    viewMethod->set_active(0);
    mapMethod->set_active(0);
    cdshape->reset();
    cdshapeH->reset();
    lhshape->reset();
    transmissionShape->reset();
    medianmap->set_active(def_params.medianmap);
    enableListener();
}

void Retinex::updateGUIToMode(int mode)
{

    if(mode ==0) {
        iterFrame->hide();
        maskFrame->hide();
        equalFrame->hide();
        viewMethod->hide();
        mapMethod->hide();
        transmissionCurveEditorG->hide();
        medianmap->hide();
    } else {
        iterFrame->show();
        maskFrame->show();
        equalFrame->show();
        viewMethod->show();
        transmissionCurveEditorG->show();
        medianmap->show();
        mapMethod->show();
       if (iter->getIntValue() > 1) {
            grad->set_sensitive (true);
            scal->set_sensitive (true);
            grads->set_sensitive (true);
       } else {
            grad->set_sensitive (false);
            scal->set_sensitive (false);
            grads->set_sensitive (false);
       }

    }

}

void Retinex::read (const ProcParams* pp, const ParamsEdited* pedited)
{
    disableListener ();
    retinexMethodConn.block (true);
    retinexColorSpaceConn.block (true);
    gammaretinexConn.block (true);
    mapMethodConn.block (true);
    viewMethodConn.block (true);
    complexmethodconn.block (true);


    if (pedited) {
        scal->setEditedState (pedited->retinex.scal ? Edited : UnEdited);
        iter->setEditedState (pedited->retinex.iter ? Edited : UnEdited);
        grad->setEditedState (pedited->retinex.grad ? Edited : UnEdited);
        grads->setEditedState (pedited->retinex.grads ? Edited : UnEdited);
        neigh->setEditedState (pedited->retinex.neigh ? Edited : UnEdited);
        gam->setEditedState (pedited->retinex.gam ? Edited : UnEdited);
        slope->setEditedState (pedited->retinex.slope ? Edited : UnEdited);
        offs->setEditedState (pedited->retinex.offs ? Edited : UnEdited);
        vart->setEditedState (pedited->retinex.vart ? Edited : UnEdited);
        limd->setEditedState (pedited->retinex.limd ? Edited : UnEdited);
        highl->setEditedState (pedited->retinex.highl ? Edited : UnEdited);
        skal->setEditedState (pedited->retinex.skal ? Edited : UnEdited);
        set_inconsistent (multiImage && !pedited->retinex.enabled);
        medianmap->set_inconsistent (!pedited->retinex.medianmap);
        radius->setEditedState       (pedited->retinex.radius ? Edited : UnEdited);
        highlights->setEditedState   (pedited->retinex.highlights ? Edited : UnEdited);
        h_tonalwidth->setEditedState (pedited->retinex.htonalwidth ? Edited : UnEdited);
        shadows->setEditedState      (pedited->retinex.shadows ? Edited : UnEdited);
        s_tonalwidth->setEditedState (pedited->retinex.stonalwidth ? Edited : UnEdited);

        if (!pedited->retinex.complexmethod) {
            complexmethod->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        if (!pedited->retinex.retinexMethod) {
            retinexMethod->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        if (!pedited->retinex.mapMethod) {
            mapMethod->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        if (!pedited->retinex.viewMethod) {
            viewMethod->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        if (!pedited->retinex.retinexcolorspace) {
            retinexcolorspace->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        if (!pedited->retinex.gammaretinex) {
            gammaretinex->set_active_text (M ("GENERAL_UNCHANGED"));
        }

        cdshape->setUnChanged  (!pedited->retinex.cdcurve);
        cdshapeH->setUnChanged  (!pedited->retinex.cdHcurve);
        transmissionShape->setUnChanged (!pedited->retinex.transmissionCurve);
        gaintransmissionShape->setUnChanged (!pedited->retinex.gaintransmissionCurve);
        lhshape->setUnChanged  (!pedited->retinex.lhcurve);
        mapshape->setUnChanged  (!pedited->retinex.mapcurve);

    }

    neigh->setValue    (pp->retinex.neigh);
    offs->setValue  (pp->retinex.offs);
    str->setValue    (pp->retinex.str);
    scal->setValue      (pp->retinex.scal);
    iter->setValue      (pp->retinex.iter);
    grad->setValue      (pp->retinex.grad);
    grads->setValue      (pp->retinex.grads);
    vart->setValue  (pp->retinex.vart);
    limd->setValue  (pp->retinex.limd);
    gam->setValue      (pp->retinex.gam);
    slope->setValue      (pp->retinex.slope);
    highl->setValue  (pp->retinex.highl);

    radius->setValue        (pp->retinex.radius);
    highlights->setValue    (pp->retinex.highlights);
    h_tonalwidth->setValue  (pp->retinex.htonalwidth);
    shadows->setValue       (pp->retinex.shadows);
    s_tonalwidth->setValue  (pp->retinex.stonalwidth);

    skal->setValue  (pp->retinex.skal);

    if (!batchMode) {
        if (pp->retinex.iter == 1)   {
            grad->set_sensitive (false);
            scal->set_sensitive (false);
            grads->set_sensitive (false);
        } else {
            grad->set_sensitive (true);
            scal->set_sensitive (true);
            grads->set_sensitive (true);
        }
    }

    setEnabled (pp->retinex.enabled);

    medianmap->set_active (pp->retinex.medianmap);
    medianmapConn.block (false);
    lastmedianmap = pp->retinex.medianmap;

    if (pp->retinex.complexmethod == "normal") {
        complexmethod->set_active(0);
    } else if (pp->retinex.complexmethod == "expert") {
        complexmethod->set_active(1);
    }


    if (pp->retinex.retinexMethod == "low") {
        retinexMethod->set_active (0);
    } else if (pp->retinex.retinexMethod == "uni") {
        retinexMethod->set_active (1);
    } else if (pp->retinex.retinexMethod == "high") {
        retinexMethod->set_active (2);
    } else if (pp->retinex.retinexMethod == "highli") {
        retinexMethod->set_active (3);
//    } else if (pp->retinex.retinexMethod == "highliplus") {
//        retinexMethod->set_active (4);
    }

    if (pp->retinex.mapMethod == "none") {
        mapMethod->set_active (0);
//    } else if (pp->retinex.mapMethod == "curv") {
//        mapMethod->set_active (1);
    } else if (pp->retinex.mapMethod == "gaus") {
        mapMethod->set_active (1);
    } else if (pp->retinex.mapMethod == "map") {
        mapMethod->set_active (2);
    } else if (pp->retinex.mapMethod == "mapT") {
        mapMethod->set_active (3);
    }

    if (pp->retinex.viewMethod == "none") {
        viewMethod->set_active (0);
    } else if (pp->retinex.viewMethod == "unsharp") {
        viewMethod->set_active (1);
    } else if (pp->retinex.viewMethod == "mask") {
        viewMethod->set_active (2);
    } else if (pp->retinex.viewMethod == "tran") {
        viewMethod->set_active (3);
    } else if (pp->retinex.viewMethod == "tran2") {
        viewMethod->set_active (4);
    }

    if (pp->retinex.retinexcolorspace == "Lab") {
        retinexcolorspace->set_active (0);
    } else if (pp->retinex.retinexcolorspace == "HSLLOG") {
        retinexcolorspace->set_active (1);
    } else if (pp->retinex.retinexcolorspace == "HSLLIN") {
        retinexcolorspace->set_active (2);
    }

    if (pp->retinex.gammaretinex == "none") {
        gammaretinex->set_active (0);
    } else if (pp->retinex.gammaretinex == "low") {
        gammaretinex->set_active (1);
    } else if (pp->retinex.gammaretinex == "mid") {
        gammaretinex->set_active (2);
    } else if (pp->retinex.gammaretinex == "hig") {
        gammaretinex->set_active (3);
    } else if (pp->retinex.gammaretinex == "fre") {
        gammaretinex->set_active (4);
    }

    retinexMethodChanged ();
    retinexColorSpaceChanged();
    gammaretinexChanged();
    mapMethodChanged ();
    viewMethodChanged ();


    medianmapConn.block (true);
    medianmapChanged ();
    medianmapConn.block (false);

    cdshape->setCurve  (pp->retinex.cdcurve);
    cdshapeH->setCurve  (pp->retinex.cdHcurve);
    lhshape->setCurve  (pp->retinex.lhcurve);
    mapshape->setCurve  (pp->retinex.mapcurve);

    retinexMethodConn.block (false);
    retinexColorSpaceConn.block (false);
    gammaretinexConn.block (false);
    mapMethodConn.block (false);
    viewMethodConn.block (false);
    transmissionShape->setCurve (pp->retinex.transmissionCurve);
    gaintransmissionShape->setCurve (pp->retinex.gaintransmissionCurve);

    complexmethodconn.block (false);

    enableListener ();

    if (complexmethod->get_active_row_number() == 0) {
        updateGUIToMode(0);
       // convertParamToNormal();

    } else {
        updateGUIToMode(1);
    }

}



void Retinex::write (ProcParams* pp, ParamsEdited* pedited)
{

    pp->retinex.str    = str->getValue ();
    pp->retinex.scal      = (int)scal->getValue ();
    pp->retinex.iter      = (int) iter->getValue ();
    pp->retinex.grad      = (int) grad->getValue ();
    pp->retinex.grads      = (int) grads->getValue ();
    pp->retinex.gam      = gam->getValue ();
    pp->retinex.slope      = slope->getValue ();
    pp->retinex.neigh    = neigh->getValue ();
    pp->retinex.offs  = (int)offs->getValue ();
    pp->retinex.vart  = (int)vart->getValue ();
    pp->retinex.limd  = (int)limd->getValue ();
    pp->retinex.highl  = (int)highl->getValue ();
    pp->retinex.skal  = (int)skal->getValue ();
    pp->retinex.cdcurve = cdshape->getCurve ();
    pp->retinex.lhcurve = lhshape->getCurve ();
    pp->retinex.cdHcurve = cdshapeH->getCurve ();
    pp->retinex.mapcurve = mapshape->getCurve ();
    pp->retinex.transmissionCurve = transmissionShape->getCurve ();
    pp->retinex.gaintransmissionCurve = gaintransmissionShape->getCurve ();
    pp->retinex.enabled      = getEnabled();
    pp->retinex.medianmap                = medianmap->get_active();

    pp->retinex.radius        = (int)radius->getValue ();
    pp->retinex.highlights    = (int)highlights->getValue ();
    pp->retinex.htonalwidth   = (int)h_tonalwidth->getValue ();
    pp->retinex.shadows       = (int)shadows->getValue ();
    pp->retinex.stonalwidth   = (int)s_tonalwidth->getValue ();

    if (pedited) {
        pedited->retinex.complexmethod    = complexmethod->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->retinex.retinexMethod    = retinexMethod->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->retinex.retinexcolorspace    = retinexcolorspace->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->retinex.gammaretinex    = gammaretinex->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->retinex.mapMethod    = mapMethod->get_active_text() != M ("GENERAL_UNCHANGED");
        pedited->retinex.viewMethod    = viewMethod->get_active_text() != M ("GENERAL_UNCHANGED");

        //%%%%%%%%%%%%%%%%%%%%%%
        pedited->retinex.str   = str->getEditedState ();
        pedited->retinex.scal     = scal->getEditedState ();
        pedited->retinex.iter     = iter->getEditedState ();
        pedited->retinex.grad     = grad->getEditedState ();
        pedited->retinex.grads     = grads->getEditedState ();
        pedited->retinex.gam     = gam->getEditedState ();
        pedited->retinex.slope     = slope->getEditedState ();
        pedited->retinex.neigh   = neigh->getEditedState ();
        pedited->retinex.offs = offs->getEditedState ();
        pedited->retinex.vart = vart->getEditedState ();
        pedited->retinex.limd = limd->getEditedState ();
        pedited->retinex.highl = highl->getEditedState ();
        pedited->retinex.skal = skal->getEditedState ();
        pedited->retinex.cdcurve   = !cdshape->isUnChanged ();
        pedited->retinex.cdHcurve   = !cdshapeH->isUnChanged ();
        pedited->retinex.transmissionCurve  = !transmissionShape->isUnChanged ();
        pedited->retinex.gaintransmissionCurve  = !gaintransmissionShape->isUnChanged ();
        pedited->retinex.mapcurve   = !mapshape->isUnChanged ();
        pedited->retinex.enabled       = !get_inconsistent();
        pedited->retinex.medianmap       = !medianmap->get_inconsistent();
        pedited->retinex.lhcurve   = !lhshape->isUnChanged ();

        pedited->retinex.radius          = radius->getEditedState ();
        pedited->retinex.highlights      = highlights->getEditedState ();
        pedited->retinex.htonalwidth     = h_tonalwidth->getEditedState ();
        pedited->retinex.shadows         = shadows->getEditedState ();
        pedited->retinex.stonalwidth     = s_tonalwidth->getEditedState ();

    }

    if (complexmethod->get_active_row_number() == 0) {
        pp->retinex.complexmethod = "normal";
    } else if (complexmethod->get_active_row_number() == 1) {
        pp->retinex.complexmethod = "expert";
    }

    if (retinexMethod->get_active_row_number() == 0) {
        pp->retinex.retinexMethod = "low";
    } else if (retinexMethod->get_active_row_number() == 1) {
        pp->retinex.retinexMethod = "uni";
    } else if (retinexMethod->get_active_row_number() == 2) {
        pp->retinex.retinexMethod = "high";
    } else if (retinexMethod->get_active_row_number() == 3) {
        pp->retinex.retinexMethod = "highli";
//    } else if (retinexMethod->get_active_row_number() == 4) {
//        pp->retinex.retinexMethod = "highliplus";
    }

    if (mapMethod->get_active_row_number() == 0) {
        pp->retinex.mapMethod = "none";
//   } else if (mapMethod->get_active_row_number() == 1) {
//       pp->retinex.mapMethod = "curv";
    } else if (mapMethod->get_active_row_number() == 1) {
        pp->retinex.mapMethod = "gaus";
    } else if (mapMethod->get_active_row_number() == 2) {
        pp->retinex.mapMethod = "map";
    } else if (mapMethod->get_active_row_number() == 3) {
        pp->retinex.mapMethod = "mapT";
    }

    if (viewMethod->get_active_row_number() == 0) {
        pp->retinex.viewMethod = "none";
    } else if (viewMethod->get_active_row_number() == 1) {
        pp->retinex.viewMethod = "unsharp";
    } else if (viewMethod->get_active_row_number() == 2) {
        pp->retinex.viewMethod = "mask";
    } else if (viewMethod->get_active_row_number() == 3) {
        pp->retinex.viewMethod = "tran";
    } else if (viewMethod->get_active_row_number() == 4) {
        pp->retinex.viewMethod = "tran2";
    }

    if (retinexcolorspace->get_active_row_number() == 0) {
        pp->retinex.retinexcolorspace = "Lab";
    } else if (retinexcolorspace->get_active_row_number() == 1) {
        pp->retinex.retinexcolorspace = "HSLLOG";
    } else if (retinexcolorspace->get_active_row_number() == 2) {
        pp->retinex.retinexcolorspace = "HSLLIN";
    }

    if (gammaretinex->get_active_row_number() == 0) {
        pp->retinex.gammaretinex = "none";
    } else if (gammaretinex->get_active_row_number() == 1) {
        pp->retinex.gammaretinex = "low";
    } else if (gammaretinex->get_active_row_number() == 2) {
        pp->retinex.gammaretinex = "mid";
    } else if (gammaretinex->get_active_row_number() == 3) {
        pp->retinex.gammaretinex = "hig";
    } else if (gammaretinex->get_active_row_number() == 4) {
        pp->retinex.gammaretinex = "fre";
    }

}

void Retinex::complexmethodChanged()
{

    if (!batchMode) {
        if (complexmethod->get_active_row_number() == 0) {
            updateGUIToMode(0);
            convertParamToNormal();

        } else {
            updateGUIToMode(1);
        }
    }

    if (listener) {
        listener->panelChanged(EvReticomplex, complexmethod->get_active_text());
    }

}



void Retinex::retinexMethodChanged()
{

    if (!batchMode) {
        if (retinexMethod->get_active_row_number() == 3) {
            highl->show();
        } else {
            highl->hide();
        }
    }

    if (listener) {
        listener->panelChanged (EvretinexMethod, retinexMethod->get_active_text ());
    }
}



void Retinex::mapMethodChanged()
{

    if (!batchMode) {
        if (mapMethod->get_active_row_number() == 1  /*|| mapMethod->get_active_row_number() == 2*/) {
            curveEditormap->show();
            highlights->show();
            h_tonalwidth->show();
            shadows->show();
            s_tonalwidth->show();
            radius->show();
        } else if (mapMethod->get_active_row_number() == 2  || mapMethod->get_active_row_number() == 3) {
            curveEditormap->show();
            highlights->show();
            h_tonalwidth->show();
            shadows->show();
            s_tonalwidth->show();
            radius->hide();
        } else {
            curveEditormap->hide();
            highlights->hide();
            h_tonalwidth->hide();
            shadows->hide();
            s_tonalwidth->hide();
            radius->hide();
        }
    }

    if (listener) {
        listener->panelChanged (EvmapMethod, mapMethod->get_active_text ());
    }
}

void Retinex::viewMethodChanged()
{
    if (!batchMode) {
        if (viewMethod->get_active_row_number() == 1 || viewMethod->get_active_row_number() == 2) {
            //vart->hide();
            offs->hide();
            limd->hide();
            transmissionCurveEditorG->hide();
            medianmap->hide();

            iterFrame->hide();
            /*
            iter->hide();
            scal->hide();
            grad->hide();
            grads->hide();
            */

            curveEditorGH->hide();
        } else if (viewMethod->get_active_row_number() == 3 || viewMethod->get_active_row_number() == 4) {
            offs->hide();
            transmissionCurveEditorG->show();
            //vart->hide();
            curveEditorGH->hide();
        } else {
            vart->show();
            neigh->show();
            offs->show();
            limd->show();
            transmissionCurveEditorG->show();
            medianmap->show();
            if (complexmethod->get_active_row_number() == 0) {
                iterFrame->hide();
            } else {
                iterFrame->show();
            }
            /*
            iter->show();
            scal->show();
            grad->show();
            grads->show();
            */

            curveEditorGH->show();
        }
    }

    if (listener) {
        listener->panelChanged (EvviewMethod, viewMethod->get_active_text ());
    }
}



void Retinex::ColorSpaceUpdateUI ()
{
    if (!batchMode) {
        curveEditorGH->show();

        if (retinexcolorspace->get_active_row_number() == 0) {
            curveEditorGD->show();
            curveEditorGDH->hide();
        } else if (retinexcolorspace->get_active_row_number() == 1) {
            curveEditorGD->hide();
            curveEditorGDH->show();
        } else if (retinexcolorspace->get_active_row_number() == 2) {
            curveEditorGD->hide();
            curveEditorGDH->show();
        }
    }
}


void Retinex::retinexColorSpaceChanged()
{
    ColorSpaceUpdateUI();

    if (listener) {
        listener->panelChanged (EvretinexColorSpace, retinexcolorspace->get_active_text ());
    }
}

void Retinex::gammaretinexChanged()
{
    if (!batchMode) {
        if (gammaretinex->get_active_row_number() == 4) {
            gam->show();
            slope->show();
        } else { /*if(gammaretinex->get_active_row_number() != 4)*/
            gam->hide();
            slope->hide();
        }
    }

    ColorSpaceUpdateUI();

    if (listener) {
        listener->panelChanged (Evretinexgamma, gammaretinex->get_active_text ());
    }
}

void Retinex::medianmapChanged ()
{
    if (batchMode) {
        if (medianmap->get_inconsistent()) {
            medianmap->set_inconsistent (false);
            medianmapConn.block (true);
            medianmap->set_active (false);
            medianmapConn.block (false);
        } else if (lastmedianmap) {
            medianmap->set_inconsistent (true);
        }

        lastmedianmap = medianmap->get_active ();
    }

    if (listener) {
        if (medianmap->get_active()) {
            if (getEnabled()) {
                listener->panelChanged (EvRetinexmedianmap, M ("GENERAL_ENABLED"));
            }
        } else {
            if (getEnabled()) {
                listener->panelChanged (EvRetinexmedianmap, M ("GENERAL_DISABLED"));
            }
        }

    }
}


void Retinex::setDefaults (const ProcParams* defParams, const ParamsEdited* pedited)
{

    neigh->setDefault (defParams->retinex.neigh);
    offs->setDefault (defParams->retinex.offs);
    str->setDefault (defParams->retinex.str);
    scal->setDefault (defParams->retinex.scal);
    iter->setDefault (defParams->retinex.iter);
    grad->setDefault (defParams->retinex.grad);
    grads->setDefault (defParams->retinex.grads);
    vart->setDefault (defParams->retinex.vart);
    limd->setDefault (defParams->retinex.limd);
    highl->setDefault (defParams->retinex.highl);
    skal->setDefault (defParams->retinex.skal);
    gam->setDefault (defParams->retinex.gam);
    slope->setDefault (defParams->retinex.slope);

    radius->setDefault (defParams->retinex.radius);
    highlights->setDefault (defParams->retinex.highlights);
    h_tonalwidth->setDefault (defParams->retinex.htonalwidth);
    shadows->setDefault (defParams->retinex.shadows);
    s_tonalwidth->setDefault (defParams->retinex.stonalwidth);

    if (pedited) {
        neigh->setDefaultEditedState (pedited->retinex.neigh ? Edited : UnEdited);
        offs->setDefaultEditedState (pedited->retinex.offs ? Edited : UnEdited);
        str->setDefaultEditedState (pedited->retinex.str ? Edited : UnEdited);
        scal->setDefaultEditedState (pedited->retinex.scal ? Edited : UnEdited);
        iter->setDefaultEditedState (pedited->retinex.iter ? Edited : UnEdited);
        grad->setDefaultEditedState (pedited->retinex.grad ? Edited : UnEdited);
        grads->setDefaultEditedState (pedited->retinex.grads ? Edited : UnEdited);
        vart->setDefaultEditedState (pedited->retinex.vart ? Edited : UnEdited);
        limd->setDefaultEditedState (pedited->retinex.limd ? Edited : UnEdited);
        highl->setDefaultEditedState (pedited->retinex.highl ? Edited : UnEdited);
        skal->setDefaultEditedState (pedited->retinex.skal ? Edited : UnEdited);
        gam->setDefaultEditedState (pedited->retinex.gam ? Edited : UnEdited);
        slope->setDefaultEditedState (pedited->retinex.slope ? Edited : UnEdited);

        radius->setDefaultEditedState       (pedited->retinex.radius ? Edited : UnEdited);
        highlights->setDefaultEditedState   (pedited->retinex.highlights ? Edited : UnEdited);
        h_tonalwidth->setDefaultEditedState (pedited->retinex.htonalwidth ? Edited : UnEdited);
        shadows->setDefaultEditedState      (pedited->retinex.shadows ? Edited : UnEdited);
        s_tonalwidth->setDefaultEditedState (pedited->retinex.stonalwidth ? Edited : UnEdited);

    } else {
        neigh->setDefaultEditedState (Irrelevant);
        offs->setDefaultEditedState (Irrelevant);
        vart->setDefaultEditedState (Irrelevant);
        limd->setDefaultEditedState (Irrelevant);
        highl->setDefaultEditedState (Irrelevant);
        skal->setDefaultEditedState (Irrelevant);
        str->setDefaultEditedState (Irrelevant);
        scal->setDefaultEditedState (Irrelevant);
        iter->setDefaultEditedState (Irrelevant);
        grad->setDefaultEditedState (Irrelevant);
        grads->setDefaultEditedState (Irrelevant);
        gam->setDefaultEditedState (Irrelevant);
        slope->setDefaultEditedState (Irrelevant);

        radius->setDefaultEditedState       (Irrelevant);
        highlights->setDefaultEditedState   (Irrelevant);
        h_tonalwidth->setDefaultEditedState (Irrelevant);
        shadows->setDefaultEditedState      (Irrelevant);
        s_tonalwidth->setDefaultEditedState (Irrelevant);

    }
}

void Retinex::setAdjusterBehavior (bool strAdd, bool neighAdd, bool limdAdd, bool offsAdd, bool vartAdd, bool gamAdd, bool slopeAdd)
{
    str->setAddMode (strAdd);
    neigh->setAddMode (neighAdd);
    limd->setAddMode (limdAdd);
    offs->setAddMode (offsAdd);
    vart->setAddMode (vartAdd);
    gam->setAddMode (gamAdd);
    slope->setAddMode (slopeAdd);
}


void Retinex::adjusterChanged(Adjuster* a, double newval)
{
    if (a == iter && !batchMode) {
        if (iter->getIntValue() > 1) {
            scal->set_sensitive (true);
            grad->set_sensitive (true);
            grads->set_sensitive (true);
        } else {
            scal->set_sensitive (false);
            grad->set_sensitive (false);
            grads->set_sensitive (false);
        }
    }

    if (!listener || !getEnabled()) {
        return;
    }

    if (a == neigh) {
        listener->panelChanged (EvLneigh, neigh->getTextValue());
    } else if (a == str) {
        listener->panelChanged (EvLstr, str->getTextValue());
    } else if (a == scal) {
        listener->panelChanged (EvLscal, scal->getTextValue());
    } else if (a == iter) {
        listener->panelChanged (EvLiter, iter->getTextValue());
    } else if (a == grad) {
        listener->panelChanged (EvLgrad, grad->getTextValue());
    } else if (a == grads) {
        listener->panelChanged (EvLgrads, grads->getTextValue());
    } else if (a == offs) {
        listener->panelChanged (EvLoffs, offs->getTextValue());
    } else if (a == vart) {
        listener->panelChanged (EvLvart, vart->getTextValue());
    } else if (a == limd) {
        listener->panelChanged (EvLlimd, limd->getTextValue());
    } else if (a == highl) {
        listener->panelChanged (EvLhighl, highl->getTextValue());
    } else if (a == skal) {
        listener->panelChanged (EvLskal, skal->getTextValue());
    } else if (a == gam) {
        listener->panelChanged (EvLgam, gam->getTextValue());
    } else if (a == slope) {
        listener->panelChanged (EvLslope, slope->getTextValue());
    } else if (a == highlights) {
        listener->panelChanged (EvLhighlights, highlights->getTextValue());
    } else if (a == h_tonalwidth) {
        listener->panelChanged (EvLh_tonalwidth, h_tonalwidth->getTextValue());
    } else if (a == shadows) {
        listener->panelChanged (EvLshadows, shadows->getTextValue());
    } else if (a ==  s_tonalwidth) {
        listener->panelChanged (EvLs_tonalwidth,  s_tonalwidth->getTextValue());
    } else if (a ==  radius) {
        listener->panelChanged (EvLradius,  radius->getTextValue());

    }
}

void Retinex::autoOpenCurve  ()
{
    cdshape->openIfNonlinear();
    cdshapeH->openIfNonlinear();
    transmissionShape->openIfNonlinear();
    gaintransmissionShape->openIfNonlinear();
    lhshape->openIfNonlinear();
    mapshape->openIfNonlinear();

}


void Retinex::curveChanged (CurveEditor* ce)
{
    if (listener && getEnabled()) {
        if (ce == cdshape) {
            listener->panelChanged (EvLCDCurve, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == cdshapeH) {
            listener->panelChanged (EvLCDHCurve, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == transmissionShape) {
            listener->panelChanged (EvRetinextransmission, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == gaintransmissionShape) {
            listener->panelChanged (EvRetinexgaintransmission, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == lhshape) {
            listener->panelChanged (EvRetinexlhcurve, M ("HISTORY_CUSTOMCURVE"));
        } else if (ce == mapshape) {
            listener->panelChanged (EvRetinexmapcurve, M ("HISTORY_CUSTOMCURVE"));
        }
    }
}

void Retinex::enabledChanged ()
{

    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged (EvRetinexEnabled, M ("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged (EvRetinexEnabled, M ("GENERAL_ENABLED"));
        } else {
            listener->panelChanged (EvRetinexEnabled, M ("GENERAL_DISABLED"));
        }
    }
}


void Retinex::trimValues (rtengine::procparams::ProcParams* pp)
{
    str->trimValue (pp->retinex.str);
    scal->trimValue (pp->retinex.scal);
    iter->trimValue (pp->retinex.iter);
    grad->trimValue (pp->retinex.grad);
    grads->trimValue (pp->retinex.grads);
    neigh->trimValue (pp->retinex.neigh);
    offs->trimValue (pp->retinex.offs);
    vart->trimValue (pp->retinex.vart);
    limd->trimValue (pp->retinex.limd);
    highl->trimValue (pp->retinex.highl);
    gam->trimValue (pp->retinex.gam);
    slope->trimValue (pp->retinex.slope);
    highlights->trimValue (pp->retinex.highlights);
    shadows->trimValue (pp->retinex.shadows);


}

void Retinex::updateCurveBackgroundHistogram(
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
    cdshape->updateBackgroundHistogram(histLRETI);
    cdshapeH->updateBackgroundHistogram(histLRETI);
}

void Retinex::colorForValue (double valX, double valY, enum ColorCaller::ElemType elemType, int callerId, ColorCaller *caller)
{

    float R = 0.f, G = 0.f, B = 0.f;

    if (elemType == ColorCaller::CCET_VERTICAL_BAR) {
        valY = 0.5;
    }

    if (callerId == 1) {         // ch - main curve

        Color::hsv2rgb01 (float (valX), float (valY), 0.5f, R, G, B);
    } else if (callerId == 2) {  // cc - bottom bar

        float value = (1.f - 0.7f) * float (valX) + 0.7f;
        // whole hue range
        // Y axis / from 0.15 up to 0.75 (arbitrary values; was 0.45 before)
        Color::hsv2rgb01 (float (valY), float (valX), value, R, G, B);
    } else if (callerId == 3) {  // lc - bottom bar

        float value = (1.f - 0.7f) * float (valX) + 0.7f;

        // whole hue range
        // Y axis / from 0.15 up to 0.75 (arbitrary values; was 0.45 before)
        Color::hsv2rgb01 (float (valY), float (valX), value, R, G, B);
    } else if (callerId == 4) {  // LH - bottom bar
        Color::hsv2rgb01 (float (valX), 0.5f, float (valY), R, G, B);
    } else if (callerId == 5) {  // HH - bottom bar
        float h = float ((valY - 0.5) * 0.3 + valX);

        if (h > 1.0f) {
            h -= 1.0f;
        } else if (h < 0.0f) {
            h += 1.0f;
        }

        Color::hsv2rgb01 (h, 0.5f, 0.5f, R, G, B);
    }

    caller->ccRed = double (R);
    caller->ccGreen = double (G);
    caller->ccBlue = double (B);
}



void Retinex::setBatchMode (bool batchMode)
{
    ToolPanel::setBatchMode (batchMode);
    neigh->showEditedCB ();
    offs->showEditedCB ();
    str->showEditedCB ();
    scal->showEditedCB ();
    iter->showEditedCB ();
    grad->showEditedCB ();
    grads->showEditedCB ();
    gam->showEditedCB ();
    slope->showEditedCB ();
    vart->showEditedCB ();
    limd->showEditedCB ();
    highl->showEditedCB ();
    radius->showEditedCB ();
    highlights->showEditedCB ();
    h_tonalwidth->showEditedCB ();
    shadows->showEditedCB ();
    s_tonalwidth->showEditedCB ();
  //  complexmethod->append(M("GENERAL_UNCHANGED"));

    skal->showEditedCB ();
    curveEditorGD->setBatchMode (batchMode);
    curveEditorGDH->setBatchMode (batchMode);
    transmissionCurveEditorG->setBatchMode (batchMode);
    gaintransmissionCurve->setBatchMode (batchMode);
    curveEditorGH->setBatchMode (batchMode);
    curveEditormap->setBatchMode (batchMode);


}
