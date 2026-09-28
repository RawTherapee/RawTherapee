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

#include <gtkmm.h>

#include "guiutils.h"
#include "multilangmgr.h"
#include "popupcommon.h"
#include "rtimage.h"
#include "threadutils.h"

PopUpCommon::PopUpCommon (Gtk::Button* thisButton, const Glib::ustring& label)
    : buttonImage (nullptr)
    , menu(new Gtk::Menu())
    , selected (-1) // -1 means that the button is invalid
{
    button = thisButton;
    hasMenu = false;
    imageContainer = Gtk::manage( new Gtk::Grid());
    setExpandAlignProperties(imageContainer, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    button->set_relief (Gtk::RELIEF_NORMAL);
    button->add(*imageContainer);

    if (!label.empty()) {
        Gtk::Label* buttonLabel = Gtk::manage ( new Gtk::Label(label + " ") );
        setExpandAlignProperties(buttonLabel, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
        imageContainer->attach(*buttonLabel, 0, 0, 1, 1);
    }

    // Create the global container and put the button in it
    buttonGroup = Gtk::manage( new Gtk::Grid());
    setExpandAlignProperties(buttonGroup, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    buttonGroup->attach(*button, 0, 0, 1, 1);
    buttonGroup->get_style_context()->add_class("image-combo");

    // Create the image for the button
    buttonImage = Gtk::manage(new RTImage());
    setExpandAlignProperties(buttonImage, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    imageContainer->attach_next_to(*buttonImage, Gtk::POS_RIGHT, 1, 1);
    buttonImage->set_no_show_all();

    // Create the button for showing the pop-up.
    arrowButton = Gtk::manage(new Gtk::Button());
    Gtk::Image *arrowImage = Gtk::manage(new Gtk::Image());
    arrowImage->set_from_icon_name("pan-down-symbolic", Gtk::ICON_SIZE_BUTTON);
    setExpandAlignProperties(arrowButton, false, false, Gtk::ALIGN_CENTER, Gtk::ALIGN_FILL);
    arrowButton->add(*arrowImage); //menuSymbol);
    arrowImage->show();
    buttonGroup->attach_next_to(*arrowButton, *button, Gtk::POS_RIGHT, 1, 1);
    arrowButton->signal_button_release_event().connect_notify(sigc::mem_fun(*this, &PopUpCommon::showMenu));
    arrowButton->get_style_context()->add_class("Right");
    arrowButton->get_style_context()->add_class("popupbutton-arrow");
    arrowButton->set_no_show_all();
}

PopUpCommon::~PopUpCommon ()
{
}

bool PopUpCommon::addEntry (const Glib::ustring& fileName, const Glib::ustring& label, Gtk::RadioButtonGroup* radioGroup)
{
    return insertEntry(getEntryCount(), fileName, label, radioGroup);
}

bool PopUpCommon::insertEntry(int position, const Glib::ustring& iconName, const Glib::ustring& label, Gtk::RadioButtonGroup *radioGroup)
{
    RTImage* image = nullptr;
    if (!iconName.empty()) {
        image = Gtk::manage(new RTImage(iconName));
    }
    bool success = insertEntryImpl(position, iconName, Glib::RefPtr<const Gio::Icon>(), image, label, radioGroup);
    if (!success && image) {
        delete image;
    }
    return success;
}

bool PopUpCommon::insertEntry(int position, const Glib::RefPtr<const Gio::Icon>& gIcon, const Glib::ustring& label, Gtk::RadioButtonGroup *radioGroup)
{
    auto image = Gtk::manage(new RTImage(gIcon, Gtk::ICON_SIZE_BUTTON));
    bool success = insertEntryImpl(position, "", gIcon, image, label, radioGroup);
    if (!success) {
        delete image;
    }
    return success;
}

bool PopUpCommon::insertEntryImpl(int position, const Glib::ustring& iconName, const Glib::RefPtr<const Gio::Icon>& gIcon, RTImage* image, const Glib::ustring& label, Gtk::RadioButtonGroup* radioGroup)
{
    if (label.empty() || position < 0 || position > getEntryCount())
        return false;

    // Create the menu item and image
    Gtk::MenuItem *newItem;
    if (radioGroup) {
        newItem = Gtk::manage(new MyRadioImageMenuItem(label, image, *radioGroup));
    }
    else {
        newItem = Gtk::manage(new MyImageMenuItem(label, image));
    }
    imageIcons.insert(imageIcons.begin() + position, gIcon);
    imageIconNames.insert(imageIconNames.begin() + position, iconName);
    images.insert(images.begin() + position, image);

    // When there is at least 1 choice, we add the arrow button
    if (images.size() == 1) {
        changeImage(iconName, gIcon);
        buttonImage->show();
        selected = 0;
        button->get_style_context()->add_class("Left");
        arrowButton->show();
        hasMenu = true;
    } else if (position <= selected) {
        selected++;
    }

    void (PopUpCommon::*entrySelectedFunc)(Gtk::Widget *) = &PopUpCommon::entrySelected;
    newItem->signal_activate ().connect (sigc::bind (sigc::mem_fun (*this, entrySelectedFunc), newItem));
    menu->insert(*newItem, position);
    return true;
}

void PopUpCommon::setEmptyImage(const Glib::ustring &fileName)
{
    emptyImageFilename = fileName;

    if (getEntryCount()) {
        return;
    }
    if (fileName.empty()) {
        buttonImage->hide();
    } else {
        changeImage(emptyImageFilename, Glib::RefPtr<const Gio::Icon>());
        buttonImage->show();
    }
}

void PopUpCommon::removeEntry(int position)
{
    if (position < 0 || position >= getEntryCount()) {
        return;
    }

    if (getEntryCount() == 1) { // Last of the entries.
        // Hide the arrow button.
        button->get_style_context()->remove_class("Left");
        arrowButton->hide();
        hasMenu = false;
        if (emptyImageFilename.empty()) {
            // Remove the button image.
            buttonImage->hide();
        } else {
            // Show the empty icon.
            changeImage(emptyImageFilename, Glib::RefPtr<const Gio::Icon>());
        }
        selected = -1;
    }
    else if (position < selected) {
        selected--;
    }
    else if (position == selected) { // Select a different entry before removing.
        int nextSelection = position + (position == getEntryCount() - 1 ? -1 : 1);
        changeImage(nextSelection);
        setButtonHint();
    }

    std::unique_ptr<Gtk::Widget> menuItem(menu->get_children()[position]);
    menu->remove(*menuItem);
    imageIcons.erase(imageIcons.begin() + position);
    imageIconNames.erase(imageIconNames.begin() + position);
    images.erase(images.begin() + position);
}

void PopUpCommon::changeImage(int position)
{
    changeImage(imageIconNames.at(position), imageIcons.at(position));
}

void PopUpCommon::changeImage(const Glib::ustring& iconName, const Glib::RefPtr<const Gio::Icon>& gIcon)
{
    if (!iconName.empty()) {
        buttonImage->set_from_icon_name(iconName, Gtk::ICON_SIZE_BUTTON);
    } else {
        buttonImage->set_from_gicon(gIcon, Gtk::ICON_SIZE_BUTTON);
    }
}

void PopUpCommon::entrySelected(Gtk::Widget* widget)
{
    if (widget != menu->get_active()) { // Not actually selected.
        return;
    }

    if (!entrySelectionMutex.trylock()) { // Already being updated.
        return;
    }
    entrySelectionMutex.unlock();

    int i = 0;
    for (const auto & child : menu->get_children()) {
        if (widget == child) {
            break;
        }
        i++;
    }

    entrySelected(i);
}

void PopUpCommon::entrySelected(int i)
{
    // Emit a signal if the selected item has changed
    if (setSelected (posToIndex(i)))
        messageChanged (posToIndex(selected));

    // Emit a signal in all case (i.e. propagate the signal_activate event)
    messageItemSelected (posToIndex(selected));
}

void PopUpCommon::setItemSensitivity (int index, bool isSensitive) {
    const auto items = menu->get_children ();
    size_t pos = indexToPos(index);
    if (pos < items.size ()) {
        items[pos]->set_sensitive (isSensitive);
    }
}


/*
 * Set the button image with the selected item
 */
bool PopUpCommon::setSelected (int entryNum)
{
    entryNum = indexToPos(entryNum);

    if (entryNum < 0 || entryNum > ((int)images.size() - 1) || (int)entryNum == selected) {
        return false;
    } else {
        // Maybe we could do something better than loading the image file each time the selection is changed !?
        changeImage(entryNum);
        selected = entryNum;
        setButtonHint();

        auto radioMenuItem = dynamic_cast<Gtk::RadioMenuItem*>(menu->get_children()[entryNum]);
        if (radioMenuItem && !radioMenuItem->get_active()) {
            MyMutex::MyLock updateLock(entrySelectionMutex);
            radioMenuItem->set_active();
        }

        return true;
    }
}

void PopUpCommon::show()
{
    menu->reposition();
    setButtonHint();
    menu->show_all();
    buttonGroup->show_all();
}

void PopUpCommon::setButtonHint()
{
    Glib::ustring hint;

    if (!buttonHint.empty()) {
        hint = buttonHint;

        if (selected > -1) {
            hint += " ";
        }
    }

    if (selected > -1) {
        auto widget = menu->get_children ()[selected];
        auto item = dynamic_cast<MyImageMenuItemInterface*>(widget);

        if (item) {
            hint += escapeHtmlChars(item->getLabel()->get_text());
        }
    }

    button->set_tooltip_markup(hint);
}

void PopUpCommon::showMenu(GdkEventButton* event)
{
    if (event->button == 1) {
        menu->popup_at_widget(arrowButton, Gdk::GRAVITY_SOUTH_WEST, Gdk::GRAVITY_NORTH_WEST, nullptr);
    }
}

void PopUpCommon::set_tooltip_text (const Glib::ustring &text)
{
    buttonHint = text;
    setButtonHint();
}
