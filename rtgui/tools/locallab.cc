/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as publishfed by
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
#include "locallab.h"

#include "options.h"
#include "rtengine/procparams.h"

using namespace rtengine;
using namespace procparams;

const Glib::ustring Locallab::TOOL_NAME = "locallab";

/* ==== LocallabToolList ==== */
LocallabToolList::LocallabToolList():
    // Tool list GUI elements
    list(Gtk::manage(new MyComboBox())),
    listTreeModel(Gtk::ListStore::create(toolRow)),

    // Tool list listener
    listListener(nullptr)
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    list->set_model(listTreeModel);
    list->pack_start(toolRow.name);
    listConn = list->signal_changed().connect(sigc::mem_fun(*this, &LocallabToolList::toolRowSelected));
    list->set_tooltip_text(M("TP_LOCALLAB_LIST_TOOLTIP"));
    // Append title row to list
    // Important: Title row shall always be the first one
    const auto titleRow = *(listTreeModel->append());
    titleRow[toolRow.id] = 0;
    titleRow[toolRow.name] = M("TP_LOCALLAB_LIST_NAME");
    listConn.block(true);
    list->set_active(titleRow);
    listConn.block(false);

    // Add ComboBox to LocallabToolList widget
    add(*list);
}

void LocallabToolList::addToolRow(const Glib::ustring &toolname, const int id)
{
    // Disable event management
    listConn.block(true);

    // Add tool name according to id
    Gtk::TreeIter insertAfter;

    for (auto &r : listTreeModel->children()) {
        if (r[toolRow.id] < id) {
            insertAfter = *r; // Tool name shall be added at least after this row
        } else {
            break; // Tool name shall be added before this row
        }
    }

    // Note: There is always at list one row (i.e. title one)

    const auto newRow = *(listTreeModel->insert_after(insertAfter));
    newRow[toolRow.id] = id;
    newRow[toolRow.name] = toolname;

    // Select title row (i.e. always first row)
    list->set_active(0);

    // Enable event management
    listConn.block(false);
}

void LocallabToolList::removeToolRow(const Glib::ustring &toolname)
{
    // Disable event management
    listConn.block(true);

    // Remove tool name row
    for (auto &r : listTreeModel->children()) {
        if (r[toolRow.name] == toolname) {
            listTreeModel->erase(*r);
            break;
        }
    }

    // Select title row (i.e. always first row)
    list->set_active(0);

    // Enable event management
    listConn.block(false);
}

void LocallabToolList::removeAllTool()
{
    // Disable event management
    listConn.block(true);

    // Remove all tools
    listTreeModel->clear();

    // Add title row again
    const auto titleRow = *(listTreeModel->append());
    titleRow[toolRow.id] = 0;
    titleRow[toolRow.name] = M("TP_LOCALLAB_LIST_NAME");

    // Select title row (i.e. always first row)
    list->set_active(0);

    // Enable event management
    listConn.block(false);
}

void LocallabToolList::toolRowSelected()
{
    // Get selected tool name
    const auto selRow = *(list->get_active());
    const Glib::ustring toolname = selRow[toolRow.name];

    // Remove selected tool name for ComboBox
    removeToolRow(toolname);

    // Warm tool list listener
    if (listListener) {
        listListener->locallabToolToAdd(toolname);
    }
}

/* ==== Locallab ==== */
Locallab::Locallab():
    FoldableToolPanel(this, TOOL_NAME, M("TP_LOCALLAB_LABEL"), false, true),

    // Spot control panel widget
    expsettings(Gtk::manage(new ControlSpotPanel())),

    // Tool list widget
    toollist(Gtk::manage(new LocallabToolList()))

    // Other widgets
    //resetshowButton(Gtk::manage(new Gtk::Button(M("TP_LOCALLAB_RESETSHOW"))))
{
    set_orientation(Gtk::ORIENTATION_VERTICAL);
    
    // Create panel widget to receive Locallab GUI elements
    ToolVBox* const panel = Gtk::manage(new ToolVBox());
    panel->set_spacing(2);

    // Add spot control panel to panel widget
    expsettings->setControlPanelListener(this);
    expsettings->setLevel(2);
    panel->pack_start(*expsettings->getExpander(), false, false);

    // Add separator
    Gtk::Separator* const separator = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    panel->pack_start(*separator, false, false);

    // Add tool list widget
    toollist->setLocallabToolListListener(this);
    panel->pack_start(*toollist, false, false);

    // Add all the tools' preview delta E buttons to one group.
    for (auto button : {
             expcolor.getPreviewDeltaEButton(),
             expexpose.getPreviewDeltaEButton(),
             expshadhigh.getPreviewDeltaEButton(),
             expvibrance.getPreviewDeltaEButton(),
             expsoft.getPreviewDeltaEButton(),
             expblur.getPreviewDeltaEButton(),
             exptonemap.getPreviewDeltaEButton(),
             expreti.getPreviewDeltaEButton(),
             expsharp.getPreviewDeltaEButton(),
             expcontrast.getPreviewDeltaEButton(),
             expcbdl.getPreviewDeltaEButton(),
             explog.getPreviewDeltaEButton(),
             expmask.getPreviewDeltaEButton(),
             expcie.getPreviewDeltaEButton(),
         }) {
        if (button) {
            delta_e_preview_button_group.register_button(*button);
        }
    }

    // Add Locallab tools to panel widget
    ToolVBox* const toolpanel = Gtk::manage(new ToolVBox());
    toolpanel->set_name("LocallabToolPanel");
    addTool(toolpanel, &expcolor);
    addTool(toolpanel, &expshadhigh);
    addTool(toolpanel, &expvibrance);
    addTool(toolpanel, &explog);
    addTool(toolpanel, &expcie);
    addTool(toolpanel, &expexpose);
    addTool(toolpanel, &expmask);
    addTool(toolpanel, &expsoft);
    addTool(toolpanel, &expblur);
    addTool(toolpanel, &exptonemap);
    addTool(toolpanel, &expreti);
    addTool(toolpanel, &expsharp);
    addTool(toolpanel, &expcontrast);
    addTool(toolpanel, &expcbdl);
    panel->pack_start(*toolpanel, false, false);

    // Add separator
 //   Gtk::Separator* const separator2 = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
 //   panel->pack_start(*separator2, false, false);

    // Add mask reset button to panel widget
    //resetshowButton->signal_pressed().connect(sigc::mem_fun(*this, &Locallab::resetshowPressed));
   // panel->pack_start(*resetshowButton);

    // Add panel widget to Locallab GUI
    pack_start(*panel);

    // Show all widgets
    show_all();

    // Update Locallab tools advice tooltips visibility based on saved option
    for (auto tool : locallabTools) {
        tool->updateAdviceTooltips(App::get().options().showtooltip);
    }

    // By default, if no photo is loaded, all Locallab tools are removed and it's not possible to add them
    // (to be necessary called after "show_all" function)
    setParamEditable(false);
}

void Locallab::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
    // Disable all listeners
    disableListener();

    // Update Locallab activation state
    setEnabled(pp->locallab.enabled);
 //   const int showsettings = options.complexity;

    // Transmit Locallab activation state to Locallab tools
    for (auto tool : locallabTools) {
        tool->isLocallabActivated(exp->getEnabled());
    }

    // TODO Manage it with read function in controlspotpanel.cc
    // Delete all existent spots
    const int spotNb = expsettings->getSpotNumber();

    for (int i = spotNb - 1; i >= 0; i--) {
        expsettings->deleteControlSpot(i);
    }

    // TODO Manage it with read function in controlspotpanel.cc
    // Add existent spots based on pp
    ControlSpotPanel::SpotRow r;

    for (int i = 0; i < (int)pp->locallab.spots.size(); i++) {
       
        r.name = pp->locallab.spots.at(i).name;
        r.isvisible = pp->locallab.spots.at(i).isvisible;

        if (pp->locallab.spots.at(i).shape == "ELI") {
            r.shape = 0;
        } else if (pp->locallab.spots.at(i).shape == "RECT")  {
            r.shape = 1;
        }

        if (pp->locallab.spots.at(i).prevMethod == "hide") {
            r.prevMethod = 0;
        } else {
            r.prevMethod = 1;
        }

        if (pp->locallab.spots.at(i).spotMethod == "norm") {
            r.spotMethod = 0;
        } else if(pp->locallab.spots.at(i).spotMethod == "exc"){
            r.spotMethod = 1;
        } else if (pp->locallab.spots.at(i).spotMethod == "full"){
            r.spotMethod = 2;
        } else if (pp->locallab.spots.at(i).spotMethod == "main"){
            r.spotMethod = 3;
        }
        
        r.sensiexclu = pp->locallab.spots.at(i).sensiexclu;
        r.structexclu = pp->locallab.spots.at(i).structexclu;

        if (pp->locallab.spots.at(i).shapeMethod == "IND") {
            r.shapeMethod = 0;
        } else if (pp->locallab.spots.at(i).shapeMethod == "SYM") {
            r.shapeMethod = 1;
        } else if (pp->locallab.spots.at(i).shapeMethod == "INDSL") {
            r.shapeMethod = 2;
        } else {
            r.shapeMethod = 3;
        }
		
        if (pp->locallab.spots.at(i).avoidgamutMethod == "NONE") {
            r.avoidgamutMethod = 0;
        } else if (pp->locallab.spots.at(i).avoidgamutMethod == "LAB") {
            r.avoidgamutMethod = 1;
        } else if (pp->locallab.spots.at(i).avoidgamutMethod == "XYZ") {
            r.avoidgamutMethod= 2;
        } else if (pp->locallab.spots.at(i).avoidgamutMethod == "XYZREL") {
            r.avoidgamutMethod= 3;
        } else if (pp->locallab.spots.at(i).avoidgamutMethod == "MUNS") {
            r.avoidgamutMethod= 4;
        } 

        r.locX = pp->locallab.spots.at(i).loc.at(0);
        r.locXL = pp->locallab.spots.at(i).loc.at(1);
        r.locY = pp->locallab.spots.at(i).loc.at(2);
        r.locYT = pp->locallab.spots.at(i).loc.at(3);
        r.centerX = pp->locallab.spots.at(i).centerX;
        r.centerY = pp->locallab.spots.at(i).centerY;
        r.rotation = pp->locallab.spots.at(i).spotangle;
        r.circrad = pp->locallab.spots.at(i).circrad;

        if (pp->locallab.spots.at(i).qualityMethod == "enh") {
            r.qualityMethod = 0;
        } else {
            r.qualityMethod = 1;
        }

        r.transit = pp->locallab.spots.at(i).transit;
        r.transitweak = pp->locallab.spots.at(i).transitweak;
        r.transitgrad = pp->locallab.spots.at(i).transitgrad;
        r.feather = pp->locallab.spots.at(i).feather;
        r.struc = pp->locallab.spots.at(i).struc;
        r.thresh = pp->locallab.spots.at(i).thresh;
        r.iter = pp->locallab.spots.at(i).iter;
        r.balan = pp->locallab.spots.at(i).balan;
        r.balanh = pp->locallab.spots.at(i).balanh;
        r.colorde = pp->locallab.spots.at(i).colorde;
        r.colorscope = pp->locallab.spots.at(i).colorscope;
        r.avoidrad = pp->locallab.spots.at(i).avoidrad;
        r.hishow = pp->locallab.spots.at(i).hishow;
        r.activ = pp->locallab.spots.at(i).activ;
        r.avoidneg = pp->locallab.spots.at(i).avoidneg;
        r.blwh = pp->locallab.spots.at(i).blwh;
        r.recurs = pp->locallab.spots.at(i).recurs;
        r.laplac = true; //pp->locallab.spots.at(i).laplac;
        r.deltae = pp->locallab.spots.at(i).deltae;
        r.scopemask = pp->locallab.spots.at(i).scopemask;
        r.denoichmask = pp->locallab.spots.at(i).denoichmask;
        r.shortc = pp->locallab.spots.at(i).shortc;
        r.lumask = pp->locallab.spots.at(i).lumask;
        //r.savrest = pp->locallab.spots.at(i).savrest;

        if (pp->locallab.spots.at(i).complexMethod == "sim") {
            r.complexMethod = 0;
        } else  if (pp->locallab.spots.at(i).complexMethod == "mod") {
            r.complexMethod = 1;
        } else  if (pp->locallab.spots.at(i).complexMethod == "all") {
            r.complexMethod = 2;
        }

        if (pp->locallab.spots.at(i).wavMethod == "D2") {
            r.wavMethod = 0;
        } else  if (pp->locallab.spots.at(i).wavMethod == "D4") {
            r.wavMethod = 1;
        } else  if (pp->locallab.spots.at(i).wavMethod == "D6") {
            r.wavMethod = 2;
        } else  if (pp->locallab.spots.at(i).wavMethod == "D10") {
            r.wavMethod = 3;
        } else  if (pp->locallab.spots.at(i).wavMethod == "D14") {
            r.wavMethod = 4;
        } else  if (pp->locallab.spots.at(i).wavMethod == "D20") {
            r.wavMethod = 5;
        }

        expsettings->addControlSpot(r);
    }

    // Select active spot
    if (pp->locallab.spots.size() > 0) {
        expsettings->setSelectedSpot(pp->locallab.selspot);
        spotName = pp->locallab.spots.at(pp->locallab.selspot).name;
    }

    // Update each Locallab tools GUI
    for (auto tool : locallabTools) {
        tool->read(pp, pedited);
    }

    // Update tool list widget
    int toolNb = 0;
    toollist->removeAllTool(); // Reset Locallab list firstly

    for (auto tool : locallabTools) {
        toolNb++;

        if (!tool->isLocallabToolAdded()) {
            toollist->addToolRow(tool->getToolName(), toolNb);
        }
    }

    // Specific case: if there is no spot, GUI isn't anymore editable (i.e. Locallab tool cannot be managed)
    if (pp->locallab.spots.size() > 0) {
        setParamEditable(true);
    } else {
        setParamEditable(false);
    }

    // Enable all listeners
    enableListener();

    // Note: No need to manage pedited as batch mode is deactivated for Locallab
}

void Locallab::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
    // Update Locallab activation state
    pp->locallab.enabled = getEnabled();

    // Transmit Locallab activation state to Locallab tools (in case of updated)
    for (auto tool : locallabTools) {
        tool->isLocallabActivated(exp->getEnabled());
    }

    const int spotPanelEvent = expsettings->getEventType();
    int spotIndex;
    rtengine::procparams::LocallabParams::LocallabSpot* newSpot;

    int imW, imH; // Size of image
    int prW, prH; // Size of preview area
    int prX, prY; // Coord of preview area center
    EditDataProvider* const provider = expsettings->getEditProvider();

    int toolNb;

    switch (spotPanelEvent) {
        case (ControlSpotPanel::SpotCreation): { // Spot creation event
            // Spot creation (default initialization)
            newSpot = new LocallabParams::LocallabSpot();
            ControlSpotPanel::SpotRow r;
            r.name = newSpot->name = M("TP_LOCALLAB_SPOTNAME");
            r.isvisible = newSpot->isvisible;

            if (newSpot->shape == "ELI") {
                r.shape = 0;
            } else if (newSpot->shape == "RECT"){
                r.shape = 1;
            }

            if (newSpot->prevMethod == "hide") {
                r.prevMethod = 0;
            } else {
                r.prevMethod = 1;
            }


            if (newSpot->spotMethod == "norm") {
                r.spotMethod = 0;
            } else if(newSpot->spotMethod == "exc") {
                r.spotMethod = 1;
            } else if(newSpot->spotMethod == "full") {
                r.spotMethod = 2;
            } else if(newSpot->spotMethod == "main") {
                r.spotMethod = 3;
            }

            r.sensiexclu = newSpot->sensiexclu;
            r.structexclu = newSpot->structexclu;

            if (newSpot->shapeMethod == "IND") {
                r.shapeMethod = 0;
            } else if (newSpot->shapeMethod == "SYM") {
                r.shapeMethod = 1;
            } else if (newSpot->shapeMethod == "INDSL") {
                r.shapeMethod = 2;
            } else {
                r.shapeMethod = 3;
            }

            if (newSpot->avoidgamutMethod == "NONE") {
                r.avoidgamutMethod = 0;
            } else if (newSpot->avoidgamutMethod == "LAB") {
                r.avoidgamutMethod = 1;
            } else if (newSpot->avoidgamutMethod == "XYZ") {
                r.avoidgamutMethod = 2;
            } else if (newSpot->avoidgamutMethod == "XYZREL") {
                r.avoidgamutMethod = 3;
            } else if (newSpot->avoidgamutMethod == "MUNS") {
                r.avoidgamutMethod = 4;
            }

            // Calculate spot size and center position according to preview area
            if (provider && !batchMode) {
                provider->getImageSize(imW, imH);
                provider->getPreviewCenterPos(prX, prY);
                provider->getPreviewSize(prW, prH);

                if (imW && imH) { // Image loaded
                    // Spot center position computation
                    newSpot->centerX = rtengine::LIM(int(int((double)prX - (double)imW / 2.) * 2000. / (double)imW), -1000, 1000);
                    newSpot->centerY = rtengine::LIM(int(int((double)prY - (double)imH / 2.) * 2000. / (double)imH), -1000, 1000);
                    // Ellipse/rectangle size computation
                    newSpot->loc.at(0) = rtengine::LIM(int(((double)prW / 2. - 5.) * 2000. / (double)imW), 2, newSpot->loc.at(0));
                    newSpot->loc.at(1) = rtengine::LIM(int(((double)prW / 2. - 5.) * 2000. / (double)imW), 2, newSpot->loc.at(1));
                    newSpot->loc.at(2) = rtengine::LIM(int(((double)prH / 2. - 5.) * 2000. / (double)imH), 2, newSpot->loc.at(2));
                    newSpot->loc.at(3) = rtengine::LIM(int(((double)prH / 2. - 5.) * 2000. / (double)imH), 2, newSpot->loc.at(3));
                }
            }

            r.locX = newSpot->loc.at(0);
            r.locXL = newSpot->loc.at(1);
            r.locY = newSpot->loc.at(2);
            r.locYT = newSpot->loc.at(3);
            r.centerX = newSpot->centerX;
            r.centerY = newSpot->centerY;

            r.rotation = newSpot->spotangle;
            r.circrad = newSpot->circrad;

            if (newSpot->qualityMethod == "enh") {
                r.qualityMethod = 0;
            } else {
                r.qualityMethod = 1;
            }

            r.transit = newSpot->transit;
            r.transitweak = newSpot->transitweak;
            r.transitgrad = newSpot->transitgrad;
            r.feather = newSpot->feather;
            r.struc = newSpot->struc;
            r.thresh = newSpot->thresh;
            r.iter = newSpot->iter;
            r.balan = newSpot->balan;
            r.balanh = newSpot->balanh;
            r.colorde = newSpot->colorde;
            r.colorscope = newSpot->colorscope;
            r.avoidrad = newSpot->avoidrad;
            r.hishow = newSpot->hishow;
            r.activ = newSpot->activ;
            r.avoidneg = newSpot->avoidneg;
            r.blwh = newSpot->blwh;
            r.recurs = newSpot->recurs;
            r.laplac = newSpot->laplac;
            r.deltae = newSpot->deltae;
            r.scopemask = newSpot->scopemask;
            r.denoichmask = newSpot->denoichmask;
            r.shortc = newSpot->shortc;
            r.lumask = newSpot->lumask;
            //r.savrest = newSpot->savrest;

            if (newSpot->complexMethod == "sim") {
                r.complexMethod = 0;
            } else  if (newSpot->complexMethod == "mod") {
                r.complexMethod = 1;
            } else  if (newSpot->complexMethod == "all") {
                r.complexMethod = 2;
            }

            if (newSpot->wavMethod == "D2") {
                r.wavMethod = 0;
            } else if (newSpot->wavMethod == "D4") {
                r.wavMethod = 1;
            } else if (newSpot->wavMethod == "D6") {
                r.wavMethod = 2;
            } else if (newSpot->wavMethod == "D10") {
                r.wavMethod = 3;
            } else if (newSpot->wavMethod == "D14") {
                r.wavMethod = 4;
            } else if (newSpot->wavMethod == "D20") {
                r.wavMethod = 5;
            }

            expsettings->addControlSpot(r);

            // ProcParams update
            pp->locallab.spots.push_back(*newSpot);
            pp->locallab.selspot = pp->locallab.spots.size() - 1;

            // New created spot selection
            expsettings->setSelectedSpot(pp->locallab.selspot);

            // Update Locallab tools GUI with new created spot
            disableListener();

            spotName = pp->locallab.spots.at(pp->locallab.selspot).name;

            for (auto tool : locallabTools) {
                tool->read(pp, pedited);
            }

            enableListener();

            // Update tool list widget
            toolNb = 0;
            toollist->removeAllTool(); // Reset Locallab list firstly

            for (auto tool : locallabTools) {
                toolNb++;

                if (!tool->isLocallabToolAdded()) {
                    toollist->addToolRow(tool->getToolName(), toolNb);
                }
            }

            if (pp->locallab.spots.size() == 1) {
                setParamEditable(true);
            }

            // Update default values according to selected spot
            setDefaults(pp, pedited);

            // Note: No need to manage pedited as batch mode is deactivated for Locallab

            break;
        }

        case (ControlSpotPanel::SpotDeletion): // Spot deletion event
            // Get deleted spot index in ProcParams and update it
            spotIndex = expsettings->getSelectedSpot();

            for (int i = 0; i < (int)pp->locallab.spots.size(); i++) {
                if (i == spotIndex) {
                    // ProcParams update
                    pp->locallab.spots.erase(pp->locallab.spots.begin() + i);
                    expsettings->deleteControlSpot(spotIndex);

                    // Select the first remaining spot before deleted one
                    if (pp->locallab.spots.size() > 0) {
                        for (int j = i - 1; j >= 0; j--) {
                            if (expsettings->setSelectedSpot(j)) { // True if an existing spot has been selected on controlspotpanel
                                pp->locallab.selspot = j;

                                break;
                            }
                        }
                    } else {
                        // Reset selspot
                        pp->locallab.selspot = 0;
                    }

                    // Update Locallab tools GUI with selected spot
                    disableListener();

                    if (pp->locallab.spots.size() > 0) {
                        spotName = pp->locallab.spots.at(pp->locallab.selspot).name;
                    }

                    for (auto tool : locallabTools) {
                        tool->read(pp, pedited);
                    }

                    enableListener();

                    // Update tool list widget
                    toolNb = 0;
                    toollist->removeAllTool(); // Reset Locallab list firstly

                    for (auto tool : locallabTools) {
                        toolNb++;

                        if (!tool->isLocallabToolAdded()) {
                            toollist->addToolRow(tool->getToolName(), toolNb);
                        }
                    }

                    if (pp->locallab.spots.size() == 0) {
                        setParamEditable(false);
                    }

                    // Update default values according to selected spot
                    setDefaults(pp, pedited);

                    // Note: No need to manage pedited as batch mode is deactivated for Locallab

                    break;
                }
            }

            break;

        case (ControlSpotPanel::SpotSelection):  // Spot selection event
            pp->locallab.selspot = expsettings->getSelectedSpot();

            // Update control spots and Locallab tools GUI with selected spot
            expsettings->setSelectedSpot(pp->locallab.selspot);
            disableListener();

            if (pp->locallab.spots.size() > 0) {
                spotName = pp->locallab.spots.at(pp->locallab.selspot).name;
            }

            for (auto tool : locallabTools) {
                tool->read(pp, pedited);
            }

            enableListener();

            // Update tool list widget
            toolNb = 0;
            toollist->removeAllTool(); // Reset Locallab list firstly

            for (auto tool : locallabTools) {
                toolNb++;

                if (!tool->isLocallabToolAdded()) {
                    toollist->addToolRow(tool->getToolName(), toolNb);
                }
            }
/*
            // Update locallab tools mask background
            if (pp->locallab.selspot < (int)maskBackRef.size()) {
                const double huer = maskBackRef.at(pp->locallab.selspot).huer;
                const double lumar = maskBackRef.at(pp->locallab.selspot).lumar;
                const double chromar = maskBackRef.at(pp->locallab.selspot).chromar;
                const float fab = maskBackRef.at(pp->locallab.selspot).fab;

                for (auto tool : locallabTools) {
                    tool->refChanged(huer, lumar, chromar, fab);
                }
            }
*/
            // Update Locallab Retinex tool min/max
            if (pp->locallab.selspot < (int)retiMinMax.size()) {
                const double cdma = retiMinMax.at(pp->locallab.selspot).cdma;
                const double cdmin = retiMinMax.at(pp->locallab.selspot).cdmin;
                const double mini = retiMinMax.at(pp->locallab.selspot).mini;
                const double maxi = retiMinMax.at(pp->locallab.selspot).maxi;
                const double Tmean = retiMinMax.at(pp->locallab.selspot).Tmean;
                const double Tsigma = retiMinMax.at(pp->locallab.selspot).Tsigma;
                const double Tmin = retiMinMax.at(pp->locallab.selspot).Tmin;
                const double Tmax = retiMinMax.at(pp->locallab.selspot).Tmax;

                expreti.updateMinMax(cdma, cdmin, mini, maxi, Tmean, Tsigma, Tmin, Tmax);
            }
            // Update Locallab Denoise tool lum/chro
            if (pp->locallab.selspot < (int) denoiselc.size()) {
                const double highres = denoiselc.at(pp->locallab.selspot).highres;
                const double nres = denoiselc.at(pp->locallab.selspot).nres;
                const double highres46 = denoiselc.at(pp->locallab.selspot).highres46;
                const double nres46 = denoiselc.at(pp->locallab.selspot).nres46;
                const  double Lhighres = denoiselc.at(pp->locallab.selspot).Lhighres;
                const double Lnres = denoiselc.at(pp->locallab.selspot).Lnres;
                const double Lhighres46 = denoiselc.at(pp->locallab.selspot).Lhighres46;
                const double Lnres46 = denoiselc.at(pp->locallab.selspot).Lnres46;

                expblur.updatedenlc(highres, nres, highres46, nres46, Lhighres, Lnres, Lhighres46, Lnres46);
            }

            // Update default values according to selected spot
            setDefaults(pp, pedited);

            // Note: No need to manage pedited as batch mode is deactivated for Locallab

            break;

        case (ControlSpotPanel::SpotDuplication): { // Spot duplication event
            newSpot = nullptr;
            spotIndex = expsettings->getSelectedSpot();

            for (int i = 0; i < (int)pp->locallab.spots.size(); i++) {
                if (i == spotIndex) {
                    newSpot = new LocallabParams::LocallabSpot(pp->locallab.spots.at(i));
                    break;
                }
            }

            if (!newSpot) {
                break;
            }

            // Spot creation (initialization at currently selected spot)
            ControlSpotPanel::SpotRow r;
            r.name = newSpot->name = newSpot->name + " - " + M("TP_LOCALLAB_DUPLSPOTNAME");
            r.isvisible = newSpot->isvisible;

            if (newSpot->shape == "ELI") {
                r.shape = 0;
            } else if (newSpot->shape == "RECT"){
                r.shape = 1;
            }

            if (newSpot->prevMethod == "hide") {
                r.prevMethod = 0;
            } else {
                r.prevMethod = 1;
            }

            if (newSpot->spotMethod == "norm") {
                r.spotMethod = 0;
            } else if (newSpot->spotMethod == "exc") {
                r.spotMethod = 1;
            } else if (newSpot->spotMethod == "full") {
                r.spotMethod = 2;
            } else if (newSpot->spotMethod == "main") {
                r.spotMethod = 3;
            }
            
            r.sensiexclu = newSpot->sensiexclu;
            r.structexclu = newSpot->structexclu;

            if (newSpot->shapeMethod == "IND") {
                r.shapeMethod = 0;
            } else if (newSpot->shapeMethod == "SYM") {
                r.shapeMethod = 1;
            } else if (newSpot->shapeMethod == "INDSL") {
                r.shapeMethod = 2;
            } else {
                r.shapeMethod = 3;
            }
            //printf("n0=%f n1=%f n2=%f n3=%f\n", (double) newSpot->loc.at(0), (double) newSpot->loc.at(1), (double) newSpot->loc.at(2), (double) newSpot->loc.at(3));
            if (newSpot->avoidgamutMethod == "NONE") {
                r.avoidgamutMethod = 0;
            } else if (newSpot->avoidgamutMethod == "LAB") {
                r.avoidgamutMethod = 1;
            } else if (newSpot->avoidgamutMethod== "XYZ") {
                r.avoidgamutMethod = 2;
            } else if (newSpot->avoidgamutMethod== "XYZREL") {
                r.avoidgamutMethod = 3;
             } else if (newSpot->avoidgamutMethod== "MUNS") {
                r.avoidgamutMethod = 4;
           } 
            //printf("n0=%f n1=%f n2=%f n3=%f\n", (double) newSpot->loc.at(0), (double) newSpot->loc.at(1), (double) newSpot->loc.at(2), (double) newSpot->loc.at(3));

            // Calculate spot size and center position according to preview area
            if (provider && !batchMode) {
                provider->getImageSize(imW, imH);
                provider->getPreviewCenterPos(prX, prY);
                provider->getPreviewSize(prW, prH);

                if (imW && imH) { // Image loaded
                    // Spot center position computation
                    newSpot->centerX = rtengine::LIM(int(int((double)prX - (double)imW / 2.) * 2000. / (double)imW), -1000, 1000);
                    newSpot->centerY = rtengine::LIM(int(int((double)prY - (double)imH / 2.) * 2000. / (double)imH), -1000, 1000);
                    // Ellipse/rectangle size computation
                    /*
                    newSpot->loc.at(0) = rtengine::LIM(int(((double)prW / 2. - 5.) * 2000. / (double)imW), 2, newSpot->loc.at(0));
                    newSpot->loc.at(1) = rtengine::LIM(int(((double)prW / 2. - 5.) * 2000. / (double)imW), 2, newSpot->loc.at(1));
                    newSpot->loc.at(2) = rtengine::LIM(int(((double)prH / 2. - 5.) * 2000. / (double)imH), 2, newSpot->loc.at(2));
                    newSpot->loc.at(3) = rtengine::LIM(int(((double)prH / 2. - 5.) * 2000. / (double)imH), 2, newSpot->loc.at(3));
                    */
                }
            }

            if(r.spotMethod == 0 || r.spotMethod == 1 ) {
                r.locX = newSpot->loc.at(0);
                r.locXL = newSpot->loc.at(1);
                r.locY = newSpot->loc.at(2);
                r.locYT = newSpot->loc.at(3);
            } else {
                r.locX = 3000.;
                r.locXL = 3000.;
                r.locY = 3000.;
                r.locYT = 3000.;
            }

            r.centerX = newSpot->centerX;
            r.centerY = newSpot->centerY;

            r.rotation = newSpot->spotangle;
            r.circrad = newSpot->circrad;

            if (newSpot->qualityMethod == "enh") {
                r.qualityMethod = 0;
            } else {
                r.qualityMethod = 1;
            }

            r.transit = newSpot->transit;
            r.transitweak = newSpot->transitweak;
            r.transitgrad = newSpot->transitgrad;
            r.feather = newSpot->feather;
            r.struc = newSpot->struc;
            r.thresh = newSpot->thresh;
            r.iter = newSpot->iter;
            r.balan = newSpot->balan;
            r.balanh = newSpot->balanh;
            r.colorde = newSpot->colorde;
            r.colorscope = newSpot->colorscope;
            r.avoidrad = newSpot->avoidrad;
            r.hishow = newSpot->hishow;
            r.activ = newSpot->activ;
            r.avoidneg = newSpot->avoidneg;
            r.blwh = newSpot->blwh;
            r.recurs = newSpot->recurs;
            r.laplac = newSpot->laplac;
            r.deltae = newSpot->deltae;
            r.scopemask = newSpot->scopemask;
            r.denoichmask = newSpot->denoichmask;
            r.shortc = newSpot->shortc;
            r.lumask = newSpot->lumask;
            //r.savrest = newSpot->savrest;

            if (newSpot->complexMethod == "sim") {
                r.complexMethod = 0;
            } else  if (newSpot->complexMethod == "mod") {
                r.complexMethod = 1;
            } else  if (newSpot->complexMethod == "all") {
                r.complexMethod = 2;
            }

            if (newSpot->wavMethod == "D2") {
                r.wavMethod = 0;
            } else if (newSpot->wavMethod == "D4") {
                r.wavMethod = 1;
            } else if (newSpot->wavMethod == "D6") {
                r.wavMethod = 2;
            } else if (newSpot->wavMethod == "D10") {
                r.wavMethod = 3;
            } else if (newSpot->wavMethod == "D14") {
                r.wavMethod = 4;
            } else if (newSpot->wavMethod == "D20") {
                r.wavMethod = 5;
            }

            expsettings->addControlSpot(r);

            // ProcParams update
            pp->locallab.spots.push_back(*newSpot);
            pp->locallab.selspot = pp->locallab.spots.size() - 1;


            // New created spot selection
            expsettings->setSelectedSpot(pp->locallab.selspot);

            // Update Locallab tools GUI with new created spot
            disableListener();

            spotName = pp->locallab.spots.at(pp->locallab.selspot).name;

            for (auto tool : locallabTools) {
                tool->read(pp, pedited);
            }

            enableListener();

            // Update tool list widget
            toolNb = 0;
            toollist->removeAllTool(); // Reset Locallab list firstly

            for (auto tool : locallabTools) {
                toolNb++;

                if (!tool->isLocallabToolAdded()) {
                    toollist->addToolRow(tool->getToolName(), toolNb);
                }
            }

            // Update default values according to selected spot
            setDefaults(pp, pedited);

            // Note: No need to manage pedited as batch mode is deactivated for Locallab

            break;
        }

        case (ControlSpotPanel::SpotAllVisibilityChanged): { // Event when updating visibility of all spots
            const auto r = expsettings->getSpot(expsettings->getSelectedSpot());

            // ProcParams update
            for (size_t i = 0; i < pp->locallab.spots.size(); i++) {
                pp->locallab.spots.at(i).isvisible = r->isvisible;
            }

            // Note: No need to manage pedited as batch mode is deactivated for Locallab

            break;
        }

        default: // Spot or locallab GUI updated
            if (pp->locallab.spots.size() > 0) {
                const auto r = expsettings->getSpot(expsettings->getSelectedSpot());

                // ProcParams update
                if (pp->locallab.selspot < (int)pp->locallab.spots.size()) {
                    // Control spot settings
                    pp->locallab.spots.at(pp->locallab.selspot).name = r->name;
                    pp->locallab.spots.at(pp->locallab.selspot).isvisible = r->isvisible;

                    if (r->shape == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).shape = "ELI";
                    } else {
                        pp->locallab.spots.at(pp->locallab.selspot).shape = "RECT";
                    }

                    if (r->prevMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).prevMethod = "hide";
                    } else {
                        pp->locallab.spots.at(pp->locallab.selspot).prevMethod = "show";
                    }


                    if (r->spotMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).spotMethod = "norm";
                    } else if (r->spotMethod == 1){
                        pp->locallab.spots.at(pp->locallab.selspot).spotMethod = "exc";
                    } else if (r->spotMethod == 2) {
                        pp->locallab.spots.at(pp->locallab.selspot).spotMethod = "full";
                    } else if (r->spotMethod == 3) {
                        pp->locallab.spots.at(pp->locallab.selspot).spotMethod = "main";
                    }

                    pp->locallab.spots.at(pp->locallab.selspot).sensiexclu = r->sensiexclu;
                    pp->locallab.spots.at(pp->locallab.selspot).structexclu = r->structexclu;

                    if (r->shapeMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).shapeMethod = "IND";
                    } else if (r->shapeMethod == 1) {
                        pp->locallab.spots.at(pp->locallab.selspot).shapeMethod = "SYM";
                    } else if (r->shapeMethod == 2) {
                        pp->locallab.spots.at(pp->locallab.selspot).shapeMethod = "INDSL";
                    } else {
                        pp->locallab.spots.at(pp->locallab.selspot).shapeMethod = "SYMSL";
                    }

                    if (r->avoidgamutMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).avoidgamutMethod = "NONE";
                    } else if (r->avoidgamutMethod == 1) {
                        pp->locallab.spots.at(pp->locallab.selspot).avoidgamutMethod = "LAB";
                    } else if (r->avoidgamutMethod == 2) {
                        pp->locallab.spots.at(pp->locallab.selspot).avoidgamutMethod = "XYZ";
                    } else if (r->avoidgamutMethod == 3) {
                        pp->locallab.spots.at(pp->locallab.selspot).avoidgamutMethod = "XYZREL";
                    } else if (r->avoidgamutMethod == 4) {
                        pp->locallab.spots.at(pp->locallab.selspot).avoidgamutMethod = "MUNS";
                    } 

                    pp->locallab.spots.at(pp->locallab.selspot).loc.at(0) = r->locX;
                    pp->locallab.spots.at(pp->locallab.selspot).loc.at(1) = r->locXL;
                    pp->locallab.spots.at(pp->locallab.selspot).loc.at(2) = r->locY;
                    pp->locallab.spots.at(pp->locallab.selspot).loc.at(3) = r->locYT;
                    pp->locallab.spots.at(pp->locallab.selspot).centerX = r->centerX;
                    pp->locallab.spots.at(pp->locallab.selspot).centerY = r->centerY;
                    pp->locallab.spots.at(pp->locallab.selspot).spotangle = r->rotation;
                    pp->locallab.spots.at(pp->locallab.selspot).circrad = r->circrad;

                    if (r->qualityMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).qualityMethod = "enh";
                    } else {
                        pp->locallab.spots.at(pp->locallab.selspot).qualityMethod = "enhden";
                    }

                    pp->locallab.spots.at(pp->locallab.selspot).transit = r->transit;
                    pp->locallab.spots.at(pp->locallab.selspot).transitweak = r->transitweak;
                    pp->locallab.spots.at(pp->locallab.selspot).transitgrad = r->transitgrad;
                    pp->locallab.spots.at(pp->locallab.selspot).feather = r->feather;
                    pp->locallab.spots.at(pp->locallab.selspot).struc = r->struc;
                    pp->locallab.spots.at(pp->locallab.selspot).thresh = r->thresh;
                    pp->locallab.spots.at(pp->locallab.selspot).iter = r->iter;
                    pp->locallab.spots.at(pp->locallab.selspot).balan = r->balan;
                    pp->locallab.spots.at(pp->locallab.selspot).balanh = r->balanh;
                    pp->locallab.spots.at(pp->locallab.selspot).colorde = r->colorde;
                    pp->locallab.spots.at(pp->locallab.selspot).colorscope = r->colorscope;
                    pp->locallab.spots.at(pp->locallab.selspot).avoidrad = r->avoidrad;
                    pp->locallab.spots.at(pp->locallab.selspot).hishow = r->hishow;
                    pp->locallab.spots.at(pp->locallab.selspot).activ = r->activ;
                    pp->locallab.spots.at(pp->locallab.selspot).avoidneg = r->avoidneg;
                    pp->locallab.spots.at(pp->locallab.selspot).blwh = r->blwh;
                    pp->locallab.spots.at(pp->locallab.selspot).recurs = r->recurs;
                    pp->locallab.spots.at(pp->locallab.selspot).laplac = r->laplac;
                    pp->locallab.spots.at(pp->locallab.selspot).deltae = r->deltae;
                    pp->locallab.spots.at(pp->locallab.selspot).scopemask = r->scopemask;
                    pp->locallab.spots.at(pp->locallab.selspot).denoichmask = r->denoichmask;
                    pp->locallab.spots.at(pp->locallab.selspot).shortc = r->shortc;
                    pp->locallab.spots.at(pp->locallab.selspot).lumask = r->lumask;
                    //pp->locallab.spots.at(pp->locallab.selspot).savrest = r->savrest;

                    if (r->complexMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).complexMethod = "sim";
                    } else if (r->complexMethod == 1) {
                        pp->locallab.spots.at(pp->locallab.selspot).complexMethod = "mod";
                    } else if (r->complexMethod == 2) {
                        pp->locallab.spots.at(pp->locallab.selspot).complexMethod = "all";
                    }

                    if (r->wavMethod == 0) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D2";
                    } else if (r->wavMethod == 1) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D4";
                    } else if (r->wavMethod == 2) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D6";
                    } else if (r->wavMethod == 3) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D10";
                    } else if (r->wavMethod == 4) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D14";
                    } else if (r->wavMethod == 5) {
                        pp->locallab.spots.at(pp->locallab.selspot).wavMethod = "D20";
                    }
                }

                for (auto tool : locallabTools) {
                    tool->write(pp, pedited);
                }

                // Note: No need to manage pedited as batch mode is deactivated for Locallab
            }
    }
}

/*
 * Note:
 * By default, this function is called when a new image/profile is loaded (after read function). In this case,
 * if there is at least one spot, default values are set to selected spot ones.
 * To keep having default values according to selected spot, this function shall also be called in the following
 * situations (after having called write function for controlspotpanel):
 * - After spot creation
 * - After spot deletion
 * - After spot selection
 * - After spot duplication
 */
void Locallab::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
    // Set default values in spot panel control
    expsettings->setDefaults(defParams, pedited);

    // Set default values in Locallab tools
    for (auto tool : locallabTools) {
        tool->setDefaults(defParams, pedited);
    }
}

void Locallab::setListener(ToolPanelListener* tpl)
{
    this->listener = tpl;

    // Set listener for spot control panel
    expsettings->setListener(tpl);

    // Set listener for locallab tools
    for (auto tool : locallabTools) {
        tool->setListener(tpl);
    }
}

void Locallab::minmaxChanged(const std::vector<locallabRetiMinMax> &minmax, int selspot)
{
    // Saving transmitted min/max data
    retiMinMax = minmax;

    // Update Locallab Retinex tool min/max
    if (selspot < (int)retiMinMax.size()) {
        const double cdma = retiMinMax.at(selspot).cdma;
        const double cdmin = retiMinMax.at(selspot).cdmin;
        const double mini = retiMinMax.at(selspot).mini;
        const double maxi = retiMinMax.at(selspot).maxi;
        const double Tmean = retiMinMax.at(selspot).Tmean;
        const double Tsigma = retiMinMax.at(selspot).Tsigma;
        const double Tmin = retiMinMax.at(selspot).Tmin;
        const double Tmax = retiMinMax.at(selspot).Tmax;

        expreti.updateMinMax(cdma, cdmin, mini, maxi, Tmean, Tsigma, Tmin, Tmax);
    }
}

void Locallab::madChanged(const std::vector<locallabDenoiseMAD> &madlc, int selspot)
{
    madllc = madlc; 
    if (selspot < (int) madllc.size()) {
        const double m0 = madllc.at(selspot).mad0;
        const double m1 = madllc.at(selspot).mad1;
        const double m2 = madllc.at(selspot).mad2;
        const double m3 = madllc.at(selspot).mad3;
        const double m4 = madllc.at(selspot).mad4;
        const double m5 = madllc.at(selspot).mad5;
        const double m6 = madllc.at(selspot).mad6;
        const double m7 = madllc.at(selspot).mad7;
        const double m8 = madllc.at(selspot).mad8;
        const double m9 = madllc.at(selspot).mad9;
        const double m10 = madllc.at(selspot).mad10;
        const double m11 = madllc.at(selspot).mad11;
        const double m12 = madllc.at(selspot).mad12;
        const double m13 = madllc.at(selspot).mad13;
        const double m14 = madllc.at(selspot).mad14;
        const double m15 = madllc.at(selspot).mad15;
        const double m16 = madllc.at(selspot).mad16;
        const double m17 = madllc.at(selspot).mad17;
        const double m18 = madllc.at(selspot).mad18;
        const double m19 = madllc.at(selspot).mad19;
        const double m20 = madllc.at(selspot).mad20;
        const bool madloc = madllc.at(selspot).madlock;
        expblur.updatemadlc(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, madloc);        
    }
}


void Locallab::denChanged(const std::vector<locallabDenoiseLC> &denlc, int selspot)
{
    // Saving transmitted min/max data
    denoiselc = denlc;
    
    //Update Locallab Denoise tool lum chro
    if (selspot < (int) denoiselc.size()) {
        const double highres = denoiselc.at(selspot).highres;
        const double nres = denoiselc.at(selspot).nres;
        const double highres46 = denoiselc.at(selspot).highres46;
        const double nres46 = denoiselc.at(selspot).nres46;
        const double Lhighres = denoiselc.at(selspot).Lhighres;
        const double Lnres = denoiselc.at(selspot).Lnres;
        const double Lhighres46 = denoiselc.at(selspot).Lhighres46;
        const double Lnres46 = denoiselc.at(selspot).Lnres46;

        expblur.updatedenlc(highres, nres, highres46, nres46, Lhighres, Lnres, Lhighres46, Lnres46);
    }
    
}

void Locallab::den2Changed(const std::vector<locallabDenoiseLC2> &denlc2, int selspot)
{
    // Saving transmitted min/max data
    denoiselc2 = denlc2;
    
    //Update Locallab Denoise tool lum chro
    if (selspot < (int) denoiselc2.size()) {
        const double deno = denoiselc2.at(selspot).denocontrastaft;
        expblur.autodenoContrastChanged(deno);
      //  expblur.updatedenlc(highres, nres, highres46, nres46, Lhighres, Lnres, Lhighres46, Lnres46);
    }
    
}



// New fonctions to change Scope color 
void Locallab::scopeChangedcol(int scope, int selspot, bool enab)
{
    if(enab) {
        expcolor.updateguiscopecolor(scope);
    }
   
}
// New fonctions to change Scope Shadows Highlight

void Locallab::scopeChangedsh(int scope, int selspot, bool enab)
{
    if(enab) {
        expshadhigh.updateguiscopesahd(scope); 
    }
   
}

// New fonctions to change Scope Vibrance

void Locallab::scopeChangedvib(int scope, int selspot, bool enab)
{
    if(enab) {
        expvibrance.updateguiscopevib(scope); 
    }
   
}

//reinit expsettings
void Locallab::scopeChangedset(int scope, int selspot, bool enab)
{
    if(enab) {
        expsettings->updateguiscopeset(30);//30 defaut value..perhaps possible to pass default value ??
    }
   
}

void Locallab::sigChanged(const std::vector<locallabcieSIG> &ciesig, int selspot)
{
     cie_sig = ciesig;

    if (selspot < (int) cie_sig.size()) {
        const double s1 = cie_sig.at(selspot).contsigq;
        const double s2 = cie_sig.at(selspot).lightsigq;
        
        expcie.updatesigloc(s1, s2);
    }
     
}

void Locallab::ciebefChanged(const std::vector<locallabcieBEF> &ciebef, int selspot)
{
    cie_bef = ciebef;
    if (selspot < (int) cie_bef.size()) {
        const double blackev = cie_bef.at(selspot).blackevbef;
        const double whiteev = cie_bef.at(selspot).whiteevbef;
        const double sourceg = cie_bef.at(selspot).sourcegbef;
        const double sourceab = cie_bef.at(selspot).sourceabbef;
        const double targetg = cie_bef.at(selspot).targetgbef;
        const double jz1 = cie_bef.at(selspot).jz1bef;
        const bool autocomput = cie_bef.at(selspot).autocomputbef;
        const bool autocie = cie_bef.at(selspot).autociebef;

        if(autocomput) {
            explog.updateAutocompute(blackev, whiteev, sourceg, sourceab, targetg, jz1);
        }
        if(autocie) {
            expcie.updateAutocompute(blackev, whiteev, sourceg, sourceab, targetg, jz1);
        }

    }

}


void Locallab::sharbefChanged(const std::vector<locallabsharBEF> &sharbef, int selspot)
{
    shar_bef = sharbef;
    if (selspot < (int) shar_bef.size()) {
        const double caprad = shar_bef.at(selspot).capradiusbef;
        const bool autorad = shar_bef.at(selspot).autoradiusbef;

      //  const double sharcon = shar_bef.at(selspot).sharcontrastbef;
      //  const bool autocon = shar_bef.at(selspot).autocontrastbef;

        if(autorad) {
            expsharp.autoDeconvRadiusChanged(caprad);
        }

    }

}

void Locallab::sharaftChanged(const std::vector<locallabsharAFT> &sharaft, int selspot)
{
    shar_aft = sharaft;
    if (selspot < (int) shar_aft.size()) {
        const double sharcon = shar_aft.at(selspot).sharcontrastaft;
        const bool autocon = shar_aft.at(selspot).autocontrastaft;
        if(autocon) {
            expsharp.autoContrastChanged(sharcon);
        }

   }

}





void Locallab::maiChanged(const std::vector<locallabsetLC> &setlc, int selspot)
{
    set_lc = setlc;
    if (selspot < (int) set_lc.size()) {
        const int spottype = set_lc.at(selspot).mainf;
        const bool iscolor = set_lc.at(selspot).iscolo;
        const bool issh = set_lc.at(selspot).iss;
        const bool isvib = set_lc.at(selspot).isvi;
        const bool isexpos = set_lc.at(selspot).isexpo;
        const bool issoft = set_lc.at(selspot).issof;
        const bool isblur = set_lc.at(selspot).isblu;
        const bool istom = set_lc.at(selspot).isto;
        const bool isret = set_lc.at(selspot).isre;
        const bool issharp = set_lc.at(selspot).isshar;
        const bool iscont = set_lc.at(selspot).iscon;
        const bool iscbdl = set_lc.at(selspot).iscbd;
        const bool islog = set_lc.at(selspot).islo;
        const bool ismas = set_lc.at(selspot).isma;
        const bool isci = set_lc.at(selspot).isci;

        if(iscolor) {
            expcolor.updateguicolor(spottype);
        }

        if(issh) {
            expshadhigh.updateguishad(spottype);
        }

        if(isvib) {
            expvibrance.updateguivib(spottype);
        }

        if(isexpos) {
            expexpose.updateguiexpos(spottype);
        }

        if(issoft) {
            expsoft.updateguisoft(spottype);
        }

        if(isblur) {
            expblur.updateguiblur(spottype);
        }

        if(istom) {
            exptonemap.updateguitone(spottype);
        }

        if(isret) {
            expreti.updateguireti(spottype);
        }

        if(issharp) {
            expsharp.updateguisharp(spottype);
        }

        if(iscont) {
            expcontrast.updateguicont(spottype);
        }

        if(iscbdl) {
            expcbdl.updateguicbdl(spottype);
        }

        if(islog) {
            explog.updateguilog(spottype);
        }

        if(ismas) {
            expmask.updateguimask(spottype);
        }

        if(isci) {
            expcie.updateguicie(spottype);
        }

        expsettings->updateguiset(spottype, iscolor, issh, isvib, isexpos, issoft, isblur, istom, isret, issharp, iscont, iscbdl, islog, ismas, isci);
    }
}

void Locallab::ghsbw2Changed(const std::vector<locallabshGHSbw2> &shghsbw2, int selspot) //update sliders black and white point if auto
{
    sh_ghsbw2 = shghsbw2;
    double bwvalueslider[2] = {0., 1.};
    bool ghsaut = true;

    if (selspot < (int) sh_ghsbw2.size()) {
        for(int i=0; i < 2; i++) {
            bwvalueslider[i] = sh_ghsbw2.at(selspot).ghsbw_slider[i];
        }
        ghsaut = sh_ghsbw2.at(selspot).ghs_auto;
        expshadhigh.updateghsbw2(bwvalueslider[0], bwvalueslider[1], ghsaut);
    }

}

void Locallab::michbwChanged(const std::vector<locallabshMICHbw> &shmichbw, int selspot) // info black and white point Michaelis
{
    sh_michbw = shmichbw;
    double bwvalue[2] = {0., 1.};
    bool michaut = true;
    if (selspot < (int) sh_michbw.size()) {
        for(int i=0; i < 2; i++) {
            bwvalue[i] = sh_michbw.at(selspot).mich_slider[i];
        }
        michaut = sh_michbw.at(selspot).mich_auto;
        expshadhigh.updatemichbw(bwvalue[0], bwvalue[1], michaut);
    }

}

void Locallab::ghsbwChanged(const std::vector<locallabshGHSbw> &shghsbw, int selspot) // info black and white point and Symmetry point auto
{
    sh_ghsbw = shghsbw;
    int bw[2] = {0, 1};
    double bwvalue[2] = {0., 1.};
    double symev = 0.;
    double midgrey = 0.;
    double maxrgb = 0.;
    double sig3 = 0.;
    double bwcol[4] = {0., 0., 0., 0.};
    bool ghsau = true;
    if (selspot < (int) sh_ghsbw.size()) {
        const bool autoradsp = sh_ghsbw.at(selspot).autoSP;

        for(int i=0; i < 2; i++) {
            bw[i] = sh_ghsbw.at(selspot).ghsbw[i];
        }

        symev = sh_ghsbw.at(selspot).ghs_sym;
        midgrey = sh_ghsbw.at(selspot).ghs_mid;
        maxrgb = sh_ghsbw.at(selspot).ghs_maxrgb;
        sig3 = sh_ghsbw.at(selspot).ghs_3sig;
        bwcol[0] = sh_ghsbw.at(selspot).ghs_color[0];
        bwcol[1] = sh_ghsbw.at(selspot).ghs_color[1];
        bwcol[2] = sh_ghsbw.at(selspot).ghs_color[2];
        bwcol[3] = sh_ghsbw.at(selspot).ghs_color[3];
        ghsau = sh_ghsbw.at(selspot).ghs_auto;
        expshadhigh.updateghsbw(bw[0], bw[1], bwvalue[0], bwvalue[1], symev, midgrey, maxrgb, sig3, bwcol[0], bwcol[1], bwcol[2], bwcol[3], ghsau);
        if(autoradsp) {
            expshadhigh.autoSPChanged(symev);
        }
    }

}
void Locallab::cieChanged(const std::vector<locallabcieLC> &cielc, int selspot)
{
    // Saving transmitted min/max data
    cie_lc = cielc;

    
    //Update Locallab Denoise tool lum chro
    if (selspot < (int) cie_lc.size()) {
        const double r1 = cie_lc.at(selspot).redxlc;
        const double r2 = cie_lc.at(selspot).redylc;
        const double g1 = cie_lc.at(selspot).grexlc;
        const double g2 = cie_lc.at(selspot).greylc;
        const double b1 = cie_lc.at(selspot).bluxlc;
        const double b2 = cie_lc.at(selspot).bluylc;
        const double w1 = cie_lc.at(selspot).wxlc;
        const double w2 = cie_lc.at(selspot).wylc;
        const double m1 = cie_lc.at(selspot).meanxlc;
        const double m2 = cie_lc.at(selspot).meanylc;
        const double me1 = cie_lc.at(selspot).meanxelc;
        const double me2 = cie_lc.at(selspot).meanyelc;
        const int pri = cie_lc.at(selspot).primlc;
        const double slg = cie_lc.at(selspot).slopeglc;
        const bool lkg = cie_lc.at(selspot).linkrgblc;

        expcie.updateiPrimloc(r1, r2, g1, g2, b1, b2, w1, w2, m1, m2, me1, me2, pri, slg, lkg);
    }
    
}


void Locallab::refChanged2(float *huerefp, float *chromarefp, float *lumarefp, float *fabrefp, int selspot)
{
        const double huer = huerefp[selspot];
        const double lumar = lumarefp[selspot];
        const double chromar = chromarefp[selspot];
        const float fab = fabrefp[selspot];
        for (auto tool : locallabTools) {
            tool->refChanged(huer, lumar, chromar, fab);
        }
}
/*
void Locallab::refChanged(const std::vector<locallabRef> &ref, int selspot)
{
    // Saving transmitted mask background data
    maskBackRef = ref;

    // Update locallab tools mask background
    if (selspot < (int)maskBackRef.size()) {
        const double huer = maskBackRef.at(selspot).huer;
        const double lumar = maskBackRef.at(selspot).lumar;
        const double chromar = maskBackRef.at(selspot).chromar;
        const float fab = maskBackRef.at(selspot).fab;

        for (auto tool : locallabTools) {
            tool->refChanged(huer, lumar, chromar, fab);
        }
    }
}
*/
void Locallab::resetMaskVisibility()
{
    // Indicate to spot control panel that no more mask preview is active
    expsettings->setMaskPrevActive(false);

    // Reset deltaE preview
    expsettings->resetDeltaEPreview();
    for (auto tool : std::initializer_list<LocallabTool *>{
             &expcolor,
             &expexpose,
             &expshadhigh,
             &expvibrance,
             &expsoft,
             &expblur,
             &exptonemap,
             &expreti,
             &expsharp,
             &expcontrast,
             &expcbdl,
             &explog,
             &expmask,
             &expcie,
         }) {
        auto button = tool->getPreviewDeltaEButton();
        auto connection = tool->getPreviewDeltaEButtonConnection();
        if (button && connection) {
            connection->block();
            button->set_active(false);
            connection->unblock();
        }
    }

    // Reset mask preview for all Locallab tools
    for (auto tool : locallabTools) {
        tool->resetMaskView();
    }
}

Locallab::llMaskVisibility Locallab::getMaskVisibility() const
{
    // Get deltaE preview state
    const bool prevDeltaE = expsettings->isDeltaEPrevActive();

    // Get mask preview from Locallab tools
    int colorMask, colorMaskinv, expMask, expMaskinv, shMask, shMaskinv, vibMask, softMask, blMask, tmMask, retiMask, sharMask, lcMask, cbMask, logMask, maskMask, cieMask;

    for (auto tool : locallabTools) {
        tool->getMaskView(colorMask, colorMaskinv, expMask, expMaskinv, shMask, shMaskinv, vibMask, softMask, blMask, tmMask, retiMask, sharMask, lcMask, cbMask, logMask, maskMask, cieMask);
    }

    // Indicate to spot control panel if one mask preview is active
    const bool isMaskActive = (colorMask == 0) || (colorMaskinv == 0) || (expMask == 0) || (expMaskinv == 0) ||
                              (shMask == 0) || (shMaskinv == 0) || (vibMask == 0) || (softMask == 0) ||
                              (blMask == 0) || (tmMask == 0) || (retiMask == 0) || (sharMask == 0) ||
                              (lcMask == 0) || (cbMask == 0) || (logMask == 0) || (maskMask == 0) || (cieMask == 0);
    expsettings->setMaskPrevActive(isMaskActive);

    return {prevDeltaE, colorMask, colorMaskinv, expMask, expMaskinv, shMask, shMaskinv, vibMask, softMask, blMask, tmMask, retiMask, sharMask, lcMask, cbMask, logMask, maskMask, cieMask};
}

//void Locallab::resetshowPressed()
//{
//    // Raise event to reset mask
//    if (listener) {
//        listener->panelChanged(Evlocallabshowreset, "");
//    }
//}

void Locallab::setEditProvider(EditDataProvider * provider)
{
    expsettings->setEditProvider(provider);
}

void Locallab::subscribe()
{
    expsettings->subscribe();
}

void Locallab::unsubscribe()
{
    expsettings->unsubscribe();
}

void Locallab::enabledChanged()
{
    if (listener) {
        if (getEnabled()) {
            listener->panelChanged(EvlocallabEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvlocallabEnabled, M("GENERAL_DISABLED"));
        }
    }
}

void Locallab::autoOpenCurve()
{
    // TODO Actually autoOpenCurve only considers linearity state of selected spot curve
}

void Locallab::foldAllButOne(LocallabTool* except)
{
    for (auto tool : locallabTools) {
        if (tool != except) {
            // All other tool expanders are fold
            tool->setExpanded(false);
        } else {
            // If fold, selected tool expander is unfold
            if (!tool->getExpanded()) {
                tool->setExpanded(true);
            }
        }
    }
}

void Locallab::openAllTools()
{
    // Set default visibility for settings panel sub-expanders
    expsettings->setDefaultExpanderVisibility();

    for (auto tool : locallabTools) {
        tool->setExpanded(true);

        // Set default visibility for tool sub-expanders
        tool->setDefaultExpanderVisibility();
    }
}

void Locallab::updateShowtooltipVisibility(bool showtooltip)
{
    for (auto tool : locallabTools) {
        tool->updateAdviceTooltips(showtooltip);
    }
}

void Locallab::spotNameChanged(const Glib::ustring &newName)
{
    spotName = newName;
}

void Locallab::addTool(Gtk::Box* where, LocallabTool* tool)
{
    tool->getExpander()->setLevel(3);
    where->pack_start(*tool->getExpander(), false, false);
    locallabTools.push_back(tool);
    tool->setLocallabToolListener(this);
    tool->setSpotNameSource(&spotName);
}

void Locallab::setParamEditable(bool cond)
{
    // Update params editable state for controlspotpanel
    expsettings->setParamEditable(cond); // TODO Move this code to controlspotpanel.cc when there is zero spot

    // Enable/disable possibility to add Locallab tool
    toollist->set_sensitive(cond);

    // Remove all Locallab tool (without raising event) only if cond is false
    if (!cond) {
        for (auto tool : locallabTools) {
            tool->removeLocallabTool(false);
        }
    }
}

void Locallab::resetToolMaskView()
{
    // Reset mask view GUI for all other Locallab tools
    for (auto tool : locallabTools) {
        tool->resetMaskView();
    }

    // Deactivate any preview delta E toggle button.
    auto active_preview_button = delta_e_preview_button_group.getActiveButton();
    if (active_preview_button) {
        active_preview_button->set_active(false);
    }
}

void Locallab::resetOtherMaskView(LocallabTool* current)
{
    // Reset deltaE preview
    expsettings->resetDeltaEPreview();

    // Reset mask view GUI for all other Locallab tools except current
    for (auto tool : locallabTools) {
        if (tool != current) {
            tool->resetMaskView();
        }
    }
}

void Locallab::toolRemoved(LocallabTool* current)
{
    // Update tool list widget according to removed tool
    int toolNb = 0;

    for (auto tool : locallabTools) {
        toolNb++;

        if (tool == current) {
            toollist->addToolRow(tool->getToolName(), toolNb);
        }
    }
}

void Locallab::locallabToolToAdd(const Glib::ustring &toolname)
{
    for (auto tool : locallabTools) {
        if (tool->getToolName() == toolname) {
            // Set expanders visibility default state when adding tool
            tool->setExpanded(true);
            tool->setDefaultExpanderVisibility();

            // Add tool
            tool->addLocallabTool(true);
        }
    }
}
