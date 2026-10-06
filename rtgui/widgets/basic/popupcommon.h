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
 *
 *  Class created by Jean-Christophe FRISCH, aka 'Hombre'
 */
#pragma once

#include "threadutils.h"

#include <memory>
#include <vector>

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <sigc++/signal.h>

namespace Gio {
class Icon;
}

namespace Gtk {

class Grid;
class Menu;
class Button;
class ImageMenuItem;
class RadioButtonGroup;
class Widget;

}  // namespace Gtk

typedef struct _GdkEventButton GdkEventButton;

class RTImage;

class PopUpCommon
{

public:
    typedef sigc::signal<void, int> type_signal_changed;
    typedef sigc::signal<void, int> type_signal_item_selected;
    type_signal_changed signal_changed();
    type_signal_item_selected signal_item_selected();
    Gtk::Grid* buttonGroup;  // this is the widget to be packed

    explicit PopUpCommon(Gtk::Button* button, const Glib::ustring& label = "");
    virtual ~PopUpCommon();
    bool addEntry(const Glib::ustring& iconName,
                  const Glib::ustring& label,
                  Gtk::RadioButtonGroup* radioGroup = nullptr);
    bool insertEntry(int position,
                     const Glib::ustring& iconName,
                     const Glib::ustring& label,
                     Gtk::RadioButtonGroup* radioGroup = nullptr);
    bool insertEntry(int position,
                     const Glib::RefPtr<const Gio::Icon>& gIcon,
                     const Glib::ustring& label,
                     Gtk::RadioButtonGroup* radioGroup = nullptr);
    /// Sets the button image to show when there are no entries.
    void setEmptyImage(const Glib::ustring& fileName);
    int getEntryCount() const;
    bool setSelected(int entryNum);
    int getSelected() const;
    void removeEntry(int position);
    void setButtonHint();
    void show();
    void set_tooltip_text(const Glib::ustring& text);
    void setItemSensitivity(int i, bool isSensitive);

private:
    type_signal_changed messageChanged;
    type_signal_item_selected messageItemSelected;

    Glib::ustring emptyImageFilename;
    std::vector<Glib::RefPtr<const Gio::Icon>> imageIcons;
    std::vector<Glib::ustring> imageIconNames;
    std::vector<const RTImage*> images;
    Glib::ustring buttonHint;
    RTImage* buttonImage;
    Gtk::Grid* imageContainer;
    std::unique_ptr<Gtk::Menu> menu;
    Gtk::Button* button;
    Gtk::Button* arrowButton;
    int selected;
    bool hasMenu;
    MyMutex entrySelectionMutex;

    void changeImage(int position);
    void changeImage(const Glib::ustring& iconName,
                     const Glib::RefPtr<const Gio::Icon>& gIcon);
    void entrySelected(Gtk::Widget* menuItem);
    bool insertEntryImpl(int position,
                         const Glib::ustring& iconName,
                         const Glib::RefPtr<const Gio::Icon>& gIcon,
                         RTImage* image,
                         const Glib::ustring& label,
                         Gtk::RadioButtonGroup* radioGroup);
    void showMenu(GdkEventButton* event);

protected:
    virtual int posToIndex(int p) const { return p; }
    virtual int indexToPos(int i) const { return i; }

    void entrySelected(int i);
};

inline PopUpCommon::type_signal_changed PopUpCommon::signal_changed()
{
    return messageChanged;
}

inline PopUpCommon::type_signal_item_selected PopUpCommon::signal_item_selected()
{
    return messageItemSelected;
}

inline int PopUpCommon::getEntryCount() const
{
    return images.size();
}

inline int PopUpCommon::getSelected() const
{
    return posToIndex(selected);
}
