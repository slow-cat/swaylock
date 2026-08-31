local pi = math.pi

local function set_color(cr, color)
	cr:set_source_rgba(color[1], color[2], color[3], color[4])
end

local function indicator_state(state)
	if state.input == "clear" then
		return "clear"
	elseif state.auth == "verifying" then
		return "verifying"
	elseif state.auth == "wrong" then
		return "wrong"
	elseif state.caps_lock and state.show_caps_lock_indicator then
		return "caps_lock"
	end
	return "input"
end

local function indicator_text(state)
	if state.input == "clear" then
		return "Cleared"
	elseif state.auth == "verifying" then
		return "Verifying"
	elseif state.auth == "wrong" then
		return "Wrong"
	elseif state.caps_lock and state.show_caps_lock_text then
		return "Caps Lock"
	elseif state.show_failed_attempts and state.failed_attempts > 0 then
		return state.failed_attempts > 999 and "999+" or tostring(state.failed_attempts)
	end
	return nil
end

return function(cr, width, height, state)
	local colors = state.colors
	set_color(cr, colors.background)
	cr:paint()

	local visible = state.auth ~= "idle"
		or state.input ~= "idle"
		or state.indicator_idle_visible
	if not visible then
		return
	end

	local scale = state.scale
	local radius = state.radius * scale
	local thickness = state.thickness * scale
	local center_x = width / 2
	local center_y = height / 2
	local current = indicator_state(state)

	cr:set_line_width(0)
	cr:arc(center_x, center_y, radius - thickness / 2, 0, 2 * pi)
	set_color(cr, colors.inside[current] or colors.inside.input)
	cr:fill_preserve()
	cr:stroke()

	cr:set_line_width(thickness)
	cr:arc(center_x, center_y, radius, 0, 2 * pi)
	set_color(cr, colors.ring[current] or colors.ring.input)
	cr:stroke()

	local text = indicator_text(state)
	cr:select_font_face(state.font)
	cr:set_font_size(state.font_size > 0 and state.font_size or radius / 3)
	local text_color = current
	if state.caps_lock and not state.show_caps_lock_indicator
			and state.show_caps_lock_text and current == "input" then
		text_color = "caps_lock"
	end
	set_color(cr, colors.text[text_color] or colors.text.input)

	if text then
		local text_width, _, x_bearing = cr:text_extents(text)
		local _, descent, font_height = cr:font_extents()
		cr:move_to(center_x - (text_width / 2 + x_bearing),
			center_y + (font_height / 2 - descent))
		cr:show_text(text)
		cr:close_path()
		cr:new_path()
	end

	if state.input == "letter" or state.input == "backspace" then
		local highlight_start = state.highlight_start * (pi / 1024)
		local highlight_range = pi / 3
		cr:set_line_width(thickness)
		cr:arc(center_x, center_y, radius,
			highlight_start, highlight_start + highlight_range)
		local highlight_color
		if state.input == "letter" then
			highlight_color = state.caps_lock and state.show_caps_lock_indicator
				and colors.caps_lock_key_highlight or colors.key_highlight
		else
			highlight_color = state.caps_lock and state.show_caps_lock_indicator
				and colors.caps_lock_backspace_highlight or colors.backspace_highlight
		end
		set_color(cr, highlight_color)
		cr:stroke()

		local inner_radius = radius - thickness / 2
		local outer_radius = radius + thickness / 2
		cr:set_line_width(2 * scale)
		set_color(cr, colors.separator)
		for _, angle in ipairs({ highlight_start, highlight_start + highlight_range }) do
			cr:move_to(center_x + math.cos(angle) * inner_radius,
				center_y + math.sin(angle) * inner_radius)
			cr:line_to(center_x + math.cos(angle) * outer_radius,
				center_y + math.sin(angle) * outer_radius)
			cr:stroke()
		end
	end

	set_color(cr, colors.line[current] or colors.line.input)
	cr:set_line_width(2 * scale)
	cr:arc(center_x, center_y, radius - thickness / 2, 0, 2 * pi)
	cr:stroke()
	cr:arc(center_x, center_y, radius + thickness / 2, 0, 2 * pi)
	cr:stroke()
end
