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
    static std::string result;

    char const* const debian_path = "/usr/share/fonts/truetype/freefont/";
    char const* const fedora_path = "/usr/share/fonts/gnu-free/";
    char const* const fedora_path2= "/usr/share/fonts/liberation-sans/";
    char const* const arch_path   = "/usr/share/fonts/TTF/";
    char const* const snap_path   = "/snap/egmde/current/usr/share/fonts/truetype/freefont/";

    char const* const default_files[] = { "FreeSansBold.ttf", "LiberationSans-Bold.ttf" };

    for (auto const default_file : default_files)
    {
        for (auto const path : { debian_path, fedora_path, fedora_path2, arch_path, snap_path })
        {
            auto const full_path = std::string{path} + default_file;
            if (access(full_path.c_str(), R_OK) == 0)
            {
                result = full_path;
                return result.c_str();
            }
        }
    }

    return result.c_str();
}
}

egmde::Printer::Printer()
{
    static char const* font_file = getenv("EGMDE_FONT");

    if (!font_file) font_file = default_font();

    if (FT_Init_FreeType(&lib))
        return;

    if (FT_New_Face(lib, font_file, 0, &face))
    {
        FT_Done_FreeType(lib);
        throw std::runtime_error{std::string{"WARNING: failed to load font: \""} +  font_file + "\"\n"
            "(Hint: try setting EGMDE_FONT=<path to a font that exists>"};
    }
}

egmde::Printer::~Printer()
{
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
}

void egmde::Printer::print(int32_t width, int32_t height, char unsigned* region_address, std::initializer_list<std::string> const& lines)
{
    std::string::size_type title_chars = 0;

    for (auto const& title : lines)
        title_chars = std::max(title.size(), title_chars);

    auto const stride = 4*width;
    auto const fwidth = width / title_chars;
    auto const title_count = lines.size();

    FT_Set_Pixel_Sizes(face, fwidth, 0);

    int title_row = 0;

    for (auto title_ : lines)
    try
    {
        auto const title = converter.from_bytes(title_.c_str());

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

        int base_x = (width - title_width)/2;
        int base_y = ((++title_row)*height)/(title_count+1) + title_height/2;

        for (auto const& ch : title)
        {
            FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
            auto const glyph = face->glyph;
            FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

            auto const& bitmap = glyph->bitmap;
            auto const x = base_x + glyph->bitmap_left;

            if (static_cast<int>(x + bitmap.width) <= width)
            {
                unsigned char* src = bitmap.buffer;

                auto const y = base_y - glyph->bitmap_top;
                auto* dest = region_address + y*stride + 4*x;

                for (auto row = 0u; row != bitmap.rows; ++row)
                {
                    for (auto col = 0u; col != 4*bitmap.width; ++col)
                        dest[col] |= (title_row ==2) ? src[col/4] : src[col/4]/2;

                    src += bitmap.pitch;
                    dest += stride;
                }
            }

            base_x += glyph->advance.x >> 6;
            base_y += glyph->advance.y >> 6;
        }
    }
    catch (std::exception const& e)
    {
        puts(e.what());
        puts(title_.c_str());
        for (auto c : title_)
            printf("%2.2x", c & 0xff);
        printf("\n");
    }
}

void egmde::Printer::footer(int32_t width, int32_t height, char unsigned* region_address, std::initializer_list<char const*> const& lines)
{
    auto const stride = 4*width;

    int help_width = 0;
    unsigned int help_height = 0;
    unsigned int line_height = 0;

    for (auto const* rawline : lines)
    {
        int line_width = 0;

        auto const line = converter.from_bytes(rawline);

        auto const fwidth = width / 60;

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

    int base_y = (height - help_height);

    for (auto const* rawline : lines)
    {
        int base_x = (width - help_width) / 2;

        auto const line = converter.from_bytes(rawline);

        for (auto const& ch : line)
        {
            FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT);
            auto const glyph = face->glyph;
            FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

            auto const& bitmap = glyph->bitmap;
            auto const x = base_x + glyph->bitmap_left;

            if (static_cast<int>(x + bitmap.width) <= width)
            {
                unsigned char* src = bitmap.buffer;

                auto const y = base_y - glyph->bitmap_top;
                auto* dest = region_address + y * stride + 4 * x;

                for (auto row = 0u; row != bitmap.rows; ++row)
                {
                    for (auto col = 0u; col != 4 * bitmap.width; ++col)
                    {
                        unsigned char pixel = (0xaf*src[col / 4]) / 0xff;
                        dest[col] = (0xff*pixel + (dest[col] * (0xff - pixel)))/0xff;
                    }

                    src += bitmap.pitch;
                    dest += stride;

                    if (dest > region_address + height * stride)
                        break;
                }
            }

            base_x += glyph->advance.x >> 6;
        }
        base_y += line_height;
    }
}
