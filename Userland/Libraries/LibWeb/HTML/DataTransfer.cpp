/*
 * Copyright (c) 2024, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Realm.h>
#include <LibWeb/Bindings/DataTransferPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/FileAPI/Blob.h>
#include <LibWeb/FileAPI/File.h>
#include <LibWeb/FileAPI/FileList.h>
#include <LibWeb/HTML/DataTransfer.h>

namespace Web::HTML {

JS_DEFINE_ALLOCATOR(DataTransfer);

namespace DataTransferEffect {

#define __ENUMERATE_DATA_TRANSFER_EFFECT(name) FlyString name = #name##_fly_string;
ENUMERATE_DATA_TRANSFER_EFFECTS
#undef __ENUMERATE_DATA_TRANSFER_EFFECT

}

JS::NonnullGCPtr<DataTransfer> DataTransfer::construct_impl(JS::Realm& realm)
{
    return realm.heap().allocate<DataTransfer>(realm, realm);
}

DataTransfer::DataTransfer(JS::Realm& realm)
    : PlatformObject(realm)
{
}

DataTransfer::~DataTransfer() = default;

void DataTransfer::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(DataTransfer);
}

void DataTransfer::set_drop_effect(String const& drop_effect)
{
    set_drop_effect(FlyString { drop_effect });
}

void DataTransfer::set_drop_effect(FlyString drop_effect)
{
    using namespace DataTransferEffect;

    // On setting, if the new value is one of "none", "copy", "link", or "move", then the attribute's current value must
    // be set to the new value. Other values must be ignored.
    if (drop_effect.is_one_of(none, copy, link, move))
        m_drop_effect = AK::move(drop_effect);
}

void DataTransfer::set_effect_allowed(String const& effect_allowed)
{
    set_effect_allowed(FlyString { effect_allowed });
}

void DataTransfer::set_effect_allowed(FlyString effect_allowed)
{
    // On setting, if drag data store's mode is the read/write mode and the new value is one of "none", "copy", "copyLink",
    // "copyMove", "link", "linkMove", "move", "all", or "uninitialized", then the attribute's current value must be set
    // to the new value. Otherwise, it must be left unchanged.
    if (m_associated_drag_data_store.has_value() && m_associated_drag_data_store->mode() == DragDataStore::Mode::ReadWrite)
        set_effect_allowed_internal(move(effect_allowed));
}

void DataTransfer::set_effect_allowed_internal(FlyString effect_allowed)
{
    // AD-HOC: We need to be able to set the effectAllowed attribute internally regardless of the state of the drag data store.
    using namespace DataTransferEffect;

    if (effect_allowed.is_one_of(none, copy, copyLink, copyMove, link, linkMove, move, all, uninitialized))
        m_effect_allowed = AK::move(effect_allowed);
}

// https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-types
ReadonlySpan<String> DataTransfer::types() const
{
    // The types attribute must return this DataTransfer object's types array.
    return m_types;
}

// https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-files
JS::NonnullGCPtr<FileAPI::FileList> DataTransfer::files() const
{
    auto& realm = this->realm();

    // 1. Start with an empty list L.
    auto files = FileAPI::FileList::create(realm);

    // 2. If the DataTransfer object is no longer associated with a drag data store, the FileList is empty. Return
    //    the empty list L.
    if (!m_associated_drag_data_store.has_value())
        return files;

    // 3. If the drag data store's mode is the protected mode, return the empty list L.
    if (m_associated_drag_data_store->mode() == DragDataStore::Mode::Protected)
        return files;

    // 4. For each item in the drag data store item list whose kind is File, add the item's data (the file, in
    //    particular its name and contents, as well as its type) to the list L.
    for (auto const& item : m_associated_drag_data_store->item_list()) {
        if (item.kind != DragDataStoreItem::Kind::File)
            continue;

        auto blob = FileAPI::Blob::create(realm, item.data, item.type_string);

        // FIXME: The FileAPI should use ByteString for file names.
        auto file_name = MUST(String::from_byte_string(item.file_name));

        // FIXME: Fill in other fields (e.g. last_modified).
        FileAPI::FilePropertyBag options {};
        options.type = item.type_string;

        auto file = MUST(FileAPI::File::create(realm, { JS::make_handle(blob) }, file_name, move(options)));
        files->add_file(file);
    }

    // 5. The files found by these steps are those in the list L.
    return files;
}

void DataTransfer::associate_with_drag_data_store(DragDataStore& drag_data_store)
{
    m_associated_drag_data_store = drag_data_store;
    update_data_transfer_types_list();
}

void DataTransfer::disassociate_with_drag_data_store()
{
    m_associated_drag_data_store.clear();
    update_data_transfer_types_list();
}

// https://html.spec.whatwg.org/multipage/dnd.html#concept-datatransfer-types
void DataTransfer::update_data_transfer_types_list()
{
    // 1. Let L be an empty sequence.
    Vector<String> types;

    // 2. If the DataTransfer object is still associated with a drag data store, then:
    if (m_associated_drag_data_store.has_value()) {
        bool contains_file = false;

        // 1. For each item in the DataTransfer object's drag data store item list whose kind is text, add an entry to L
        //    consisting of the item's type string.
        for (auto const& item : m_associated_drag_data_store->item_list()) {
            switch (item.kind) {
            case DragDataStoreItem::Kind::Text:
                types.append(item.type_string);
                break;
            case DragDataStoreItem::Kind::File:
                contains_file = true;
                break;
            }
        }

        // 2. If there are any items in the DataTransfer object's drag data store item list whose kind is File, then add
        //    an entry to L consisting of the string "Files". (This value can be distinguished from the other values
        //    because it is not lowercase.)
        if (contains_file)
            types.append("Files"_string);
    }

    // 3. Set the DataTransfer object's types array to the result of creating a frozen array from L.
    m_types = move(types);
}

}
