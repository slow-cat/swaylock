local seed_data = swaylock.read_file("/proc/sys/kernel/random/uuid")
local seed = 1729

if seed_data and #seed_data >= 8 then
  seed = 0
  for i = 1, 8 do
    seed = (seed * 257 + string.byte(seed_data, i)) % 2147483647
  end
end

math.randomseed(seed)
math.random()
math.random()
math.random()

local max_generation = 10
local trees = {}

local function random_between(low, high)
  return low + (high - low) * math.random()
end

local function copy_tips(tips)
  local copy = {}
  for i, tip in ipairs(tips) do
    copy[i] = tip
  end
  return copy
end

local function new_tree(width, height)
  local scale = math.min(width, height)
  local base_x = width / 2
  local base_y = height * 0.88
  local length = scale * 0.18
  local angle = -math.pi / 2 + random_between(-0.035, 0.035)
  local end_x = base_x + math.cos(angle) * length
  local end_y = base_y + math.sin(angle) * length
  local initial_tip = {
    x = end_x,
    y = end_y,
    length = length * random_between(0.68, 0.77),
    angle = angle,
    generation = 1,
  }

  return {
    branches = {
      {
        x1 = base_x,
        y1 = base_y,
        x2 = end_x,
        y2 = end_y,
        width = max_generation * scale * 0.00135,
      },
    },
    tips = { initial_tip },
    initial_tip = initial_tip,
    history = {},
    last_highlight = nil,
    scale = scale,
  }
end

local function tree_for(state, width, height)
  local key = table.concat({ state.output, width, height }, ":")
  if not trees[key] then
    trees[key] = new_tree(width, height)
  end
  return trees[key]
end

local function add_branch(tree, tip, angle, length_scale)
  local generation = tip.generation + 1
  local length = tip.length * length_scale
  local end_x = tip.x + math.cos(angle) * length
  local end_y = tip.y + math.sin(angle) * length

  tree.branches[#tree.branches + 1] = {
    x1 = tip.x,
    y1 = tip.y,
    x2 = end_x,
    y2 = end_y,
    width = math.max(1,
      (max_generation - generation + 1) * tree.scale * 0.00135),
  }

  return {
    x = end_x,
    y = end_y,
    length = length * random_between(0.68, 0.77),
    angle = angle,
    generation = generation,
  }
end

local function grow_tree(tree)
  local selected = {}
  local growable = {}
  local selected_count = 0

  for i, tip in ipairs(tree.tips) do
    if tip.generation < max_generation then
      growable[#growable + 1] = i
      if math.random() < 0.7 then
        selected[i] = true
        selected_count = selected_count + 1
      end
    end
  end

  if #growable == 0 then
    return
  end

  if selected_count == 0 then
    selected[growable[math.random(#growable)]] = true
  end

  tree.history[#tree.history + 1] = {
    branch_count = #tree.branches,
    tips = copy_tips(tree.tips),
  }

  local next_tips = {}
  for i, tip in ipairs(tree.tips) do
    if selected[i] then
      local spread = random_between(0.30, 0.52)
      next_tips[#next_tips + 1] = add_branch(tree, tip,
        tip.angle - spread, 1)
      next_tips[#next_tips + 1] = add_branch(tree, tip,
        tip.angle + spread * random_between(0.88, 1.12),
        random_between(0.91, 1.04))
    else
      next_tips[#next_tips + 1] = tip
    end
  end

  tree.tips = next_tips
end

local function shrink_tree(tree)
  local previous = tree.history[#tree.history]
  if not previous then
    return
  end

  for i = #tree.branches, previous.branch_count + 1, -1 do
    tree.branches[i] = nil
  end
  tree.tips = previous.tips
  tree.history[#tree.history] = nil
end

local function reset_tree(tree)
  for i = #tree.branches, 2, -1 do
    tree.branches[i] = nil
  end
  tree.tips = { tree.initial_tip }
  tree.history = {}
end

local function update_tree(tree, state)
  if state.input == "clear" then
    reset_tree(tree)
    tree.last_highlight = state.highlight_start
    return
  end

  if tree.last_highlight == nil then
    tree.last_highlight = state.highlight_start
    if state.input == "letter" then
      grow_tree(tree)
    end
    return
  end

  if state.highlight_start == tree.last_highlight then
    return
  end

  tree.last_highlight = state.highlight_start
  if state.input == "letter" then
    grow_tree(tree)
  elseif state.input == "backspace" then
    shrink_tree(tree)
  end
end

local function status(state)
  if state.auth == "verifying" then
    return "Verifying"
  elseif state.auth == "wrong" then
    return "Wrong password"
  elseif state.input == "clear" then
    return "Cleared"
  elseif state.caps_lock and state.show_caps_lock_text then
    return "Caps Lock"
  elseif state.show_failed_attempts and state.failed_attempts > 0 then
    return tostring(state.failed_attempts) .. " failed attempts"
  end
  return ""
end

return function(cr, width, height, state)
  local tree = tree_for(state, width, height)
  update_tree(tree, state)

  local color = state.colors.text.input
  cr:save()
  cr:set_source_rgba(color[1], color[2], color[3], color[4])

  for _, branch in ipairs(tree.branches) do
    cr:set_line_width(branch.width)
    cr:move_to(branch.x1, branch.y1)
    cr:line_to(branch.x2, branch.y2)
    cr:stroke()
  end

  local message = status(state)
  if message ~= "" then
    local font_size = state.font_size > 0 and state.font_size * state.scale or 22 * state.scale
    cr:select_font_face(state.font)
    cr:set_font_size(font_size)
    local text_width = cr:text_extents(message)
    cr:move_to((width - text_width) / 2, height - 34 * state.scale)
    cr:show_text(message)
  end

  cr:restore()
end
