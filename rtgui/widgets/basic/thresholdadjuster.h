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
 */
#pragma once

#include <gtkmm.h>

#include "editedstate.h"
#include "guiutils.h"
#include "thresholdselector.h"

class ThresholdAdjuster;

/*
 * TODO: Maybe we could just send back the history string instead of the individual values?
 */
class ThresholdAdjusterListener
{

public:
    virtual ~ThresholdAdjusterListener() = default;
    // to be used by listener that has created a ThresholdAdjuster with with single threshold and precision > 0
    virtual void
    adjusterChanged(ThresholdAdjuster* a, double newBottom, double newTop) = 0;
    // to be used by listener that has created a ThresholdAdjuster with with double threshold and precision > 0
    virtual void adjusterChanged(ThresholdAdjuster* a,
                                 double newBottomLeft,
                                 double newTopLeft,
                                 double newBottomRight,
                                 double newTopRight) = 0;
    // to be used by listener that has created a ThresholdAdjuster with with single threshold and precision == 0
    virtual void adjusterChanged(ThresholdAdjuster* a, int newBottom, int newTop) = 0;
    // to be used by listener that has created a ThresholdAdjuster with with double threshold and precision == 0
    virtual void adjusterChanged(ThresholdAdjuster* a,
                                 int newBottomLeft,
                                 int newTopLeft,
                                 int newBottomRight,
                                 int newTopRight) = 0;
    virtual void adjusterChanged2(ThresholdAdjuster* a,
                                  int newBottomL,
                                  int newTopL,
                                  int newBottomR,
                                  int newTopR) = 0;
};

class ThresholdAdjuster : public Gtk::Box
{

protected:
    Gtk::Box* hbox;
    Gtk::Label* label;
    ThresholdSelector tSelector;
    //MySpinButton* spin;
    Gtk::Button* reset;
    ThresholdAdjusterListener* adjusterListener;
    sigc::connection delayConnection;
    //sigc::connection spinChange;
    sigc::connection selectorChange;
    sigc::connection editedChange;
    double initialDefaultVal[4];  // default value at construction time
    EditedState editedState;
    EditedState defEditedState;
    Gtk::CheckButton* editedCheckBox;
    bool afterReset;
    bool blocked;
    bool addMode;
    bool separatedMode;
    int delay;

    double shapeValue(double a);
    void refreshLabelStyle();
    void initObject(Glib::ustring label, bool editedcb);
    void sendToListener();

public:
    // Custom Threshold widget with 2 cursors (Top and Bottom) and a custom curve provided by the instanciator,
    // each cursor having his own range, default value and precision
    ThresholdAdjuster(Glib::ustring label,
                      double minValueBottom,
                      double maxValueBottom,
                      double defBottom,
                      Glib::ustring labelBottom,
                      unsigned int precisionBottom,
                      double minValueTop,
                      double maxValueTop,
                      double defTop,
                      Glib::ustring labelTop,
                      unsigned int precisionTop,
                      ThresholdCurveProvider* curveProvider,
                      bool editedCheckBox = false);
    // Simple Threshold widget with 2 linked sliders (0->1 or 1->0)
    ThresholdAdjuster(Glib::ustring label,
                      double minValue,
                      double maxValue,
                      double defBottom,
                      double defTop,
                      unsigned int precision,
                      bool startAtOne,
                      bool editedCheckBox = false);
    // Simple Threshold widget with 4 linked sliders by pairs (0->1->0 or 1->0->1)
    ThresholdAdjuster(Glib::ustring label,
                      double minValue,
                      double maxValue,
                      double defBottomLeft,
                      double defTopLeft,
                      double defBottomRight,
                      double defTopRight,
                      unsigned int precision,
                      bool startAtOne,
                      bool editedCheckBox = false);

    ~ThresholdAdjuster() override;
    void setAdjusterListener(ThresholdAdjusterListener* alistener)
    {
        adjusterListener = alistener;
    }

    void setBgCurveProvider(ThresholdCurveProvider* provider);
    ThresholdSelector* getThresholdSelector() { return &tSelector; };

    template <typename T> rtengine::procparams::Threshold<T> getValue()
    {
        return tSelector.getPositions<T>();
    }
    void getValue(double& bottom, double& top);
    void
    getValue(double& bottomLeft, double& topLeft, double& bottomRight, double& topRight);
    void getValue(int& bottom, int& top);
    void getValue(int& bottomLeft, int& topLeft, int& bottomRight, int& topRight);
    void getValue(Glib::ustring& bottom, Glib::ustring& top);
    void getValue(Glib::ustring& bottomLeft,
                  Glib::ustring& topLeft,
                  Glib::ustring& bottomRight,
                  Glib::ustring& topRight);
    template <class T> void setValue(const rtengine::procparams::Threshold<T>& tValues)
    {
        tSelector.setPositions<T>(tValues);
    }
    void setValue(double bottom, double top);
    void setValue(double bottomLeft, double topLeft, double bottomRight, double topRight);
    void setEnabled(bool enabled);
    template <typename T> void setDefault(const rtengine::procparams::Threshold<T>& tresh)
    {
        tSelector.setDefaults<T>(tresh);
    }
    void setDefault(double defBottom, double defTop);
    void setDefault(double defBottomLeft,
                    double defTopLeft,
                    double defBottomRight,
                    double defTopRight);
    void setEditedState(EditedState eState);
    EditedState getEditedState();
    void setDefaultEditedState(EditedState eState);
    void showEditedCB();
    void block(bool isBlocked) { blocked = isBlocked; }
    void setBgGradient(const std::vector<GradientMilestone>& milestones)
    {
        tSelector.coloredBar.setBgGradient(milestones);
    }
    void setBgColorProvider(ColorProvider* cp, int i)
    {
        tSelector.coloredBar.setColorProvider(cp, i);
    }
    void setUpdatePolicy(eUpdatePolicy policy) { tSelector.setUpdatePolicy(policy); }

    //void spinChanged ();
    void selectorChanged();
    bool notifyListener();
    void resetPressed(GdkEventButton* event);
    void editedToggled();
    Glib::ustring getHistoryString();
    void set_tooltip_markup(const Glib::ustring& markup);
    // this set_tooltip_text method is to set_tooltip_markup, and text can contain markups
    void set_tooltip_text(const Glib::ustring& text);
};
