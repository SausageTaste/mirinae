local scene = require('scene')

do
    local e = scene.create_static_actor("asset/models/sponza/sponza.dmd")
    e:get_transform():set_scale(0.01, 0.01, 0.01)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("ThinMatrix/Character Running.dmd")

    local a = e:get_anim_state()
    a:set_anim_idx(0)

    local t = e:get_transform()
    t:set_pos(-3, 0, -0.5)
    t:rotate(90, 0, 1, 0)
    t:set_scale(0.15, 0.15, 0.15)

    print("Skinned actor created:", e:get_id(), e:get_respath())
end

do
    bard = scene.create_skinned_actor("Sung/bard/bard_subset.dmd")

    local a = bard:get_anim_state()
    a:set_anim_name("sc_groupdance_10")

    local t = bard:get_transform()
    t:set_pos(3, 0, 0.5)
    t:rotate(180, 0, 1, 0)

    print("Skinned actor created:", bard:get_id(), bard:get_respath())
end

do
    artist = scene.create_skinned_actor("Sung/artist/artist_subset.dmd")

    local a = artist:get_anim_state()
    a:set_anim_name("sc_groupdance_10")

    local t = artist:get_transform()
    t:set_pos(3, 0, 0)
    t:rotate(180, 0, 1, 0)

    print("Skinned actor created:", artist:get_id(), artist:get_respath())
end

do
    slayer = scene.create_skinned_actor("Sung/slayer/slayer_subset.dmd")

    local a = slayer:get_anim_state()
    a:set_anim_name("sc_groupdance_10")

    local t = slayer:get_transform()
    t:set_pos(3, 0, -0.5)
    t:rotate(180, 0, 1, 0)

    print("Skinned actor created:", slayer:get_id(), slayer:get_respath())
end


function cycle_anim(offset)
    actors = {bard, artist, slayer}

    for _, actor in ipairs(actors) do
        local a = actor:get_anim_state()
        local idx = a:get_cur_anim_idx()
        if idx ~= nil then
            a:set_anim_idx((idx + offset) % a:get_anim_count())
        end
    end
end


function set_anim_speed(speed)
    actors = {bard, artist, slayer}

    for _, actor in ipairs(actors) do
        local a = actor:get_anim_state()
        a:set_anim_speed(speed)
    end
end
