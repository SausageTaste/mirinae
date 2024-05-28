local scene = require('scene')

do
    local e = scene.create_static_actor("asset/models/sponza/sponza.dmd")
    e:get_transform():set_scale(0.01, 0.01, 0.01)
    print("Static actor created:", e:get_id(), e:get_respath())
end

do
    local e = scene.create_skinned_actor("ThinMatrix/Character Running.dmd")
    e:get_transform():set_scale(0.15, 0.15, 0.15)
    print("Skinned actor created:", e:get_id(), e:get_respath())
end

do
    bard = scene.create_skinned_actor("Sung/bard/bard_subset.dmd")

    local a = bard:get_anim_state()
    a:set_anim_name("sc_groupdance_1")

    local t = bard:get_transform()
    t:set_pos(3, 0, -0.25)
    t:rotate(180, 0, 1, 0)

    print("Skinned actor created:", bard:get_id(), bard:get_respath())
end

do
    artist = scene.create_skinned_actor("Sung/artist/artist_subset.dmd")

    local a = artist:get_anim_state()
    a:set_anim_name("sc_groupdance_1")

    local t = artist:get_transform()
    t:set_pos(3, 0, 0.25)
    t:rotate(180, 0, 1, 0)

    print("Skinned actor created:", artist:get_id(), artist:get_respath())
end


function cycle_anim(offset)
    do
        local a = bard:get_anim_state()
        local idx = a:get_cur_anim_idx()
        if idx ~= nil then
            a:set_anim_idx((idx + offset) % a:get_anim_count())
        end
    end

    do
        local a = artist:get_anim_state()
        local idx = a:get_cur_anim_idx()
        if idx ~= nil then
            a:set_anim_idx((idx + offset) % a:get_anim_count())
        end
    end
end
