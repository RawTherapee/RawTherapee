/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
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
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 *  2017 Jacques Desmis <jdesmis@gmail.com>
 *  2019 Pierre Cabrera <pierre.cab@gmail.com>
 */
#ifndef _LOCALLAB_H_
#define _LOCALLAB_H_

#pragma once

#include "controlspotpanel.h"
#include "guiutils.h"
#include "tools/locallabtools.h"

/* ==== LocallabToolListListener ==== */
class LocallabToolList;
class LocallabToolListListener
{
public:
    LocallabToolListListener() {};
    virtual ~LocallabToolListListener() {};

    virtual void locallabToolToAdd(const Glib::ustring &toolname) = 0;
};

/* ==== LocallabToolList ==== */
class LocallabToolList:
    public Gtk::Box
{
private:
    // Tree model to manage ComboBox rows
    class ToolRow:
        public Gtk::TreeModel::ColumnRecord
    {
    public:
        Gtk::TreeModelColumn<int> id;
        Gtk::TreeModelColumn<Glib::ustring> name;

        ToolRow()
        {
            add(id);
            add(name);
        }
    };

    // Tool list GUI widgets
    MyComboBox* const list;
    sigc::connection listConn;
    ToolRow toolRow;
    Glib::RefPtr<Gtk::ListStore> listTreeModel;

    // Tool list listener
    LocallabToolListListener* listListener;

public:
    LocallabToolList();

    // Setter for tool list listener
    void setLocallabToolListListener(LocallabToolListListener* ltll)
    {
        listListener = ltll;
    }

    // Tool list management function
    void addToolRow(const Glib::ustring &toolname, const int id);
    void removeToolRow(const Glib::ustring &toolname);
    void removeAllTool();

private:
    // Tool list event management function
    void toolRowSelected();
};

/* ==== Locallab ==== */
class Locallab :
    public ToolParamBlock,
    public FoldableToolPanel,
    public rtengine::LocallabListener,
    public ControlPanelListener,
    public LocallabToolListener,
    public LocallabToolListListener
{
private:
    // Spot control panel widget
    ControlSpotPanel* const expsettings;

    // Tool list widget
    LocallabToolList* const toollist;

    // Locallab tool widgets
    LocallabColor expcolor;
    LocallabExposure expexpose;
    LocallabShadow expshadhigh;
    LocallabVibrance expvibrance;
    LocallabSoft expsoft;
    LocallabBlur expblur;
    LocallabTone exptonemap;
    LocallabRetinex expreti;
    LocallabSharp expsharp;
    LocallabContrast expcontrast;
    LocallabCBDL expcbdl;
    LocallabLog explog;
    LocallabMask expmask;
    Locallabcie expcie;

    OptionalRadioButtonGroup delta_e_preview_button_group;

    std::vector<LocallabTool*> locallabTools;

    // Locallab tools mask background management data
    std::vector<locallabRetiMinMax> retiMinMax;

    // Locallab tools mask background management data
    std::vector<locallabDenoiseLC> denoiselc;
    std::vector<locallabDenoiseMAD> madllc;

    std::vector<locallabDenoiseLC2> denoiselc2;
    std::vector<locallabcieBEF> cie_bef;

    std::vector<locallabsharBEF> shar_bef;
    std::vector<locallabsharAFT> shar_aft;

    std::vector<locallabcieLC> cie_lc;

    std::vector<locallabshGHSbw2> sh_ghsbw2;

    std::vector<locallabshGHSbw> sh_ghsbw;

    std::vector<locallabshMICHbw> sh_michbw;

    std::vector<locallabsetLC> set_lc;

    std::vector<locallabcieSIG> cie_sig;

    std::vector<locallabcieFIN> cie_fin;

    // Locallab tools mask background management data
    std::vector<locallabRef> maskBackRef;

    // Other widgets
    //Gtk::Button* const resetshowButton;

    Glib::ustring spotName;

public:
    static const Glib::ustring TOOL_NAME;

    Locallab();

    // FoldableToolPanel management functions
    void read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited = nullptr) override;
    void setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited = nullptr) override;
    void setListener(ToolPanelListener* tpl) override;

    // Locallab Retinex tool min/man management function
    void minmaxChanged(const std::vector<locallabRetiMinMax> &minmax, int selspot) override;
    
    // new functions for global - normal use
//    void mainChanged(int spottype, int selspot, bool iscolor, bool issh, bool isvib, bool isexpos, bool issoft, bool isblur, bool istom, bool isret, bool issharp, bool iscont, bool iscbdl, bool islog, bool ismas, bool isci)override;
    void scopeChangedcol(int scope, int selspot, bool enab)override;
    void scopeChangedsh(int scope, int selspot, bool enab)override;
    void scopeChangedvib(int scope, int selspot, bool enab)override;
    void scopeChangedset(int scope, int selspot, bool enab)override;
    
    void maiChanged(const std::vector<locallabsetLC> &setlc, int selspot) override;
    
    //Locallab denoise 
    // Locallab Retinex tool min/man management function
    void denChanged(const std::vector<locallabDenoiseLC> &denlc, int selspot) override;
    void madChanged(const std::vector<locallabDenoiseMAD> &madlc, int selspot) override;
    void den2Changed(const std::vector<locallabDenoiseLC2> &denlc2, int selspot) override;
   
    // Locallab CIE tool primaries function
    void cieChanged(const std::vector<locallabcieLC> &cielc, int selspot) override;

    // Locallab SH GHS tool Black point & White point GHS function && Michaelis White and Black point
    void ghsbwChanged(const std::vector<locallabshGHSbw> &shghsbw, int selspot) override;//GHS
    void ghsbw2Changed(const std::vector<locallabshGHSbw2> &shghsbw2, int selspot) override;//GHS
    void michbwChanged(const std::vector<locallabshMICHbw> &shmichbw, int selspot) override;//Michaelis

    // Locallab Log Encoding and Cam16 autocompute function
    void ciebefChanged(const std::vector<locallabcieBEF> &ciebef, int selspot) override;

    // Locallab Sharp
    void sharbefChanged(const std::vector<locallabsharBEF> &sharbef, int selspot) override;
    void sharaftChanged(const std::vector<locallabsharAFT> &sharaft, int selspot) override;

    // Locallab CIE
    void sigChanged(const std::vector<locallabcieSIG> &ciesig, int selspot) override;
    void finChanged(const std::vector<locallabcieFIN> &ciefin, int selspot) override;


    // Locallab tools mask background management function
//    void refChanged(const std::vector<locallabRef> &ref, int selspot) override;
    void refChanged2(float *huerefp, float *chromarefp, float *lumarefp, float *fabrefp, int selspot)override;

    // Mask visibility management functions
    struct llMaskVisibility {
        bool previewDeltaE;
        int colorMask;
        int colorMaskinv;
        int expMask;
        int expMaskinv;
        int SHMask;
        int SHMaskinv;
        int vibMask;
        int softMask;
        int blMask;
        int tmMask;
        int retiMask;
        int sharMask;
        int lcMask;
        int cbMask;
        int logMask;
        int maskMask;
        int cieMask;
    };

    void resetMaskVisibility();
    llMaskVisibility getMaskVisibility() const;

    // Other widgets event functions
    //void resetshowPressed();

    // EditProvider management function
    void setEditProvider(EditDataProvider* provider) override;
    void subscribe();
    void unsubscribe();

    // FoldableToolPanel event function
    void enabledChanged() override;

    // Curve management function
    void autoOpenCurve() override;

    // Locallab tools expanders management functions
    void foldAllButOne(LocallabTool* except);
    void openAllTools();

    // Locallab tools advice tooltips management function
    void updateShowtooltipVisibility(bool showtooltip);

private:
    // Locallab tools management functions
    void addTool(Gtk::Box* where, LocallabTool* tool);

    // Locallab GUI management function
    void setParamEditable(bool cond);

    // ControlSpotListener function
    void resetToolMaskView() override;
    void spotNameChanged(const Glib::ustring &newName) override;

    // LocallabToolListener function
    void resetOtherMaskView(LocallabTool* current) override;
    void toolRemoved(LocallabTool* current) override;

    // LocallabToolListListener function
    void locallabToolToAdd(const Glib::ustring &toolname) override;
};

#endif
