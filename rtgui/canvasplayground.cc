/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
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

#include "canvas/imagecanvas.h"
#include "canvas/imagemodel.h"
#include "canvas/imagerender.h"

#include "config.h"
#include "guiutils.h"
#include "gtk4.h"
#include "options.h"
#include "pathutils.h"
#include "rtscalable.h"

#include "rtengine/imagefloat.h"
#include "rtengine/procparams.h"
#include "rtengine/rtengine.h"
#include "rtengine/stdimagesource.h"

#include <fmt/format.h>
#include <gtkmm.h>

using namespace rt::canvas;
using namespace rtengine;
using namespace rtengine::procparams;

namespace {

class CursorTracker : public CursorMonitor
{
public:
    void onEnter(const CanvasModel* model, WidgetPoint pos) override
    {
        update(model, pos);
    }

    void onMotion(const CanvasModel* model, WidgetPoint pos) override
    {
        update(model, pos);
    }

    void update(const CanvasModel* model, WidgetPoint pos);

    Gtk::Label* lpos = nullptr;
    Gtk::Label* l2c = nullptr;
    Gtk::Label* l2w = nullptr;
    Gtk::Label* c2l = nullptr;
    Gtk::Label* c2w = nullptr;
    Gtk::Label* w2l = nullptr;
    Gtk::Label* w2c = nullptr;
};

class CanvasPlayground : public Gtk::Box
{
public:
    CanvasPlayground();

    void setupImage();

    void onCameraBoundsChanged();
    void onZoom11Clicked();
    void onZoomFitClicked();

    bool onWindowFocusOut(GdkEventFocus* event);
    bool onKeyPressed(guint keyval, guint keycode, GdkModifierType state);
    void onKeyReleased(guint keyval, guint keycode, GdkModifierType state)
    {
        m_canvas->onKeyReleased(keyval, keycode, state);
    }

    ImageCanvas* canvas() const { return m_canvas; }

private:
    void setupControls();
    void setupImageBuffer();

    void onSmoothSensitivityChanged();
    void onSmoothPanSensitivityChanged();
    void onDiscreteScrollDirectionToggled();
    void onSmoothScrollDirectionToggled();
    void onScrollModeToggled();

    std::unique_ptr<ImageCanvasModel> m_canvas_model;

    std::unique_ptr<StdImageSource> m_img_src;
    std::unique_ptr<Imagefloat> m_img;

    std::unique_ptr<ImageRenderer> m_img_renderer;
    std::unique_ptr<DebugRenderer> m_debug_renderer;
    std::unique_ptr<EditorRenderer> m_editor_renderer;

    std::unique_ptr<CursorTracker> m_cursor_event_listener;

    ImageCanvas* m_canvas;
    Gtk::Box* m_control_box;
    Gtk::ComboBoxText* m_camera_bounds;
    Gtk::Scale m_zoom_sense_slider;
    Gtk::Scale m_pan_sense_slider;
    Gtk::CheckButton* m_discrete_scroll_dir_button;
    Gtk::CheckButton* m_smooth_scroll_dir_button;
    Gtk::CheckButton* m_scroll_mode_button;
};

template <class T>
void updatePosLabel(Gtk::Label* label, const T& point)
{
    label->set_text(
        fmt::format("x: {:6.1f} y: {:6.1f}", point.x.value(), point.y.value()));
}

}  // namespace

CanvasPlayground::CanvasPlayground()
    : m_canvas_model(std::make_unique<ImageCanvasModel>()),
      m_img_renderer(std::make_unique<ImageRenderer>()),
      m_debug_renderer(std::make_unique<DebugRenderer>(DebugRenderer::ALL)),
      m_editor_renderer(std::make_unique<EditorRenderer>(m_img_renderer.get()))
{
    m_editor_renderer->setDebugRenderer(m_debug_renderer.get());

    auto paned = rt::make_managed<Gtk::Paned>(Gtk::ORIENTATION_HORIZONTAL);
    paned->set_wide_handle(true);
    paned->set_hexpand(true);
    paned->set_vexpand(true);
    paned->signal_realize().connect([&]() {
        int total = paned->get_allocated_width();
        paned->set_position(total - 300);
    });

    m_canvas = rt::make_managed<ImageCanvas>(m_canvas_model.get());
    m_canvas->enablePanZoom(true);
    m_canvas->setRenderer(m_editor_renderer.get());
    paned->pack1(*m_canvas, true, true);

    auto scrolled = rt::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scrolled->set_vexpand(true);

    m_control_box = rt::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    m_control_box->set_hexpand(true);
    m_control_box->set_vexpand(true);
    m_control_box->set_spacing(4);

    setupControls();

    scrolled->add(*m_control_box);
    paned->pack2(*scrolled, true, false);

    pack_start(*paned, true, true);

    show_all();

    m_zoom_sense_slider.signal_value_changed().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onSmoothSensitivityChanged));
    m_pan_sense_slider.signal_value_changed().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onSmoothPanSensitivityChanged));
}

void CanvasPlayground::setupControls()
{
    auto header = rt::make_managed<Gtk::Label>("Controls");
    header->set_markup("<span font='18' weight='bold'>Controls</span>");
    m_control_box->pack_start(*header, false, false);

    m_cursor_event_listener = std::make_unique<CursorTracker>();
    m_canvas->addCursorMonitor(m_cursor_event_listener.get());
    {
        auto add_label = [&](const Glib::ustring& text) {
            auto label = rt::make_managed<Gtk::Label>(text);
            m_control_box->pack_start(*label, false, false);
            auto value_label = rt::make_managed<Gtk::Label>();
            m_control_box->pack_start(*value_label, false, false);
            return value_label;
        };
        m_cursor_event_listener->lpos = add_label("Widget Position");
        m_cursor_event_listener->l2c = add_label("Widget -> Camera");
        m_cursor_event_listener->l2w = add_label("Widget -> World");
        m_cursor_event_listener->c2l = add_label("Camera -> Widget");
        m_cursor_event_listener->c2w = add_label("Camera -> World");
        m_cursor_event_listener->w2l = add_label("World -> Widget");
        m_cursor_event_listener->w2c = add_label("World -> Camera");
    }

    m_canvas_model->setCameraBounds(EditorSession::CameraBounds::EDITOR);
    m_camera_bounds = rt::make_managed<Gtk::ComboBoxText>();
    m_camera_bounds->append("Free");
    m_camera_bounds->append("Editor");
    m_camera_bounds->append("Inspector Panel");
    m_camera_bounds->append("Inspector Window");
    m_camera_bounds->set_active(1);
    m_camera_bounds->signal_changed().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onCameraBoundsChanged));
    m_control_box->pack_start(*m_camera_bounds, false, false);

    auto zoom11_button = rt::make_managed<Gtk::Button>("Zoom 1:1");
    zoom11_button->signal_clicked().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onZoom11Clicked));
    m_control_box->pack_start(*zoom11_button, false, false);

    auto zoom_fit_button = rt::make_managed<Gtk::Button>("Zoom Fit");
    zoom_fit_button->signal_clicked().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onZoomFitClicked));
    m_control_box->pack_start(*zoom_fit_button, false, false);

    auto adjustment = Gtk::Adjustment::create(
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_DEFAULT,
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MAX,
        1, 1);
    m_zoom_sense_slider.set_adjustment(adjustment);
    m_zoom_sense_slider.set_digits(0);
    auto sense_label = rt::make_managed<Gtk::Label>("Smooth Scroll Zoom Sensitivity");
    m_control_box->pack_start(*sense_label, false, false);
    m_control_box->pack_start(m_zoom_sense_slider, false, false);

    auto pan_adjustment = Gtk::Adjustment::create(
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_DEFAULT,
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MAX,
        1, 1);
    m_pan_sense_slider.set_adjustment(pan_adjustment);
    m_pan_sense_slider.set_digits(0);
    auto pan_sense_label = rt::make_managed<Gtk::Label>("Smooth Scroll Pan Sensitivity");
    m_control_box->pack_start(*pan_sense_label, false, false);
    m_control_box->pack_start(m_pan_sense_slider, false, false);

    m_discrete_scroll_dir_button =
        rt::make_managed<Gtk::CheckButton>("Reverse discrete scroll direction");
    m_discrete_scroll_dir_button->signal_clicked().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onDiscreteScrollDirectionToggled));
    m_control_box->pack_start(*m_discrete_scroll_dir_button, false, false);

    m_smooth_scroll_dir_button =
        rt::make_managed<Gtk::CheckButton>("Reverse smooth scroll direction");
    m_smooth_scroll_dir_button->signal_clicked().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onSmoothScrollDirectionToggled));
    m_control_box->pack_start(*m_smooth_scroll_dir_button, false, false);

    m_scroll_mode_button = rt::make_managed<Gtk::CheckButton>("Pan on scroll");
    m_scroll_mode_button->signal_clicked().connect(
        sigc::mem_fun(*this, &CanvasPlayground::onScrollModeToggled));
    m_control_box->pack_start(*m_scroll_mode_button, false, false);

    auto text_entry = rt::make_managed<Gtk::Entry>();
    m_control_box->pack_start(*text_entry, false, false);
}

void CanvasPlayground::setupImage()
{
    m_img_src = std::make_unique<StdImageSource>();
    int load_result = m_img_src->load(App::get().argv1());
    if (load_result != IMIO_SUCCESS) {
        fmt::println(stderr, "ERROR: Failed to load image {}",
                     App::get().argv1().c_str());
        switch (load_result) {
            case IMIO_CANNOTREADFILE:
                fmt::println(stderr, "  Cannot read file");
                break;
            case IMIO_INVALIDHEADER:
                fmt::println(stderr, "  Invalid header");
                break;
            case IMIO_HEADERERROR:
                fmt::println(stderr, "  Header error");
                break;
            case IMIO_READERROR:
                fmt::println(stderr, "  Read error");
                break;
            case IMIO_VARIANTNOTSUPPORTED:
                fmt::println(stderr, "  Variant not supported");
                break;
            case IMIO_FILETYPENOTSUPPORTED:
                fmt::println(stderr, "  File type not supported");
                break;
            default:
                fmt::println(stderr, "  Unknown error");
                break;
        }
        return;
    }

    int img_width = 0;
    int img_height = 0;
    m_img_src->getFullSize(img_width, img_height);
    m_img = std::make_unique<Imagefloat>(img_width, img_height);

    ProcParams params;
    ColorTemp color_temp;
    PreviewProps preview_props(0, 0, 600, 400, 1);
    m_img_src->getImage(color_temp, 0, m_img.get(), preview_props,
                        params.toneCurve, params.raw);

    setupImageBuffer();
}

void CanvasPlayground::setupImageBuffer()
{
    int img_width = 0;
    int img_height = 0;
    m_img_src->getFullSize(img_width, img_height);
    IntWorldSize img_size{IntWorldScalar(img_width), IntWorldScalar(img_height)};

    auto img_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32, img_width, img_height);

    unsigned char* data = img_surface->get_data();
    int stride = img_surface->get_stride();

    for (int y = 0; y < img_height; ++y) {
        unsigned char* row = data + y * stride;

        for (int x = 0; x < img_width; ++x) {
            unsigned char* pixel = row + 4 * x;

            m_img->convertTo(m_img->b(y, x), *(pixel + 0));
            m_img->convertTo(m_img->g(y, x), *(pixel + 1));
            m_img->convertTo(m_img->r(y, x), *(pixel + 2));
            *(pixel + 3) = 255;
        }
    }

    img_surface->mark_dirty();
    m_canvas_model->image().setImageSurface(img_surface, img_size);
    m_canvas_model->session()->queueDraw();
}

void CanvasPlayground::onSmoothSensitivityChanged()
{
    m_canvas->setSmoothScrollZoomSensitivity(
        static_cast<int>(m_zoom_sense_slider.get_value()),
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_ZOOM_SENSITIVITY_MAX);
}

void CanvasPlayground::onSmoothPanSensitivityChanged()
{
    m_canvas->setSmoothScrollPanSensitivity(
        static_cast<int>(m_pan_sense_slider.get_value()),
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MIN,
        Options::SMOOTH_SCROLL_PAN_SENSITIVITY_MAX);
}

void CanvasPlayground::onCameraBoundsChanged()
{
    Glib::ustring text = m_camera_bounds->get_active_text();
    auto bounds = EditorSession::CameraBounds::NONE;
    if (text == "Editor") {
        bounds = EditorSession::CameraBounds::EDITOR;
    } else if (text == "Inspector Panel") {
        bounds = EditorSession::CameraBounds::INSPECTOR_PANEL;
    } else if (text == "Inspector Window") {
        bounds = EditorSession::CameraBounds::INSPECTOR_WINDOW;
    }
    m_canvas_model->setCameraBounds(bounds);
}

void CanvasPlayground::onZoom11Clicked()
{
    m_canvas_model->session()->zoom11();
}

void CanvasPlayground::onZoomFitClicked()
{
    m_canvas_model->zoomFit();
}

void CanvasPlayground::onDiscreteScrollDirectionToggled()
{
    m_canvas->setReverseDiscreteScrollDirection(
        m_discrete_scroll_dir_button->get_active());
}

void CanvasPlayground::onSmoothScrollDirectionToggled()
{
    m_canvas->setReverseSmoothScrollDirection(m_smooth_scroll_dir_button->get_active());
}

void CanvasPlayground::onScrollModeToggled()
{
    if (m_scroll_mode_button->get_active()) {
        m_canvas->setScrollMode(ScrollMode::PAN);
    } else {
        m_canvas->setScrollMode(ScrollMode::ZOOM);
    }
}

bool CanvasPlayground::onWindowFocusOut(GdkEventFocus* event)
{
    m_canvas_model->session()->onWindowFocusLost(m_canvas_model.get());
    return false;
}

bool CanvasPlayground::onKeyPressed(guint keyval, guint keycode, GdkModifierType state)
{
    bool handled =  m_canvas->onKeyPressed(keyval, keycode, state);
    if (handled) return true;

    return false;
}

int main(int argc, char* argv[])
{
    auto gtk_app = Gtk::Application::create(
        argc, argv, "org.rawtherapee.canvasplayground",
        Gio::APPLICATION_HANDLES_COMMAND_LINE);

    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(
        R"(
            #RtCanvas {
                background-color: #808080;
            }
        )");
    Gtk::StyleContext::add_provider_for_screen(
        Gdk::Screen::get_default(),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    Gtk::Window window;
    window.set_title("RawTherapee Canvas Playground");
    window.set_default_size(1280, 720);
    window.add_events(Gdk::FOCUS_CHANGE_MASK);

    Glib::RefPtr<Gdk::Screen> screen = Gdk::Screen::get_default();
    if (screen) {
        Gtk::Settings::get_for_screen(screen)->property_gtk_theme_name() = "Adwaita";
        Gtk::Settings::get_for_screen(screen)->property_gtk_application_prefer_dark_theme() = true;
        Gtk::Settings::get_for_screen(screen)->property_gtk_icon_theme_name() = "rawtherapee";
        RTScalable::init(&window);
    }

    CanvasPlayground playground;
    window.signal_focus_out_event().connect(
        sigc::mem_fun(playground, &CanvasPlayground::onWindowFocusOut));

    // To ensure key presses are received by the canvas without having to grab
    // focus, the owning window should forward events instead.
    auto key_controller = rt::gtk4::EventControllerKey::create(&window);
    // When a widget does have focus (e.g. text entry box), the window must not
    // steal key events. Instead, only handle events that weren't consumed by
    // the target/focused widget. This is done by setting the event controller
    // to the BUBBLE phase.
    key_controller->set_propagation_phase(GTK_PHASE_BUBBLE);
    key_controller->signal_key_pressed().connect(
        sigc::mem_fun(playground, &CanvasPlayground::onKeyPressed));
    key_controller->signal_key_released().connect(
        sigc::mem_fun(playground, &CanvasPlayground::onKeyReleased));

    playground.show();
    window.add(playground);

    auto parse_args = [&](const Glib::RefPtr<Gio::ApplicationCommandLine>& cmd) -> int {
        int gio_argc = 0;
        char** gio_argv = cmd->get_arguments(gio_argc);

        if (gio_argc != 2) {
            fmt::println("Usage: rawtherapee-canvas FILE");
            return 1;
        }

        Glib::ustring exe_path = Glib::path_get_dirname(
            Glib::canonicalize_filename(gio_argv[0]));

        auto& app = App::get();
        if (Glib::path_is_absolute(DATA_SEARCH_PATH)) {
            app.setArgv0(DATA_SEARCH_PATH);
        } else {
            app.setArgv0(Glib::build_filename(exe_path, DATA_SEARCH_PATH));
        }
        app.setArgv1(gio_argv[1]);

        Glib::ustring icon_path = Glib::build_filename(App::get().argv0(), "icons");
        Glib::RefPtr<Gtk::IconTheme> default_icon_theme = Gtk::IconTheme::get_default();
        default_icon_theme->append_search_path(icon_path);

        gtk_app->activate();
        return 0;
    };

    gtk_app->signal_command_line().connect(parse_args, false);
    gtk_app->signal_activate().connect([&]() {
        auto& app = App::get();
        app.setArgv0(DATA_SEARCH_PATH);
        app.setCreditsPath(CREDITS_SEARCH_PATH);
        app.setLicensePath(LICENCE_SEARCH_PATH);

        auto& options = app.mut_options();
        options.rtSettings.lensfunDbDirectory = LENSFUN_DB_PATH;
        options.rtSettings.lensfunDbBundleDirectory = LENSFUN_DB_PATH;
        try {
            options.load(true);
        } catch (const Options::Error& e) {
            fmt::println(stderr, "ERROR: {}", e.get_msg().c_str());
            return;
        }

        playground.setupImage();
        window.show();

        gtk_app->add_window(window);
    });

    return gtk_app->run(argc, argv);
}

void CursorTracker::update(const CanvasModel* model, WidgetPoint pos)
{
    const CameraState& camera = model->session()->camera();

    CameraPoint l2c_pos = widgetToCamera(pos, camera);
    WorldPoint l2w_pos = widgetToWorld(pos, camera);

    WidgetPoint c2l_pos = cameraToWidget(l2c_pos, camera);
    WorldPoint c2w_pos = cameraToWorld(l2c_pos, camera);

    WidgetPoint w2l_pos = worldToWidget(l2w_pos, camera);
    CameraPoint w2c_pos = worldToCamera(l2w_pos, camera);

    updatePosLabel(lpos, pos);
    updatePosLabel(l2c, l2c_pos);
    updatePosLabel(l2w, l2w_pos);
    updatePosLabel(c2l, c2l_pos);
    updatePosLabel(c2w, c2w_pos);
    updatePosLabel(w2l, w2l_pos);
    updatePosLabel(w2c, w2c_pos);
}
