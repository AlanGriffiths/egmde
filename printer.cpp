/*
 * Copyright © 2016, 2018 Octopull Limited
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

#include "printer.h"

#include <unistd.h>
#include <iostream>

namespace
{
auto default_font() -> char const*
{
    char const* const default_file = "FreeSansBold.ttf";

    static std::string result{default_file};

    char const* const debian_path = "/usr/share/fonts/truetype/freefont/";
    char const* const fedora_path = "/usr/share/fonts/gnu-free/";

    for (auto const path : { debian_path, fedora_path })
    {
        auto const full_path = std::string{path} + default_file;
        if (access(full_path.c_str(), R_OK) == 0)
            result = full_path;
    }

    return result.c_str();
}
}

egmde::Printer::Printer()
{
    static char const* font_file = getenv("MIRCADE_FONT");

    if (!font_file) font_file = default_font();

    if (FT_Init_FreeType(&lib))
        return;

    if (FT_New_Face(lib, font_file, 0, &face))
    {
        FT_Done_FreeType(lib);
        throw std::runtime_error{std::string{"WARNING: failed to load titlebar font: \""} +  font_file + "\"\n"};
    }
}

egmde::Printer::~Printer()
{
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
}

void egmde::Printer::print(MirGraphicsRegion const& region, std::string const& title_)
{
    auto const title = converter.from_bytes(title_);
    auto const fwidth = region.width / title.size();

    FT_Set_Pixel_Sizes(face, fwidth, 0);

    int title_width = 0;
    unsigned int title_height = 0;

    for (auto const& ch : title)
    {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
        auto const glyph = face->glyph;
        FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

        title_width += glyph->advance.x >> 6;
        title_height = std::max(title_height, glyph->bitmap.rows);
    }

    int base_x = (region.width - title_width)/2;
    int base_y = title_height + (region.height- title_height)/2;

    for (auto const& ch : title)
    {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
        auto const glyph = face->glyph;
        FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

        auto const& bitmap = glyph->bitmap;
        auto const x = base_x + glyph->bitmap_left;

        if (static_cast<int>(x + bitmap.width) <= region.width)
        {
            unsigned char* src = bitmap.buffer;

            auto const y = base_y - glyph->bitmap_top;
            char* dest = region.vaddr + y*region.stride + 4*x;

            for (auto row = 0u; row != bitmap.rows; ++row)
            {
                for (auto col = 0u; col != 4*bitmap.width; ++col)
                    dest[col] |= src[col/4];

                src += bitmap.pitch;
                dest += region.stride;
            }
        }

        base_x += glyph->advance.x >> 6;
        base_y += glyph->advance.y >> 6;
    }
}

void egmde::Printer::footer(MirGraphicsRegion const& region, std::initializer_list<char const*> const& lines)
{
    int help_width = 0;
    unsigned int help_height = 0;
    unsigned int line_height = 0;

    for (auto const* rawline : lines)
    {
        int line_width = 0;

        auto const line = converter.from_bytes(rawline);

        auto const fwidth = region.width / 60;

        FT_Set_Pixel_Sizes(face, fwidth, 0);

        for (auto const& ch : line)
        {
            FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
            auto const glyph = face->glyph;
            FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

            line_width += glyph->advance.x >> 6;
            line_height = std::max(line_height, glyph->bitmap.rows + glyph->bitmap.rows/2);
        }

        if (help_width < line_width) help_width = line_width;
        help_height += line_height;
    }

    int base_y = (region.height - help_height);
    auto* const region_address = reinterpret_cast<char unsigned*>(region.vaddr);

    for (auto const* rawline : lines)
    {
        int base_x = (region.width - help_width)/2;

        auto const line = converter.from_bytes(rawline);

        for (auto const& ch : line)
        {
            FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
            auto const glyph = face->glyph;
            FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

            auto const& bitmap = glyph->bitmap;
            auto const x = base_x + glyph->bitmap_left;

            if (static_cast<int>(x + bitmap.width) <= region.width)
            {
                unsigned char* src = bitmap.buffer;

                auto const y = base_y - glyph->bitmap_top;
                auto* dest = region_address + y * region.stride + 4 * x;

                for (auto row = 0u; row != bitmap.rows; ++row)
                {
                    for (auto col = 0u; col != 4 * bitmap.width; ++col)
                    {
                        unsigned char pixel = (0xaf*src[col / 4]) / 0xff;
                        dest[col] = (0xff*pixel + (dest[col] * (0xff - pixel)))/0xff;
                    }

                    src += bitmap.pitch;
                    dest += region.stride;

                    if (dest > region_address + region.height * region.stride)
                        break;
                }
            }

            base_x += glyph->advance.x >> 6;
        }
        base_y += line_height;
    }
}
