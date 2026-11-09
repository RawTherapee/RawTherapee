/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "toolpanel.h"
#include "widgets/basic/adjuster.h"

#include "rtengine/aspectratios.h"
#include "rtengine/procevents.h"
#include "rtengine/procparams.h"

#include <gdkmm/rgba.h>
#include <giomm/liststore.h>
#include <glibmm/refptr.h>

#include <array>
#include <memory>
#include <vector>

class ColorPreview;
class MyComboBoxText;
class RTImage;
struct ParamsEdited;

namespace Gtk {

class ListBox;
class ToggleButton;

} // namespace Gtk

class CropGuide final :
    public ToolParamBlock,
    public AdjusterListener,
    public FoldableToolPanel {
public:
    static const Glib::ustring TOOL_NAME;

    CropGuide();

    void read(const rtengine::procparams::ProcParams* pp,
              const ParamsEdited* pedited = nullptr) override;
    void write(rtengine::procparams::ProcParams* pp,
               ParamsEdited* pedited = nullptr) override;
    void trimValues(rtengine::procparams::ProcParams* pp) override;
    void setBatchMode(bool batchMode) override;

    void setAdjusterBehavior(bool bleed);
    void adjusterChanged(Adjuster* adj, double newVal) override;

    void enabledChanged() override;
    bool onPresetToggled(GdkEventButton* event, size_t index);
    void onGoldenTriangleMirror();
    void onGoldenTriangleReset();
    void onGoldenRatioRotate();
    void onGoldenRatioMirror();
    void onGoldenRatioReset();

private:
    struct AspectRatioModel : public Glib::Object {
        static Glib::RefPtr<AspectRatioModel> create()
        {
            return Glib::RefPtr<AspectRatioModel>(new AspectRatioModel);
        }

        AspectRatio aspect_ratio;
        size_t index = 0;

        Gdk::RGBA color;
        bool active = false;
        bool is_portrait = false;
        bool visible = true;
    };

    struct Preset {
        ColorPreview* preview = nullptr;
        Gtk::ToggleButton* visibility_button = nullptr;
        sigc::connection visibility_conn;
        Gdk::RGBA color;
        bool is_dirty = false;
    };

    void setupEvents();
    void setupPresets();
    void setupAspectRatioGuides();

    void onPresetPickColor(size_t index);
    void onAspectRatioComboChanged();
    void onAspectRatioPresetToggled(Gtk::ToggleButton* button, size_t index);
    void onAspectRatioPresetPickColor(size_t index, ColorPreview* preview);
    void onAspectRatioPresetRotate(size_t index);
    void onAspectRatioPresetRemoved(size_t index);
    void onBasisChanged();

    void refreshAvailableAspectRatios();
    int compareAspectRatioModels(const Glib::RefPtr<const AspectRatioModel>& lhs,
                                 const Glib::RefPtr<const AspectRatioModel>& rhs) const;

    Gtk::Widget* createAspectRatioModelControls(
        const Glib::RefPtr<AspectRatioModel>& item);

    Adjuster* m_bleed;
    Gtk::ListBox* m_aspect_ratio_listbox;
    MyComboBoxText* m_available_aspect_ratios_combo;
    sigc::connection m_available_aspect_ratios_conn;

    Glib::RefPtr<Gio::ListStore<AspectRatioModel>> m_aspect_ratio_store;
    std::vector<Glib::RefPtr<AspectRatioModel>> m_aspect_ratio_presets;

    std::array<Preset, 9> m_presets;
    bool m_mirror_golden_triangle;
    bool m_dirty_mirror_golden_triangle;

    bool m_rotate_golden_ratio;
    bool m_dirty_rotate_golden_ratio;
    bool m_mirror_golden_ratio;
    bool m_dirty_mirror_golden_ratio;

    bool m_dirty_aspect_ratios;

    MyComboBoxText* m_basis;
    sigc::connection m_basis_changed;

    rtengine::ProcEvent EvCropGuideEnabled;
    rtengine::ProcEvent EvCropGuidePresetChanged;
    rtengine::ProcEvent EvCropGuideAspectRatioPresetChanged;
    rtengine::ProcEvent EvCropGuideBleedChanged;
    rtengine::ProcEvent EvCropGuideBasisChanged;
};
