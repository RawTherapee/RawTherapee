/*
*  This file is part of RawTherapee.
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

#include "editwindow.h"
#include "editorpanel.h"
#include "filepanel.h"
#include "rtengine/procparams.h"
#include "options.h"
#include "preferences.h"
#include "cursormanager.h"
#include "rtwindow.h"
#include <gtk/gtk.h>
#include "threadutils.h"

// Check if the system has more than one display and option is set
bool EditWindow::isMultiDisplayEnabled()
{
    const auto screen = Gdk::Screen::get_default();

    if (screen) {
        return App::get().options().multiDisplayMode > 0 && screen->get_display()->get_n_monitors() > 1;
    } else {
        return false; // There is no default screen
    }
}

// Should only be created once
EditWindow* EditWindow::getInstance(RTWindow* p)
{
    struct EditWindowInstance
    {
        EditWindow editWnd;

        explicit EditWindowInstance(RTWindow* p) : editWnd(p)
        {
        }
    };

    static EditWindowInstance instance_(p);
    return &instance_.editWnd;
}

EditWindow::EditWindow (RTWindow* p)
    : parent(p)
    , isFullscreen(false)
    , isClosed(true)
    , isMinimized(false)
{
    // For UNIX system, set app icon
#ifndef _WIN32
    set_default_icon_name("rawtherapee");
#endif

    set_title_decorated("");
    set_modal(false);
    set_resizable(true);
    const auto& options = App::get().options();
    set_default_size(options.meowWidth, options.meowHeight);

    property_destroy_with_parent().set_value(false);

    mainNB = Gtk::manage(new Gtk::Notebook ());
    mainNB->set_scrollable(true);
    mainNB->signal_switch_page().connect_notify(sigc::mem_fun(*this, &EditWindow::on_mainNB_switch_page));

    signal_key_press_event().connect(sigc::mem_fun(*this, &EditWindow::keyPressed));
    signal_window_state_event().connect(sigc::mem_fun(*this, &EditWindow::on_window_state_event));
    onConfEventConn = signal_configure_event().connect(sigc::mem_fun(*this, &EditWindow::on_configure_event));

    Gtk::Box* mainBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    mainBox->pack_start(*mainNB);

    add(*mainBox);

}

void EditWindow::restoreWindow()
{
    if (isClosed) {
        onConfEventConn.block(true); // Avoid getting size and position while window is being moved, maximized, ...

        int meowMonitor = 0; // By default, set to main monitor
        const auto display = get_screen()->get_display();
        const auto& options = App::get().options();

        if (isMultiDisplayEnabled()) {
            if (options.meowMonitor >= 0) { // Use display from last session if available
                meowMonitor = std::max(0, std::min(options.meowMonitor, display->get_n_monitors() - 1));
            } else { // Determine the main RT window display
                const Glib::RefPtr<Gdk::Window> &wnd = parent->get_window();

                // Retrieve window monitor ID
                const int monitor_nb = display->get_n_monitors();

                for (int id = 0; id < monitor_nb; id++) {
                    if (display->get_monitor_at_window(wnd) == display->get_monitor(id)) {
                        meowMonitor = id;
                        break;
                    }
                }
            }
        }

        Gdk::Rectangle lMonitorRect;
        display->get_monitor(meowMonitor)->get_geometry(lMonitorRect);

#ifdef __APPLE__
        // Get macOS menu bar height
        Gdk::Rectangle lWorkAreaRect;
        display->get_monitor(std::min(meowMonitor, display->get_n_monitors() - 1))->get_workarea(lWorkAreaRect);
        const int macMenuBarHeight = lWorkAreaRect.get_y();

        // Place RT window to saved one in options file
        if (options.meowX <= lMonitorRect.get_x() + lMonitorRect.get_width()
                && options.meowX >= 0
                && options.meowY <= lMonitorRect.get_y() + lMonitorRect.get_height() - macMenuBarHeight
                && options.meowY >= 0) {
            move(options.meowX, options.meowY + macMenuBarHeight);
        } else {
            move(lMonitorRect.get_x(), lMonitorRect.get_y() + macMenuBarHeight);
        }
#else
        // Place RT window to saved one in options file
        if (options.meowX <= lMonitorRect.get_x() + lMonitorRect.get_width()
                && options.meowX >= 0
                && options.meowY <= lMonitorRect.get_y() + lMonitorRect.get_height()
                && options.meowY >= 0) {
            move(options.meowX, options.meowY);
        } else {
            move(lMonitorRect.get_x(), lMonitorRect.get_y());
        }
#endif

        // Maximize RT window according to options file
        if (options.meowMaximized) {
            maximize();
        } else {
            unmaximize();
            resize(options.meowWidth, options.meowHeight);
        }

        isClosed = false;

        onConfEventConn.block(false);
    }
}

void EditWindow::on_realize()
{
    Gtk::Window::on_realize();

    CursorManager::forEditWindow().init(get_window());
}

bool EditWindow::on_configure_event(GdkEventConfigure* event)
{
    auto& options = App::get().mut_options();
    if (!options.meowMaximized && !isFullscreen && !isMinimized) {
        get_position(options.meowX, options.meowY);
        get_size(options.meowWidth, options.meowHeight);
    }

    return Gtk::Widget::on_configure_event(event);
}

bool EditWindow::on_window_state_event(GdkEventWindowState* event)
{
    auto& options = App::get().mut_options();
    // Retrieve RT window states
    options.meowMaximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
    isMinimized = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
    isFullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    return Gtk::Widget::on_window_state_event(event);
}

void EditWindow::on_mainNB_switch_page(Gtk::Widget* widget, guint page_num)
{
    //if (page_num > 1) {
    EditorPanel *ep = static_cast<EditorPanel*>(widget);

    if (mainNB->get_n_pages() > 1 && page_num <= (filesEdited.size() - 1)) {
        set_title_decorated(ep->getFileName());
    }

    ep->setAspect();
    //}
}

void EditWindow::addEditorPanel (EditorPanel* ep, const std::string &name)
{
    ep->setParent (parent);
    ep->setParentWindow(this);
    ep->setExternalEditorChangedSignal(&externalEditorChangedSignal);

    // construct closeable tab for the image
    Gtk::Box* hb = Gtk::manage (new Gtk::Box ());
    hb->pack_start (*Gtk::manage (new RTImage ("aperture")));
    hb->pack_start (*Gtk::manage (new Gtk::Label (Glib::path_get_basename (name))));
    hb->set_tooltip_markup (name);
    Gtk::Button* closeb = Gtk::manage (new Gtk::Button ());
    closeb->set_image (*Gtk::manage(new RTImage ("cancel-small", Gtk::ICON_SIZE_BUTTON)));
    closeb->set_relief (Gtk::RELIEF_NONE);
    closeb->set_focus_on_click (false);

    // make the button as small as possible thanks via css
    closeb->set_name("notebook_close_button");

    closeb->signal_clicked().connect( sigc::bind (sigc::mem_fun(*this, &EditWindow::remEditorPanel) , ep));
    hb->pack_end (*closeb);
    hb->set_spacing (2);
    hb->show_all ();

    mainNB->append_page (*ep, *hb);
    mainNB->set_current_page (mainNB->page_num (*ep));
    mainNB->set_tab_reorderable (*ep, true);

    set_title_decorated(name);

    epanels[ name ] = ep;
    filesEdited.insert ( name );
    parent->fpanel->refreshEditedState (filesEdited);

    show_all();
}

void EditWindow::remEditorPanel (EditorPanel* ep)
{
    if (ep->getIsProcessing()) {
        return;    // Will crash if destroyed while loading
    }

    ep->setExternalEditorChangedSignal(nullptr);
    epanels.erase (ep->getFileName());
    filesEdited.erase (ep->getFileName ());
    parent->fpanel->refreshEditedState (filesEdited);

    mainNB->remove_page (*ep);

    if (mainNB->get_n_pages() > 0) {
        EditorPanel* ep1 = static_cast<EditorPanel*>(mainNB->get_nth_page (mainNB->get_current_page()));
        set_title_decorated(ep1->getFileName());
    } else {
        set_title_decorated("");
    }

    // TODO: save options if wanted
}

bool EditWindow::selectEditorPanel(const std::string &name)
{
    std::map<Glib::ustring, EditorPanel*>::iterator iep = epanels.find(name);

    if (iep != epanels.end()) {
        mainNB->set_current_page (mainNB->page_num (*iep->second));
        set_title_decorated(name);
        return true;
    }

    return false;
}

void EditWindow::toFront ()
{
    // When using the secondary window on the same monitor as the primary window we need to present the secondary window.
    // If we don't, it will stay in background when opening 2nd, 3rd... editor, which is annoying
    // It will also deiconify the window
    // To avoid unexpected behavior while window is being updated, present() function is called after at idle
    idle_register.add(
        [this]()-> bool
        {
            onConfEventConn.block(true); // Avoid getting size and position while window is being moved, maximized, ...
            present();
            onConfEventConn.block(false);

            return false;
        }
    );
}

bool EditWindow::keyPressed (GdkEventKey* event)
{
    bool ctrl = event->state & GDK_CONTROL_MASK;

    if(event->keyval == GDK_KEY_F11) {
        toggleFullscreen();
        return true;
    } else {
        if(mainNB->get_n_pages () > 0) { //pass the handling for the editor panels, if there are any
            if (event->keyval == GDK_KEY_w && ctrl) { //remove editor panel
                EditorPanel* ep = static_cast<EditorPanel*>(mainNB->get_nth_page (mainNB->get_current_page()));
                remEditorPanel (ep);
                return true;
            } else if(mainNB->get_n_pages () > 0) {
                EditorPanel* ep = static_cast<EditorPanel*>(mainNB->get_nth_page (mainNB->get_current_page()));
                return ep->handleShortcutKey (event);
            }
        }

        return false;
    }
}

void EditWindow::toggleFullscreen()
{
    onConfEventConn.block(true); // Avoid getting size and position while window is getting fullscreen

    isFullscreen ? unfullscreen() : fullscreen();

    onConfEventConn.block(false);
}

void EditWindow::get_position(int& x, int& y) const
{
    // Call native function
    Gtk::Window::get_position(x, y);

    // Retrieve display (concatenation of all monitors) size
    int width = 0, height = 0;
    const auto display = get_screen()->get_display();
    const int nbMonitors = display->get_n_monitors();

    for (int i = 0; i < nbMonitors; i++) {
        Gdk::Rectangle lMonitorRect;
        display->get_monitor(i)->get_geometry(lMonitorRect);
        width = std::max(width, lMonitorRect.get_x() + lMonitorRect.get_width());
        height = std::max(height, lMonitorRect.get_y() + lMonitorRect.get_height());
    }

    // Saturate position at monitor limits to avoid unexpected behavior (fixes #6233)
    x = std::min(width, std::max(0, x));
    y = std::min(height, std::max(0, y));
}

void EditWindow::writeOptions()
{
    auto& options = App::get().mut_options();
    if (is_visible()) {
        if (isMultiDisplayEnabled()) {
            // Retrieve window monitor ID
            options.meowMonitor = 0;
            const auto display = get_screen()->get_display();
            const int monitor_nb = display->get_n_monitors();

            for (int id = 0; id < monitor_nb; id++) {
                if (display->get_monitor_at_window(get_window()) == display->get_monitor(id)) {
                    options.windowMonitor = id;
                    break;
                }
            }
        }

        if (!options.meowMaximized && !isFullscreen && !isMinimized) {
            get_position(options.meowX, options.meowY);
            get_size(options.meowWidth, options.meowHeight);
        }
    }
}

bool EditWindow::on_delete_event(GdkEventAny* event)
{

    if (!closeOpenEditors()) {
        return true;
    }

    writeOptions();
    hide();
    isClosed = true;

    return false;
}

bool EditWindow::isProcessing ()
{
    for ( std::set <Glib::ustring>::iterator iter = filesEdited.begin(); iter != filesEdited.end(); ++iter ) {
        if (epanels[*iter]->getIsProcessing()) {
            return true;
        }
    }

    return false;
}

bool EditWindow::closeOpenEditors()
{
    // Check if any editor is still processing, and do NOT quit if so. Otherwise crashes and inconsistent caches
    if (isProcessing()) {
        return false;
    }

    if (epanels.size()) {
        int page = mainNB->get_current_page();
        Gtk::Widget *w = mainNB->get_nth_page(page);
        bool optionsWritten = false;

        for (std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin(); i != epanels.end(); ++i) {
            if (i->second == w) {
                i->second->writeOptions();
                optionsWritten = true;
            }
        }

        if (!optionsWritten) {
            // fallback solution: save the options of the first editor panel
            std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin();
            i->second->writeOptions();
        }
    }

    for ( std::set <Glib::ustring>::iterator iter = filesEdited.begin(); iter != filesEdited.end(); ++iter ) {
        mainNB->remove_page (*epanels[*iter]);
    }

    epanels.clear();
    filesEdited.clear();
    parent->fpanel->refreshEditedState (filesEdited);

    return true;
}

void EditWindow::set_title_decorated(Glib::ustring fname)
{
    Glib::ustring subtitle;

    if (!fname.empty()) {
        subtitle = " - " + fname;
    }

    set_title("RawTherapee " + M("EDITWINDOW_TITLE") + subtitle);
}

void EditWindow::updateExternalEditorWidget(int selectedIndex, const std::vector<ExternalEditor> &editors)
{
    for (const auto& panel : epanels) {
        panel.second->updateExternalEditorWidget(selectedIndex, editors);
    }
}

void EditWindow::updateToolPanelToolLocations(
        const std::vector<Glib::ustring> &favorites, bool cloneFavoriteTools)
{
    for (const auto& panel : epanels) {
        panel.second->updateToolPanelToolLocations(favorites, cloneFavoriteTools);
    }
}
