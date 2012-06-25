/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* keywords used within CSS property values */

/******

  This file contains the list of all parsed CSS keywords
  See nsCSSKeywords.h for access to the enum values for keywords

  It is designed to be used as inline input to nsCSSKeywords.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro CSS_KEY which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name, id). 'id' must always be the same as 'name'
  except that all hyphens ('-') in 'name' are converted to underscores ('_')
  in 'id'. This lets us do nice things with the macros without having to
  copy/convert strings at runtime.

  'name' entries *must* use only lowercase characters.

  ** Break these invariants and bad things will happen. **

 ******/

// OUTPUT_CLASS=nsCSSKeywords
// MACRO_NAME=CSS_KEY

CSS_KEY(-moz-activehyperlinktext, _moz_activehyperlinktext)
CSS_KEY(-moz-all, _moz_all)
CSS_KEY(-moz-alt-content, _moz_alt_content)
CSS_KEY(-moz-anchor-decoration, _moz_anchor_decoration)
CSS_KEY(-moz-arabic-indic, _moz_arabic_indic)
CSS_KEY(-moz-available, _moz_available)
CSS_KEY(-moz-bengali, _moz_bengali)
CSS_KEY(-moz-box, _moz_box)
CSS_KEY(-moz-button, _moz_button)
CSS_KEY(-moz-buttondefault, _moz_buttondefault)
CSS_KEY(-moz-buttonhoverface, _moz_buttonhoverface)
CSS_KEY(-moz-buttonhovertext, _moz_buttonhovertext)
CSS_KEY(-moz-cellhighlight, _moz_cellhighlight)
CSS_KEY(-moz-cellhighlighttext, _moz_cellhighlighttext)
CSS_KEY(-moz-center, _moz_center)
CSS_KEY(-moz-cjk-earthly-branch, _moz_cjk_earthly_branch)
CSS_KEY(-moz-cjk-heavenly-stem, _moz_cjk_heavenly_stem)
CSS_KEY(-moz-combobox, _moz_combobox)
CSS_KEY(-moz-comboboxtext, _moz_comboboxtext)
CSS_KEY(-moz-block-height, _moz_block_height)
CSS_KEY(-moz-deck, _moz_deck)
CSS_KEY(-moz-default-background-color, _moz_default_background_color)
CSS_KEY(-moz-default-color, _moz_default_color)
CSS_KEY(-moz-desktop, _moz_desktop)
CSS_KEY(-moz-devanagari, _moz_devanagari)
CSS_KEY(-moz-dialog, _moz_dialog)
CSS_KEY(-moz-dialogtext, _moz_dialogtext)
CSS_KEY(-moz-document, _moz_document)
CSS_KEY(-moz-dragtargetzone, _moz_dragtargetzone)
CSS_KEY(-moz-element, _moz_element)
CSS_KEY(-moz-eventreerow, _moz_eventreerow)
CSS_KEY(-moz-ethiopic-halehame, _moz_ethiopic_halehame)
CSS_KEY(-moz-ethiopic-numeric, _moz_ethiopic_numeric)
CSS_KEY(-moz-ethiopic-halehame-am, _moz_ethiopic_halehame_am)
CSS_KEY(-moz-ethiopic-halehame-ti-er, _moz_ethiopic_halehame_ti_er)
CSS_KEY(-moz-ethiopic-halehame-ti-et, _moz_ethiopic_halehame_ti_et)
CSS_KEY(-moz-field, _moz_field)
CSS_KEY(-moz-fieldtext, _moz_fieldtext)
CSS_KEY(-moz-fit-content, _moz_fit_content)
#ifdef MOZ_FLEXBOX
CSS_KEY(-moz-flex, _moz_flex)
#endif // MOZ_FLEXBOX
CSS_KEY(-moz-grabbing, _moz_grabbing)
CSS_KEY(-moz-grab, _moz_grab)
CSS_KEY(-moz-grid-group, _moz_grid_group)
CSS_KEY(-moz-grid-line, _moz_grid_line)
CSS_KEY(-moz-grid, _moz_grid)
CSS_KEY(-moz-groupbox, _moz_groupbox)
CSS_KEY(-moz-gujarati, _moz_gujarati)
CSS_KEY(-moz-gurmukhi, _moz_gurmukhi)
CSS_KEY(-moz-hangul-consonant, _moz_hangul_consonant)
CSS_KEY(-moz-hidden-unscrollable, _moz_hidden_unscrollable)
CSS_KEY(-moz-hangul, _moz_hangul)
CSS_KEY(-moz-hyperlinktext, _moz_hyperlinktext)
CSS_KEY(-moz-html-cellhighlight, _moz_html_cellhighlight)
CSS_KEY(-moz-html-cellhighlighttext, _moz_html_cellhighlighttext)
CSS_KEY(-moz-image-rect, _moz_image_rect)
CSS_KEY(-moz-info, _moz_info)
CSS_KEY(-moz-initial, _moz_initial)
CSS_KEY(-moz-inline-box, _moz_inline_box)
#ifdef MOZ_FLEXBOX
CSS_KEY(-moz-inline-flex, _moz_inline_flex)
#endif // MOZ_FLEXBOX
CSS_KEY(-moz-inline-grid, _moz_inline_grid)
CSS_KEY(-moz-inline-stack, _moz_inline_stack)
CSS_KEY(-moz-isolate, _moz_isolate)
CSS_KEY(-moz-japanese-formal, _moz_japanese_formal)
CSS_KEY(-moz-japanese-informal, _moz_japanese_informal)
CSS_KEY(-moz-kannada, _moz_kannada)
CSS_KEY(-moz-khmer, _moz_khmer)
CSS_KEY(-moz-lao, _moz_lao)
CSS_KEY(-moz-left, _moz_left)
CSS_KEY(-moz-list, _moz_list)
CSS_KEY(-moz-mac-alternateprimaryhighlight, _moz_mac_alternateprimaryhighlight)
CSS_KEY(-moz-mac-chrome-active, _moz_mac_chrome_active)
CSS_KEY(-moz-mac-chrome-inactive, _moz_mac_chrome_inactive)
CSS_KEY(-moz-mac-focusring, _moz_mac_focusring)
CSS_KEY(-moz-mac-menuselect, _moz_mac_menuselect)
CSS_KEY(-moz-mac-menushadow, _moz_mac_menushadow)
CSS_KEY(-moz-mac-menutextdisable, _moz_mac_menutextdisable)
CSS_KEY(-moz-mac-menutextselect, _moz_mac_menutextselect)
CSS_KEY(-moz-mac-disabledtoolbartext, _moz_mac_disabledtoolbartext)
CSS_KEY(-moz-mac-secondaryhighlight, _moz_mac_secondaryhighlight)
CSS_KEY(-moz-malayalam, _moz_malayalam)
CSS_KEY(-moz-max-content, _moz_max_content)
CSS_KEY(-moz-menuhover, _moz_menuhover)
CSS_KEY(-moz-menuhovertext, _moz_menuhovertext)
CSS_KEY(-moz-menubartext, _moz_menubartext)
CSS_KEY(-moz-menubarhovertext, _moz_menubarhovertext)
CSS_KEY(-moz-middle-with-baseline, _moz_middle_with_baseline)
CSS_KEY(-moz-min-content, _moz_min_content)
CSS_KEY(-moz-myanmar, _moz_myanmar)
CSS_KEY(-moz-nativehyperlinktext, _moz_nativehyperlinktext)
CSS_KEY(-moz-none, _moz_none)
CSS_KEY(-moz-oddtreerow, _moz_oddtreerow)
CSS_KEY(-moz-oriya, _moz_oriya)
CSS_KEY(-moz-persian, _moz_persian)
CSS_KEY(-moz-plaintext, _moz_plaintext)
CSS_KEY(-moz-popup, _moz_popup)
CSS_KEY(-moz-pull-down-menu, _moz_pull_down_menu)
CSS_KEY(-moz-right, _moz_right)
CSS_KEY(-moz-scrollbars-horizontal, _moz_scrollbars_horizontal)
CSS_KEY(-moz-scrollbars-none, _moz_scrollbars_none)
CSS_KEY(-moz-scrollbars-vertical, _moz_scrollbars_vertical)
CSS_KEY(-moz-show-background, _moz_show_background)
CSS_KEY(-moz-simp-chinese-formal, _moz_simp_chinese_formal)
CSS_KEY(-moz-simp-chinese-informal, _moz_simp_chinese_informal)
CSS_KEY(-moz-stack, _moz_stack)
CSS_KEY(-moz-tamil, _moz_tamil)
CSS_KEY(-moz-telugu, _moz_telugu)
CSS_KEY(-moz-thai, _moz_thai)
CSS_KEY(-moz-trad-chinese-formal, _moz_trad_chinese_formal)
CSS_KEY(-moz-trad-chinese-informal, _moz_trad_chinese_informal)
CSS_KEY(-moz-urdu, _moz_urdu)
CSS_KEY(-moz-use-system-font, _moz_use_system_font)
CSS_KEY(-moz-use-text-color, _moz_use_text_color)
CSS_KEY(-moz-visitedhyperlinktext, _moz_visitedhyperlinktext)
CSS_KEY(-moz-window, _moz_window)
CSS_KEY(-moz-workspace, _moz_workspace)
CSS_KEY(-moz-zoom-in, _moz_zoom_in)
CSS_KEY(-moz-zoom-out, _moz_zoom_out)
CSS_KEY(absolute, absolute)
CSS_KEY(active, active)
CSS_KEY(activeborder, activeborder)
CSS_KEY(activecaption, activecaption)
CSS_KEY(alias, alias)
CSS_KEY(all, all)
CSS_KEY(all-scroll, all_scroll)
CSS_KEY(alternate, alternate)
CSS_KEY(alternate-reverse, alternate_reverse)
CSS_KEY(always, always)
CSS_KEY(appworkspace, appworkspace)
CSS_KEY(armenian, armenian)
CSS_KEY(auto, auto)
CSS_KEY(avoid, avoid)
CSS_KEY(background, background)
CSS_KEY(backwards, backwards)
CSS_KEY(baseline, baseline)
CSS_KEY(bidi-override, bidi_override)
CSS_KEY(blink, blink)
CSS_KEY(block, block)
CSS_KEY(block-axis, block_axis)
CSS_KEY(bold, bold)
CSS_KEY(bolder, bolder)
CSS_KEY(border-box, border_box)
CSS_KEY(both, both)
CSS_KEY(bottom, bottom)
CSS_KEY(bottom-outside, bottom_outside)
CSS_KEY(bounding-box, bounding_box)
CSS_KEY(break-all, break_all)
CSS_KEY(break-word, break_word)
CSS_KEY(button, button)
CSS_KEY(buttonface, buttonface)
CSS_KEY(buttonhighlight, buttonhighlight)
CSS_KEY(buttonshadow, buttonshadow)
CSS_KEY(buttontext, buttontext)
CSS_KEY(capitalize, capitalize)
CSS_KEY(caption, caption)
CSS_KEY(captiontext, captiontext)
CSS_KEY(cell, cell)
CSS_KEY(center, center)
CSS_KEY(ch, ch)
CSS_KEY(circle, circle)
CSS_KEY(cjk-ideographic, cjk_ideographic)
CSS_KEY(clip, clip)
CSS_KEY(close-quote, close_quote)
CSS_KEY(closest-corner, closest_corner)
CSS_KEY(closest-side, closest_side)
CSS_KEY(cm, cm)
CSS_KEY(col-resize, col_resize)
CSS_KEY(collapse, collapse)
#ifdef MOZ_FLEXBOX
CSS_KEY(column, column)
CSS_KEY(column-reverse, column_reverse)
#endif // MOZ_FLEXBOX
CSS_KEY(condensed, condensed)
CSS_KEY(contain, contain)
CSS_KEY(content-box, content_box)
CSS_KEY(context-menu, context_menu)
CSS_KEY(continuous, continuous)
CSS_KEY(copy, copy)
CSS_KEY(cover, cover)
CSS_KEY(crop, crop)
CSS_KEY(cross, cross)
CSS_KEY(crosshair, crosshair)
CSS_KEY(currentcolor, currentcolor)
CSS_KEY(dashed, dashed)
CSS_KEY(decimal, decimal)
CSS_KEY(decimal-leading-zero, decimal_leading_zero)
CSS_KEY(default, default)
CSS_KEY(deg, deg)
CSS_KEY(dialog, dialog)
CSS_KEY(disabled, disabled)
CSS_KEY(disc, disc)
CSS_KEY(dotted, dotted)
CSS_KEY(double, double)
CSS_KEY(e-resize, e_resize)
CSS_KEY(each-box, each_box)
CSS_KEY(ease, ease)
CSS_KEY(ease-in, ease_in)
CSS_KEY(ease-in-out, ease_in_out)
CSS_KEY(ease-out, ease_out)
CSS_KEY(element, element)
CSS_KEY(elements, elements)
CSS_KEY(ellipse, ellipse)
CSS_KEY(ellipsis, ellipsis)
CSS_KEY(em, em)
CSS_KEY(embed, embed)
CSS_KEY(enabled, enabled)
CSS_KEY(end, end)
CSS_KEY(ex, ex)
CSS_KEY(expanded, expanded)
CSS_KEY(extra-condensed, extra_condensed)
CSS_KEY(extra-expanded, extra_expanded)
CSS_KEY(ew-resize, ew_resize)
CSS_KEY(farthest-side, farthest_side)
CSS_KEY(farthest-corner, farthest_corner)
CSS_KEY(fill, fill)
CSS_KEY(fixed, fixed)
CSS_KEY(flat, flat)
CSS_KEY(flex-end, flex_end)
CSS_KEY(flex-start, flex_start)
CSS_KEY(forwards, forwards)
CSS_KEY(georgian, georgian)
CSS_KEY(grad, grad)
CSS_KEY(graytext, graytext)
CSS_KEY(groove, groove)
CSS_KEY(hebrew, hebrew)
CSS_KEY(help, help)
CSS_KEY(hidden, hidden)
CSS_KEY(hide, hide)
CSS_KEY(highlight, highlight)
CSS_KEY(highlighttext, highlighttext)
CSS_KEY(hiragana, hiragana)
CSS_KEY(hiragana-iroha, hiragana_iroha)
CSS_KEY(horizontal, horizontal)
CSS_KEY(hz, hz)
CSS_KEY(icon, icon)
CSS_KEY(ignore, ignore)
CSS_KEY(in, in)
CSS_KEY(interlace, interlace)
CSS_KEY(inactive, inactive)
CSS_KEY(inactiveborder, inactiveborder)
CSS_KEY(inactivecaption, inactivecaption)
CSS_KEY(inactivecaptiontext, inactivecaptiontext)
CSS_KEY(infinite, infinite)
CSS_KEY(infobackground, infobackground)
CSS_KEY(infotext, infotext)
CSS_KEY(inherit, inherit)
CSS_KEY(inline, inline)
CSS_KEY(inline-axis, inline_axis)
CSS_KEY(inline-block, inline_block)
CSS_KEY(inline-table, inline_table)
CSS_KEY(inset, inset)
CSS_KEY(inside, inside)
CSS_KEY(interpolatematrix, interpolatematrix)
CSS_KEY(italic, italic)
CSS_KEY(justify, justify)
CSS_KEY(katakana, katakana)
CSS_KEY(katakana-iroha, katakana_iroha)
CSS_KEY(keep-all, keep_all)
CSS_KEY(khz, khz)
CSS_KEY(landscape, landscape)
CSS_KEY(large, large)
CSS_KEY(larger, larger)
CSS_KEY(left, left)
CSS_KEY(lighter, lighter)
CSS_KEY(line-through, line_through)
CSS_KEY(linear, linear)
CSS_KEY(list-item, list_item)
CSS_KEY(logical, logical)
CSS_KEY(lower-alpha, lower_alpha)
CSS_KEY(lower-greek, lower_greek)
CSS_KEY(lower-latin, lower_latin)
CSS_KEY(lower-roman, lower_roman)
CSS_KEY(lowercase, lowercase)
CSS_KEY(ltr, ltr)
CSS_KEY(manual, manual)
CSS_KEY(margin-box, margin_box)
CSS_KEY(matrix, matrix)
CSS_KEY(matrix3d, matrix3d)
CSS_KEY(medium, medium)
CSS_KEY(menu, menu)
CSS_KEY(menutext, menutext)
CSS_KEY(message-box, message_box)
CSS_KEY(middle, middle)
CSS_KEY(mix, mix)
CSS_KEY(mm, mm)
CSS_KEY(move, move)
CSS_KEY(ms, ms)
CSS_KEY(n-resize, n_resize)
CSS_KEY(narrower, narrower)
CSS_KEY(ne-resize, ne_resize)
CSS_KEY(nesw-resize, nesw_resize)
CSS_KEY(no-close-quote, no_close_quote)
CSS_KEY(no-drop, no_drop)
CSS_KEY(no-open-quote, no_open_quote)
CSS_KEY(no-repeat, no_repeat)
CSS_KEY(none, none)
CSS_KEY(normal, normal)
CSS_KEY(not-allowed, not_allowed)
CSS_KEY(nowrap, nowrap)
CSS_KEY(ns-resize, ns_resize)
CSS_KEY(nw-resize, nw_resize)
CSS_KEY(nwse-resize, nwse_resize)
CSS_KEY(oblique, oblique)
CSS_KEY(open-quote, open_quote)
CSS_KEY(outset, outset)
CSS_KEY(outside, outside)
CSS_KEY(overline, overline)
CSS_KEY(padding-box, padding_box)
CSS_KEY(painted, painted)
CSS_KEY(paused, paused)
CSS_KEY(pc, pc)
CSS_KEY(perspective, perspective)
CSS_KEY(physical, physical)
CSS_KEY(pointer, pointer)
CSS_KEY(portrait, portrait)
CSS_KEY(pre, pre)
CSS_KEY(pre-wrap, pre_wrap)
CSS_KEY(pre-line, pre_line)
CSS_KEY(preserve-3d, preserve_3d)
CSS_KEY(progress, progress)
CSS_KEY(progressive, progressive)
CSS_KEY(pt, pt)
CSS_KEY(px, px)
CSS_KEY(rad, rad)
CSS_KEY(read-only, read_only)
CSS_KEY(read-write, read_write)
CSS_KEY(relative, relative)
CSS_KEY(repeat, repeat)
CSS_KEY(repeat-x, repeat_x)
CSS_KEY(repeat-y, repeat_y)
CSS_KEY(reverse, reverse)
CSS_KEY(ridge, ridge)
CSS_KEY(right, right)
CSS_KEY(rotate, rotate)
CSS_KEY(rotate3d, rotate3d)
CSS_KEY(rotatex, rotatex)
CSS_KEY(rotatey, rotatey)
CSS_KEY(rotatez, rotatez)
CSS_KEY(round, round)
#ifdef MOZ_FLEXBOX
CSS_KEY(row, row)
#endif // MOZ_FLEXBOX
CSS_KEY(row-resize, row_resize)
#ifdef MOZ_FLEXBOX
CSS_KEY(row-reverse, row_reverse)
#endif // MOZ_FLEXBOX
CSS_KEY(rtl, rtl)
CSS_KEY(running, running)
CSS_KEY(s, s)
CSS_KEY(s-resize, s_resize)
CSS_KEY(scale, scale)
CSS_KEY(scale3d, scale3d)
CSS_KEY(scalex, scalex)
CSS_KEY(scaley, scaley)
CSS_KEY(scalez, scalez)
CSS_KEY(scroll, scroll)
CSS_KEY(scrollbar, scrollbar)
CSS_KEY(scrollbar-small, scrollbar_small)
CSS_KEY(se-resize, se_resize)
CSS_KEY(select-after, select_after)
CSS_KEY(select-all, select_all)
CSS_KEY(select-before, select_before)
CSS_KEY(select-menu, select_menu)
CSS_KEY(select-same, select_same)
CSS_KEY(semi-condensed, semi_condensed)
CSS_KEY(semi-expanded, semi_expanded)
CSS_KEY(separate, separate)
CSS_KEY(show, show)
CSS_KEY(skewx, skewx)
CSS_KEY(skewy, skewy)
CSS_KEY(small, small)
CSS_KEY(small-caps, small_caps)
CSS_KEY(small-caption, small_caption)
CSS_KEY(smaller, smaller)
CSS_KEY(soft, soft)
CSS_KEY(solid, solid)
CSS_KEY(space-around, space_around)
CSS_KEY(space-between, space_between)
CSS_KEY(square, square)
CSS_KEY(start, start)
CSS_KEY(static, static)
CSS_KEY(status-bar, status_bar)
CSS_KEY(step-end, step_end)
CSS_KEY(step-start, step_start)
CSS_KEY(stretch, stretch)
CSS_KEY(stretch-to-fit, stretch_to_fit)
CSS_KEY(stroke, stroke)
CSS_KEY(sub, sub)
CSS_KEY(super, super)
CSS_KEY(sw-resize, sw_resize)
CSS_KEY(table, table)
CSS_KEY(table-caption, table_caption)
CSS_KEY(table-cell, table_cell)
CSS_KEY(table-column, table_column)
CSS_KEY(table-column-group, table_column_group)
CSS_KEY(table-footer-group, table_footer_group)
CSS_KEY(table-header-group, table_header_group)
CSS_KEY(table-row, table_row)
CSS_KEY(table-row-group, table_row_group)
CSS_KEY(text, text)
CSS_KEY(text-bottom, text_bottom)
CSS_KEY(text-top, text_top)
CSS_KEY(thick, thick)
CSS_KEY(thin, thin)
CSS_KEY(threeddarkshadow, threeddarkshadow)
CSS_KEY(threedface, threedface)
CSS_KEY(threedhighlight, threedhighlight)
CSS_KEY(threedlightshadow, threedlightshadow)
CSS_KEY(threedshadow, threedshadow)
CSS_KEY(toggle, toggle)
CSS_KEY(top, top)
CSS_KEY(top-outside, top_outside)
CSS_KEY(translate, translate)
CSS_KEY(translate3d, translate3d)
CSS_KEY(translatex, translatex)
CSS_KEY(translatey, translatey)
CSS_KEY(translatez, translatez)
CSS_KEY(transparent, transparent) // for nsComputedDOMStyle only
CSS_KEY(tri-state, tri_state)
CSS_KEY(ultra-condensed, ultra_condensed)
CSS_KEY(ultra-expanded, ultra_expanded)
CSS_KEY(underline, underline)
CSS_KEY(upper-alpha, upper_alpha)
CSS_KEY(upper-latin, upper_latin)
CSS_KEY(upper-roman, upper_roman)
CSS_KEY(uppercase, uppercase)
CSS_KEY(vertical, vertical)
CSS_KEY(vertical-text, vertical_text)
CSS_KEY(visible, visible)
CSS_KEY(visiblefill, visiblefill)
CSS_KEY(visiblepainted, visiblepainted)
CSS_KEY(visiblestroke, visiblestroke)
CSS_KEY(w-resize, w_resize)
CSS_KEY(wait, wait)
CSS_KEY(wavy, wavy)
CSS_KEY(wider, wider)
CSS_KEY(window, window)
CSS_KEY(windowframe, windowframe)
CSS_KEY(windowtext, windowtext)
CSS_KEY(write-only, write_only)
CSS_KEY(x-large, x_large)
CSS_KEY(x-small, x_small)
CSS_KEY(xx-large, xx_large)
CSS_KEY(xx-small, xx_small)

// Appearance keywords for widget styles
CSS_KEY(radio, radio)
CSS_KEY(checkbox, checkbox)
CSS_KEY(button-bevel, button_bevel)
CSS_KEY(toolbox, toolbox)
CSS_KEY(toolbar, toolbar)
CSS_KEY(toolbarbutton, toolbarbutton)
CSS_KEY(toolbargripper, toolbargripper)
CSS_KEY(dualbutton, dualbutton)
CSS_KEY(toolbarbutton-dropdown, toolbarbutton_dropdown)
CSS_KEY(button-arrow-up, button_arrow_up)
CSS_KEY(button-arrow-down, button_arrow_down)
CSS_KEY(button-arrow-next, button_arrow_next)
CSS_KEY(button-arrow-previous, button_arrow_previous)
CSS_KEY(separator, separator)
CSS_KEY(splitter, splitter)
CSS_KEY(statusbar, statusbar)
CSS_KEY(statusbarpanel, statusbarpanel)
CSS_KEY(resizerpanel, resizerpanel)
CSS_KEY(resizer, resizer)
CSS_KEY(listbox, listbox)
CSS_KEY(listitem, listitem)
CSS_KEY(treeview, treeview)
CSS_KEY(treeitem, treeitem)
CSS_KEY(treetwisty, treetwisty)
CSS_KEY(treetwistyopen, treetwistyopen)
CSS_KEY(treeline, treeline)
CSS_KEY(treeheader, treeheader)
CSS_KEY(treeheadercell, treeheadercell)
CSS_KEY(treeheadersortarrow, treeheadersortarrow)
CSS_KEY(progressbar, progressbar)
CSS_KEY(progressbar-vertical, progressbar_vertical)
CSS_KEY(progresschunk, progresschunk)
CSS_KEY(progresschunk-vertical, progresschunk_vertical)
CSS_KEY(tab, tab)
CSS_KEY(tabpanels, tabpanels)
CSS_KEY(tabpanel, tabpanel)
CSS_KEY(tab-scroll-arrow-back, tabscrollarrow_back)
CSS_KEY(tab-scroll-arrow-forward, tabscrollarrow_forward)
CSS_KEY(tooltip, tooltip)
CSS_KEY(spinner, spinner)
CSS_KEY(spinner-upbutton, spinner_upbutton)
CSS_KEY(spinner-downbutton, spinner_downbutton)
CSS_KEY(spinner-textfield, spinner_textfield)
CSS_KEY(scrollbarbutton-up, scrollbarbutton_up)
CSS_KEY(scrollbarbutton-down, scrollbarbutton_down)
CSS_KEY(scrollbarbutton-left, scrollbarbutton_left)
CSS_KEY(scrollbarbutton-right, scrollbarbutton_right)
CSS_KEY(scrollbartrack-horizontal, scrollbartrack_horizontal)
CSS_KEY(scrollbartrack-vertical, scrollbartrack_vertical)
CSS_KEY(scrollbarthumb-horizontal, scrollbarthumb_horizontal)
CSS_KEY(scrollbarthumb-vertical, scrollbarthumb_vertical)
CSS_KEY(sheet, sheet)
CSS_KEY(textfield, textfield)
CSS_KEY(textfield-multiline, textfield_multiline)
CSS_KEY(caret, caret)
CSS_KEY(searchfield, searchfield)
CSS_KEY(menubar, menubar)
CSS_KEY(menupopup, menupopup)
CSS_KEY(menuitem, menuitem)
CSS_KEY(checkmenuitem, checkmenuitem)
CSS_KEY(radiomenuitem, radiomenuitem)
CSS_KEY(menucheckbox, menucheckbox)
CSS_KEY(menuradio, menuradio)
CSS_KEY(menuseparator, menuseparator)
CSS_KEY(menuarrow, menuarrow)
CSS_KEY(menuimage, menuimage)
CSS_KEY(menuitemtext, menuitemtext)
CSS_KEY(menulist, menulist)
CSS_KEY(menulist-button, menulistbutton)
CSS_KEY(menulist-text, menulisttext)
CSS_KEY(menulist-textfield, menulisttextfield)
CSS_KEY(meterbar, meterbar)
CSS_KEY(meterchunk, meterchunk)
CSS_KEY(scale-horizontal, scale_horizontal)
CSS_KEY(scale-vertical, scale_vertical)
CSS_KEY(scalethumb-horizontal, scalethumb_horizontal)
CSS_KEY(scalethumb-vertical, scalethumb_vertical)
CSS_KEY(scalethumbstart, scalethumbstart)
CSS_KEY(scalethumbend, scalethumbend)
CSS_KEY(scalethumbtick, scalethumbtick)
CSS_KEY(groupbox, groupbox)
CSS_KEY(checkbox-container, checkboxcontainer)
CSS_KEY(radio-container, radiocontainer)
CSS_KEY(checkbox-label, checkboxlabel)
CSS_KEY(radio-label, radiolabel)
CSS_KEY(button-focus, buttonfocus)
CSS_KEY(-moz-win-media-toolbox, _moz_win_media_toolbox)
CSS_KEY(-moz-win-communications-toolbox, _moz_win_communications_toolbox)
CSS_KEY(-moz-win-browsertabbar-toolbox, _moz_win_browsertabbar_toolbox)
CSS_KEY(-moz-win-mediatext, _moz_win_mediatext)
CSS_KEY(-moz-win-communicationstext, _moz_win_communicationstext)
CSS_KEY(-moz-win-glass, _moz_win_glass)
CSS_KEY(-moz-win-borderless-glass, _moz_win_borderless_glass)
CSS_KEY(-moz-window-titlebar, _moz_window_titlebar)
CSS_KEY(-moz-window-titlebar-maximized, _moz_window_titlebar_maximized)
CSS_KEY(-moz-window-frame-left, _moz_window_frame_left)
CSS_KEY(-moz-window-frame-right, _moz_window_frame_right)
CSS_KEY(-moz-window-frame-bottom, _moz_window_frame_bottom)
CSS_KEY(-moz-window-button-close, _moz_window_button_close)
CSS_KEY(-moz-window-button-minimize, _moz_window_button_minimize)
CSS_KEY(-moz-window-button-maximize, _moz_window_button_maximize)
CSS_KEY(-moz-window-button-restore, _moz_window_button_restore)
CSS_KEY(-moz-window-button-box, _moz_window_button_box)
CSS_KEY(-moz-window-button-box-maximized, _moz_window_button_box_maximized)
CSS_KEY(-moz-mac-unified-toolbar, _moz_mac_unified_toolbar)
CSS_KEY(-moz-win-exclude-glass, _moz_win_exclude_glass)
CSS_KEY(alphabetic, alphabetic)
CSS_KEY(bevel, bevel)
CSS_KEY(butt, butt)
CSS_KEY(central, central)
CSS_KEY(crispedges, crispedges)
//CSS_KEY(end, end)
CSS_KEY(evenodd, evenodd)
CSS_KEY(geometricprecision, geometricprecision)
CSS_KEY(hanging, hanging)
CSS_KEY(ideographic, ideographic)
CSS_KEY(linearrgb, linearrgb)
CSS_KEY(mathematical, mathematical)
//CSS_KEY(middle, middle)
CSS_KEY(miter, miter)
CSS_KEY(no-change, no_change)
CSS_KEY(non-scaling-stroke, non_scaling_stroke)
CSS_KEY(nonzero, nonzero)
CSS_KEY(optimizelegibility, optimizelegibility)
CSS_KEY(optimizequality, optimizequality)
CSS_KEY(optimizespeed, optimizespeed)
CSS_KEY(reset-size, reset_size)
//CSS_KEY(square, square)
//CSS_KEY(start, start)
CSS_KEY(srgb, srgb)
CSS_KEY(text-after-edge, text_after_edge)
CSS_KEY(text-before-edge, text_before_edge)
CSS_KEY(use-script, use_script)
CSS_KEY(-moz-crisp-edges, _moz_crisp_edges)
