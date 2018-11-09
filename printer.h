/*
 * Copyright © 2016 Octopull Limited
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef EGMDE_PRINTER_H
#define EGMDE_PRINTER_H

#include <mir_toolkit/client_types.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <codecvt>
#include <locale>

namespace egmde
{
struct Printer
{
    Printer();
    ~Printer();
    Printer(Printer const&) = delete;
    Printer& operator=(Printer const&) = delete;

    void print(MirGraphicsRegion const& region, std::string const& title);
    void footer(MirGraphicsRegion const& region, std::initializer_list<char const*> const& lines);

private:
    struct Codecvt : std::codecvt_byname<wchar_t, char, std::mbstate_t>
    {
        Codecvt() : std::codecvt_byname<wchar_t, char, std::mbstate_t>("C") {}
        ~Codecvt() = default;
    };

    std::wstring_convert<Codecvt> converter;

    FT_Library lib;
    FT_Face face;
};
}

#endif //EGMDE_PRINTER_H
