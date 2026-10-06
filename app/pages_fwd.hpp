#pragma once

#include "context.hpp"

/**
 * @brief Forward declarations breaking the mutual page-to-page navigation cycle (Home ->
 * Library, Library -> Home via Back, Settings -> Home, ...): header-only components can't
 * form that cycle across separate files the way old C translation units could (a .c file
 * can call a function only forward-declared in a shared .h; an inline function's body needs
 * the callee's declaration in the SAME translation unit). Every page's own header includes
 * this one and defines its factory/handler *bodies* against these signatures alone -- none
 * of them need `pages` complete, since they only ever call another page's factory through a
 * pointer, never touch `nav.<other_page>` fields directly.
 *
 * The few functions that DO dereference `nav.<specific_page>` (the eight `page_*` factories
 * themselves, plus `show_book_info`/`show_delete_confirm`/`show_about_confirm`) are defined
 * in app.hpp, after `struct pages` is complete -- that's the one place in this split that
 * still needs every page's full type.
 */
namespace app {

struct pages;
struct library_page;

inline page::context* page_splash(context& app, pages& nav);
inline page::context* page_home(context& app, pages& nav);
inline page::context* page_library(context& app, pages& nav);
inline page::context* page_favorites(context& app, pages& nav);
inline page::context* page_file_manager(context& app, pages& nav);
inline page::context* page_settings(context& app, pages& nav);
inline page::context* page_sleep(context& app, pages& nav);
inline page::context* page_reader(context& app, pages& nav);

inline void open_book(context& app, pages& nav, int index);
inline void show_book_info(context& app, pages& nav, int index);
inline void show_delete_confirm(context& app, pages& nav, library_page& requester);
inline void show_about_confirm(context& app, pages& nav);

}  // namespace app
